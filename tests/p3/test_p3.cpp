// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests

TEST(reg_rejects_null) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}

TEST(reg_find_missing_returns_null) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("grep")));
    CHECK(r.find("grep") != nullptr);
    CHECK(r.find("nonexistent") == nullptr);
    CHECK(r.find("") == nullptr);
}

TEST(reg_empty_table_invariants) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.find("anything") == nullptr);
}

TEST(ws_normalization_collapses_dots_and_slashes) {
    TempDir td;
    Workspace ws(td.path());
    fs::path out;
    std::string reason;

    // Redundant slashes and single dot components
    REQUIRE(ws.resolve("foo//bar/./baz", &out, &reason));
    CHECK(out == ws.root() / "foo/bar/baz");

    // Parent directory traversal that remains strictly inside root
    REQUIRE(ws.resolve("dir1/dir2/../dir3/file.txt", &out, &reason));
    CHECK(out == ws.root() / "dir1/dir3/file.txt");
}

TEST(calc_nested_parentheses_stress) {
    double val = 0.0;
    std::string err;
    // Test many parentheses and precedence interactions
    REQUIRE(eval("((((1 + 2) * 3) + 4) * 2) - 1", &val, &err));
    // (((3 * 3) + 4) * 2) - 1 = ((9 + 4) * 2) - 1 = (13 * 2) - 1 = 25
    CHECK(val == 25.0);
}


TEST(reg_for_each_visits_all) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("echo")));
    REQUIRE(r.add(std::make_unique<NamedTool>("grep")));
    REQUIRE(r.add(std::make_unique<NamedTool>("sed")));
    REQUIRE(r.add(std::make_unique<NamedTool>("awk")));

    std::vector<std::string> seenNames;
    r.for_each([&](const Tool& tool) {
        seenNames.push_back(tool.name());
    });

    CHECK(seenNames.size() == 4);
    std::sort(seenNames.begin(), seenNames.end());
    CHECK(seenNames[0] == "awk");
    CHECK(seenNames[1] == "echo");
    CHECK(seenNames[2] == "grep");
    CHECK(seenNames[3] == "sed");
}

TEST(reg_scale_10000_tools) {
    ToolRegistry r;
    Tool* firstToolPtr = nullptr;

    // Insert 10,000 tools: tool_0 to tool_9999
    for (int i = 0; i < 10000; i++) {
        std::string toolName = "tool_" + std::to_string(i);
        auto toolObj = std::make_unique<NamedTool>(toolName);
        if (i == 0) {
            firstToolPtr = toolObj.get();
        }
        REQUIRE(r.add(std::move(toolObj)));
        // Spec required: load factor must never exceed 1.0 after any add()
        CHECK(r.load_factor() <= 1.0);
    }

    CHECK(r.size() == 10000);

    // Pointer stability check: pointer to first tool must be unchanged across all resizes
    CHECK(r.find("tool_0") == firstToolPtr);

    // Check a sample of tools throughout the range
    CHECK(r.find("tool_500") != nullptr);
    CHECK(r.find("tool_4999") != nullptr);
    CHECK(r.find("tool_9999") != nullptr);
    CHECK(r.find("non_existent_tool") == nullptr);

    // Required probe bound check: max chain length must be <= 64
    std::size_t maxProbe = r.max_probe_length();
    CHECK(maxProbe <= 64);

    // Print values as mandated by spec §6 item 3
    std::printf("\n[Scale Test 10,000 tools]\n");
    std::printf("  bucket_count     = %zu\n", r.bucket_count());
    std::printf("  load_factor      = %.4f\n", r.load_factor());
    std::printf("  max_probe_length = %zu\n", maxProbe);
}

