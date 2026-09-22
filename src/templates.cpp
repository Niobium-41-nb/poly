#include "templates.h"

namespace poly {

const char* tpl_generator() {
    return R"GEN(// 生成器：按参数随机生成一组测试数据
// 约定：argv[1..] 为脚本中传入的参数；--seed=N 可显式指定随机种子。
#include "testlib.h"

int main(int argc, char* argv[]) {
    registerGen(argc, argv, 1);

    int n = opt<int>(1, 10);        // 第一个参数：元素个数
    int vmax = opt<int>(2, 1000);   // 第二个参数：元素的绝对值上限

    printf("%d\n", n);
    for (int i = 0; i < n; i++) {
        if (i) printf(" ");
        printf("%d", rnd.next(-vmax, vmax));
    }
    printf("\n");
    return 0;
}
)GEN";
}

const char* tpl_validator() {
    return R"VAL(// 校验器：检查测试数据是否满足题面约束
#include "testlib.h"

int main(int argc, char* argv[]) {
    registerValidation(argc, argv);

    int n = inf.readInt(1, 100000, "n");
    inf.readEoln();
    std::vector<int> a = inf.readInts(n, -1000000000, 1000000000, "a");
    inf.readEoln();
    inf.readEof();
    return 0;
}
)VAL";
}

const char* tpl_checker() {
    return R"CHK(// 自定义 checker：与标准答案比较
// 调用方式：checker <input> <output> <answer>
#include "testlib.h"

int main(int argc, char* argv[]) {
    registerTestlibCmd(argc, argv);

    long long expected = ans.readLong();
    long long found = ouf.readLong();
    if (expected != found)
        quitf(_wa, "答案错误：期望 %lld，实际 %lld", expected, found);

    if (!ouf.seekEof()) quitf(_wa, "输出有多余内容");
    if (!ans.seekEof()) quitf(_fail, "标准答案有多余内容");
    quitf(_ok, "答案正确：%lld", found);
}
)CHK";
}

const char* tpl_interactor() {
    return R"INT(// 交互器：与选手程序通过标准输入输出双向交互
// 调用方式：interactor <input> <output> <answer>
#include "testlib.h"

int main(int argc, char* argv[]) {
    registerInteraction(argc, argv);

    int n = inf.readInt();
    int secret = inf.readInt();

    int lo = 1, hi = n, queries = 0;
    while (true) {
        if (queries++ > 100)
            quitf(_wa, "查询次数超过限制");
        int guess = ouf.readInt(lo, hi, "guess");
        if (guess == secret) {
            tout << "correct" << std::endl;
            break;
        }
        if (guess < secret) {
            lo = guess + 1;
            tout << "bigger" << std::endl;
        } else {
            hi = guess - 1;
            tout << "smaller" << std::endl;
        }
    }
    quitf(_ok, "猜对了，共 %d 次询问", queries);
}
)INT";
}

const char* tpl_testscript() {
    return R"SCR(# 测试点生成脚本
#
# 语法：
#   <命令> [参数...] [ | <命令> [参数...] ]...  >  <编号 | 起-止>
#   以 # 或 // 开头的行为注释；命令名取自 files/ 下编译出的可执行文件名（去掉 .cpp）。
#   不带 "> 编号" 时按出现顺序自动编号。
#
# 可用变量（在参数中做文本替换）：
#   {i}     当前测试点编号（区间生成时逐个递增）
#   {seed}  由测试点编号派生的确定性随机种子
#
# 生成数据（10 个测试点）
gen 1 1 > 1
gen 2 10 > 2
gen 10 1000 --seed={seed} > 3-5
gen 1000 1000000000 --seed={seed} > 6-10
)SCR";
}

const char* tpl_statement() {
    return R"STMT(# 题目名称

## 题目描述

给定 $n$ 个整数，请求出它们的和。

## 输入格式

第一行一个整数 $n$（$1 \le n \le 10^5$）。

第二行 $n$ 个整数 $a_1, a_2, \dots, a_n$（$|a_i| \le 10^9$），相邻整数之间用一个空格分隔。

## 输出格式

输出一行一个整数，表示总和。

## 样例

{{samples}}

## 数据范围

- 对于 30% 的数据，$n \le 10$，$|a_i| \le 1000$。
- 对于 100% 的数据，$n \le 10^5$，$|a_i| \le 10^9$。

**样例说明**：第一组样例中 $1 + 2 + 3 = 6$。
)STMT";
}

const char* tpl_tutorial() {
    return R"TUT(# 题解

设总和为 $S$，初值 $S = 0$，依次读入每个 $a_i$ 并累加，最终输出 $S$。

注意答案可能超过 32 位整数范围，需要使用 64 位整数（C++ 中的 `long long`）。

时间复杂度 $O(n)$，空间复杂度 $O(1)$。
)TUT";
}

const char* tpl_solution_main() {
    return R"SOL(// 标准程序（主正确解）
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    if (scanf("%d", &n) != 1) return 0;
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        long long x;
        if (scanf("%lld", &x) != 1) return 0;
        sum += x;
    }
    printf("%lld\n", sum);
    return 0;
}
)SOL";
}

const char* tpl_readme() {
    return R"RM(# 题目工作区

```
<problem>/
  problem.json       题目配置（时限、内存、checker、解法、生成器等）
  files/             数据制作脚本
    gen.cpp          生成器（可多个）
    validator.cpp    校验器（可选）
    checker.cpp      自定义 checker（可选，设置 checkerFile 后启用）
    interactor.cpp   交互器（可选，交互题）
    testscript.txt   测试点生成脚本
    testlib.h        本目录使用的 testlib 头文件
  solutions/         各种解法（main 为标准程序）
  tests/             生成的测试点（01, 02, ...）
  statements/        statement.md 题面 / tutorial.md 题解 / 渲染产物
  output/            编译产物与评测中间文件
  stress/            对拍失败用例
```

常用命令：

```bash
poly build   <problem>          # 编译生成器/校验器/checker/所有解法
poly gen     <problem>          # 按脚本生成测试点
poly validate <problem>         # 校验所有测试点
poly test    <problem>          # 用所有解法跑全部测试点，输出判定表
poly run     <problem> -s main  # 单独跑一个解法
poly stress  <problem> -s main -s brute --gen gen -n 1000
poly statement <problem> --html # 渲染题面
poly package <problem> -o out.zip
```
)RM";
}

const char* tpl_workspace_config() {
    return R"CFG({
  "compiler": "g++",
  "std": "-std=c++17",
  "cxxflags": "",
  "jobs": 0
}
)CFG";
}

}  // namespace poly
