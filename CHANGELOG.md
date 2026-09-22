# 更新日志

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

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

### 发布产物

| 文件 | 说明 |
| --- | --- |
| `poly-0.1.0-setup.exe` | Windows 安装包（用户级免管理员，可加入 PATH，卸载还原） |
| `poly-0.1.0-win64.zip` | 便携版（解压即用） |
| `SHA256SUMS.txt` | 以上两者的 SHA-256 |

安装包内的 `testlib.h` 版权归 Mike Mirzayanov 所有（MIT 许可，文件头保留原始声明）。
