#include "judge.h"

#include <algorithm>

#include "checker.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "proc.h"
#include "strutil.h"

namespace poly {

namespace {

std::string tail_lines(const std::string& text, size_t count) {
    std::vector<std::string> lines = split_lines(strip_cr(text));
    std::string out;
    size_t start = lines.size() > count ? lines.size() - count : 0;
    for (size_t i = start; i < lines.size(); i++) {
        std::string line = trim(lines[i]);
        if (line.empty()) continue;
        if (line.size() > 160) line = line.substr(0, 157) + "...";
        if (!out.empty()) out += " | ";
        out += line;
    }
    return out;
}

}  // namespace

std::string compiled_name(const std::string& stem, const std::string& suffix) { return stem + suffix; }

std::string answer_rel_dir() { return fs::join("output", "answers"); }

std::string answer_rel(int test, const Problem& p) {
    return fs::join(answer_rel_dir(), p.testFileName(test));
}

TestOutcome judge_test(JudgeEnv& env, const std::string& solExe, const std::string& solName, int testIndex,
                       const JudgeLimits& lim, bool check) {
    const Problem& p = env.problem;
    TestOutcome r;
    r.test = testIndex;

    std::string testBase = p.testFileName(testIndex);
    std::string testRel = fs::join("tests", testBase);
    std::string runDirRel = fs::join("output", "runs");
    std::string outRel = fs::join(runDirRel, str("{}.{}.out", solName, testBase));
    std::string errRel = fs::join(runDirRel, str("{}.{}.err", solName, testBase));
    fs::mkdirs(p.path(runDirRel));

    ProcOptions o;
    o.workDir = p.dir;
    o.stdinFile = p.path(testRel);
    o.stdoutFile = p.path(outRel);
    o.stderrFile = p.path(errRel);
    o.timeLimitMs = lim.timeLimitMs;
    o.memoryLimitKb = lim.memoryLimitKb;

    ProcResult pr = run_process(solExe, o);
    r.timeMs = pr.cpuMs;
    r.wallMs = pr.wallMs;
    r.memoryKb = pr.peakCommitKb;

    if (!pr.started) {
        r.verdict = Verdict::FAIL;
        r.message = pr.error;
        return r;
    }

    std::string errText;
    fs::read_file(p.path(errRel), errText);
    r.crashLog = tail_lines(errText, 3);

    if (pr.timedOut) {
        r.verdict = Verdict::TL;
        r.message = str("超过时限 {} ms", static_cast<long long>(lim.timeLimitMs));
        return r;
    }
    if (pr.oom) {
        r.verdict = Verdict::ML;
        r.message = str("使用内存 {} KB，超过限制 {} KB", pr.peakCommitKb, lim.memoryLimitKb);
        return r;
    }
    if (pr.exitCode != 0) {
        r.verdict = Verdict::RE;
        r.message = str("退出码 {}", pr.exitCode);
        return r;
    }

    if (!check) {
        r.verdict = Verdict::OK;
        return r;
    }

    std::string ansRel = answer_rel(testIndex, p);
    if (!fs::is_file(p.path(ansRel))) {
        r.verdict = Verdict::FAIL;
        r.message = str("缺少标准答案 {}（请先运行 poly gen 与 poly test 生成）", ansRel);
        return r;
    }

    bool custom = !p.checkerFile.empty();
    if (!custom) {
        CheckResult cr = builtin_check(p.checker, p.path(testRel), p.path(outRel), p.path(ansRel));
        r.verdict = cr.verdict;
        r.message = cr.message;
        return r;
    }

    if (env.checkerExe.empty() || !fs::is_file(env.checkerExe)) {
        r.verdict = Verdict::FAIL;
        r.message = "自定义 checker 尚未编译（poly build）";
        return r;
    }
    ProcOptions co;
    co.workDir = p.dir;
    co.args = {testRel, outRel, ansRel, num(testIndex)};
    co.wallLimitMs = 30000;
    co.memoryLimitKb = 1024 * 1024;
    ProcResult cr = run_process(env.checkerExe, co);
    if (!cr.started) {
        r.verdict = Verdict::FAIL;
        r.message = cr.error;
        return r;
    }
    if (cr.timedOut) {
        r.verdict = Verdict::FAIL;
        r.message = "checker 超时";
        return r;
    }
    std::string text = cr.out.empty() ? cr.err : cr.out;
    r.verdict = verdict_from_process(cr.exitCode, text, r.message);
    if (r.verdict == Verdict::FAIL && r.message.empty()) {
        r.message = str("checker 退出码 {}", cr.exitCode);
    }
    return r;
}

bool ensure_answers(JudgeEnv& env, const std::string& answerSolution, std::vector<std::string>& errors) {
    Problem& p = env.problem;
    if (p.interactive) {
        // 交互题的标准答案是交互过程记录，这里先放空文件占位（交互器只要求文件可打开）。
        fs::mkdirs(p.path(answer_rel_dir()));
        for (int idx : p.testIndices()) {
            std::string ansAbs = p.path(answer_rel(idx, p));
            if (!fs::is_file(ansAbs)) fs::write_file(ansAbs, "");
        }
        return true;
    }
    std::string name = answerSolution;
    if (name.empty()) {
        if (p.findSolution("main")) {
            name = "main";
        } else {
            for (const SolutionInfo& s : p.solutions) {
                if (s.expected == "ac" || s.expected == "ok") {
                    name = s.name;
                    break;
                }
            }
        }
    }
    if (name.empty()) {
        errors.push_back("找不到可用的标准程序（请在 problem.json 中把某个解法标为 main）");
        return false;
    }
    const SolutionInfo* sol = p.findSolution(name);
    if (!sol) {
        errors.push_back(str("标准程序 {} 不存在", name));
        return false;
    }
    std::string exeRel = fs::join(fs::join("output", "bin"), compiled_name(sol->name, env.exeSuffix));
    std::string exeAbs = p.path(exeRel);
    if (!fs::is_file(exeAbs)) {
        errors.push_back(str("标准程序 {} 尚未编译（poly build）", name));
        return false;
    }

    fs::mkdirs(p.path(answer_rel_dir()));
    for (int idx : p.testIndices()) {
        std::string ansRel = answer_rel(idx, p);
        std::string ansAbs = p.path(ansRel);
        if (fs::is_file(ansAbs) && fs::file_size(ansAbs) >= 0) {
            if (fs::modified_time(ansAbs) >= fs::modified_time(p.path(fs::join("tests", p.testFileName(idx))))) {
                continue;
            }
        }
        std::string runDirRel = fs::join("output", "runs");
        std::string outRel = fs::join(runDirRel, str("answer.{}.out", p.testFileName(idx)));
        std::string errRel = fs::join(runDirRel, str("answer.{}.err", p.testFileName(idx)));
        fs::mkdirs(p.path(runDirRel));

        ProcOptions o;
        o.workDir = p.dir;
        o.stdinFile = p.path(fs::join("tests", p.testFileName(idx)));
        o.stdoutFile = p.path(outRel);
        o.stderrFile = p.path(errRel);
        o.timeLimitMs = static_cast<double>(p.timeLimitMs) * 3.0 + 3000.0;
        o.memoryLimitKb = p.memoryLimitKb * 2 + 262144;

        ProcResult pr = run_process(exeAbs, o);
        if (!pr.started || pr.timedOut || pr.exitCode != 0) {
            errors.push_back(str("标准程序 {} 在测试点 {} 上失败（退出码 {}，超时 {}）", name, idx, pr.exitCode,
                                 pr.timedOut ? "是" : "否"));
            continue;
        }
        if (!fs::copy_file(p.path(outRel), ansAbs)) {
            errors.push_back(str("无法写入标准答案 {}", ansRel));
        }
    }
    return errors.empty();
}

}  // namespace poly
