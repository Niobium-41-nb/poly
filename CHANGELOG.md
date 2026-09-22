# 更新日志

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.1.1] — 2026-09-22

### 新增

* **窗口版可以换工作区了**：工具栏新增「切换工作区…」按钮，弹出系统文件夹选择框，
  选中的目录会立即生效（重新扫描题目列表并刷新状态栏），并记住到
  `%APPDATA%\poly\ui.json`，下次启动如果没有 `-w` 就直接用它。

### 修复

* **窗口版的 `--version` / `--help` 被忽略**：`poly-gui --version` 不打印版本、而是直接把界面窗口弹出来
  （脚本里会卡出一个窗口）。现在会像命令行那样输出到调用它的终端（重定向到文件同样有效），
  双击运行（没有控制台）时用消息框提示。
* **窗口版的 `-w` 完全不生效**：`poly-gui.exe -w <目录>` 会被当成「命令名 + 位置参数」解析，
  `-w` 被吞掉，工作区仍走默认值。现在启动时先补一个占位命令再解析参数。
* **窗口版「打开工作区」点了没反应**：从开始菜单/桌面快捷方式启动时，工作区默认落在
  安装目录下的 `problems`，而这个目录并不存在；按钮直接对不存在的路径调 `ShellExecute`，
  于是既不弹资源管理器也不报错。现在打开界面前就会创建工作区，
  创建失败或打开失败都会给出明确提示（并写进日志框）。
* 窗口版未指定 `-w` 时的默认工作区由「当前目录（即安装目录）」改为 **`文档\poly`**：
  否则卸载会把安装目录整个删掉，连带用户造的题目一起丢。
* 窗口版窗口/任务栏图标改用 exe 内嵌图标（之前是系统默认图标）。

### 发布产物

| 文件 | 说明 |
| --- | --- |
| `poly-0.1.1-setup.exe` | Windows 安装包（用户级免管理员，可加入 PATH，卸载还原） |
| `poly-0.1.1-win64.zip` | 便携版（解压即用） |
| `SHA256SUMS.txt` | 以上两者的 SHA-256 |

## [0.1.0] — 2026-09-22

首个发布版本：本地离线出题工作台（Polygon 复刻），命令行 + 原生窗口 + 本地网页三套入口，
支持五种题目包格式的导出与四种格式的导入（含 HOJ 适配）。

### 新增

* 出题流程：`poly init` / `build` / `gen` / `validate` / `test` / `run` / `stress` / `clean` / `doctor`
* 自研判题器：Job Object 资源限制、按 CPU 周期计时、命名管道对接 testlib 交互器
* checker：内置 `ncmp`/`wcmp`/`lcmp`/`fcmp`/`dcmp`/`rcmp4`/`rcmp6`/`rcmp9`/`yesno`，也支持自定义 `files/checker.cpp`
* 题面：Markdown → HTML / PDF（样例自动注入、MathJax 公式）
* 打包导出：`polygon`（默认）/ `qduoj` / `fps` / `hydro` / **`hoj`**
* 题目包导入：`poly import [qduoj|fps|hydro|hoj] <文件>`，格式可省略（按内容自动识别）
* 界面：`poly ui` 打开原生 Win32 窗口工作台，`poly ui --web` 打开本地网页工作台
* **HOJ 原生包**（`--format hoj`）：按后端 `ImportProblemVO` 生成
  `problem_<id>.json` + 同名测试数据目录，可直接在 HOJ 后台「导入题目 → HOJ」上传；
  `spj` / 交互题走 `spjCode`，OI 计分题按 `type=1` 导出
* 内置官方 testlib.h（构建时嵌入二进制，`poly testlib` 可再导出）

### 修复

* `hoj` 曾经被当成 `hydro` 的别名（`poly package --format hoj` 会导出 Hydro 包），现拆分为独立格式
* `problem.json` 带 UTF-8 BOM（记事本 / “UTF-8 with BOM” 保存）时，JSON 解析会直接失败
* 安装包向导中文显示为乱码：`installer/ChineseSimplified.isl` 生成时被 `WebClient.DownloadString`
  按系统 ANSI（GBK）解码，文件里存的就是乱码字符。现改为按**原始字节**下载并补上 UTF-8 BOM
  （Inno Setup 只会把带 BOM 的脚本/语言文件当 UTF-8 读）

### 发布产物

| 文件 | 说明 |
| --- | --- |
| `poly-0.1.0-setup.exe` | Windows 安装包（用户级免管理员，可加入 PATH，卸载还原） |
| `poly-0.1.0-win64.zip` | 便携版（解压即用） |
| `SHA256SUMS.txt` | 以上两者的 SHA-256 |

安装包内的 `testlib.h` 版权归 Mike Mirzayanov 所有（MIT 许可，文件头保留原始声明）。
