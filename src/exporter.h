#pragma once

#include <string>
#include <vector>

#include "cli.h"
#include "judge.h"
#include "problem.h"

namespace poly {

// 题目包格式：
//   Polygon —— 本工具自己的兼容子集（problem.xml + tests + solutions + ...）
//   QDUOJ   —— QDUOJ 后台「导出题目」产出的压缩包（<n>/problem.json + <n>/testcase/）
//   FPS     —— HUSTOJ 的 freeproblemset XML（单个 .xml 文件）
//   Hydro   —— HydroOJ 通用 zip（<pid>/problem.yaml + problem.md + testdata/）
//   HOJ     —— Hcode Online Judge 原生 zip（problem_<id>.json + problem_<id>/*.in|*.out）
enum class ExportFormat { Polygon, Qduoj, Fps, Hydro, Hoj };

// 接受 polygon/poly、qduoj/qd、fps/freeproblemset、hydro/hydrooj、hoj 及其大小写变体。
bool parse_export_format(const std::string& name, ExportFormat& out);
const char* export_format_name(ExportFormat format);

// 默认输出文件名（不含目录），例如 demo.qduoj.zip。
std::string default_export_name(const Problem& p, ExportFormat format);

// 导出到 args 指定的位置（--output），成功返回 true。
bool export_problem(Context& ctx, Problem& p, JudgeEnv& env, const std::vector<int>& tests,
                    ExportFormat format, const Args& args);

}  // namespace poly
