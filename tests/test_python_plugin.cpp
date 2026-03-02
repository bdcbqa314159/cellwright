#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include "plugin/PyFunctionPlugin.hpp"
#include "core/CellValue.hpp"
#include <fstream>
#include <filesystem>

namespace py = pybind11;
using namespace magic;

// ── Global Python interpreter (one per process) ────────────────────────────

static py::scoped_interpreter* guard = nullptr;

class PythonEnv : public ::testing::Environment {
public:
    void SetUp() override { guard = new py::scoped_interpreter{}; }
    void TearDown() override { delete guard; guard = nullptr; }
};

testing::Environment* const python_env =
    testing::AddGlobalTestEnvironment(new PythonEnv);

// ── Helper: write a temp .py file ──────────────────────────────────────────

static std::filesystem::path write_temp_py(const std::string& name,
                                            const std::string& code) {
    auto dir = std::filesystem::temp_directory_path() / "magic_py_tests";
    std::filesystem::create_directories(dir);
    auto path = dir / (name + ".py");
    std::ofstream(path) << code;
    return path;
}

// ── Basic numeric round-trip ───────────────────────────────────────────────

TEST(PyPlugin, NumericRoundTrip) {
    auto path = write_temp_py("add", R"(
functions = {
    "add": {"min_args": 2, "max_args": 2},
}

def add(a, b):
    return a + b
)");

    PyFunctionPlugin plugin(path);

    auto descs = plugin.describe_functions();
    ASSERT_EQ(descs.size(), 1u);
    EXPECT_EQ(descs[0].name, "add");
    EXPECT_EQ(descs[0].min_args, 2);
    EXPECT_EQ(descs[0].max_args, 2);

    auto r = plugin.call("add", {CellValue{3.0}, CellValue{4.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 7.0);
}

// ── String round-trip ──────────────────────────────────────────────────────

TEST(PyPlugin, StringRoundTrip) {
    auto path = write_temp_py("greet", R"(
functions = {
    "greet": {"min_args": 1, "max_args": 1},
}

def greet(name):
    return "Hello, " + str(name)
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("greet", {CellValue{std::string("World")}});
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "Hello, World");
}

// ── Boolean round-trip ─────────────────────────────────────────────────────

TEST(PyPlugin, BoolRoundTrip) {
    auto path = write_temp_py("ispos", R"(
functions = {
    "ispos": {"min_args": 1, "max_args": 1},
}

def ispos(x):
    return x > 0
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("ispos", {CellValue{5.0}});
    ASSERT_TRUE(std::holds_alternative<bool>(r));
    EXPECT_TRUE(std::get<bool>(r));

    r = plugin.call("ispos", {CellValue{-1.0}});
    ASSERT_TRUE(std::holds_alternative<bool>(r));
    EXPECT_FALSE(std::get<bool>(r));
}

// ── describe_functions ─────────────────────────────────────────────────────

TEST(PyPlugin, DescribeFunctions) {
    auto path = write_temp_py("multi", R"(
functions = {
    "foo": {"min_args": 0, "max_args": 0},
    "bar": {"min_args": 1, "max_args": 3},
}

def foo():
    return 42

def bar(a, b=0, c=0):
    return a + b + c
)");

    PyFunctionPlugin plugin(path);
    auto descs = plugin.describe_functions();
    EXPECT_EQ(descs.size(), 2u);
}

// ── Error mapping: ZeroDivisionError → DIV0 ───────────────────────────────

TEST(PyPlugin, DivByZero) {
    auto path = write_temp_py("divz", R"(
functions = {
    "divz": {"min_args": 2, "max_args": 2},
}

def divz(a, b):
    return a / b
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("divz", {CellValue{1.0}, CellValue{0.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::DIV0);
}

// ── Error mapping: ValueError → VALUE ──────────────────────────────────────

TEST(PyPlugin, ValueError) {
    auto path = write_temp_py("valr", R"(
functions = {
    "valr": {"min_args": 1, "max_args": 1},
}

def valr(x):
    raise ValueError("bad value")
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("valr", {CellValue{1.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── Error mapping: KeyError → NAME ─────────────────────────────────────────

TEST(PyPlugin, KeyError) {
    auto path = write_temp_py("kerr", R"(
functions = {
    "kerr": {"min_args": 1, "max_args": 1},
}

def kerr(x):
    d = {}
    return d[x]
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("kerr", {CellValue{std::string("missing")}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}

// ── Unknown function → NAME error ──────────────────────────────────────────

TEST(PyPlugin, UnknownFunction) {
    auto path = write_temp_py("unk", R"(
functions = {
    "known": {"min_args": 0, "max_args": 0},
}

def known():
    return 1
)");

    PyFunctionPlugin plugin(path);
    auto r = plugin.call("nonexistent", {});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}

// ── Validation: missing functions dict → throws ────────────────────────────

TEST(PyPlugin, MissingFunctionsDict) {
    auto path = write_temp_py("nofuncs", R"(
def oops():
    return 1
)");

    EXPECT_THROW(PyFunctionPlugin{path}, std::runtime_error);
}

// ── Validation: missing def → throws ───────────────────────────────────────

TEST(PyPlugin, MissingDef) {
    auto path = write_temp_py("nodef", R"(
functions = {
    "ghost": {"min_args": 0, "max_args": 0},
}
)");

    EXPECT_THROW(PyFunctionPlugin{path}, std::runtime_error);
}

// ── py_bond integration ────────────────────────────────────────────────────

class PyBondTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin = std::make_unique<PyFunctionPlugin>(PY_BOND_PLUGIN_PATH);
    }
    std::unique_ptr<PyFunctionPlugin> plugin;

    std::string makeBond(double coupon, double ytm, double maturity, double freq) {
        auto r = plugin->call("pybond", {
            CellValue{coupon}, CellValue{ytm}, CellValue{maturity}, CellValue{freq}
        });
        EXPECT_TRUE(is_string(r));
        return as_string(r);
    }
};

TEST_F(PyBondTest, ParBondPrice) {
    auto h = makeBond(0.05, 0.05, 10, 2);
    EXPECT_TRUE(h.starts_with("PyBond#"));
    auto r = plugin->call("pybondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 100.0, 1e-9);
}

TEST_F(PyBondTest, PremiumBondPrice) {
    auto h = makeBond(0.05, 0.035, 10, 2);
    auto r = plugin->call("pybondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_GT(as_number(r), 100.0);
}

TEST_F(PyBondTest, DurationSanity) {
    auto h = makeBond(0.05, 0.05, 10, 2);
    auto r = plugin->call("pybondDuration", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_GT(as_number(r), 0.0);
    EXPECT_LT(as_number(r), 10.0);
}

TEST_F(PyBondTest, UnknownHandle) {
    auto r = plugin->call("pybondPrice", {CellValue{std::string("PyBond#9999")}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}

TEST_F(PyBondTest, HandleRoundTrip) {
    auto h = makeBond(0.06, 0.04, 5, 2);

    auto ytm = plugin->call("pybondYtm", {CellValue{h}});
    ASSERT_TRUE(is_number(ytm));
    EXPECT_DOUBLE_EQ(as_number(ytm), 0.04);

    auto coupon = plugin->call("pybondCoupon", {CellValue{h}});
    ASSERT_TRUE(is_number(coupon));
    EXPECT_DOUBLE_EQ(as_number(coupon), 0.06);
}
