#include "compile.h"

#include <algorithm>

#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "plat.h"
#include "proc.h"
#include "strutil.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace poly {

namespace {

const char kBackslash = static_cast<char>(92);

void strip_trailing_separators(std::string& d) {
    while (!d.empty() && (d.back() == '/' || d.back() == kBackslash)) d.pop_back();
}

bool is_ascii(const std::string& s) {
    for (unsigned char c : s)
        if (c >= 0x80) return false;
    return true;
}

bool dir_usable(const std::string& dir) {
    if (dir.empty() || !fs::is_dir(dir)) return false;
    std::string probe = fs::join(dir, "poly_probe.tmp");
    if (!fs::write_file(probe, "x")) return false;
    fs::remove_file(probe);
    return true;
}

std::vector<std::string> candidate_dirs() {
    std::vector<std::string> raw;
    for (const std::string& e :
         {env_get("POLY_TMPDIR"), env_get("TEMP"), env_get("TMP"), env_get("TMPDIR")}) {
        if (!e.empty()) raw.push_back(e);
    }
    std::string local = env_get("LOCALAPPDATA");
    if (!local.empty()) raw.push_back(fs::join(local, "Temp"));
    std::string home = fs::home_dir();
    if (!home.empty()) raw.push_back(fs::join(fs::join(home, "AppData"), fs::join("Local", "Temp")));
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, buf);
    if (n > 0 && n < MAX_PATH) raw.push_back(to_utf8(std::wstring(buf, n)));
#endif
    raw.push_back(fs::temp_dir());
    raw.push_back("C:/tmp");
    raw.push_back("D:/tmp");

    std::vector<std::string> out;
    for (std::string d : raw) {
        strip_trailing_separators(d);
        if (d.empty() || !is_ascii(d)) continue;
        if (std::find(out.begin(), out.end(), d) != out.end()) continue;
        out.push_back(d);
    }
    return out;
}

std::vector<std::string> path_dirs() {
    std::vector<std::string> out;
    std::string p = env_get("PATH");
    char sep = p.find(';') != std::string::npos ? ';' : ':';
    for (std::string d : split(p, sep)) {
        strip_trailing_separators(d);
        if (!d.empty()) out.push_back(d);
    }
    return out;
}

void add_candidate(std::vector<std::string>& out, const std::string& p) {
    if (p.empty() || !fs::is_file(p)) return;
    if (std::find(out.begin(), out.end(), p) != out.end()) return;
    out.push_back(p);
}

std::vector<std::string> compiler_names() {
#ifdef _WIN32
    return {"g++.exe", "g++", "clang++.exe", "clang++", "c++.exe"};
#else
    return {"g++", "clang++", "c++"};
#endif
}

bool smoke_test(const std::string& exe, const std::string& stdFlag) {
    std::string dir = ascii_temp_dir();
    if (dir.empty()) return false;
    std::string src = fs::join(dir, "poly_smoke.cpp");
    std::string obj = fs::join(dir, "poly_smoke.o");
    fs::write_file(src, "int main(){return 0;}\n");
    fs::remove_file(obj);

    ProcOptions o;
    o.workDir = dir;
    o.args = {stdFlag, "poly_smoke.cpp", "-o", "poly_smoke.o"};
    o.captureOutput = true;
    o.wallLimitMs = 60000;
    ProcResult r = run_process(exe, o);
    bool ok = r.started && r.exitCode == 0 && fs::is_file(obj);
    fs::remove_file(src);
    fs::remove_file(obj);
    return ok;
}

}  // namespace

std::string ascii_temp_dir() {
    static std::string cached;
    if (!cached.empty()) return cached;
    for (const std::string& d : candidate_dirs()) {
        if (dir_usable(d)) {
            cached = d;
            return cached;
        }
    }
    for (const std::string& d : {"C:/tmp", "D:/tmp"}) {
        if (fs::mkdirs(d) && dir_usable(d)) {
            cached = d;
            return cached;
        }
    }
    std::string fallback = fs::join(fs::temp_dir(), "poly");
    fs::mkdirs(fallback);
    log_warn(str("找不到纯 ASCII 的临时目录，回退到 {}", fallback));
    cached = fallback;
    return cached;
}

std::string exe_suffix() {
#ifdef _WIN32
    return ".exe";
#else
    return std::string();
#endif
}

