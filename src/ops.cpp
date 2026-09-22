// 操作分发：把「界面上的一个动作」翻译成一次命令行调用。
//
// 两个界面（poly ui --web 的网页、poly-gui 的 Win32 窗口）都走这里，
// 保证界面操作 = 命令行操作，不会出现两套逻辑。
#include "ops.h"

#include "commands.h"
#include "format.h"
#include "log.h"
#include "strutil.h"

namespace poly {

std::string OpRequest::get(const std::string& key, const std::string& def) const {
    auto it = params.find(key);
    if (it == params.end()) return def;
    return it->second;
}

long long OpRequest::get_int(const std::string& key, long long def) const {
    auto it = params.find(key);
    if (it == params.end()) return def;
    return to_int(it->second, def);
}

const std::vector<OpInfo>& operation_table() {
    static const std::vector<OpInfo> kOps = {
        {"build", "编译", true},
        {"gen", "生成测试点", true},
        {"validate", "校验测试点", true},
        {"test", "评测（全部解法）", true},
        {"run", "运行单个解法", true},
        {"stress", "对拍", true},
        {"statement", "渲染题面", true},
        {"clean", "清理中间产物", true},
        {"package", "导出题目包", true},
        {"doctor", "环境自检", false},
    };
    return kOps;
}

int run_operation(Context& ctx, const OpRequest& req, std::string& label) {
    const std::string& action = req.action;
    const std::string& name = req.problem;
    // 命令行走 resolve_context 从位置参数取题目名，这里直接调 cmd_* 就得自己填。
    ctx.problemName = name;

    Args a;
    if (!name.empty()) a.positional.push_back(name);

    if (action == "doctor") {
        a.command = "doctor";
        a.positional.clear();
        label = "环境自检";
        return cmd_doctor(ctx, a);
    }
    if (action == "build") {
        a.command = "build";
        label = name.empty() ? "编译" : ("编译 " + name);
        return cmd_build(ctx, a);
    }
    if (action == "gen") {
        a.command = "gen";
        label = name.empty() ? "生成测试点" : ("生成测试点 " + name);
        return cmd_generate(ctx, a);
    }
    if (action == "validate") {
        a.command = "validate";
        label = name.empty() ? "校验测试点" : ("校验测试点 " + name);
        return cmd_validate(ctx, a);
    }
    if (action == "test") {
        a.command = "test";
        label = name.empty() ? "评测" : ("评测 " + name);
        return cmd_test(ctx, a);
    }
    if (action == "run") {
        a.command = "run";
        std::string sol = req.get("solution");
        if (sol.empty()) {
            log_err("请先选择要运行的解法");
            return 1;
        }
        a.values["solution"].push_back(sol);
        long long t = req.get_int("test", 0);
        if (t > 0) a.values["test"].push_back(num(t));
        label = "运行 " + name + " / " + sol + (t > 0 ? ("（测试点 " + num(t) + "）") : "");
        return cmd_run(ctx, a);
    }
    if (action == "stress") {
        a.command = "stress";
        std::string s1 = req.get("solution");
        std::string s2 = req.get("solution2");
        if (s1.empty() || s2.empty()) {
            log_err("对拍需要选两个解法");
            return 1;
        }
        if (s1 == s2) {
            log_err("对拍的两个解法不能相同");
            return 1;
        }
        a.values["solution"].push_back(s1);
        a.values["solution"].push_back(s2);
        long long rounds = req.get_int("rounds", 300);
        if (rounds < 1) rounds = 1;
        if (rounds > 100000) rounds = 100000;
        a.values["iterations"].push_back(num(rounds));
        label = str("对拍 {} vs {}（{} 轮）", s1, s2, rounds);
        return cmd_stress(ctx, a);
    }
    if (action == "statement") {
        a.command = "statement";
        a.flags.insert("html");
        label = name.empty() ? "渲染题面" : ("渲染题面 " + name);
        return cmd_statement(ctx, a);
    }
    if (action == "clean") {
        a.command = "clean";
        label = name.empty() ? "清理中间产物" : ("清理 " + name);
        return cmd_clean(ctx, a);
    }
    if (action == "package") {
        a.command = "package";
        std::string format = req.get("format", "polygon");
        a.values["format"].push_back(format);
        std::string output = req.get("output");
        if (!output.empty()) a.values["output"].push_back(output);
        label = "导出 " + format + (name.empty() ? "" : ("（" + name + "）"));
        return cmd_package(ctx, a);
    }
    label = "未知操作";
    log_err(str("未知操作 {}", action));
    return 1;
}

}  // namespace poly
