#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <sstream>
#include <windows.h>

namespace UnitTest {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class Registry {
public:
    static Registry& Instance() {
        static Registry reg;
        return reg;
    }

    void Add(const std::string& name, std::function<void()> func) {
        tests.push_back({ name, func });
    }

    const std::vector<TestCase>& GetTests() const {
        return tests;
    }

private:
    std::vector<TestCase> tests;
};

struct TestRegisterer {
    TestRegisterer(const std::string& name, std::function<void()> func) {
        Registry::Instance().Add(name, func);
    }
};

class AssertionException : public std::exception {
public:
    AssertionException(const std::string& msg, const char* file, int line) {
        std::ostringstream ss;
        ss << file << ":" << line << " — " << msg;
        message = ss.str();
    }
    const char* what() const noexcept override {
        return message.c_str();
    }
private:
    std::string message;
};

inline void SetConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

inline int RunAll() {
    auto& tests = Registry::Instance().GetTests();
    int total = (int)tests.size();
    int passed = 0;
    int failed = 0;

    SetConsoleColor(11); // Cyan
    std::cout << "\n======================================================================\n";
    std::cout << "          LENOVO ENERGYCONTROL — AUTOMATED TEST SUITE                 \n";
    std::cout << "======================================================================\n\n";
    SetConsoleColor(15);

    auto startTotal = std::chrono::high_resolution_clock::now();

    for (const auto& test : tests) {
        std::cout << " [ RUN      ] " << test.name << " ... " << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        bool ok = true;
        std::string failMsg;

        try {
            test.func();
        } catch (const AssertionException& ex) {
            ok = false;
            failMsg = ex.what();
        } catch (const std::exception& ex) {
            ok = false;
            failMsg = std::string("Unexpected std::exception: ") + ex.what();
        } catch (...) {
            ok = false;
            failMsg = "Unknown uncaught exception!";
        }

        auto elapsedMs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start).count() / 1000.0;

        if (ok) {
            SetConsoleColor(10); // Green
            std::cout << "PASS";
            SetConsoleColor(8); // Gray
            std::cout << " (" << elapsedMs << " ms)\n";
            SetConsoleColor(15);
            passed++;
        } else {
            SetConsoleColor(12); // Red
            std::cout << "FAIL";
            SetConsoleColor(8);
            std::cout << " (" << elapsedMs << " ms)\n";
            SetConsoleColor(12);
            std::cout << "   └──> " << failMsg << "\n";
            SetConsoleColor(15);
            failed++;
        }
    }

    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTotal).count();

    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << " Результати тестування: ";
    if (failed == 0) {
        SetConsoleColor(10); // Green
        std::cout << "УСІ " << passed << " ТЕСТІВ УСПІШНО ПРОЙДЕНО!";
    } else {
        SetConsoleColor(12); // Red
        std::cout << failed << " ТЕСТІВ ПРОВАЛЕНО з " << total;
    }
    SetConsoleColor(15);
    std::cout << " (" << totalElapsedMs << " ms)\n";
    std::cout << "======================================================================\n\n";

    return (failed == 0) ? 0 : 1;
}

} // namespace UnitTest

#define TEST_CASE(name) \
    static void Test_##name(); \
    static UnitTest::TestRegisterer reg_##name(#name, Test_##name); \
    static void Test_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) throw UnitTest::AssertionException("Assertion failed: " #expr, __FILE__, __LINE__)

#define ASSERT_FALSE(expr) \
    if (expr) throw UnitTest::AssertionException("Assertion failed: !(" #expr ")", __FILE__, __LINE__)

#define ASSERT_EQ(val1, val2) \
    if ((val1) != (val2)) { \
        std::ostringstream ss; \
        ss << "Expected equality of " #val1 " (" << (val1) << ") and " #val2 " (" << (val2) << ")"; \
        throw UnitTest::AssertionException(ss.str(), __FILE__, __LINE__); \
    }

#define ASSERT_NE(val1, val2) \
    if ((val1) == (val2)) { \
        std::ostringstream ss; \
        ss << "Expected inequality of " #val1 " and " #val2 " (" << (val1) << ")"; \
        throw UnitTest::AssertionException(ss.str(), __FILE__, __LINE__); \
    }

#define ASSERT_STREQ(s1, s2) \
    if ((s1) != (s2)) { \
        throw UnitTest::AssertionException(std::string("Expected string equality: '") + std::string(s1) + "' != '" + std::string(s2) + "'", __FILE__, __LINE__); \
    }
