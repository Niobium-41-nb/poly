#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "pipeline.h"
#include "strutil.h"

namespace poly {

int cmd_generate(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;

    Compiler cc;
    std::string err;
    if (!ensure_built(ctx, p, "gen", cc, err)) {
        log_err(err);
        return 1;
    }

    std::vector<int> only;
    for (const std::string& spec : args.get_all("test")) {
        for (int t : parse_test_list(spec)) only.push_back(t);
    }
    if (!args.get("tests").empty()) {
        for (int t : parse_test_list(args.get("tests"))) only.push_back(t);
    }

    log_step(str("生成测试点 {}{}", p.name, only.empty() ? "" : str("（仅 {} 个）", only.size())));
    GenerateStats stats = generate_tests(ctx, p, cc, only);
    for (size_t i = 0; i < stats.errors.size() && i < 8; i++) log_err(stats.errors[i]);
    if (stats.errors.size() > 8) log_err(str("... 还有 {} 条错误", stats.errors.size() - 8));
    if (stats.failed > 0) return 1;
    if (stats.generated == 0 && only.empty()) {
        log_warn("脚本没有生成任何测试点");
        
        return 0;
    }
    log_ok(str("共生成 {} 个测试点（tests/ 目录现在有 {} 个）", stats.generated, p.testCount()));

    if (!args.has_flag("no-validate") && !p.validator.empty()) {
        if (!ensure_built(ctx, p, "validator", cc, err)) {
            log_warn(str("校验器无法编译：{}", err));
            return 0;
        }
        std::vector<std::string> errors;
        int checked = 0;
        if (!validate_tests(ctx, p, cc, only, errors, checked)) {
            for (size_t i = 0; i < errors.size() && i < 8; i++) log_err(errors[i]);
            if (errors.size() > 8) log_err(str("... 还有 {} 条错误", errors.size() - 8));
            return 1;
        }
        log_ok(str("{} 个测试点全部通过校验", checked));
    }
    return 0;
}

}  // namespace poly
