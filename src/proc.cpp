#include "proc.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include "format.h"
#include "fsutil.h"
#include "plat.h"
#include "strutil.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace poly {

namespace {

double now_ms() {
    using namespace std::chrono;
    static const steady_clock::time_point t0 = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - t0).count();
}

double effective_wall_limit(const ProcOptions& opt) {
    if (opt.wallLimitMs > 0) return opt.wallLimitMs;
    if (opt.timeLimitMs > 0) return opt.timeLimitMs * 2.0 + 2000.0;
    return 0;
}

}  // namespace

#ifdef _WIN32

static std::string win_error_message(DWORD code) {
    LPWSTR buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buf, 0,
                             nullptr);
    std::string msg;
    if (n && buf) {
        msg = to_utf8(std::wstring(buf, n));
        msg = trim(msg);
    }
    if (msg.empty()) msg = fmt("windows error %lu", static_cast<unsigned long>(code));
    if (buf) LocalFree(buf);
    return msg;
}

static HANDLE create_job(long long memKb) {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    memset(&info, 0, sizeof(info));
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (memKb > 0) {
        info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        info.ProcessMemoryLimit = static_cast<SIZE_T>(memKb * 1024 + (2 << 20));
    }
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    return job;
}

typedef BOOL(WINAPI* query_cycles_t)(HANDLE, PULONG64);

static query_cycles_t query_thread_cycles() {
    static query_cycles_t fn = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (k32) fn = reinterpret_cast<query_cycles_t>(reinterpret_cast<void*>(GetProcAddress(k32, "QueryThreadCycleTime")));
    }
    return fn;
}

static query_cycles_t query_process_cycles() {
    static query_cycles_t fn = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (k32)
            fn = reinterpret_cast<query_cycles_t>(
                reinterpret_cast<void*>(GetProcAddress(k32, "QueryProcessCycleTime")));
    }
    return fn;
}

// Cycle counter frequency, measured against QueryPerformanceCounter.  This
// gives sub-millisecond CPU measurements where GetProcessTimes() would be
// quantised to the 15.6 ms system timer tick.
static double cpu_cycles_per_second() {
    static double hz = 0;
    static bool measured = false;
    if (measured) return hz;
    measured = true;
    query_cycles_t qct = query_thread_cycles();
    if (!qct) return hz;
    LARGE_INTEGER freq, a, b;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) return hz;
    ULONG64 c0 = 0, c1 = 0;
    if (!qct(GetCurrentThread(), &c0)) return hz;
    QueryPerformanceCounter(&a);
    volatile unsigned long long spin = 0;
    double elapsed = 0;
    do {
        for (int i = 0; i < 20000; i++) spin += static_cast<unsigned long long>(i);
        QueryPerformanceCounter(&b);
        elapsed = static_cast<double>(b.QuadPart - a.QuadPart) / static_cast<double>(freq.QuadPart);
    } while (elapsed < 0.01);
    if (!qct(GetCurrentThread(), &c1)) return hz;
    double rate = static_cast<double>(c1 - c0) / elapsed;
    if (rate > 1e8) hz = rate;
    return hz;
}

static bool get_cpu_ms(HANDLE proc, double& cpuMs) {
    query_cycles_t qpc_cycles = query_process_cycles();
    double hz = cpu_cycles_per_second();
    if (qpc_cycles && hz > 0) {
        ULONG64 cycles = 0;
        if (qpc_cycles(proc, &cycles)) {
            cpuMs = static_cast<double>(cycles) * 1000.0 / hz;
            return true;
        }
    }
    FILETIME creation, exitT, kernel, user;
    if (!GetProcessTimes(proc, &creation, &exitT, &kernel, &user)) return false;
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    cpuMs = static_cast<double>(k.QuadPart + u.QuadPart) / 10000.0;
    return true;
}

static void get_memory(HANDLE proc, long long& peakWsKb, long long& peakCommitKb) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(proc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        long long ws = static_cast<long long>(pmc.PeakWorkingSetSize) / 1024;
        long long cs = static_cast<long long>(pmc.PeakPagefileUsage) / 1024;
        if (ws > peakWsKb) peakWsKb = ws;
        if (cs > peakCommitKb) peakCommitKb = cs;
    }
}

struct WinSpawn {
    HANDLE job = nullptr;
    HANDLE process = nullptr;
    HANDLE thread = nullptr;
    DWORD pid = 0;
    bool ok = false;
    std::string error;
};

static WinSpawn win_spawn(const std::string& exe, const std::vector<std::string>& args,
                          const std::string& workDir, HANDLE hIn, HANDLE hOut, HANDLE hErr, long long memKb) {
    WinSpawn sp;
    std::wstring wexe = to_wide(exe);
    std::wstring wcmd = to_wide(quote_arg(exe) + (args.empty() ? std::string() : " " + join_args(args)));
    std::wstring wdir = workDir.empty() ? std::wstring() : to_wide(workDir);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hIn ? hIn : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hOut ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = hErr ? hErr : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    std::vector<wchar_t> cmdBuf(wcmd.begin(), wcmd.end());
    cmdBuf.push_back(L'\0');

    if (!CreateProcessW(wexe.c_str(), cmdBuf.data(), nullptr, nullptr, TRUE, flags, nullptr,
                        wdir.empty() ? nullptr : wdir.c_str(), &si, &pi)) {
        sp.error = str("无法启动进程 {}: {}", exe, win_error_message(GetLastError()));
        return sp;
    }

    sp.job = create_job(memKb);
    if (sp.job) {
        if (!AssignProcessToJobObject(sp.job, pi.hProcess)) {
            CloseHandle(sp.job);
            sp.job = nullptr;
        }
    }
    ResumeThread(pi.hThread);
    sp.process = pi.hProcess;
    sp.thread = pi.hThread;
    sp.pid = pi.dwProcessId;
    sp.ok = true;
    return sp;
}

static ProcResult win_finish(WinSpawn& sp, const ProcOptions& opt, double startWall, bool timedOutFlag) {
    ProcResult r;
    r.started = true;
    r.timedOut = timedOutFlag;
    long long peakWs = 0, peakCommit = 0;
    double cpu = 0;
    get_cpu_ms(sp.process, cpu);
    get_memory(sp.process, peakWs, peakCommit);
    r.cpuMs = cpu;
    r.peakMemoryKb = peakWs;
    r.peakCommitKb = peakCommit;

    DWORD code = 0;
    GetExitCodeProcess(sp.process, &code);
    r.exitCode = static_cast<int>(code);

    if (code == STILL_ACTIVE) {
        TerminateJobObject(sp.job, 1);
        WaitForSingleObject(sp.process, 3000);
        GetExitCodeProcess(sp.process, &code);
        r.exitCode = static_cast<int>(code);
        r.timedOut = true;
    }

    r.wallMs = now_ms() - startWall;
    if (opt.memoryLimitKb > 0 && peakCommit > opt.memoryLimitKb) r.oom = true;
    if (!r.timedOut && !r.oom && r.exitCode != 0) r.crashed = true;
    return r;
}

