#pragma once

#include <string>
#include <vector>

#include "json.h"

namespace poly {

struct SolutionInfo {
    std::string name;      // "main"
    std::string file;      // relative path, e.g. "solutions/main.cpp"
    std::string expected;  // ac | wa | tl | ml | re | pe | any
    std::string tag;       // main | correct | brute | wrong | slow ...
    int points = -1;       // for scoring problems, -1 = single group
};

struct Problem {
    std::string name;
    std::string dir;

    int timeLimitMs = 2000;
    long long memoryLimitKb = 262144;
    std::string checker = "ncmp";  // builtin name, or "custom" when checkerFile is set
    bool interactive = false;
    bool multitest = false;
    std::string note;

    std::vector<SolutionInfo> solutions;
    std::vector<std::string> generators;  // "files/gen.cpp"
    std::string validator;                // "files/validator.cpp" or empty
    std::string checkerFile;              // "files/checker.cpp" or empty
    std::string interactor;               // "files/interactor.cpp" or empty
    std::string testScript;               // "files/testscript.txt"

    std::vector<int> sampleTests;
    std::vector<std::string> tags;

    Json extra;

    std::string path(const std::string& rel) const;
    std::string filesDir() const;
    std::string solutionsDir() const;
    std::string testsDir() const;
    std::string outputDir() const;
    std::string statementsDir() const;
    std::string stressDir() const;

    std::string testPath(int index) const;
    std::string testFileName(int index) const;
    std::vector<int> testIndices() const;
    int testCount() const;

    const SolutionInfo* findSolution(const std::string& name) const;

    Json to_json() const;
    void from_json(const Json& j);

    bool save(std::string& err) const;
    static bool load(const std::string& dir, Problem& out, std::string& err);

    static int testDigits(int count);
    static std::string testName(int index, int digits);
};

// Locates problems/<name> under the workspace and loads it.
bool load_problem(const std::string& workspace, const std::string& name, Problem& out, std::string& err);
std::vector<std::string> list_problems(const std::string& workspace);

// Re-scans solutions/*.cpp, keeping existing per-solution overrides.
void sync_solutions(Problem& p);

}  // namespace poly
