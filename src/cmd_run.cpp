#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "log.h"
#include "pipeline.h"
#include "strutil.h"
#include "table.h"

namespace poly {

namespace {

std::vector<int> collect_tests(const Problem& p, const Args& args) {
    std::vector<int> only;
    for (const std::string& spec : args.get_all("test")) {
        for (int t : parse_test_list(spec)) only.push_back(t);
    }
    if (!args.get("tests").empty()) {
        for (int t : parse_test_list(args.get("tests"))) only.push_back(t);
    }
    int single = 0;
    if (!args.get("test").empty() && is_integer(args.get("test"))) single = static_cast<int>(to_int(args.get("test")));
    return selected_tests(p, only, single);
}

}  // namespace

int cmd_run(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    std::string solName = args.get("solution");
    if (solName.empty()) {
        for (const std::string& s : args.positional) {
            if (s == ctx.problemName) continue;
            solName = s;
            break;
        }
    }
    if (solName.empty()) {
        log_err("请用 -s <解法名> 指定要运行的解法（poly info 可查看解法列表）");
        return 1;
    }
    const SolutionInfo* sol = p.findSolution(solName);
    if (!sol) {
        log_err(str("解法 {} 不存在", solName));
        return 1;
    }

    JudgeEnv env;
    std::string err;
    if (!prepare_judge(ctx, p, "solutions", env, err)) {
        log_err(err);
        return 1;
    }

    JudgeLimits lim;
    lim.timeLimitMs = static_cast<double>(args.get_int("time-limit", p.timeLimitMs));
    lim.memoryLimitKb = args.get_int("memory-limit", p.memoryLimitKb / 1024) * 1024;
    bool check = !args.has_flag("no-check");

    std::string exeRel = solution_exe_rel(sol->name, env.exeSuffix);
    std::string exeAbs = p.path(exeRel);
    if (!fs::is_file(exeAbs)) {
        log_err(str("{} 尚未编译（poly build）", sol->file));
        return 1;
    }

    if (check && !p.interactive) {
        std::vector<std::string> aerrs;
        ensure_answers(env, args.get("answer-solution"), aerrs);
        for (const std::string& e : aerrs) log_err(e);
        if (!aerrs.empty()) return 1;
    }

    std::vector<int> tests = collect_tests(p, args);
    if (tests.empty()) {
        log_err("没有可运行的测试点（先执行 poly gen）");
        return 1;
    }

    log_step(str("运行 {} / {}", p.name, sol->name));
    Column colTest("TEST"), colVerdict("VERDICT"), colTime("TIME"), colMem("MEMORY"), colMsg("MESSAGE");
    Verdict worst = Verdict::OK;
    double maxTime = 0;
    long long maxMem = 0;
    int failures = 0;

    for (int t : tests) {
        TestOutcome r = p.interactive ? judge_interactive(env, exeAbs, sol->name, t, lim)
                                      : judge_test(env, exeAbs, sol->name, t, lim, check);
        colTest.cells.push_back(num(t));
        colVerdict.cells.push_back(colorize_verdict(verdict_text(r.verdict)));
        colTime.cells.push_back(r.timeMs > 0 ? num_fixed(r.timeMs, 0) + " ms" : "-");
        colMem.cells.push_back(r.memoryKb > 0 ? human_mem_kb(r.memoryKb) : "-");
        std::string msg = r.message;
        if (msg.empty() && !r.crashLog.empty()) msg = r.crashLog;
        colMsg.cells.push_back(msg);
        if (r.verdict != Verdict::OK) failures++;
        if (r.verdict == Verdict::TL || r.verdict == Verdict::ML || r.verdict == Verdict::RE) worst = r.verdict;
        maxTime = std::max(maxTime, r.timeMs);
        maxMem = std::max(maxMem, r.memoryKb);
        if (failures > 0 && args.has_flag("stop-on-fail")) break;
    }
    print_table({colTest, colVerdict, colTime, colMem, colMsg});

    std::string summary = str("{} 个测试点，{} 个未通过；最大用时 {} / 最大内存 {}", tests.size(), failures,
                              human_ms(maxTime), human_mem_kb(maxMem));
    if (failures == 0)
        log_ok(summary);
    else
        log_err(summary);
    (void)worst;
    return failures == 0 ? 0 : 1;
}

}  // namespace poly