static ProcResult win_run(const std::string& exe, const ProcOptions& opt) {
    ProcResult fail;
    HANDLE hIn = nullptr, hOut = nullptr, hErr = nullptr;
    bool closeIn = false, closeOut = false, closeErr = false;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    auto openFile = [&](const std::string& path, DWORD access, DWORD creation) -> HANDLE {
        return CreateFileW(to_wide(path).c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, creation,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    };

    if (!opt.stdinFile.empty()) {
        hIn = openFile(opt.stdinFile, GENERIC_READ, OPEN_EXISTING);
        if (hIn == INVALID_HANDLE_VALUE) {
            fail.error = str("无法打开输入文件 {}", opt.stdinFile);
            return fail;
        }
        closeIn = true;
    }
    if (!opt.stdoutFile.empty()) {
        hOut = openFile(opt.stdoutFile, GENERIC_WRITE, CREATE_ALWAYS);
        if (hOut == INVALID_HANDLE_VALUE) {
            fail.error = str("无法创建输出文件 {}", opt.stdoutFile);
            if (closeIn) CloseHandle(hIn);
            return fail;
        }
        closeOut = true;
    }
    if (!opt.stderrFile.empty()) {
        hErr = openFile(opt.stderrFile, GENERIC_WRITE, CREATE_ALWAYS);
        if (hErr == INVALID_HANDLE_VALUE) {
            fail.error = str("无法创建错误文件 {}", opt.stderrFile);
            if (closeIn) CloseHandle(hIn);
            if (closeOut) CloseHandle(hOut);
            return fail;
        }
        closeErr = true;
    }

    std::string cwd = opt.workDir.empty() ? fs::cwd() : opt.workDir;
    double start = now_ms();
    WinSpawn sp = win_spawn(exe, opt.args, cwd, hIn, hOut, hErr, opt.memoryLimitKb);

    if (closeIn) CloseHandle(hIn);
    if (closeOut) CloseHandle(hOut);
    if (closeErr) CloseHandle(hErr);

    if (!sp.ok) {
        fail.error = sp.error;
        return fail;
    }

    double wallLimit = effective_wall_limit(opt);
    bool timedOut = false;
    for (;;) {
        DWORD w = WaitForSingleObject(sp.process, 4);
        if (w == WAIT_OBJECT_0) break;
        double wall = now_ms() - start;
        if (opt.timeLimitMs > 0) {
            double cpu = 0;
            if (get_cpu_ms(sp.process, cpu) && cpu > opt.timeLimitMs) {
                timedOut = true;
                break;
            }
        }
        if (wallLimit > 0 && wall > wallLimit) {
            timedOut = true;
            break;
        }
    }

    if (timedOut) {
        TerminateJobObject(sp.job, 1);
        WaitForSingleObject(sp.process, 3000);
    }

    ProcResult r = win_finish(sp, opt, start, timedOut);
    CloseHandle(sp.thread);
    CloseHandle(sp.process);
    if (sp.job) CloseHandle(sp.job);
    return r;
}

InteractiveRun run_interactive(const std::string& solutionExe, const std::vector<std::string>& solutionArgs,
                               const std::string& interactorExe, const std::vector<std::string>& interactorArgs,
                               const ProcOptions& opt, const std::string& solutionErrFile,
                               const std::string& interactorErrFile) {
    InteractiveRun run;
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // testlib 的交互器约定：
    //   inf  = argv[1] 输入文件
    //   tout -> argv[2] 指定的“输出文件”
    //   ouf  <- stdin
    //   ans  = argv[3] 答案文件
    // 两条通路都用命名管道表示，选手程序的 stdin/stdout 直接对接它们。
    static LONG pipeSeq = 0;
    long long unique = static_cast<long long>(GetCurrentProcessId()) * 1000 +
                       static_cast<long long>(InterlockedIncrement(&pipeSeq));
    std::string toSolution = str("\\\\.\\pipe\\polyi2s{}", unique);
    std::string fromSolution = str("\\\\.\\pipe\\polys2i{}", unique);

    HANDLE i2s = CreateNamedPipeW(to_wide(toSolution).c_str(), PIPE_ACCESS_INBOUND,
                                  PIPE_TYPE_BYTE | PIPE_WAIT, 1, 1 << 20, 1 << 20, 0, &sa);
    HANDLE s2i = CreateNamedPipeW(to_wide(fromSolution).c_str(), PIPE_ACCESS_INBOUND,
                                  PIPE_TYPE_BYTE | PIPE_WAIT, 1, 1 << 20, 1 << 20, 0, &sa);
    if (i2s == INVALID_HANDLE_VALUE || s2i == INVALID_HANDLE_VALUE) {
        if (i2s != INVALID_HANDLE_VALUE) CloseHandle(i2s);
        if (s2i != INVALID_HANDLE_VALUE) CloseHandle(s2i);
        run.error = str("无法创建命名管道：{}", win_error_message(GetLastError()));
        return run;
    }

    // 选手程序 stdout 用的写端，由我们打开后交给选手程序继承。
    HANDLE s2iClient = CreateFileW(to_wide(fromSolution).c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0,
                                   nullptr);
    if (s2iClient == INVALID_HANDLE_VALUE) {
        CloseHandle(i2s);
        CloseHandle(s2i);
        run.error = str("无法连接命名管道：{}", win_error_message(GetLastError()));
        return run;
    }

    HANDLE solErr = nullptr, intErr = nullptr, intOut = nullptr;
    if (!solutionErrFile.empty()) {
        solErr = CreateFileW(to_wide(solutionErrFile).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (solErr == INVALID_HANDLE_VALUE) solErr = nullptr;
    }
    if (!interactorErrFile.empty()) {
        intErr = CreateFileW(to_wide(interactorErrFile).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (intErr == INVALID_HANDLE_VALUE) intErr = nullptr;
        intOut = CreateFileW(to_wide(interactorErrFile + ".out").c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
        if (intOut == INVALID_HANDLE_VALUE) intOut = nullptr;
    }

    std::vector<std::string> iargs = interactorArgs;
    if (iargs.size() >= 2) iargs[1] = toSolution;
    std::string cwd = opt.workDir.empty() ? fs::cwd() : opt.workDir;
    double start = now_ms();

    // 1) 先启动交互器：它会打开 argv[2]（toSolution）作为客户端。
    WinSpawn itr = win_spawn(interactorExe, iargs, cwd, s2i, intOut, intErr, opt.memoryLimitKb);
    if (!itr.ok) {
        run.error = itr.error;
        CloseHandle(s2iClient);
        CloseHandle(i2s);
        CloseHandle(s2i);
        if (solErr) CloseHandle(solErr);
        if (intErr) CloseHandle(intErr);
        if (intOut) CloseHandle(intOut);
        return run;
    }

    // 2) 等它接上（有上限，避免交互器启动失败时永久阻塞）。
    std::atomic<bool> connected(false);
    std::thread connector([&]() {
        BOOL ok = ConnectNamedPipe(i2s, nullptr);
        if (!ok && GetLastError() == ERROR_PIPE_CONNECTED) ok = TRUE;
        connected.store(ok != FALSE);
    });
    bool opened = false;
    for (int waited = 0; waited < 5000; waited += 5) {
        if (connected.load()) {
            opened = true;
            break;
        }
        if (WaitForSingleObject(itr.process, 0) == WAIT_OBJECT_0) break;
        Sleep(5);
    }
    if (!opened) {
        if (itr.job) TerminateJobObject(itr.job, 1);
        WaitForSingleObject(itr.process, 3000);
        connector.join();
        CloseHandle(itr.thread);
        CloseHandle(itr.process);
        if (itr.job) CloseHandle(itr.job);
        CloseHandle(s2iClient);
        CloseHandle(i2s);
        CloseHandle(s2i);
        if (solErr) CloseHandle(solErr);
        if (intErr) CloseHandle(intErr);
        if (intOut) CloseHandle(intOut);
        run.error = "交互器没有打开 argv[2] 指定的输出管道";
        return run;
    }
    connector.detach();

    // 3) 启动选手程序：stdin = toSolution 读端，stdout = fromSolution 写端。
    SetHandleInformation(s2i, HANDLE_FLAG_INHERIT, 0);
    WinSpawn sol = win_spawn(solutionExe, solutionArgs, cwd, i2s, s2iClient, solErr, opt.memoryLimitKb);
    if (!sol.ok) {
        run.error = sol.error;
        if (itr.job) TerminateJobObject(itr.job, 1);
        WaitForSingleObject(itr.process, 3000);
        CloseHandle(itr.thread);
        CloseHandle(itr.process);
        if (itr.job) CloseHandle(itr.job);
        CloseHandle(s2iClient);
        CloseHandle(i2s);
        CloseHandle(s2i);
        if (solErr) CloseHandle(solErr);
        if (intErr) CloseHandle(intErr);
        if (intOut) CloseHandle(intOut);
        return run;
    }

    CloseHandle(s2iClient);
    CloseHandle(i2s);
    CloseHandle(s2i);
    if (solErr) CloseHandle(solErr);
    if (intErr) CloseHandle(intErr);
    if (intOut) CloseHandle(intOut);

    double wallLimit = effective_wall_limit(opt);
    bool timedOut = false;
    for (;;) {
        DWORD w = WaitForSingleObject(itr.process, 4);
        if (w == WAIT_OBJECT_0) break;
        if (opt.timeLimitMs > 0) {
            double cpu = 0;
            double solCpu = 0;
            bool itrOver = get_cpu_ms(itr.process, cpu) && cpu > opt.timeLimitMs;
            bool solOver = get_cpu_ms(sol.process, solCpu) && solCpu > opt.timeLimitMs;
            if (itrOver || solOver) {
                timedOut = true;
                break;
            }
        }
        if (wallLimit > 0 && now_ms() - start > wallLimit) {
            timedOut = true;
            break;
        }
    }
    if (timedOut) {
        if (itr.job) TerminateJobObject(itr.job, 1);
        WaitForSingleObject(itr.process, 3000);
    }

    DWORD solCode = 0;
    GetExitCodeProcess(sol.process, &solCode);
    if (solCode == STILL_ACTIVE) {
        if (sol.job) TerminateJobObject(sol.job, 1);
        WaitForSingleObject(sol.process, 3000);
    }

    WinSpawn itrCopy = itr;
    run.interactor = win_finish(itrCopy, opt, start, timedOut);
    WinSpawn solCopy = sol;
    run.solution = win_finish(solCopy, opt, start, false);
    run.ok = true;

    CloseHandle(itr.thread);
    CloseHandle(itr.process);
    if (itr.job) CloseHandle(itr.job);
    CloseHandle(sol.thread);
    CloseHandle(sol.process);
    if (sol.job) CloseHandle(sol.job);
    return run;
}

bool spawn_piped(const std::string& exe, const ProcOptions& opt, ProcHandle& out, std::string& err) {
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE childInRd = nullptr, parentInWr = nullptr;
    HANDLE parentOutRd = nullptr, childOutWr = nullptr;
    if (!CreatePipe(&childInRd, &parentInWr, &sa, 0) || !CreatePipe(&parentOutRd, &childOutWr, &sa, 0)) {
        err = str("创建管道失败");
        return false;
    }
    SetHandleInformation(parentInWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(parentOutRd, HANDLE_FLAG_INHERIT, 0);

    HANDLE hErr = nullptr;
    if (!opt.stderrFile.empty()) {
        hErr = CreateFileW(to_wide(opt.stderrFile).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hErr == INVALID_HANDLE_VALUE) hErr = nullptr;
    }

    std::string cwd = opt.workDir.empty() ? fs::cwd() : opt.workDir;
    WinSpawn sp = win_spawn(exe, opt.args, cwd, childInRd, childOutWr, hErr, opt.memoryLimitKb);

    CloseHandle(childInRd);
    CloseHandle(childOutWr);
    if (hErr) CloseHandle(hErr);

    if (!sp.ok) {
        err = sp.error;
        CloseHandle(parentInWr);
        CloseHandle(parentOutRd);
        return false;
    }

    out.started = true;
    out.job = sp.job;
    out.process = sp.process;
    out.thread = sp.thread;
    out.inFd = _open_osfhandle(reinterpret_cast<intptr_t>(parentInWr), _O_BINARY);
    out.outFd = _open_osfhandle(reinterpret_cast<intptr_t>(parentOutRd), _O_BINARY);
    out.startWall = now_ms();
    return true;
}

bool handle_alive(ProcHandle& h) {
    if (!h.process) return false;
    DWORD code = 0;
    GetExitCodeProcess(static_cast<HANDLE>(h.process), &code);
    return code == STILL_ACTIVE;
}

void terminate_handle(ProcHandle& h) {
    if (h.job) TerminateJobObject(static_cast<HANDLE>(h.job), 1);
    if (h.process) TerminateProcess(static_cast<HANDLE>(h.process), 1);
}

ProcResult wait_handle(ProcHandle& h, const ProcOptions& opt) {
    ProcResult r;
    if (!h.started) {
        r.error = h.error;
        return r;
    }
    r.started = true;
    double wallLimit = effective_wall_limit(opt);
    bool timedOut = false;
    for (;;) {
        DWORD w = WaitForSingleObject(static_cast<HANDLE>(h.process), 4);
        if (w == WAIT_OBJECT_0) break;
        if (opt.timeLimitMs > 0) {
            double cpu = 0;
            if (get_cpu_ms(static_cast<HANDLE>(h.process), cpu) && cpu > opt.timeLimitMs) {
                timedOut = true;
                break;
            }
        }
        if (wallLimit > 0 && now_ms() - h.startWall > wallLimit) {
            timedOut = true;
            break;
        }
    }
    WinSpawn sp;
    sp.process = static_cast<HANDLE>(h.process);
    sp.job = static_cast<HANDLE>(h.job);
    if (timedOut) {
        TerminateJobObject(sp.job, 1);
        WaitForSingleObject(sp.process, 3000);
    }
    r = win_finish(sp, opt, h.startWall, timedOut);
    return r;
}

void close_handle(ProcHandle& h) {
    if (h.thread) CloseHandle(static_cast<HANDLE>(h.thread));
    if (h.process) CloseHandle(static_cast<HANDLE>(h.process));
    if (h.job) CloseHandle(static_cast<HANDLE>(h.job));
    h.thread = h.process = h.job = nullptr;
    h.started = false;
}

long long write_fd(long long fd, const char* data, size_t n) {
    if (fd < 0) return -1;
    size_t total = 0;
    while (total < n) {
        int chunk = static_cast<int>((n - total) > (1u << 20) ? (1u << 20) : (n - total));
        int w = _write(static_cast<int>(fd), data + total, chunk);
        if (w <= 0) return total > 0 ? static_cast<long long>(total) : -1;
        total += static_cast<size_t>(w);
    }
    return static_cast<long long>(total);
}

long long read_fd(long long fd, char* buf, size_t n) {
    if (fd < 0) return -1;
    int r = _read(static_cast<int>(fd), buf, static_cast<unsigned>(n));
    return r;
}

void close_fd(long long fd) {
    if (fd >= 0) _close(static_cast<int>(fd));
}

static ProcResult win_run_capture(const std::string& exe, const ProcOptions& opt) {
    std::string tmpBase = fs::unique_temp_path("polycap");
    ProcOptions o = opt;
    o.stdoutFile = tmpBase + ".out";
    o.stderrFile = tmpBase + ".err";
    if (!opt.stdinText.empty()) o.stdinFile = tmpBase + ".in";
    if (!opt.stdinText.empty() && opt.stdinFile.empty()) fs::write_file(o.stdinFile, opt.stdinText);

    ProcResult r = win_run(exe, o);
    fs::read_file(o.stdoutFile, r.out);
    fs::read_file(o.stderrFile, r.err);
    fs::remove_file(o.stdoutFile);
    fs::remove_file(o.stderrFile);
    if (!opt.stdinText.empty() && opt.stdinFile.empty()) fs::remove_file(o.stdinFile);
    return r;
}

#else  // ---------------------------------------------------------------- POSIX

#include <sys/stat.h>

static ProcResult posix_run(const std::string& exe, const ProcOptions& opt, int* childInFd, int* childOutFd) {
    ProcResult r;
    int inFd = opt.stdinFile.empty() ? -1 : open(opt.stdinFile.c_str(), O_RDONLY);
    int outFd = opt.stdoutFile.empty() ? -1 : open(opt.stdoutFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int errFd = opt.stderrFile.empty() ? -1 : open(opt.stderrFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

    double start = now_ms();
    pid_t pid = fork();
    if (pid < 0) {
        r.error = "fork failed";
        return r;
    }
    if (pid == 0) {
        if (inFd >= 0) dup2(inFd, 0);
        if (childInFd) dup2(*childInFd, 0);
        if (outFd >= 0) dup2(outFd, 1);
        if (childOutFd) dup2(*childOutFd, 1);
        if (errFd >= 0) dup2(errFd, 2);
        if (opt.memoryLimitKb > 0) {
            struct rlimit rl;
            rl.rlim_cur = rl.rlim_max = static_cast<rlim_t>(opt.memoryLimitKb) * 1024;
            setrlimit(RLIMIT_AS, &rl);
        }
        if (opt.timeLimitMs > 0) {
            struct rlimit rl;
            rl.rlim_cur = static_cast<rlim_t>((opt.timeLimitMs + 999) / 1000) + 1;
            rl.rlim_max = rl.rlim_cur + 2;
            setrlimit(RLIMIT_CPU, &rl);
        }
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const std::string& a : opt.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        if (!opt.workDir.empty()) {
            if (chdir(opt.workDir.c_str()) != 0) _exit(127);
        }
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    if (inFd >= 0) close(inFd);
    if (outFd >= 0) close(outFd);
    if (errFd >= 0) close(errFd);

    r.started = true;
    double wallLimit = effective_wall_limit(opt);
    int status = 0;
    bool timedOut = false;
    for (;;) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) break;
        double wall = now_ms() - start;
        if (wallLimit > 0 && wall > wallLimit) {
            timedOut = true;
            break;
        }
        struct timespec ts{0, 2000000};
        nanosleep(&ts, nullptr);
    }
    if (timedOut) kill(pid, SIGKILL);
    struct rusage ru;
    memset(&ru, 0, sizeof(ru));
    wait4(pid, &status, 0, &ru);

    r.cpuMs = static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0 +
              static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
    r.peakMemoryKb = static_cast<long long>(ru.ru_maxrss);
    r.peakCommitKb = r.peakMemoryKb;
    r.wallMs = now_ms() - start;
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        r.exitCode = 128 + sig;
        if (sig == SIGXCPU || sig == SIGKILL) r.timedOut = true;
        if (sig == SIGSEGV || sig == SIGABRT || sig == SIGBUS) r.crashed = true;
    } else {
        r.exitCode = WEXITSTATUS(status);
    }
    if (timedOut) r.timedOut = true;
    if (opt.memoryLimitKb > 0 && r.peakCommitKb > opt.memoryLimitKb) r.oom = true;
    if (!r.timedOut && !r.oom && r.exitCode != 0) r.crashed = true;
    return r;
}

static ProcResult posix_run_capture(const std::string& exe, const ProcOptions& opt) {
    std::string tmpBase = fs::unique_temp_path("polycap");
    ProcOptions o = opt;
    o.stdoutFile = tmpBase + ".out";
    o.stderrFile = tmpBase + ".err";
    if (!opt.stdinText.empty() && opt.stdinFile.empty()) {
        o.stdinFile = tmpBase + ".in";
        fs::write_file(o.stdinFile, opt.stdinText);
    }
    ProcResult r = posix_run(exe, o, nullptr, nullptr);
    fs::read_file(o.stdoutFile, r.out);
    fs::read_file(o.stderrFile, r.err);
    fs::remove_file(o.stdoutFile);
    fs::remove_file(o.stderrFile);
    if (opt.stdinFile.empty() && !o.stdinFile.empty()) fs::remove_file(o.stdinFile);
    return r;
}

bool spawn_piped(const std::string& exe, const ProcOptions& opt, ProcHandle& out, std::string& err) {
    int p1[2], p2[2];
    if (pipe(p1) != 0 || pipe(p2) != 0) {
        err = "pipe failed";
        return false;
    }
    ProcResult r = posix_run(exe, opt, &p1[0], &p2[1]);
    if (!r.started) {
        err = r.error;
        return false;
    }
    close(p1[0]);
    close(p2[1]);
    out.started = true;
    out.inFd = p1[1];
    out.outFd = p2[0];
    out.startWall = now_ms();
    return true;
}

bool handle_alive(ProcHandle& h) {
    if (h.pid <= 0) return false;
    int status = 0;
    return waitpid(h.pid, &status, WNOHANG) == 0;
}

void terminate_handle(ProcHandle& h) {
    if (h.pid > 0) kill(h.pid, SIGKILL);
}

ProcResult wait_handle(ProcHandle& h, const ProcOptions& opt) {
    ProcResult r;
    r.started = true;
    int status = 0;
    double wallLimit = effective_wall_limit(opt);
    bool timedOut = false;
    for (;;) {
        pid_t w = waitpid(h.pid, &status, WNOHANG);
        if (w == h.pid) break;
        if (wallLimit > 0 && now_ms() - h.startWall > wallLimit) {
            timedOut = true;
            break;
        }
        struct timespec ts{0, 2000000};
        nanosleep(&ts, nullptr);
    }
    if (timedOut) kill(h.pid, SIGKILL);
    struct rusage ru;
    memset(&ru, 0, sizeof(ru));
    wait4(h.pid, &status, 0, &ru);
    r.cpuMs = static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0 +
              static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
    r.peakMemoryKb = static_cast<long long>(ru.ru_maxrss);
    r.peakCommitKb = r.peakMemoryKb;
    r.wallMs = now_ms() - h.startWall;
    r.timedOut = timedOut;
    if (WIFSIGNALED(status)) {
        r.exitCode = 128 + WTERMSIG(status);
        if (WTERMSIG(status) == SIGXCPU) r.timedOut = true;
    } else {
        r.exitCode = WEXITSTATUS(status);
    }
    if (opt.memoryLimitKb > 0 && r.peakCommitKb > opt.memoryLimitKb) r.oom = true;
    if (!r.timedOut && !r.oom && r.exitCode != 0) r.crashed = true;
    return r;
}

void close_handle(ProcHandle& h) {
    h.started = false;
    h.pid = 0;
}

long long write_fd(long long fd, const char* data, size_t n) {
    if (fd < 0) return -1;
    size_t total = 0;
    while (total < n) {
        ssize_t w = ::write(static_cast<int>(fd), data + total, n - total);
        if (w <= 0) return total > 0 ? static_cast<long long>(total) : -1;
        total += static_cast<size_t>(w);
    }
    return static_cast<long long>(total);
}

long long read_fd(long long fd, char* buf, size_t n) {
    if (fd < 0) return -1;
    return ::read(static_cast<int>(fd), buf, n);
}

void close_fd(long long fd) {
    if (fd >= 0) ::close(static_cast<int>(fd));
}

InteractiveRun run_interactive(const std::string& solutionExe, const std::vector<std::string>& solutionArgs,
                               const std::string& interactorExe, const std::vector<std::string>& interactorArgs,
                               const ProcOptions& opt, const std::string& solutionErrFile,
                               const std::string& interactorErrFile) {
    InteractiveRun run;
    int s2i[2], i2s[2];
    if (pipe(s2i) != 0 || pipe(i2s) != 0) {
        run.error = "pipe failed";
        return run;
    }
    double start = now_ms();
    auto apply_limits = [&]() {
        if (opt.memoryLimitKb > 0) {
            struct rlimit rl;
            rl.rlim_cur = rl.rlim_max = static_cast<rlim_t>(opt.memoryLimitKb) * 1024;
            setrlimit(RLIMIT_AS, &rl);
        }
        if (opt.timeLimitMs > 0) {
            struct rlimit rl;
            rl.rlim_cur = static_cast<rlim_t>((opt.timeLimitMs + 999) / 1000) + 1;
            rl.rlim_max = rl.rlim_cur + 2;
            setrlimit(RLIMIT_CPU, &rl);
        }
    };
    auto child_stderr = [&](const std::string& path) {
        if (path.empty()) return;
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) dup2(fd, 2);
    };

    pid_t sol = fork();
    if (sol == 0) {
        dup2(i2s[0], 0);
        dup2(s2i[1], 1);
        child_stderr(solutionErrFile);
        close(s2i[0]);
        close(s2i[1]);
        close(i2s[0]);
        close(i2s[1]);
        apply_limits();
        if (!opt.workDir.empty() && chdir(opt.workDir.c_str()) != 0) _exit(127);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(solutionExe.c_str()));
        for (const std::string& a : solutionArgs) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(solutionExe.c_str(), argv.data());
        _exit(127);
    }

    pid_t itr = fork();
    if (itr == 0) {
        dup2(s2i[0], 0);
        dup2(i2s[1], 1);
        child_stderr(interactorErrFile);
        close(s2i[0]);
        close(s2i[1]);
        close(i2s[0]);
        close(i2s[1]);
        apply_limits();
        if (!opt.workDir.empty() && chdir(opt.workDir.c_str()) != 0) _exit(127);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(interactorExe.c_str()));
        for (const std::string& a : interactorArgs) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(interactorExe.c_str(), argv.data());
        _exit(127);
    }

    close(s2i[0]);
    close(s2i[1]);
    close(i2s[0]);
    close(i2s[1]);

    double wallLimit = effective_wall_limit(opt);
    int status = 0;
    bool timedOut = false;
    for (;;) {
        pid_t w = waitpid(itr, &status, WNOHANG);
        if (w == itr) break;
        if (wallLimit > 0 && now_ms() - start > wallLimit) {
            timedOut = true;
            break;
        }
        struct timespec ts{0, 2000000};
        nanosleep(&ts, nullptr);
    }
    if (timedOut) kill(itr, SIGKILL);
    struct rusage iru;
    memset(&iru, 0, sizeof(iru));
    wait4(itr, &status, 0, &iru);

    kill(sol, SIGKILL);
    int solStatus = 0;
    struct rusage sru;
    memset(&sru, 0, sizeof(sru));
    wait4(sol, &solStatus, 0, &sru);

    auto fill = [&](ProcResult& r, int st, const struct rusage& ru) {
        r.started = true;
        r.cpuMs = static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0 +
                  static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
        r.peakMemoryKb = static_cast<long long>(ru.ru_maxrss);
        r.peakCommitKb = r.peakMemoryKb;
        r.wallMs = now_ms() - start;
        if (WIFSIGNALED(st))
            r.exitCode = 128 + WTERMSIG(st);
        else
            r.exitCode = WEXITSTATUS(st);
    };
    fill(run.interactor, status, iru);
    run.interactor.timedOut = timedOut;
    fill(run.solution, solStatus, sru);
    if (opt.memoryLimitKb > 0) {
        if (run.interactor.peakCommitKb > opt.memoryLimitKb) run.interactor.oom = true;
        if (run.solution.peakCommitKb > opt.memoryLimitKb) run.solution.oom = true;
    }
    run.ok = true;
    return run;
}

#endif

ProcResult run_process(const std::string& exe, const ProcOptions& opt) {
    if (!fs::is_file(exe)) {
        ProcResult r;
        r.error = str("可执行文件不存在: {}（请先执行 poly build）", exe);
        return r;
    }
#ifdef _WIN32
    if (opt.captureOutput) return win_run_capture(exe, opt);
    return win_run(exe, opt);
#else
    if (opt.captureOutput) return posix_run_capture(exe, opt);
    return posix_run(exe, opt, nullptr, nullptr);
#endif
}

ProcResult run_capture(const std::string& exe, const std::vector<std::string>& args, const std::string& workDir,
                       double timeoutMs) {
    ProcOptions o;
    o.args = args;
    o.workDir = workDir;
    o.captureOutput = true;
    o.timeLimitMs = 0;
    o.wallLimitMs = timeoutMs;
    return run_process(exe, o);
}

}  // namespace poly
