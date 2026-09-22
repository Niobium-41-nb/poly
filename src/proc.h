#pragma once

#include <string>
#include <vector>

namespace poly {

struct ProcOptions {
    std::vector<std::string> args;
    std::string workDir;
    std::string stdinFile;
    std::string stdoutFile;
    std::string stderrFile;
    std::string stdinText;   // used by run_capture
    double timeLimitMs = 0;  // CPU limit, 0 = unlimited
    double wallLimitMs = 0;  // 0 = derived from timeLimitMs
    long long memoryLimitKb = 0;
    bool captureOutput = false;
    std::string env;         // extra "K=V" entries, ';' separated
};

struct ProcResult {
    bool started = false;
    int exitCode = -1;
    bool timedOut = false;
    bool oom = false;
    bool crashed = false;
    double wallMs = 0;
    double cpuMs = 0;
    long long peakMemoryKb = 0;
    long long peakCommitKb = 0;
    std::string error;
    std::string out;
    std::string err;
};

ProcResult run_process(const std::string& exe, const ProcOptions& opt);

ProcResult run_capture(const std::string& exe, const std::vector<std::string>& args,
                       const std::string& workDir = std::string(), double timeoutMs = 30000);

// ---- bidirectional pipe support (interactive problems) ----

struct ProcHandle {
    bool started = false;
    std::string error;
    long long inFd = -1;   // write to the child's stdin
    long long outFd = -1;  // read from the child's stdout
    double startWall = 0;
#ifdef _WIN32
    void* job = nullptr;
    void* process = nullptr;
    void* thread = nullptr;
#else
    int pid = 0;
#endif
};

bool spawn_piped(const std::string& exe, const ProcOptions& opt, ProcHandle& out, std::string& err);
ProcResult wait_handle(ProcHandle& h, const ProcOptions& opt);
void terminate_handle(ProcHandle& h);
void close_handle(ProcHandle& h);
bool handle_alive(ProcHandle& h);

struct InteractiveRun {
    bool ok = false;
    std::string error;
    ProcResult solution;
    ProcResult interactor;
};

// Connects two processes: solution.stdout -> interactor.stdin and
// interactor.stdout -> solution.stdin, then waits for the interactor and
// terminates the solution.  `opt` supplies the working directory and limits.
InteractiveRun run_interactive(const std::string& solutionExe, const std::vector<std::string>& solutionArgs,
                               const std::string& interactorExe, const std::vector<std::string>& interactorArgs,
                               const ProcOptions& opt, const std::string& solutionErrFile,
                               const std::string& interactorErrFile);

long long write_fd(long long fd, const char* data, size_t n);
long long read_fd(long long fd, char* buf, size_t n);
void close_fd(long long fd);

}  // namespace poly
