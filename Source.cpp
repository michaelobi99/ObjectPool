#include <array>
#include <chrono>
#include <iostream>
#include "pool.h"


template <typename T>
using MyPool = ObjectPool<T>;

class ExpensiveObject
{
public:
    ExpensiveObject() { /* ... */ }
    virtual ~ExpensiveObject() = default;
    // Member functions to populate the object with specific information.
    // Member functions to retrieve the object data.
    // (not shown)
private:
    // An expensive data member.
    std::array<double, 4 * 1024 * 1024> m_data;
    // Other data members (not shown)
};

template <typename T = ExpensiveObject>
std::shared_ptr<T> getExpensiveObject(MyPool<T>& pool) {
    auto object{ pool.acquireObject() };
    return object;
}

void processExpensiveObject(ExpensiveObject& object) { /* ... */ }

int main() {
    const size_t NumberOfIterations{ 500'000 };
    std::println("Starting loop using pool...");
    MyPool<ExpensiveObject> requestPool;
    auto start1{ std::chrono::steady_clock::now() };
    for (size_t i{ 0 }; i < NumberOfIterations; ++i) {
        auto object{ getExpensiveObject(requestPool) };
        processExpensiveObject(*object.get());
    }
    auto end1{ std::chrono::steady_clock::now() };
    auto diff1{ end1 - start1 };
    std::println("{}", std::chrono::duration<double,std:: milli>(diff1));
    std::println("Starting loop using new/delete...");
    auto start2{ std::chrono::steady_clock::now() };
    for (size_t i{ 0 }; i < NumberOfIterations; ++i) {
        auto object{ std::make_unique<ExpensiveObject>() };
        processExpensiveObject(*object);
    }
    auto end2{ std::chrono::steady_clock::now() };
    auto diff2{ end2 - start2 };
    std::println("{}", std::chrono::duration<double, std::milli>(diff2));
}