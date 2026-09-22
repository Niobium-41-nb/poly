#pragma once

#include <string>

#include "cli.h"

namespace poly {

// 启动本地出题工作台（HTTP 服务）并阻塞运行，Ctrl+C 退出。
int run_webui(Context& ctx, const std::string& host, int port, bool openBrowser);

}  // namespace poly
