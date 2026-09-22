#pragma once

#include <map>
#include <string>
#include <vector>

#include "cli.h"

namespace poly {

// 网页工作台与 Win32 窗口界面共用的「一次操作」描述。
// 每个操作最终都会构造 Args 并调用同一个 cmd_* 函数，因此界面与命令行的行为一致。
struct OpRequest {
    std::string action;   // build / gen / validate / test / run / stress / statement / clean / package / doctor
    std::string problem;  // 题目名，工作区级操作为空
    std::map<std::string, std::string> params;

    std::string get(const std::string& key, const std::string& def = std::string()) const;
    long long get_int(const std::string& key, long long def) const;
};

struct OpInfo {
    const char* action;
    const char* label;
    bool needsProblem;  // 是否需要选中题目
};

// 界面按这个表生成按钮。
const std::vector<OpInfo>& operation_table();

// 执行一个操作；返回 cmd_* 的退出码，label 输出可读的操作名。
int run_operation(Context& ctx, const OpRequest& req, std::string& label);

}  // namespace poly
