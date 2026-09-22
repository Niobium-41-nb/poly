// 对拍：用生成器造随机数据，比较两个解法的输出，失败用例自动留档。
#include <algorithm>
#include <chrono>
#include <random>

#include "checker.h"
#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "judge.h"
#include "log.h"
#include "pipeline.h"
#include "proc.h"
#include "script.h"
#include "strutil.h"

namespace poly {

namespace {

struct GenArgs {
    std::vector<std::string> argv;  // 参数（不含生成器名）
};

std::vector<GenArgs> collect_arg_sets(const Problem& p, const std::string& genStem) {
    std::vector<GenArgs> out;
    std::string text;
    if (!fs::read_file(p.path(p.testScript), text)) return out;
    Script script;
    std::string err;
    if (!parse_test_script(text, script, err)) return out;
    for (const ScriptCommand& cmd : script.commands) {
        if (cmd.stages.empty()) continue;
        if (cmd.stages[0].empty() || cmd.stages[0][0] != genStem) continue;
        GenArgs g;
        for (size_t i = 1; i < cmd.stages[0].size(); i++) g.argv.push_back(cmd.stages[0][i]);
        bool dup = false;
        for (const GenArgs& e : out)
            if (e.argv == g.argv) dup = true;
        if (!dup) out.push_back(g);
    }
    return out;
}

bool looks_numeric(const std::string& s) { return is_integer(s); }

std::vector<std::string> jitter_args(const std::vector<std::string>& args, std::mt19937_64& rng, bool sweep) {
    std::vector<std::string> out;
    for (const std::string& a : args) {
        if (a.find('{') != std::string::npos) {
            out.push_back(a);
            continue;
        }
        if (sweep && looks_numeric(a) && a.size() < 12) {
            long long v = to_int(a, 1);
            if (v > 1) {
                long long lo = std::max<long long>(1, v / 10);
                long long hi = std::max<long long>(lo, v);
                std::uniform_int_distribution<long long> dist(lo, hi);
                v = dist(rng);
            }
            out.push_back(num(v));
            continue;
        }
        out.push_back(a);
    }
    return out;
}

std::string verdict_of(const ProcResult& r, const JudgeLimits& lim) {
    if (!r.started) return "FAIL";
    if (r.timedOut) return "TL";
    if (r.oom) return "ML";
    if (r.exitCode != 0) return "RE";
    (void)lim;
    return "OK";
}

void save_artifact(const Problem& p, const std::string& name, const std::string& data) {
    fs::write_file(p.path(fs::join("stress", name)), data);
}

}  // namespace

int cmd_stress(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;
    if (p.interactive) {
        log_err("交互题暂不支持对拍");
        return 1;
    }

    JudgeEnv env;
    std::string err;
    if (!prepare_judge(ctx, p, "solutions", env, err)) {
        log_err(err);
        return 1;
    }

    std::vector<std::string> names = args.get_all("solution");
    std::string nameA = names.size() > 0 ? names[0] : std::string();
    std::string nameB = names.size() > 1 ? names[1] : std::string();
    if (nameA.empty()) nameA = p.findSolution("main") ? "main" : (p.solutions.empty() ? "" : p.solutions[0].name);
    if (nameB.empty()) {
        for (const SolutionInfo& s : p.solutions) {
            if (s.name != nameA) {
                nameB = s.name;
                break;
            }
        }
    }
    if (nameA.empty() || nameB.empty()) {
        log_err("对拍需要两个解法（-s 正解 -s 暴力）");
        return 1;
    }
    const SolutionInfo* sa = p.findSolution(nameA);
    const SolutionInfo* sb = p.findSolution(nameB);
    if (!sa || !sb) {
        log_err(str("解法不存在：{}", sa ? nameB : nameA));
        return 1;
    }

    std::string genStem = args.get("gen");
    if (genStem.empty()) {
        for (const SolutionInfo& s : p.solutions) (void)s;
        for (const std::string& g : p.generators) {
            genStem = fs::stem(g);
            break;
        }
        if (genStem.empty()) {
            for (const std::string& f : fs::list_dir_sorted(p.filesDir(), true)) {
                std::string base = fs::basename(f);
                if (starts_with(to_lower(base), "gen") && has_suffix_ci(base, ".cpp")) {
                    genStem = fs::stem(base);
                    break;
                }
            }
        }
    }
    if (genStem.empty()) {
        log_err("找不到生成器（--gen <名字>）");
        return 1;
    }

    std::string genExe = p.path(generator_exe_rel(genStem, env.exeSuffix));
    if (!fs::is_file(genExe)) {
        if (!ensure_built(ctx, p, "gen", env.compiler, err)) {
            log_err(err);
            return 1;
        }
    }
    if (!fs::is_file(genExe)) {
        log_err(str("生成器 {} 尚未编译", genStem));
        return 1;
    }

    std::string exeA = p.path(solution_exe_rel(sa->name, env.exeSuffix));
    std::string exeB = p.path(solution_exe_rel(sb->name, env.exeSuffix));
    if (!fs::is_file(exeA) || !fs::is_file(exeB)) {
        log_err("解法尚未编译（poly build）");
        return 1;
    }

    int iterations = static_cast<int>(args.get_int("iterations", args.get_int("n", 200)));
    bool untilTl = args.has_flag("until-tl");
    bool sweep = !args.has_flag("no-sweep");
    unsigned long long seed = static_cast<unsigned long long>(args.get_int("seed", 0));
    if (seed == 0) {
        seed = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }
    std::mt19937_64 rng(seed);

    std::vector<GenArgs> argSets = collect_arg_sets(p, genStem);
    if (argSets.empty()) argSets.push_back(GenArgs{});

    JudgeLimits lim;
    lim.timeLimitMs = static_cast<double>(args.get_int("time-limit", p.timeLimitMs));
    lim.memoryLimitKb = args.get_int("memory-limit", p.memoryLimitKb / 1024) * 1024;

    fs::mkdirs(p.stressDir());
    std::string tmpDir = fs::join("stress", "tmp");
    fs::mkdirs(p.path(tmpDir));

    log_step(str("对拍 {}：{}（正解） vs {}（暴力），{} 轮，种子 {}", p.name, sa->name, sb->name, iterations, seed));
    if (argSets.size() > 1) log_debug(str("从测试脚本收集到 {} 组生成器参数", argSets.size()));

    int mismatches = 0;
    int done = 0;
    for (int iter = 1; iter <= iterations; iter++) {
        std::uniform_int_distribution<size_t> pick(0, argSets.size() - 1);
        std::vector<std::string> genArgs = jitter_args(argSets[pick(rng)].argv, rng, sweep);
        for (std::string& a : genArgs) {
            a = replace_all(a, "{i}", num(iter));
            a = replace_all(a, "{seed}", num(static_cast<long long>(rng() % 1000000007ULL)));
        }
        genArgs.push_back(str("--seed={}", static_cast<long long>(rng() % 1000000007ULL)));

        std::string inRel = fs::join(tmpDir, "input.txt");
        ProcOptions go;
        go.workDir = p.dir;
        go.args = genArgs;
        go.stdoutFile = p.path(inRel);
        go.wallLimitMs = 60000;
        ProcResult gr = run_process(genExe, go);
        if (!gr.started || gr.exitCode != 0) {
            log_err(str("第 {} 轮：生成器失败（退出码 {}）", iter, gr.exitCode));
            break;
        }

        struct Side {
            std::string rel;
            ProcResult res;
        };
        auto run_side = [&](const std::string& exe, const std::string& tag) -> Side {
            Side s;
            s.rel = fs::join(tmpDir, tag + ".out");
            ProcOptions o;
            o.workDir = p.dir;
            o.stdinFile = p.path(inRel);
            o.stdoutFile = p.path(s.rel);
            o.stderrFile = p.path(fs::join(tmpDir, tag + ".err"));
            o.timeLimitMs = lim.timeLimitMs;
            o.memoryLimitKb = lim.memoryLimitKb;
            s.res = run_process(exe, o);
            return s;
        };

        Side a = run_side(exeA, "a");
        Side b = run_side(exeB, "b");
        done++;

        std::string va = verdict_of(a.res, lim);
        std::string vb = verdict_of(b.res, lim);
        if (va != "OK") {
            log_err(str("第 {} 轮：正解 {} 的结果是 {}，对拍终止（数据可能是非法的）", iter, sa->name, va));
            std::string input;
            fs::read_file(p.path(inRel), input);
            save_artifact(p, str("bad-input.{}.txt", iter), input);
            break;
        }

        bool bad = false;
        std::string reason;
        if (vb != "OK") {
            if (untilTl || vb == "RE" || vb == "ML") {
                bad = true;
                reason = str("{} 结果为 {}", sb->name, vb);
            }
        } else {
            CheckResult cr;
            if (p.checkerFile.empty()) {
                cr = builtin_check(p.checker, p.path(inRel), p.path(b.rel), p.path(a.rel));
            } else if (env.checkerExe.empty()) {
                log_err("自定义 checker 尚未编译");
                break;
            } else {
                ProcOptions co;
                co.workDir = p.dir;
                co.args = {inRel, b.rel, a.rel, num(iter)};
                co.wallLimitMs = 30000;
                ProcResult crr = run_process(env.checkerExe, co);
                cr.verdict = verdict_from_process(crr.exitCode, crr.out.empty() ? crr.err : crr.out, cr.message);
            }
            if (cr.verdict != Verdict::OK) {
                bad = true;
                reason = str("{}: {}", verdict_text(cr.verdict), cr.message);
            }
        }

        if (bad) {
            mismatches++;
            std::string input, outA, outB;
            fs::read_file(p.path(inRel), input);
            fs::read_file(p.path(a.rel), outA);
            fs::read_file(p.path(b.rel), outB);
            std::string prefix = str("fail.{}", iter);
            save_artifact(p, prefix + ".txt", input);
            save_artifact(p, prefix + ".ans", outA);
            save_artifact(p, prefix + ".bad", outB);
            save_artifact(p, prefix + ".info",
                          str("生成器参数: {}\n{} 用时 {} ms / {} KB\n{} 用时 {} ms / {} KB\n结论: {}\n",
                              join(genArgs, " "), sa->name, num_fixed(a.res.cpuMs, 3), a.res.peakCommitKb,
                              sb->name, num_fixed(b.res.cpuMs, 3), b.res.peakCommitKb, reason));
            log_err(str("第 {} 轮发现差异：{}", iter, reason));
            log_info(str("  用例已保存到 stress/{}.txt", prefix));
            if (!args.has_flag("keep-going")) break;
        } else if (iter % 25 == 0) {
            log_info(str("  已对拍 {} 轮，暂未发现差异", iter));
        }
    }

    if (mismatches == 0) {
        log_ok(str("{} 轮对拍未发现差异", done));
        return 0;
    }
    log_err(str("{} 轮对拍发现 {} 处差异，用例保存在 stress/", done, mismatches));
    return 1;
}

}  // namespace poly
