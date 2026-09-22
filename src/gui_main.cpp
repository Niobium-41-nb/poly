// poly-gui：出题工作台的窗口版入口。
//
// 与命令行的 bin/poly.exe 是同一份代码，只是用 GUI 子系统链接（-mwindows），
// 因此双击运行不会弹出控制台窗口；参数与 poly 完全一致（常用：-w <工作区>）。
#include <string>
#include <vector>

#ifndef _WIN32
#include <cstdio>
int main() {
    std::fprintf(stderr, "poly-gui 只在 Windows 上可用（poly ui --web 可打开网页版工作台）\n");
    return 1;
}
#else

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include "cli.h"
#include "format.h"
#include "log.h"
#include "plat.h"
#include "win32ui.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    poly::init_console();
    poly::log_configure(false, false, false);

    std::vector<std::string> raw = poly::get_args_utf8(0, nullptr);
    // 双击启动时命令行只有程序名，此时保持空参数即可。
    poly::Args args;
    std::string err;
    if (raw.size() > 1 && !poly::parse_args(std::vector<std::string>(raw.begin() + 1, raw.end()), args, err)) {
        MessageBoxW(nullptr, poly::to_wide("参数错误：" + err).c_str(), L"poly-gui", MB_OK | MB_ICONERROR);
        return 2;
    }
    poly::Context ctx;
    poly::resolve_context(args, ctx, err);
    return poly::run_win32_ui(ctx);
}

#endif  // _WIN32
