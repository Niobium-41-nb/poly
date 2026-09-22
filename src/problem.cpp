#include "problem.h"

#include <algorithm>

#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "strutil.h"

namespace poly {

std::string Problem::path(const std::string& rel) const { return fs::join(dir, rel); }
std::string Problem::filesDir() const { return path("files"); }
std::string Problem::solutionsDir() const { return path("solutions"); }
std::string Problem::testsDir() const { return path("tests"); }
std::string Problem::outputDir() const { return path("output"); }
std::string Problem::statementsDir() const { return path("statements"); }
std::string Problem::stressDir() const { return path("stress"); }

int Problem::testDigits(int count) {
    int digits = 2;
    int cap = 100;
    while (count >= cap) {
        digits++;
        cap *= 10;
    }
    return digits;
}

std::string Problem::testName(int index, int digits) {
    return lpad(num(index), static_cast<size_t>(digits));
}

std::string Problem::testPath(int index) const {
    int digits = testDigits(std::max(testCount(), index + 1));
    return fs::join(testsDir(), testName(index, digits));
}

std::string Problem::testFileName(int index) const {
    for (const std::string& f : fs::list_dir(testsDir(), true)) {
        std::string base = fs::basename(f);
        if (is_integer(base) && static_cast<int>(to_int(base)) == index) return base;
    }
    return testName(index, testDigits(index + 1));
}

std::vector<int> Problem::testIndices() const {
    std::vector<int> out;
    for (const std::string& p : fs::list_dir(testsDir(), true)) {
        std::string base = fs::basename(p);
        if (is_integer(base)) out.push_back(static_cast<int>(to_int(base)));
    }
    std::sort(out.begin(), out.end());
    return out;
}

int Problem::testCount() const { return static_cast<int>(testIndices().size()); }

const SolutionInfo* Problem::findSolution(const std::string& name) const {
    for (const SolutionInfo& s : solutions)
        if (s.name == name) return &s;
    return nullptr;
}

Json Problem::to_json() const {
    Json j = Json::object();
    j.set("name", name);
    j.set("timeLimitMs", timeLimitMs);
    j.set("memoryLimitKb", memoryLimitKb);
    j.set("checker", checker);
    j.set("interactive", interactive);
    j.set("multitest", multitest);
    if (!note.empty()) j.set("note", note);

    Json sols = Json::array();
    for (const SolutionInfo& s : solutions) {
        Json sj = Json::object();
        sj.set("name", s.name);
        sj.set("file", s.file);
        sj.set("expected", s.expected);
        sj.set("tag", s.tag);
        if (s.points >= 0) sj.set("points", s.points);
        sols.push_back(sj);
    }
    j.set("solutions", sols);

    Json gens = Json::array();
    for (const std::string& g : generators) gens.push_back(Json(g));
    j.set("generators", gens);

    j.set("validator", validator);
    j.set("checkerFile", checkerFile);
    j.set("interactor", interactor);
    j.set("testScript", testScript);

    Json samples = Json::array();
    for (int s : sampleTests) samples.push_back(Json(s));
    j.set("sampleTests", samples);

    Json tagArray = Json::array();
    for (const std::string& t : tags) tagArray.push_back(Json(t));
    j.set("tags", tagArray);

    if (!extra.is_null()) j.set("extra", extra);
    return j;
}

void Problem::from_json(const Json& j) {
    name = j["name"].as_string(name);
    timeLimitMs = static_cast<int>(j["timeLimitMs"].as_int(timeLimitMs));
    memoryLimitKb = j["memoryLimitKb"].as_int(memoryLimitKb);
    checker = j["checker"].as_string(checker);
    interactive = j["interactive"].as_bool(interactive);
    multitest = j["multitest"].as_bool(multitest);
    note = j["note"].as_string(note);

    if (j.has("solutions") && j["solutions"].is_array()) {
        solutions.clear();
        for (const Json& sj : j["solutions"].items()) {
            SolutionInfo s;
            s.name = sj["name"].as_string();
            s.file = sj["file"].as_string();
            s.expected = to_lower(sj["expected"].as_string("ac"));
            s.tag = sj["tag"].as_string();
            s.points = static_cast<int>(sj["points"].as_int(-1));
            if (s.name.empty() && !s.file.empty()) s.name = fs::stem(s.file);
            if (!s.name.empty()) solutions.push_back(s);
        }
    }

    if (j.has("generators") && j["generators"].is_array()) {
        generators.clear();
        for (const Json& g : j["generators"].items()) generators.push_back(g.as_string());
    }
    validator = j["validator"].as_string(validator);
    checkerFile = j["checkerFile"].as_string(checkerFile);
    interactor = j["interactor"].as_string(interactor);
    testScript = j["testScript"].as_string(testScript);

    sampleTests.clear();
    if (j.has("sampleTests") && j["sampleTests"].is_array()) {
        for (const Json& s : j["sampleTests"].items()) sampleTests.push_back(static_cast<int>(s.as_int()));
    }

    tags.clear();
    if (j.has("tags") && j["tags"].is_array()) {
        for (const Json& t : j["tags"].items()) tags.push_back(t.as_string());
    }

    if (j.has("extra")) extra = j["extra"];
}

bool Problem::save(std::string& err) const {
    if (!fs::is_dir(dir)) {
        err = str("题目目录不存在: {}", dir);
        return false;
    }
    Json j = to_json();
    if (!fs::write_file(path("problem.json"), j.dump(2) + "\n")) {
        err = str("无法写入 {}", path("problem.json"));
        return false;
    }
    return true;
}

bool Problem::load(const std::string& pdir, Problem& out, std::string& err) {
    std::string cfgPath = fs::join(pdir, "problem.json");
    std::string text;
    if (!fs::read_file(cfgPath, text)) {
        err = str("找不到配置文件 {}", cfgPath);
        return false;
    }
    Json j;
    std::string perr;
    if (!Json::parse(text, j, &perr)) {
        err = str("{} 解析失败: {}", cfgPath, perr);
        return false;
    }
    out = Problem();
    out.dir = fs::absolute(pdir);
    out.name = fs::basename(out.dir);
    out.from_json(j);
    if (out.name.empty()) out.name = fs::basename(out.dir);
    if (out.testScript.empty()) out.testScript = "files/testscript.txt";
    sync_solutions(out);
    return true;
}

bool load_problem(const std::string& workspace, const std::string& name, Problem& out, std::string& err) {
    std::string dir = fs::join(workspace, name);
    if (!fs::is_dir(dir)) {
        err = str("题目 {} 不存在（查找路径 {}）", name, dir);
        return false;
    }
    return Problem::load(dir, out, err);
}

std::vector<std::string> list_problems(const std::string& workspace) {
    std::vector<std::string> out;
    if (!fs::is_dir(workspace)) return out;
    for (const std::string& p : fs::list_dir_sorted(workspace, false)) {
        if (!fs::is_dir(p)) continue;
        if (!fs::is_file(fs::join(p, "problem.json"))) continue;
        out.push_back(fs::basename(p));
    }
    return out;
}

void sync_solutions(Problem& p) {
    std::vector<SolutionInfo> updated;
    for (const std::string& f : fs::list_dir_sorted(p.solutionsDir(), true)) {
        if (!has_suffix_ci(f, ".cpp") && !has_suffix_ci(f, ".cc") && !has_suffix_ci(f, ".cxx")) continue;
        std::string base = fs::basename(f);
        std::string sname = fs::stem(base);
        const SolutionInfo* existing = nullptr;
        for (const SolutionInfo& s : p.solutions)
            if (s.name == sname) existing = &s;
        if (existing) {
            SolutionInfo s = *existing;
            s.file = fs::join("solutions", base);
            updated.push_back(s);
        } else {
            SolutionInfo s;
            s.name = sname;
            s.file = fs::join("solutions", base);
            s.expected = "ac";
            s.tag = (sname == "main") ? "main" : "";
            updated.push_back(s);
        }
    }
    p.solutions = updated;
}

}  // namespace poly
