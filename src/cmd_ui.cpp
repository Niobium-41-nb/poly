// poly ui：打开出题工作台。
//   Windows：默认打开原生 Win32 窗口（bin/poly-gui.exe）；加 --web 改成浏览器版。
//   其它平台：浏览器版（与 poly ui --web 同一套实现）。
#include <string>

#include "commands.h"
#include "compile.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "plat.h"
#include "proc.h"
#include "strutil.h"
#include "webui.h"
#ifdef _WIN32
#include "win32ui.h"
#endif

namespace poly {

namespace {

bool workspace_ready(Context& ctx) {
    if (fs::is_dir(ctx.workspace)) return true;
    log_warn(str("工作区 {} 不存在，将自动创建", ctx.workspace));
    return fs::mkdirs(ctx.workspace);
}

}  // namespace

int cmd_ui(Context& ctx, const Args& args) {
    bool useWeb = args.has_flag("web");
    if (!workspace_ready(ctx)) {
        log_err(str("无法创建工作区 {}", ctx.workspace));
        return 1;
    }

#ifdef _WIN32
    if (!useWeb) {
        // 优先用 GUI 子系统的 poly-gui.exe：从终端启动也不会有多余的控制台窗口
        std::string gui = fs::join(fs::exe_dir(), "poly-gui" + exe_suffix());
        if (fs::is_file(gui)) {
            log_step(str("打开窗口版出题工作台（{}）", gui));
            ProcOptions o;
            o.args = {"-w", ctx.workspace};
            ProcResult r = run_process(gui, o);
            if (!r.started) {
                log_err(r.error.empty() ? std::string("无法启动 poly-gui.exe") : r.error);
                return 1;
            }
            return r.exitCode == 0 ? 0 : 1;
        }
        log_debug("没有找到 poly-gui.exe，改为在当前进程里创建窗口");
        return run_win32_ui(ctx);
    }
#else
    (void)useWeb;
#endif

    int port = static_cast<int>(args.get_int("port", 2333));
    if (port < 0 || port > 65535) {
        log_err(str("端口不合法：{}", port));
        return 1;
    }
    std::string host = args.get("host", "127.0.0.1");
    bool openBrowser = !args.has_flag("no-open") && !args.has_flag("no-open-browser");
    if (env_get("POLY_NO_OPEN") == "1") openBrowser = false;
    return run_webui(ctx, host, port, openBrowser);
}

}  // namespace poly