std::string toolchain_hint() {
    return "提示：编译器返回了错误却没有输出，通常说明 PATH 中混用了两套不兼容的工具链"
           "（例如 MSYS2 的 ucrt64 与 mingw64），或 TMP/TEMP 指向了不存在的目录。"
           "可以用 --compiler <路径> 指定编译器，或设置环境变量 POLY_CXX。";
}

static std::vector<std::string> compiler_candidates(const std::string& want) {
    std::vector<std::string> out;
    std::vector<std::string> names = compiler_names();

    add_candidate(out, want);
    if (!want.empty() && !contains(want, "/")) {
        for (const std::string& d : path_dirs()) add_candidate(out, fs::join(d, want));
    }
    for (const std::string& n : names) add_candidate(out, fs::resolve_executable(n));

    std::vector<std::string> roots;
    for (const std::string& d : path_dirs()) {
        std::string a = fs::dirname(d);
        std::string b = fs::dirname(a);
        if (!b.empty() && b != "." && b != a) roots.push_back(b);
    }
    for (const char* r : {"C:/msys64", "D:/msys64", "C:/msys2", "D:/msys2", "C:/msys32"}) roots.push_back(r);
    for (const std::string& root : roots) {
        for (const char* pfx : {"ucrt64", "mingw64", "clang64", "clangarm64"}) {
            for (const std::string& n : names) {
                add_candidate(out, fs::join(root, fs::join(pfx, fs::join("bin", n))));
            }
        }
    }
    return out;
}

bool resolve_compiler(const std::string& want, const std::string& stdFlag, Compiler& out, std::string& err) {
    out = Compiler();
    out.stdFlag = stdFlag;
    std::vector<std::string> cands = compiler_candidates(want);
    if (cands.empty()) {
        err = str("找不到 C++ 编译器（尝试过 {}）。{}", want.empty() ? "g++" : want, toolchain_hint());
        return false;
    }
    std::string failedExample;
    for (const std::string& c : cands) {
        if (!smoke_test(c, stdFlag)) {
            if (failedExample.empty()) failedExample = c;
            continue;
        }
        out.exe = c;
        out.binDir = fs::dirname(c);
        out.ok = true;
        if (!out.binDir.empty()) {
#ifdef _WIN32
            env_set("PATH", out.binDir + ";" + env_get("PATH"));
#else
            env_set("PATH", out.binDir + ":" + env_get("PATH"));
#endif
        }
        ProcResult v = run_capture(c, {"--version"}, ascii_temp_dir(), 15000);
        if (v.started) {
            for (const std::string& l : split_lines(v.out)) {
                if (!trim(l).empty()) {
                    out.version = trim(l);
                    break;
                }
            }
        }
        return true;
    }
    err = str("所有候选编译器都无法编译最简单的测试程序（例如 {}）。{}", failedExample, toolchain_hint());
    return false;
}

bool compile_to(const Compiler& cc, const std::string& workDir, const std::string& srcRel,
                const std::string& outRel, const std::vector<std::string>& extraArgs, std::string& log,
                std::string& err) {
    std::string outAbs = fs::join(workDir, outRel);
    std::string outDir = fs::dirname(outAbs);
    if (!outDir.empty() && outDir != ".") fs::mkdirs(outDir);
    fs::remove_file(outAbs);

    std::vector<std::string> args;
    args.push_back(cc.stdFlag);
    args.push_back("-O2");
    for (const std::string& f : cc.flags) args.push_back(f);
    for (const std::string& f : extraArgs) args.push_back(f);
    args.push_back(srcRel);
    args.push_back("-o");
    args.push_back(outRel);

    ProcOptions o;
    o.workDir = workDir;
    o.args = args;
    o.captureOutput = true;
    o.wallLimitMs = 120000;
    ProcResult r = run_process(cc.exe, o);
    log = r.err;
    if (log.empty()) log = r.out;
    if (!r.started) {
        err = r.error;
        return false;
    }
    if (r.exitCode != 0) {
        err = trim(log);
        if (err.empty()) {
            err = str("编译器异常退出（退出码 {}），没有任何输出。{}", r.exitCode, toolchain_hint());
        }
        return false;
    }
    if (!fs::is_file(outAbs)) {
        err = str("编译似乎成功，但没有生成 {}", outRel);
        return false;
    }
    return true;
}

}  // namespace poly
