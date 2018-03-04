#include "polymorphic_allocator.hpp"
#include "stack_allocator.hpp"
#include "reporting_allocator.hpp"
#include "global_allocator.hpp"
#include "fallback_allocator.hpp"
#include "pool_allocator.hpp"
#include "segregating_allocator.hpp"

#include <iostream>
#include <vector>
#include <mutex>
#include <chrono>
#include <utility>

constexpr unsigned long long
operator""_KiB(unsigned long long literal) noexcept {
    return literal * (1ull << 10ull);
}

constexpr unsigned long long
operator""_MiB(unsigned long long literal) noexcept {
    return literal * (1ull << 20ull);
}

constexpr unsigned long long
operator""_GiB(unsigned long long literal) noexcept {
    return literal * (1ull << 30ull);
}

constexpr unsigned long long
operator""_TiB(unsigned long long literal) noexcept {
    return literal * (1ull << 40ull);
}

template <typename T>
using VectorT = std::vector<T, gregjm::PolymorphicAllocatorAdaptor<T>>;
using StringT = std::basic_string<char, std::char_traits<char>,
                                  gregjm::PolymorphicAllocatorAdaptor<char>>;

void double_alloc(gregjm::PolymorphicAllocator &alloc) {
    constexpr std::size_t SIZE = 1 << 15;

    VectorT<double> numbers{ gregjm::make_adaptor<double>(alloc) };

    for (std::size_t i = 0; i < SIZE; ++i) {
        numbers.push_back(0.0);
    }

    numbers.shrink_to_fit();
    numbers.erase(numbers.begin() + numbers.size() / 2, numbers.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        numbers.push_back(0.0);
    }

    numbers.shrink_to_fit();
}

void string_alloc(gregjm::PolymorphicAllocator &alloc) {
    constexpr std::size_t SIZE = 1 << 15;
    constexpr char LONG_STRING[] = "this is a long string that won't fit";
    constexpr char SHORT_STRING[] = "short";

    VectorT<StringT> strings{ gregjm::make_adaptor<StringT>(alloc) };

    for (std::size_t i = 0; i < SIZE; ++i) {
        strings.emplace_back(LONG_STRING, gregjm::make_adaptor<char>(alloc));
        strings.emplace_back(SHORT_STRING, gregjm::make_adaptor<char>(alloc));
    }

    strings.shrink_to_fit();
    strings.erase(strings.begin() + strings.size() / 2, strings.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        strings.emplace_back(LONG_STRING, gregjm::make_adaptor<char>(alloc));
        strings.emplace_back(SHORT_STRING, gregjm::make_adaptor<char>(alloc));
    }

    strings.shrink_to_fit();
}

void global_test() {
    using AllocT = gregjm::GlobalAllocator<>;

    AllocT alloc;

    double_alloc(alloc);

    string_alloc(alloc);
}

// this is really slow
void stack_test() {
    using AllocT = gregjm::FallbackAllocator<gregjm::StackAllocator<512_KiB>,
                                             gregjm::GlobalAllocator<>>;

    AllocT alloc;

    double_alloc(alloc);
    string_alloc(alloc);
}

void pool_test() {
    using AllocT = gregjm::FallbackAllocator<
        gregjm::PoolAllocator<8_MiB, gregjm::GlobalAllocator<>>,
        gregjm::GlobalAllocator<>
    >;

    AllocT alloc;

    double_alloc(alloc);
    string_alloc(alloc);
}

void segregating_test() {
    using AllocT = gregjm::SegregatingAllocator<
        16, gregjm::StackAllocator<512_KiB>, gregjm::GlobalAllocator<>
    >;

    AllocT alloc;

    double_alloc(alloc);
    string_alloc(alloc);
}

void segregating_pool_test() {
    using AllocT = gregjm::SegregatingAllocator<
        16, gregjm::StackAllocator<512_KiB>,
        gregjm::FallbackAllocator<
            gregjm::PoolAllocator<8_MiB, gregjm::GlobalAllocator<>>,
            gregjm::GlobalAllocator<>
        >
    >;

    AllocT alloc;

    double_alloc(alloc);
    string_alloc(alloc);
}

void wait() {
    char buffer;

    while (std::cin >> buffer) { }
}

template <typename Rep = long double, typename Period = std::ratio<1>,
          typename Function, typename ...Args>
std::chrono::duration<Rep, Period> time(Function &&f, Args &&...args) {
    const auto start = std::chrono::high_resolution_clock::now();
    std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
    const auto stop = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<Rep, Period>{ stop - start };
}

void run_global_tests(const std::size_t num_tests) {
    long double global_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        global_duration += time(global_test).count();
    }

    std::cerr << "global tests took " << global_duration << " seconds\n";
}

void run_pool_tests(const std::size_t num_tests) {
    long double pool_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        pool_duration += time(pool_test).count();
    }

    std::cerr << "pool tests took " << pool_duration << " seconds\n";
}

void run_segregating_tests(const std::size_t num_tests) {
    long double segregating_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        segregating_duration += time(segregating_test).count();
    }

    std::cerr << "segregating tests took " << segregating_duration
        << " seconds\n";
}

void run_segregating_pool_tests(const std::size_t num_tests) {
    long double segregating_pool_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        segregating_pool_duration += time(segregating_pool_test).count();
    }

    std::cerr << "segregating pool tests took " << segregating_pool_duration
        << " seconds\n";
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
    
    constexpr std::size_t NUM_TESTS = 128;

    run_global_tests(NUM_TESTS);
    wait();
}
