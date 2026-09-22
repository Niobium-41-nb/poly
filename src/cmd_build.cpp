#include <algorithm>

#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "pipeline.h"
#include "strutil.h"

namespace poly {

namespace {

const std::set<std::string> kTargets = {"all", "gen", "validator", "checker", "interactor", "solutions"};

std::string target_of(const Context& ctx, const Args& args) {
    for (const std::string& s : args.positional) {
        if (s == ctx.problemName) continue;
        if (kTargets.count(s)) return s;
    }
    return "all";
}

}  // namespace

int cmd_build(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    BuildOptions opt;
    opt.what = target_of(ctx, args);
    opt.rebuild = args.has_flag("rebuild");
    opt.quiet = false;

    log_step(str("编译 {}（{}）", p.name, opt.what));
    Compiler cc;
    std::vector<std::string> errors;
    build_problem(ctx, p, opt, cc, errors);
    for (const std::string& e : errors) log_err(e);
    if (!errors.empty()) return 1;

    if (!cc.version.empty()) log_debug(str("编译器 {}", cc.version));

    int count = 0;
    for (const std::string& f : fs::list_dir_sorted(p.path(fs::join("output", "bin")), true)) count++;
    log_ok(str("{} 个可执行文件位于 {}", count, fs::join("output", "bin")));
    return 0;
}

}  // namespace poly
