# poly —— 本地离线的 Codeforces Polygon 复刻（C++17 命令行版）

`poly` 是一个用 C++17 从零实现的**本地离线出题系统**，把 Polygon 的核心工作流
（写题面 → 写生成器/校验器/checker → 造数据 → 用各档解法验证 → 对拍 → 打包导出）
压缩成一个单文件可执行程序，全部在本地完成，不依赖网络与服务器。

```
poly init demo      # 建题目工作区
poly build demo     # 编译生成器/校验器/checker/所有解法
poly gen demo       # 按脚本生成测试点并校验
poly test demo      # 所有解法跑全部测试点，输出判定表
poly stress demo -s main -s brute -n 500   # 对拍
poly statement demo --pdf                  # 渲染题面（HTML/PDF）
poly package demo                          # 导出 zip 包（Polygon 兼容）
poly package demo --format qduoj           # 导出 QDUOJ 压缩包
poly package demo --format fps             # 导出 FPS（freeproblemset XML）
poly package demo --format hydro           # 导出 Hydro 通用 zip
poly package demo --format hoj             # 导出 HOJ 原生 zip（可直接传到 HOJ 后台）
poly import hydro lesson.zip               # 导入题目包（格式可省略，自动识别）
poly ui                                    # 启动本地出题工作台（浏览器操作）
```

---

## 0. 安装

