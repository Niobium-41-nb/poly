#include "pipeline.h"

#include <algorithm>
#include <set>

#include "checker.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "proc.h"
#include "script.h"
#include "strutil.h"
#include "testlib_source.h"

namespace poly {

namespace {

std::vector<std::string> testlib_flags() { return {"-Ifiles", "-I."}; }

bool needs_rebuild(const Problem& p, const std::string& srcRel, const std::string& exeRel) {
    std::string exeAbs = p.path(exeRel);
    if (!fs::is_file(exeAbs)) return true;
    std::string srcAbs = p.path(srcRel);
    if (!fs::is_file(srcAbs)) return true;
    return fs::modified_time(srcAbs) > fs::modified_time(exeAbs);
}

// output/bin 下的产物是用某个具体编译器（连同它的 libstdc++）编出来的。
// 机器上常同时存在多套 MinGW（ucrt64 / mingw64 / Conda / RedPanda …），
// 混用两套运行库的二进制会在退出时随机崩溃（0xC0000005），而增量编译只看
// 修改时间、不会因此重建。这里用一枚指纹记录上次使用的编译器，一旦变了就全部重建。
std::string toolchain_stamp_rel() { return fs::join(fs::join("output", "bin"), ".toolchain"); }

std::string toolchain_signature(const Compiler& cc) {
    return cc.exe + "\n" + cc.version + "\n" + cc.stdFlag + "\n" + join(cc.flags, " ") + "\n";
}

bool toolchain_changed(const Problem& p, const Compiler& cc) {
    return fs::read_file(p.path(toolchain_stamp_rel())) != toolchain_signature(cc);
}

void write_toolchain_stamp(const Problem& p, const Compiler& cc) {
    fs::write_file(p.path(toolchain_stamp_rel()), toolchain_signature(cc));
}

void ensure_testlib(const Problem& p) {
    std::string dst = p.path(fs::join("files", "testlib.h"));
    if (fs::is_file(dst)) return;
    const std::string& src = testlib_source();
    if (!src.empty()) fs::write_file(dst, src);
}

struct Job {
    std::string srcRel;
    std::string outRel;
    std::vector<std::string> flags;
    std::string what;
    std::string label;
};

void add_job(std::vector<Job>& jobs, const Problem& p, const std::string& srcRel, const std::string& outRel,
             const std::vector<std::string>& flags, const std::string& what, const std::string& label) {
    if (srcRel.empty() || !fs::is_file(p.path(srcRel))) return;
    Job j;
    j.srcRel = srcRel;
    j.outRel = outRel;
    j.flags = flags;
    j.what = what;
    j.label = label;
    jobs.push_back(j);
}

}  // namespace

std::string generator_exe_rel(const std::string& stem, const std::string& suffix) {
    return fs::join(fs::join("output", "bin"), stem + suffix);
}

std::string solution_exe_rel(const std::string& stem, const std::string& suffix) {
    return fs::join(fs::join("output", "bin"), stem + suffix);
}

std::string checker_exe_rel(const Problem& p, const std::string& suffix) {
    if (p.checkerFile.empty()) return std::string();
    return fs::join(fs::join("output", "bin"), "checker" + suffix);
}

std::string interactor_exe_rel(const Problem& p, const std::string& suffix) {
    if (p.interactor.empty()) return std::string();
    return fs::join(fs::join("output", "bin"), "interactor" + suffix);
}

static std::vector<Job> collect_jobs(const Problem& p, const std::string& suffix, const std::string& what) {
    std::vector<Job> jobs;
    bool all = (what == "all");
    if (all || what == "gen") {
        std::vector<std::string> gens = p.generators;
        if (gens.empty()) {
            for (const std::string& f : fs::list_dir_sorted(p.filesDir(), true)) {
                std::string base = fs::basename(f);
                if (starts_with(to_lower(base), "gen") && has_suffix_ci(base, ".cpp")) {
                    gens.push_back(fs::join("files", base));
                }
            }
        }
        for (const std::string& g : gens) {
            std::string stem = fs::stem(g);
            add_job(jobs, p, g, generator_exe_rel(stem, suffix), testlib_flags(), "gen", stem);
        }
    }
    if (all || what == "validator") {
        add_job(jobs, p, p.validator, fs::join(fs::join("output", "bin"), "validator" + suffix), testlib_flags(),
                "validator", "validator");
    }
    if (all || what == "checker") {
        add_job(jobs, p, p.checkerFile, checker_exe_rel(p, suffix), testlib_flags(), "checker", "checker");
    }
    if (all || what == "interactor") {
        add_job(jobs, p, p.interactor, interactor_exe_rel(p, suffix), testlib_flags(), "interactor", "interactor");
    }
    if (all || what == "solutions") {
        for (const SolutionInfo& s : p.solutions) {
            add_job(jobs, p, s.file, solution_exe_rel(s.name, suffix), {}, "solutions", s.name);
        }
    }
    return jobs;
}

bool build_problem(Context& ctx, Problem& p, const BuildOptions& opt, Compiler& cc,
                   std::vector<std::string>& errors) {
    std::string cerr;
    if (!resolve_compiler(ctx.compiler, ctx.stdFlag, cc, cerr)) {
        errors.push_back(cerr);
        return false;
    }
    if (!opt.quiet) log_debug(str("使用编译器 {}", cc.exe));
    ensure_testlib(p);
    sync_solutions(p);

    std::string suffix = exe_suffix();
    std::vector<Job> jobs = collect_jobs(p, suffix, opt.what);
    if (jobs.empty()) {
        if (!opt.quiet) log_warn(str("没有需要编译的目标（{}）", opt.what));
        return true;
    }
    int compiled = 0, skipped = 0;
    bool forceAll = opt.rebuild || toolchain_changed(p, cc);
    if (forceAll && !opt.rebuild && !opt.quiet) {
        log_info("编译器与上次编译产出不同，全部重建");
    }
    for (const Job& j : jobs) {
        if (!forceAll && !needs_rebuild(p, j.srcRel, j.outRel)) {
            skipped++;
            continue;
        }
        std::string logText, err;
        if (!compile_to(cc, p.dir, j.srcRel, j.outRel, j.flags, logText, err)) {
            std::string compact = replace_all(trim(logText), "\n", "\n        ");
            errors.push_back(str("[{}] 编译失败：{}\n        {}", j.what, j.srcRel,
                                 compact.empty() ? err : compact));
            continue;
        }
        compiled++;
        if (!opt.quiet) log_debug(str("编译 {}", j.srcRel));
    }
    if (!opt.quiet) log_info(str("编译完成：{} 个目标（{} 个已是最新）", compiled, skipped));
    write_toolchain_stamp(p, cc);
    return errors.empty();
}

bool ensure_built(Context& ctx, Problem& p, const std::string& what, Compiler& cc, std::string& err) {
    std::vector<std::string> errors;
    BuildOptions opt;
    opt.what = what;
    opt.quiet = true;
    if (!build_problem(ctx, p, opt, cc, errors)) {
        err = join(errors, "\n");
        return false;
    }
    return true;
}

std::vector<int> selected_tests(const Problem& p, const std::vector<int>& only, int single) {
    std::vector<int> all = p.testIndices();
    if (single > 0) {
        for (int t : all)
            if (t == single) return {t};
        return {};
    }
    if (only.empty()) return all;
    std::set<int> want(only.begin(), only.end());
    std::vector<int> out;
    for (int t : all)
        if (want.count(t)) out.push_back(t);
    return out;
}


GenerateStats generate_tests(Context& ctx, Problem& p, Compiler& cc, const std::vector<int>& only) {
    (void)ctx;
    (void)cc;
    GenerateStats stats;
    std::string text;
    if (!fs::read_file(p.path(p.testScript), text)) {
        stats.errors.push_back(str("读不到测试脚本 {}", p.testScript));
        return stats;
    }
    Script script;
    std::string perr;
    if (!parse_test_script(text, script, perr)) {
        stats.errors.push_back(str("{}：{}", p.testScript, perr));
        return stats;
    }
    if (script.commands.empty()) {
        stats.errors.push_back(str("{} 中没有任何生成命令", p.testScript));
        return stats;
    }

    std::set<int> wanted(only.begin(), only.end());
    std::string suffix = exe_suffix();
    std::string tmpDirRel = fs::join("output", "gen_tmp");
    fs::mkdirs(p.path(tmpDirRel));
    fs::mkdirs(p.testsDir());

    int autoIndex = 1;
    for (const ScriptCommand& cmd : script.commands) {
        int from = cmd.hasTarget ? cmd.from : autoIndex;
        int to = cmd.hasTarget ? cmd.to : autoIndex;
        for (int idx = from; idx <= to; idx++) {
            if (!cmd.hasTarget) autoIndex = idx + 1;
            if (!wanted.empty() && !wanted.count(idx)) continue;

            std::string lineRef = str("{}:{}（测试点 {}）", p.testScript, cmd.line, idx);
            std::string stdinRel;
            bool failed = false;
            for (size_t stage = 0; stage < cmd.stages.size(); stage++) {
                const std::vector<std::string>& argv = cmd.stages[stage];
                std::string stem = argv[0];
                std::string exeAbs = p.path(generator_exe_rel(stem, suffix));
                if (!fs::is_file(exeAbs)) {
                    stats.errors.push_back(str("{}：生成器 {} 尚未编译", lineRef, stem));
                    failed = true;
                    break;
                }
                std::string outRel = fs::join(tmpDirRel, str("stage.{}.{}.txt", idx, stage));
                ProcOptions o;
                o.workDir = p.dir;
                for (size_t a = 1; a < argv.size(); a++) o.args.push_back(argv[a]);
                if (!stdinRel.empty()) o.stdinFile = p.path(stdinRel);
                o.stdoutFile = p.path(outRel);
                o.wallLimitMs = 120000;
                o.memoryLimitKb = 2LL * 1024 * 1024;
                ProcResult pr = run_process(exeAbs, o);
                if (!pr.started || pr.timedOut || pr.exitCode != 0) {
                    stats.errors.push_back(str("{}：生成器 {} 失败（退出码 {}，超时 {}）", lineRef, stem,
                                               pr.exitCode, pr.timedOut ? "是" : "否"));
                    failed = true;
                    break;
                }
                stdinRel = outRel;
            }
            if (failed) {
                stats.failed++;
                continue;
            }
            std::string data;
            if (!fs::read_file(p.path(stdinRel), data)) {
                stats.errors.push_back(str("{}：无法读取生成结果", lineRef));
                stats.failed++;
                continue;
            }
            std::string dstRel = fs::join("tests", Problem::testName(idx, 2));
            if (!fs::write_file(p.path(dstRel), data)) {
                stats.errors.push_back(str("{}：无法写入 {}", lineRef, dstRel));
                stats.failed++;
                continue;
            }
            stats.generated++;
            log_debug(str("生成测试点 {}（{} 字节）", idx, data.size()));
        }
    }
    return stats;
}

bool validate_tests(Context& ctx, Problem& p, Compiler& cc, const std::vector<int>& only,
                    std::vector<std::string>& errors, int& checked) {
    (void)ctx;
    (void)cc;
    checked = 0;
    if (p.validator.empty()) return true;
    std::string exeAbs = p.path(fs::join(fs::join("output", "bin"), "validator" + exe_suffix()));
    if (!fs::is_file(exeAbs)) {
        errors.push_back("validator 尚未编译（poly build）");
        return false;
    }
    fs::mkdirs(p.path(fs::join("output", "runs")));
    for (int idx : selected_tests(p, only, 0)) {
        std::string base = p.testFileName(idx);
        ProcOptions o;
        o.workDir = p.dir;
        o.stdinFile = p.path(fs::join("tests", base));
        o.captureOutput = true;
        o.wallLimitMs = 120000;
        o.memoryLimitKb = 2LL * 1024 * 1024;
        ProcResult pr = run_process(exeAbs, o);
        checked++;
        if (!pr.started) {
            errors.push_back(str("测试点 {}：无法运行 validator（{}）", idx, pr.error));
            continue;
        }
        if (pr.timedOut) {
            errors.push_back(str("测试点 {}：validator 超时", idx));
            continue;
        }
        if (pr.exitCode != 0) {
            std::string msg = trim(pr.out.empty() ? pr.err : pr.out);
            if (msg.empty()) msg = "validator 校验失败";
            errors.push_back(str("测试点 {}：{}", idx, replace_all(msg, "\n", " ")));
        }
    }
    return errors.empty();
}

bool prepare_judge(Context& ctx, Problem& p, const std::string& what, JudgeEnv& env, std::string& err) {
    env = JudgeEnv();
    env.problem = p;
    env.exeSuffix = exe_suffix();
    if (!ensure_built(ctx, p, what, env.compiler, err)) return false;
    std::string ckRel = checker_exe_rel(p, env.exeSuffix);
    if (!ckRel.empty()) env.checkerExe = p.path(ckRel);
    std::string itRel = interactor_exe_rel(p, env.exeSuffix);
    if (!itRel.empty()) env.interactorExe = p.path(itRel);
    return true;
}

std::vector<int> parse_test_list(const std::string& spec) {
    std::vector<int> out;
    for (const std::string& partRaw : split(spec, ',', false)) {
        std::string part = trim(partRaw);
        if (part.empty()) continue;
        size_t dash = part.find('-');
        if (dash == std::string::npos) {
            if (is_integer(part)) out.push_back(static_cast<int>(to_int(part)));
            continue;
        }
        std::string a = trim(part.substr(0, dash));
        std::string b = trim(part.substr(dash + 1));
        if (!is_integer(a) || !is_integer(b)) continue;
        int from = static_cast<int>(to_int(a));
        int to = static_cast<int>(to_int(b));
        for (int i = from; i <= to; i++) out.push_back(i);
    }
    return out;
}

}  // namespace poly
