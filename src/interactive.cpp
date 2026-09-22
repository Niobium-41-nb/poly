// Interactive problems: the interactor and the solution talk over two pipes,
// the verdict comes from the interactor's exit code.
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "log.h"
#include "proc.h"
#include "strutil.h"

namespace poly {

TestOutcome judge_interactive(JudgeEnv& env, const std::string& solExe, const std::string& solName, int testIndex,
                             const JudgeLimits& lim) {
    const Problem& p = env.problem;
    TestOutcome r;
    r.test = testIndex;

    if (env.interactorExe.empty() || !fs::is_file(env.interactorExe)) {
        r.verdict = Verdict::FAIL;
        r.message = "交互器尚未编译（poly build interactor）";
        return r;
    }

    std::string base = p.testFileName(testIndex);
    std::string testRel = fs::join("tests", base);
    std::string ansRel = answer_rel(testIndex, p);
    std::string runDirRel = fs::join("output", "runs");
    fs::mkdirs(p.path(runDirRel));
    std::string outRel = fs::join(runDirRel, str("{}.{}.inter", solName, base));
    std::string solErrRel = fs::join(runDirRel, str("{}.{}.err", solName, base));
    std::string intErrRel = fs::join(runDirRel, str("interactor.{}.err", base));

    if (!fs::is_file(p.path(ansRel))) {
        // testlib 的交互器需要 <answer-file> 参数可打开；交互题没有标准答案，放空文件即可。
        fs::mkdirs(p.path(answer_rel_dir()));
        fs::write_file(p.path(ansRel), "");
    }

    ProcOptions o;
    o.workDir = p.dir;
    o.timeLimitMs = lim.timeLimitMs;
    o.wallLimitMs = lim.timeLimitMs * 2.0 + 3000.0;
    o.memoryLimitKb = lim.memoryLimitKb;

    InteractiveRun run = run_interactive(solExe, {}, env.interactorExe, {testRel, outRel, ansRel}, o,
                                         p.path(solErrRel), p.path(intErrRel));
    if (!run.ok) {
        r.verdict = Verdict::FAIL;
        r.message = run.error;
        return r;
    }

    r.timeMs = run.solution.cpuMs;
    r.wallMs = run.solution.wallMs;
    r.memoryKb = run.solution.peakCommitKb;

    std::string intErr;
    fs::read_file(p.path(intErrRel), intErr);
    std::string intOut;
    fs::read_file(p.path(outRel), intOut);
    std::string text = intErr.empty() ? intOut : intErr;

    if (run.solution.timedOut || run.interactor.timedOut) {
        r.verdict = Verdict::TL;
        r.message = str("超过时限 {} ms", static_cast<long long>(lim.timeLimitMs));
        return r;
    }
    if (run.interactor.oom || run.solution.oom) {
        r.verdict = Verdict::ML;
        r.message = str("超过内存限制 {} KB", lim.memoryLimitKb);
        return r;
    }

    std::string message;
    Verdict v = verdict_from_process(run.interactor.exitCode, text, message);

    if (v == Verdict::OK && run.solution.exitCode != 0) {
        r.verdict = Verdict::RE;
        r.message = str("选手程序退出码 {}（交互器认为正确）", run.solution.exitCode);
        return r;
    }
    if (v == Verdict::FAIL && message.empty()) {
        if (run.solution.exitCode != 0) {
            r.verdict = Verdict::RE;
            r.message = str("选手程序退出码 {}", run.solution.exitCode);
            return r;
        }
        message = str("交互器退出码 {}", run.interactor.exitCode);
    }
    r.verdict = v;
    r.message = message;
    if (r.message.empty() && run.solution.exitCode != 0) {
        r.crashLog = str("选手程序退出码 {}", run.solution.exitCode);
    }
    return r;
}

}  // namespace poly
