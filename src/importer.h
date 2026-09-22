#pragma once

#include <string>
#include <utility>
#include <vector>

#include "cli.h"

namespace poly {

// 能导入的题目包格式（与 poly package --format 的各格式可互换）。
enum class ImportFormat { Qduoj, Fps, Hydro, Hoj };

bool parse_import_format(const std::string& name, ImportFormat& out);
const char* import_format_name(ImportFormat format);

// 解析出来的单道题目（还没落盘）。
struct ImportedProblem {
    std::string title;
    std::string pid;              // 包里的原始编号，用于派生目录名
    long long timeLimitMs = 1000;
    long long memoryLimitKb = 262144;

    // 题面：要么是分好的四段，要么是一整篇现成的 markdown（Hydro）
    std::string description;
    std::string input;
    std::string output;
    std::string hint;
    std::string fullStatement;
    std::string tutorial;

    std::vector<std::string> tags;
    std::vector<std::string> notes;  // 解析过程中的提示

    struct Sample {
        std::string input;
        std::string output;
    };
    std::vector<Sample> samples;

    struct Case {
        std::string name;  // 包内的输入文件名（仅用于提示）
        std::string input;
        std::string output;
        bool hasOutput = false;
    };
    std::vector<Case> tests;

    bool interactive = false;
    std::string checkerLanguage;
    std::string checkerSource;  // spj / checker 源码
    std::string interactorSource;
    std::string mainSolution;   // 可选：包里的 C++ 标程
    std::string mainSolutionLanguage;
};

// 解析题目包（不落盘）。一个 FPS 文件里可能有多个 <item>，因此返回的是列表。
bool parse_problem_package(ImportFormat format, const std::string& filename, const std::string& data,
                           std::vector<ImportedProblem>& out, std::string& err);

struct ImportOutcome {
    std::string name;   // 落盘后的题目名（可直接用于 poly 的其他命令）
    std::string dir;
    int tests = 0;
    bool interactive = false;
    bool hasChecker = false;
    std::vector<std::string> notes;
};

// 解析并落盘成 poly 题目工作区；只要有至少一道题导入成功就返回 true。
bool import_into_workspace(const Context& ctx, ImportFormat format, const std::string& filename,
                           const std::string& data, std::vector<ImportOutcome>& out, std::string& err);

}  // namespace poly
