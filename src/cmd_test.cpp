#include <algorithm>

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

std::string cell_for(Verdict v) {
    std::string t = verdict_text(v);
    return colorize_verdict(t);
}

}  // namespace

int cmd_test(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;
    if (p.solutions.empty()) {
        log_err("solutions/ 下没有任何解法");
        return 1;
    }

    JudgeEnv env;
    std::string err;
    if (!prepare_judge(ctx, p, "all", env, err)) {
        log_err(err);
        return 1;
    }

    JudgeLimits lim;
    lim.timeLimitMs = static_cast<double>(args.get_int("time-limit", p.timeLimitMs));
    lim.memoryLimitKb = args.get_int("memory-limit", p.memoryLimitKb / 1024) * 1024;
    bool check = !args.has_flag("no-check");

    std::vector<int> tests = selected_tests(p, {}, 0);
    if (tests.empty()) {
        log_err("还没有测试点（先执行 poly gen）");
        return 1;
    }

    if (check && !p.interactive) {
        std::vector<std::string> aerrs;
        ensure_answers(env, args.get("answer-solution"), aerrs);
        for (const std::string& e : aerrs) log_err(e);
        if (!aerrs.empty()) return 1;
    }

    log_step(str("评测 {}（{} 个测试点，{} 个解法）", p.name, tests.size(), p.solutions.size()));

    bool allExpected = true;
    double globalMaxTime = 0;

    for (const SolutionInfo& sol : p.solutions) {
        std::string exeAbs = p.path(solution_exe_rel(sol.name, env.exeSuffix));
        if (!fs::is_file(exeAbs)) {
            log_err(str("{} 尚未编译（poly build）", sol.file));
            allExpected = false;
            continue;
        }
        std::vector<TestOutcome> outcomes;
        std::vector<std::string> cellVerdicts;
        double maxTime = 0;
        long long maxMem = 0;
        int failures = 0;
        std::string firstFailure;
        for (int t : tests) {
            TestOutcome r = p.interactive ? judge_interactive(env, exeAbs, sol.name, t, lim)
                                          : judge_test(env, exeAbs, sol.name, t, lim, check);
            outcomes.push_back(r);
            cellVerdicts.push_back(cell_for(r.verdict));
            if (r.verdict != Verdict::OK) {
                failures++;
                if (firstFailure.empty()) firstFailure = str("#{}: {} {}", t, verdict_text(r.verdict), r.message);
            }
            maxTime = std::max(maxTime, r.timeMs);
            maxMem = std::max(maxMem, r.memoryKb);
        }
        globalMaxTime = std::max(globalMaxTime, maxTime);

        Verdict summaryVerdict = Verdict::OK;
        for (const TestOutcome& r : outcomes) {
            switch (r.verdict) {
                case Verdict::TL: summaryVerdict = Verdict::TL; break;
                case Verdict::ML: if (summaryVerdict != Verdict::TL) summaryVerdict = Verdict::ML; break;
                case Verdict::RE: if (summaryVerdict == Verdict::OK) summaryVerdict = Verdict::RE; break;
                case Verdict::FAIL: if (summaryVerdict == Verdict::OK) summaryVerdict = Verdict::FAIL; break;
                case Verdict::WA:
                case Verdict::PE:
                    if (summaryVerdict == Verdict::OK) summaryVerdict = r.verdict;
                    break;
                default: break;
            }
        }

        std::string expected = sol.expected.empty() ? "ac" : sol.expected;
        bool satisfied = verdict_is_expected(summaryVerdict, expected);
        if (!satisfied) allExpected = false;

        std::string status;
        if (satisfied) {
            status = "符合预期";
        } else {
            status = str("不符合预期（期望 {}）", to_upper(expected));
        }

        log_raw(str("{}  [{}]\n", sol.name, sol.tag.empty() ? "solution" : sol.tag));

        size_t shown = tests.size();
        if (shown > 25) shown = 25;
        Column cTest("TEST"), cVerdict("VERDICT");
        for (size_t i = 0; i < shown; i++) {
            cTest.cells.push_back(num(tests[i]));
            cVerdict.cells.push_back(cellVerdicts[i]);
        }
        if (tests.size() > shown) {
            cTest.cells.push_back("...");
            cVerdict.cells.push_back(str("+{}", tests.size() - shown));
        }
        print_table({cTest, cVerdict});
        log_raw(str("  结果 {}  用时 {}  内存 {}  {}\n", colorize_verdict(verdict_text(summaryVerdict)),
                    human_ms(maxTime), human_mem_kb(maxMem), status));
        if (!firstFailure.empty()) log_raw(str("  首个失败：{}\n", firstFailure));
        log_raw("\n");
    }

    if (allExpected) {
        log_ok("所有解法的表现都与 problem.json 中的预期一致");
        return 0;
    }
    log_err("有解法的表现与预期不一致（用 poly config <题目> solution.<名字>.expected <ac|wa|tl|ml|re|any> 调整）");
    return 1;
}

}  // namespace poly
