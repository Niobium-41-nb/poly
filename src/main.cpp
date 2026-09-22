#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "cli.h"
#include "commands.h"
#include "format.h"
#include "log.h"
#include "plat.h"
#include "version.h"

// 命令表与 help 在 commands.cpp（窗口版入口 gui_main.cpp 也要用），这里只有控制台入口。
int main(int argc, char** argv) {
    poly::init_console();
    std::vector<std::string> raw = poly::get_args_utf8(argc, argv);

    if (raw.size() < 2) {
        poly::log_configure(poly::stdout_is_tty(), false, false);
        poly::Args empty;
        empty.command = "help";
        poly::Context ctx;
        return poly::cmd_help(ctx, empty);
    }

    if (raw[1] == "--version" || raw[1] == "-version" || raw[1] == "version") {
        std::printf("poly %s\n", POLY_VERSION);
        return 0;
    }

    std::vector<std::string> rest(raw.begin() + 1, raw.end());
    poly::Args args;
    std::string err;
    if (!poly::parse_args(rest, args, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 2;
    }
    if (args.command.empty()) args.command = "help";
    if (args.has_flag("version")) {
        std::printf("poly %s\n", POLY_VERSION);
        return 0;
    }
    if (args.has_flag("help") && args.command != "help") {
        poly::Context ctx;
        return poly::cmd_help(ctx, args);
    }

    const poly::CommandSpec* c = poly::find_command(args.command);
    if (!c) {
        std::fprintf(stderr, "未知命令: %s\n\n", args.command.c_str());
        poly::Args empty;
        empty.command = "help";
        poly::Context ctx;
        poly::cmd_help(ctx, empty);
        return 2;
    }

    poly::Context ctx;
    poly::resolve_context(args, ctx, err);
    poly::log_configure(ctx.color, ctx.verbose, ctx.quiet);

    try {
        return c->fn(ctx, args);
    } catch (const std::exception& e) {
        poly::log_err(std::string("内部错误: ") + e.what());
        return 1;
    } catch (...) {
        poly::log_err("内部错误: 未知异常");
        return 1;
    }
}