**Windows 安装包**（推荐）：在 [Releases](https://github.com/Niobium-41-nb/poly/releases) 里下载
`poly-<版本>-setup.exe`，双击安装。安装是**用户级、免管理员**的：

| 项目 | 说明 |
| --- | --- |
| 安装目录 | `%LOCALAPPDATA%\Programs\poly`（卸载时整个删掉） |
| 组件 | `poly.exe`（命令行）、`poly-gui.exe`（窗口界面）、`testlib\testlib.h`、`README.md` |
| 可选任务 | 把安装目录加入用户 `PATH`（默认勾选；卸载时会把 `PATH` 精确还原）与创建桌面快捷方式（默认不勾） |
| 开始菜单 | 「poly 出题工作台」「使用说明（README）」「卸载」 |

静默安装（脚本化部署 / CI）：

```powershell
poly-0.1.1-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART `
    /DIR="C:\tools\poly" /MERGETASKS=addtopath
# 卸载： "<安装目录>\unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
```

**便携版**：下载 `poly-<版本>-win64.zip`，解压到任意目录直接运行，不需要安装器与注册表。

**从源码构建**：见下一节。

> `poly` 的每个功能都不依赖安装位置：只要 `poly.exe` 在 `PATH` 里（或直接用绝对路径调用）即可。
> 窗口界面（`poly-gui.exe`）与命令行完全等价，两者调同一套命令实现。
>
> **工作区位置**：命令行默认用当前目录下的 `problems/`；从开始菜单/桌面双击窗口版时
> 没有当前目录可言，默认用 **`文档\poly`**（可用 `poly ui -w <目录>` 或 `poly-gui -w <目录>` 指定）。
> 窗口版工具栏里还有一个 **「切换工作区…」** 按钮，选完立即生效并记住
> （存在 `%APPDATA%\poly\ui.json`，下次启动没有 `-w` 时就用它）。

---

## 1. 环境与构建

| 项目 | 要求 |
| --- | --- |
| 编译器 | 支持 C++17 的 g++ / clang++（本机为 MSYS2，`g++ 16.1.0` 已验证） |
| 构建 | GNU Make（无需 cmake） |
| 运行平台 | Windows（完整支持：Job Object 限制、命名管道交互）与 Linux/POSIX（`setrlimit` + `fifo`，按同一套接口实现） |
| 可选 | Python 3（仅用于构建期把 testlib.h 嵌入二进制；缺失时退化为从磁盘读取） |
| 可选 | xelatex / pdflatex（`poly statement --pdf` 才需要） |

```bash
make            # 生成 bin/poly.exe（命令行）与 bin/poly-gui.exe（窗口版，仅 Windows）
make env        # 打印构建环境探测结果
make clean      # 删 build/，make distclean 连 bin/ 一起删
```

> **MSYS2 用户请注意**：本机 PATH 中同时存在 `ucrt64` 与 `mingw64` 两套工具链时，
> 混用会导致编译器进程静默失败；另外 GNU Make 在 Windows 上会清掉 `TMP/TEMP`，
> 使 binutils 回退到不可写的 `C:\WINDOWS`。`Makefile` 已自动处理这两件事
> （`tools/pick-toolchain.sh` 会挑一个真正能编译的编译器，并把 `TMP/TEMP` 设为真实 Windows 路径）。
> `poly` 自身同样会探测可用的编译器，因此命令行中直接 `poly build` 也能正常工作。
>
> **图标与版本信息**：`assets/*.rc` 由 `windres` 编译后链进两个 exe（中文串必须带
> `--codepage=65001`）；`windres` 不在时 Makefile 会自动跳过，不影响构建。
> `assets/icon.ico` 由 `tools/make-icon.ps1` 生成（256 用 PNG 帧、小尺寸用 DIB 帧，
> 保证旧 API 也能读）。

---

## 2. 工作区结构

```
problems/                      # 工作区（可用 poly.json 配置全局项）
  poly.json                    # {"compiler":"g++","std":"-std=c++17","cxxflags":"","jobs":0}
  demo/                        # 一道题
    problem.json               # 题目配置：时限、内存、checker、解法、生成器……
    files/                     # 造数据的程序
      gen.cpp                  #   生成器（可以有多个 gen1.cpp / gen2.cpp）
      validator.cpp            #   校验器（可省略）
      checker.cpp              #   自定义 checker（设置 checkerFile 后启用）
      interactor.cpp           #   交互器（--interactive 题目）
      testscript.txt           #   测试点生成脚本
      testlib.h                #   随题目复制的 testlib 头文件
    solutions/                 # 各种解法，main.cpp 为标准程序（自动扫描）
    tests/                     # 生成的测试点（01, 02, ...）
    statements/                # statement.md / tutorial.md 及其渲染产物
    output/                    # 编译产物（output/bin）、运行日志、标准答案
    stress/                    # 对拍失败用例
    README.md
```

---

## 3. 命令一览

| 命令 | 说明 |
| --- | --- |
| `poly init <名字>` | 创建题目（`--checker custom` / `--interactive` / `--time-limit` / `--memory-limit`） |
| `poly list` | 列出工作区所有题目 |
| `poly info <题目>` | 题目详情与解法表 |
| `poly config <题目> <键> [值]` | 读写配置（`--list` 查看全部键） |
| `poly build <题目> [all\|gen\|validator\|checker\|interactor\|solutions]` | 编译（增量：只编译比源文件旧的） |
| `poly gen <题目> [--test 1,3-5] [--no-validate]` | 生成测试点并校验 |
| `poly validate <题目>` | 用校验器检查测试点 |
| `poly run <题目> -s <解法> [--test N] [--no-check]` | 单解法判题 |
| `poly test <题目>` | 所有解法跑全部测试点，输出判定表（按 `expected` 判断是否符合预期） |
| `poly stress <题目> -s 正解 -s 暴力 [-n 轮数] [--gen 生成器] [--until-tl] [--keep-going]` | 对拍 |
| `poly statement <题目> [--html] [--pdf] [--open]` | 渲染题面（默认 HTML，样例自动注入） |
| `poly package <题目> [--format 格式] [-o 输出]` | 打包导出：`polygon`（默认）/ `qduoj` / `fps` / `hydro` / `hoj` |
| `poly import [qduoj\|fps\|hydro\|hoj] <文件>` | 导入题目包（省略格式时按内容自动识别） |
| `poly ui [--port N] [--host H] [--no-open]` | 启动本地出题工作台（默认 127.0.0.1:2333） |
| `poly clean <题目> [--tests\|--stress\|--all]` | 清理中间产物 |
| `poly doctor` | 环境自检（编译器、testlib、工作区） |
| `poly testlib <路径>` | 导出内置 testlib.h（`--print` 直接打印） |

全局选项：`-p/--problem`、`-w/--workspace`、`-j/--jobs`、`-v/--verbose`、`-q/--quiet`、`--no-color`、`--compiler <路径>`。

常用配置键：

```bash
poly config demo timeLimit 2000          # ms
poly config demo memoryLimit 256         # MB（支持 512m / 262144k）
poly config demo checker ncmp            # 内置 checker
poly config demo checkerFile files/checker.cpp
poly config demo validator files/validator.cpp
poly config demo solution.wa.expected wa # 期望这道错误解只能拿 WA
```

---

## 4. 测试点生成脚本

`files/testscript.txt`，每行一条命令：

```
<生成器> [参数...] [ | <生成器> [参数...] ]...   >   <编号 | 起-止>
```

```
# 单点
gen 1 1 > 1
# 区间（{i} 逐个递增）
gen 10 1000 --seed={seed} > 3-5
# 需要两步加工时用管道（前一个生成器的输出作为后一个的 stdin）
gen 100000 1000000000 --seed={seed} | shuffle > 6
```

* 以 `#` 或 `//` 开头的是注释；不写 `> 编号` 时按出现顺序自动编号。
* 参数中的 `{i}` 替换为当前测试点编号，`{seed}` 替换为按编号派生的确定性随机种子。
* `poly gen` 生成后会立刻用 validator 校验（`--no-validate` 可跳过）。

---

## 5. 判定与期望

| 判定 | 含义 |
| --- | --- |
| `OK` | 通过（checker 返回 `_ok`） |
| `WA` / `PE` | 答案错误 / 格式错误 |
| `TL` / `ML` | 超时 / 超内存 |
| `RE` | 运行时错误（非零退出码） |
| `CE` | 编译错误 |
| `FAIL` | checker/交互器自身报错，或运行器级失败 |

`problem.json` 里每个解法带一个 `expected`（`ac` / `wa` / `tl` / `ml` / `re` / `pe` / `any`），
`poly test` 会逐条比对并给出“符合预期 / 不符合预期”，用于验证数据强度。

内置 checker：`ncmp`（默认，token 比较、数字按数值比较）、`wcmp`（token 严格比较）、
`lcmp`（逐行忽略行尾空白）、`fcmp`/`dcmp`/`rcmp4`/`rcmp6`/`rcmp9`（浮点容差）、`yesno`。
自定义 checker 用 `files/checker.cpp` 编译，调用约定与 Polygon 一致：
`checker <input> <output> <answer> [test]`。

---

## 6. testlib 兼容性

`testlib/` 目录下是**官方 testlib.h**（Mike Mirzayanov，`Copyright (c) 2005-2023`），
构建时被嵌入 `poly` 可执行文件；`poly init` 会把它复制到题目的 `files/testlib.h`，
`poly testlib <路径>` 可以再导出一次。因此**已有的 Polygon 题目代码（生成器、校验器、
checker、交互器）可以直接编译运行**，判定值也与 testlib 完全一致：

```cpp
enum TResult { _ok = 0, _wa = 1, _pe = 2, _fail = 3, _dirt = 4, _points = 5, _unexpected_eof = 8 };
```

四类程序的入口约定（与 testlib 相同）：

| 程序 | 入口 | 参数 |
| --- | --- | --- |
| 生成器 | `registerGen(argc, argv, 1)` | 脚本给出的参数直接作为 `argv[1..]` |
| 校验器 | `registerValidation(argc, argv)` | 测试点从 **stdin** 读入 |
| checker | `registerTestlibCmd(argc, argv)` | `<input> <output> <answer> [test]` |
| 交互器 | `registerInteraction(argc, argv)` | `<input> <output> <answer>` |

**交互题通路**：testlib 的交互器把消息写进 `argv[2]` 指定的文件、从 **stdin** 读选手输出，
所以 `poly` 用两条**命名管道**把这四个方向对接起来：

```
交互器 --tout--> argv[2] 命名管道 --> 选手 stdin
选手 stdout --命名管道--> 交互器 stdin（ouf）
```

---

## 7. 导出格式

`poly package <题目>` 把题目导出成题目包。用 `--format` 选择格式
（`--qduoj` / `--fps` / `--hydro` / `--hoj` 是等价简写），`-o` 可指定输出文件或目录：

| 格式 | 产物 | 结构 | 可直接导入 |
| --- | --- | --- | --- |
| `polygon`（默认） | `demo.zip` | `problem.xml` + `tests/` + `solutions/` + … | Polygon 兼容工具 |
| `qduoj` | `demo.qduoj.zip` | `1/problem.json` + `1/testcase/*.in\|*.out` | QDUOJ（后台导入）、Hydro、HUSTOJ |
| `fps` | `demo.fps.xml` | freeproblemset XML，测试数据内嵌在 CDATA 里 | HUSTOJ、QDUOJ、Hydro |
| `hydro` | `demo.hydro.zip` | `demo/problem.yaml` + `problem.md` + `testdata/` | HydroOJ、HUSTOJ |
| `hoj` | `demo.hoj.zip` | `problem_demo.json` + `problem_demo/*.in\|*.out` | **HOJ（后台导入）** |

四种外部格式的字段都按**对方实现**来生成，而不是「看起来差不多」，主要约定：

* **FPS**：版本号写 `1.2`（QDUOJ 的解析器只接受 1.1/1.2，HUSTOJ 与 Hydro 不校验）；
  `<time_limit>` 能整除 1 秒时用 `unit="s"`，否则用 `unit="ms"`；内存用 `unit="mb"`；
  `<test_input name="N">` 与 `<test_output name="N">` 成对出现——**即使没有答案文件也会写空的
  `<test_output>`**，因为 QDUOJ 解析器遇到连续两个 `test_input` 会直接报错；
  自定义 checker 写进 `<spj>`，交互题写 `<spj>`（交互器源码）+ `<interactor>`。
* **QDUOJ**：`time_limit` 用毫秒、`memory_limit` 用 MB，文本字段一律
  `{"format":"html","value":"..."}`；`test_case_score` 按 QDUOJ 的 ACM 习惯每条 100 分；
  没有答案文件（spj/交互题）时只打包输入，并把 `output_name` 记为 `-`。
* **Hydro**：`testdata/config.yaml` 必须自带（Hydro 的导入不会替你生成），
  时限/内存写成 `2000ms` / `256m`，每个测试点一个 `subtasks` 条目且**总分合计 100**；
  自定义 checker 用 `checker` + `checker_type: testlib`，交互题用 `type: interactive` + `interactor`
  （无答案时 `output` 记为 `/dev/null`，与 Hydro 自己的导入器一致）；
  题面放在 `problem.md`，样例展开成 ```` ```input1 ```` / ```` ```output1 ```` 代码块
  （Hydro 会把它们渲染成样例框）。
* **HOJ**（Hcode Online Judge 后台「导入题目 → HOJ」）：压缩包根目录下必须是
  `problem_<id>.json` + 同名目录 `problem_<id>/`（HOJ 靠文件名与目录名一一对应来配对，
  且根目录**除了 json 不能再放别的文件**）；json 就是后端的 `ImportProblemVO`：
  `problem`（字段名与 `problem` 表的属性一致：`problemId`/`title`/`description`/`input`/
  `output`/`hint`/`examples`/`timeLimit`(ms)/`memoryLimit`(**kb**)/`judgeMode`…）、
  `samples`（**只是文件名清单**，数据在目录里）、`languages`、`tags`、`codeTemplates`。
  两个坑：`languages`/`samples`/`tags`/`codeTemplates` **即使为空也必须写出**（后端直接
  遍历它们，缺字段会 NPE）；`author` **故意不写**（不为空时会当作外键写库，名字不存在就失败，
  留空 HOJ 会自动填成导入者）。样例展示字段 `examples` 是拼起来的
  `<input>…</input><output>…</output>`——两段之间不能有换行，否则前端解析不出来。
  题目的 `checker` 会写成 `judgeMode: spj` + `spjCode`，交互器写 `judgeMode: interactive`
  + `spjCode`（HOJ 把两者都存在同一对字段里）；`points` 非负的题目按 OI 题型导出（考点分数合计 100）。

四种包都能反向导入（`poly import` 会自动识别；HOJ 包的特征是根目录就有 json）：
上面的 json 结构会拆回题面 markdown、`tests/`、`files/checker.cpp`、`files/interactor.cpp`，
包里的 `examples` 在无测试数据时充当样例。

题面按 `## 输入格式` / `## 输出格式` / `## 数据范围` 这类小标题拆成
描述 / 输入 / 输出 / 提示四段（`qduoj`、`fps` 用得上）；认不出来的小标题会连标题一起塞进
题目描述，不会丢内容。缺小标题时会用占位文本并给出告警。因为样例来自 `tests/` 而不是题面，
`{{samples}}` 占位符在 `hydro` 的 `problem.md` 里会被展开成真正的样例块。

> 提示：`qduoj` / `hydro` 里的题目 ID 由题目名派生（只保留字母数字，必要时补 `P` 前缀），
> 以满足 Hydro 的 `pid` 规则；`hoj` 的 `problemId` 同样来自题目名（HOJ 会自行转大写，
> 但要求全站唯一，重复时导入会报「重复失败的题目ID」）；导入后都可以直接改。

---

## 8. 运行器实现细节

* **进程**：`CreateProcessW` + `CREATE_SUSPENDED`，先加入 **Job Object** 再恢复执行，
  确保内存上限（`JOB_OBJECT_LIMIT_PROCESS_MEMORY` + 1 MB 余量）在第一条指令前生效；
  `KILL_ON_JOB_CLOSE` 保证即使 `poly` 意外退出也不会留下野进程。
* **时间**：CPU 时间优先用 `QueryProcessCycleTime` 除以开机时标定出的周期频率，
  避免 `GetProcessTimes` 受 15.6 ms 系统时钟节拍量化（否则 1 ms 级时限完全不可用），
  失败时回退到 `GetProcessTimes`。
* **内存**：取峰值提交量与峰值工作集，超过题面限制即判 `ML`。
* **路径**：全流程用 UTF-8 字符串，Windows 侧统一转宽字符调用系统 API；
  交给编译器的路径一律是**相对 ASCII 路径**（binutils 使用 ANSI API，
  但相对路径由内核按 Unicode 工作目录解析），因此工作区放在
  `D:\算法竞赛\出题软件` 这类中文路径下也能正常编译。
* **磁盘**：评测中间文件放在 `output/runs`，标准答案缓存在 `output/answers`
  （源文件更新后自动重建）。

---

## 9. 已知限制

* 不提供 Web UI（Polygon 的网页部分是服务器形态；本复刻是纯命令行）。
* 不提供沙箱级安全隔离（只做资源限制，不做系统调用过滤）。
* 打包格式是 Polygon 包的**兼容子集**（`problem.xml` 含题目/测试/解法/checker 等主要字段），
  未实现 `assets`、多 testset、hack 支持等高级特性。
* 导出的 QDUOJ / FPS / Hydro / HOJ 包只包含**传统题型**能表达的信息：
  文件 IO（`filename`）、多测试点分组计分、函数式交互、答案提交题等没有对应字段；
  FPS 会把全部测试数据内嵌进同一个 XML，题包很大时（数十 MB 以上）导入方可能很慢。
  HOJ 侧的 `codeTemplates`（代码模板）与 `userExtraFile`/`judgeExtraFile`（附加文件）
  没有对应字段，导出时不写、导入时忽略。
* zip 使用 stored（不压缩）方式写出，压缩率不是目标；标准工具均可正常解压。
* 交互题暂不支持对拍；`poly stress` 需要非交互题。
* 题面渲染支持 Markdown 子集（标题/列表/表格/代码/强调/链接/行内公式），
  `--mathjax-url` 可挂载 MathJax 渲染公式。

---

## 10. 故障排查

| 现象 | 原因与处理 |
| --- | --- |
| `编译器无法启动（没有任何输出）` | PATH 中混用了两套 MSYS2 工具链。用 `poly doctor` 查看，或 `poly build --compiler <可用 g++ 路径>`；也可设 `POLY_CXX`。 |
| 生成/编译报 `Cannot create temporary file in C:\WINDOWS` | `TMP/TEMP` 指向了不存在的目录，把 `TEMP` 设为真实 Windows 目录（`poly` 自身会探测，Makefile 也已处理）。 |
| `poly test` 报“不符合预期” | 用 `poly config <题目> solution.<名字>.expected <判定>` 修正期望值。 |
| 交互题一直 `TL` | 交互器必须使用 `tout` 输出消息（对应 `argv[2]`），不能直接写 stdout。 |
| 中文路径下的编译 | 已支持；若遇到问题请先用 `poly doctor` 自检。 |

---

## 11. 内置示例题目

工作区里已经有两道可直接跑通的示例题，可作为写新题时的模板：

| 题目 | 类型 | 内容 |
| --- | --- | --- |
| `problems/demo` | 普通题 | 求和；含生成器、校验器、标准程序、错误解法、10 个测试点、题面/题解 |
| `problems/guess` | **交互题** | 猜数；含交互器（`tout`/`ouf`）、校验器、二分标准程序、错误的线性解法 |

> 示例题属于本地工作区数据，`.gitignore` 默认忽略整个 `problems/`，仓库里不含它们；
> 想在新机器上拿到同样的两题，用 `poly init` / `poly project` 重新生成即可。

```bash
poly test demo      # 期望全部符合预期
poly test guess     # 交互题：main 全 OK，wa 全 WA
poly package demo -o /tmp/demo.zip
poly package demo --hydro            # 换成 Hydro 题库包的格式
poly package demo --hoj              # 导出 HOJ 后台可直接导入的原生 zip
poly package guess --qduoj           # 交互题自动带 spj（交互器源码）
```

`output/` 下保留每次评测的中间产物，便于排查：

```
output/bin/            编译产物（生成器、校验器、checker、交互器、各解法）
output/answers/        标准答案缓存（测试点或源文件更新后自动重建）
output/runs/           每个测试点的选手输出 / 标准错误 / 交互器日志（*.err.out）
output/gen_tmp/        生成器管道的中间结果
```
