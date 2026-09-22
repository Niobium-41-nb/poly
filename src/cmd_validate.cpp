#include "commands.h"
#include "format.h"
#include "log.h"
#include "pipeline.h"
#include "strutil.h"

namespace poly {

int cmd_validate(Context& ctx, const Args& args) {
    Problem p;
    if (!ctx.requireProblem(p)) return 1;
    if (p.validator.empty()) {
        log_warn(str("题目 {} 没有配置 validator，跳过", p.name));
        return 0;
    }
    Compiler cc;
    std::string err;
    if (!ensure_built(ctx, p, "validator", cc, err)) {
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

    log_step(str("校验测试点 {}", p.name));
    std::vector<std::string> errors;
    int checked = 0;
    bool ok = validate_tests(ctx, p, cc, only, errors, checked);
    for (size_t i = 0; i < errors.size() && i < 10; i++) log_err(errors[i]);
    if (errors.size() > 10) log_err(str("... 还有 {} 条错误", errors.size() - 10));
    if (!ok) return 1;
    log_ok(str("{} 个测试点全部合法", checked));
    return 0;
}

}  // namespace poly
