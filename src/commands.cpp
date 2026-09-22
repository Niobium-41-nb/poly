// 命令表与 help。
// 单独成文件的原因：控制台版（main.cpp）与窗口版（gui_main.cpp）都要链接它。
#include "commands.h"

#include <string>

#include "format.h"
#include "log.h"
#include "strutil.h"
#include "version.h"

namespace poly {

namespace {

const CommandSpec kCommands[] = {
    {"init", "<题目名> [选项]", "创建题目工作区（--checker custom / --interactive / --time-limit / --memory-limit）",
     cmd_init},
    {"list", "", "列出工作区中的所有题目", cmd_list},
    {"info", "<题目名>", "查看题目详情与解法列表", cmd_info},
    {"config", "<题目名> <键> [值]", "读取或修改题目配置（--list 查看全部键）", cmd_config},
    {"build", "<题目名> [目标]", "编译生成器、校验器、checker、交互器与所有解法", cmd_build},
    {"gen", "<题目名>", "按脚本生成测试点", cmd_generate},
    {"validate", "<题目名>", "用校验器检查所有测试点", cmd_validate},
    {"run", "<题目名> -s <解法>", "运行单个解法并判题（--check / --test N）", cmd_run},
    {"test", "<题目名>", "运行所有解法跑全部测试点，输出判定表", cmd_test},
    {"stress", "<题目名> -s <正解> -s <暴力>", "对拍（--gen 生成器 / -n 轮数 / --until-tl）", cmd_stress},
    {"statement", "<题目名>", "渲染题面（--html / --pdf / --open）", cmd_statement},
    {"package", "<题目名>", "打包导出（--format polygon|qduoj|fps|hydro|hoj，-o 输出文件）", cmd_package},
    {"import", "[格式] <文件>", "导入题目包（qduoj / fps / hydro / hoj，格式可省略自动识别）", cmd_import},
    {"ui", "", "打开出题工作台窗口（--web 改用浏览器版 / --port / --no-open）", cmd_ui},
    {"clean", "<题目名>", "清理中间产物（--tests / --stress / --all）", cmd_clean},
    {"doctor", "", "检查编译器等运行环境", cmd_doctor},
    {"testlib", "[路径]", "导出内置 testlib.h（--print 输出到终端）", cmd_testlib},
    {"help", "[命令]", "显示帮助", cmd_help},
    {nullptr, nullptr, nullptr, nullptr},
};

void print_usage() {
    log_raw(str("poly {} — 本地离线 Polygon 复刻（出题系统）\n", POLY_VERSION));
    log_raw("\n用法: poly <命令> [参数...]\n\n命令:\n");
    size_t width = 0;
    for (const CommandSpec* c = command_table(); c->name; c++) {
        size_t w = std::string(c->name).size() + 1 + std::string(c->args).size();
        if (w > width) width = w;
    }
    for (const CommandSpec* c = command_table(); c->name; c++) {
        std::string left = std::string(c->name);
        if (c->args[0]) left += std::string(" ") + c->args;
        log_raw(str("  {}  {}\n", rpad(left, width), c->desc));
    }
    log_raw("\n全局选项:\n");
    log_raw("  -p, --problem <名字>   指定题目\n");
    log_raw("  -w, --workspace <目录> 指定工作区（默认 ./problems）\n");
    log_raw("  -j, --jobs <N>         并行度\n");
    log_raw("  -v, --verbose          输出详细信息\n");
    log_raw("  -q, --quiet            安静模式\n");
    log_raw("      --no-color         禁用彩色输出\n");
    log_raw("      --json             JSON 格式输出\n");
}

}  // namespace

const CommandSpec* command_table() { return kCommands; }

int cmd_help(Context& ctx, const Args& args) {
    (void)ctx;
    std::string topic = args.positional.empty() ? std::string() : args.positional[0];
    if (topic.empty()) {
        print_usage();
        return 0;
    }
    const CommandSpec* c = find_command(topic);
    if (!c) {
        log_err(str("未知命令: {}", topic));
        return 1;
    }
    log_raw(str("poly {} {}\n  {}\n", c->name, c->args, c->desc));
    return 0;
}

}  // namespace poly
