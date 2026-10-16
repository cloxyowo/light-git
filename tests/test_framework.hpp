#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <cstdlib>

namespace test_framework {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline void register_test(const std::string& name, std::function<void()> func) {
    get_tests().push_back({name, std::move(func)});
}

#define TEST(name) \
    static void test_func_##name(); \
    static struct TestRegister_##name { \
        TestRegister_##name() { \
            test_framework::register_test(#name, test_func_##name); \
        } \
    } test_register_instance_##name; \
    static void test_func_##name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " == " #b " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if (!((a) != (b))) { \
            std::ostringstream ss; \
            ss << "Assertion failed: " #a " != " #b " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

#define ASSERT_THROWS(expr) \
    do { \
        bool caught = false; \
        try { \
            expr; \
        } catch (...) { \
            caught = true; \
        } \
        if (!caught) { \
            std::ostringstream ss; \
            ss << "Expected exception from: " #expr " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

inline int run_all_tests() {
    int passed = 0;
    int failed = 0;
    std::cout << "[----------] Running " << get_tests().size() << " tests\n";

    for (const auto& t : get_tests()) {
        std::cout << "[ RUN      ] " << t.name << "\n";
        try {
            t.func();
            std::cout << "[       OK ] " << t.name << "\n";
            passed++;
        } catch (const std::exception& e) {
            std::cout << "[  FAILED  ] " << t.name << "\n    " << e.what() << "\n";
            failed++;
        } catch (...) {
            std::cout << "[  FAILED  ] " << t.name << "\n    Unknown exception caught\n";
            failed++;
        }
    }

    std::cout << "[==========] " << passed + failed << " tests ran: "
              << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}

} // namespace test_framework