TEST(reg_alternative_weak_hash) {
    // Spec §6 item 4: compute longest chain that weak hash s[0] + s.size() would produce
    // using the final bucket count from the 10,000 tool scale test (16384).
    std::size_t tableBuckets = 16384;
    std::vector<int> weakCounts(tableBuckets, 0);

    for (int i = 0; i < 10000; i++) {
        std::string toolName = "tool_" + std::to_string(i);
        // Weak hash from spec §4.3: s[0] + s.size()
        std::size_t weakHashVal = static_cast<unsigned char>(toolName[0]) + toolName.size();
        std::size_t targetBucket = weakHashVal % tableBuckets;
        weakCounts[targetBucket]++;
    }

    int longestWeakChain = 0;
    for (int count : weakCounts) {
        if (count > longestWeakChain) {
            longestWeakChain = count;
        }
    }

    std::printf("\n[Alternative Weak Hash: s[0] + s.size()]\n");
    std::printf("  longest weak chain = %d (vs FNV-1a max_probe_length <= 64)\n\n", longestWeakChain);
    CHECK(longestWeakChain > 1000); // Confirms severe clustering of the bad hash
}

TEST(ws_spec_table_all_cases) {
    TempDir td;
    Workspace ws(td.path());
    fs::path out;
    std::string reason;

    // Spec §4.4 table entries:
    // 1. notes/todo.txt -> accept -> root/notes/todo.txt
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");

    // 2. ./notes//todo.txt -> accept -> root/notes/todo.txt
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");

    // 3. . -> accept -> root
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());

    // 4. a/../b -> accept -> root/b
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");

    // 5. a/.. -> accept -> root
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());

    // 6. new/dir/file.txt (doesn't exist) -> accept -> root/new/dir/file.txt
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new/dir/file.txt");

    // 7. my..notes.txt -> accept -> root/my..notes.txt
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");

    // 8. "" -> reject (step 1)
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");

    // 9. notes\0/x (contains NUL) -> reject (step 2)
    std::string nulPath = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(nulPath, &out, &reason));
    CHECK(!reason.empty());

    // 10. /etc/passwd -> reject (step 3)
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());

    // 11. .. -> reject (step 5)
    CHECK(!ws.resolve("..", &out, &reason));
    CHECK(!reason.empty());

    // 12. ../etc/passwd -> reject (step 5)
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());

    // 13. a/../../b -> reject (step 5)
    CHECK(!ws.resolve("a/../../b", &out, &reason));
    CHECK(!reason.empty());

    // 14. notes/../../ws/notes/todo.txt -> reject (step 5)
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_spec_table_all_cases) {
    double val = 0.0;
    std::string err;

    // Spec §4.6 table entries:
    // 2 + 3 * 4 = 14
    REQUIRE(eval("2 + 3 * 4", &val, &err));
    CHECK(val == 14.0);

    // 10 - 4 - 3 = 3 (tests left-to-right evaluation of same precedence)
    REQUIRE(eval("10 - 4 - 3", &val, &err));
    CHECK(val == 3.0);

    // 100 / 10 / 5 = 2
    REQUIRE(eval("100 / 10 / 5", &val, &err));
    CHECK(val == 2.0);

    // (2 + 3) * 4 = 20
    REQUIRE(eval("(2 + 3) * 4", &val, &err));
    CHECK(val == 20.0);

    // 2 * (3 + 4) - 5 = 9
    REQUIRE(eval("2 * (3 + 4) - 5", &val, &err));
    CHECK(val == 9.0);

    // -3 * -3 = 9
    REQUIRE(eval("-3 * -3", &val, &err));
    CHECK(val == 9.0);

    // 7 / 2 = 3.5
    REQUIRE(eval("7 / 2", &val, &err));
    CHECK(val == 3.5);

    // 1 / (2 - 2) -> error: division by zero
    CHECK(!eval("1 / (2 - 2)", &val, &err));
    CHECK(err == "division by zero");
}

TEST(calc_custom_complex_and_nested) {
    double val = 0.0;
    std::string err;

    // Expression 1: mixed precedence with multiple nested parens
    REQUIRE(eval("((10 + 2) * (8 - 3)) / 4", &val, &err));
    CHECK(val == 15.0);

    // Expression 2: negative numbers with unary minus and decimals
    REQUIRE(eval("-5.5 + 2.5 * -2", &val, &err));
    CHECK(val == -10.5);

    // Expression 3: zero numerator is valid
    REQUIRE(eval("0 / 5", &val, &err));
    CHECK(val == 0.0);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }

