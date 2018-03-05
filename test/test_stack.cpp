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
#include <random>

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

constexpr std::size_t SIZE = 1 << 8;
constexpr char LONG_STRING[] = "this is a long string that won't fit";
constexpr char SHORT_STRING[] = "short";
constexpr std::size_t NUM_TESTS = 64;

std::random_device generator;

void double_alloc(gregjm::PolymorphicAllocator &alloc) {
    VectorT<double> u{ gregjm::make_adaptor<double>(alloc) };
    VectorT<double> v{ gregjm::make_adaptor<double>(alloc) };

    for (std::size_t i = 0; i < SIZE; ++i) {
        u.push_back(0.0);
        v.push_back(0.0);
    }

    u.shrink_to_fit();
    v.shrink_to_fit();
    u.erase(u.begin() + u.size() / 2, u.end());
    v.erase(v.begin() + v.size() / 2, v.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        u.push_back(0.0);
        v.push_back(0.0);
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, u.size() - 1 };

    for (std::size_t i = 0; i < u.size(); ++i) {
        using std::swap;

        swap(u[distribution(generator)], v[distribution(generator)]);
    }
}

void double_alloc() {
    std::vector<double> u;
    std::vector<double> v;

    for (std::size_t i = 0; i < SIZE; ++i) {
        u.push_back(0.0);
        v.push_back(0.0);
    }

    u.shrink_to_fit();
    v.shrink_to_fit();
    u.erase(u.begin() + u.size() / 2, u.end());
    v.erase(v.begin() + v.size() / 2, v.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        u.push_back(0.0);
        v.push_back(0.0);
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, u.size() - 1 };

    for (std::size_t i = 0; i < u.size(); ++i) {
        using std::swap;

        swap(u[distribution(generator)], v[distribution(generator)]);
    }
}

void string_alloc(gregjm::PolymorphicAllocator &alloc) {
    VectorT<StringT> u{ gregjm::make_adaptor<StringT>(alloc) };
    VectorT<StringT> v{ gregjm::make_adaptor<StringT>(alloc) };

    for (std::size_t i = 0; i < SIZE; ++i) {
        u.emplace_back(LONG_STRING, gregjm::make_adaptor<char>(alloc));
        v.emplace_back(SHORT_STRING, gregjm::make_adaptor<char>(alloc));
    }

    u.shrink_to_fit();
    v.shrink_to_fit();
    u.erase(u.begin() + u.size() / 2, u.end());
    v.erase(v.begin() + v.size() / 2, v.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        v.emplace_back(LONG_STRING, gregjm::make_adaptor<char>(alloc));
        u.emplace_back(SHORT_STRING, gregjm::make_adaptor<char>(alloc));
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, u.size() - 1 };

    for (std::size_t i = 0; i < u.size(); ++i) {
        using std::swap;

        auto &u_elem1 = u[distribution(generator)];
        auto &v_elem1 = v[distribution(generator)];

        u_elem1.insert(u_elem1.cbegin(), v_elem1.cbegin(), v_elem1.cend());

        auto &u_elem2 = u[distribution(generator)];
        auto &v_elem2 = v[distribution(generator)];

        v_elem2.insert(v_elem2.cbegin(), u_elem2.cbegin(), u_elem2.cend());
    }
}

void string_alloc() {
    std::vector<std::string> u;
    std::vector<std::string> v;

    for (std::size_t i = 0; i < SIZE; ++i) {
        u.emplace_back(LONG_STRING);
        v.emplace_back(SHORT_STRING);
    }

    u.shrink_to_fit();
    v.shrink_to_fit();
    u.erase(u.begin() + u.size() / 2, u.end());
    v.erase(v.begin() + v.size() / 2, v.end());

    for (std::size_t i = 0; i < SIZE / 2; ++i) {
        v.emplace_back(LONG_STRING);
        u.emplace_back(SHORT_STRING);
    }

    std::uniform_int_distribution<std::size_t> distribution{ 0, u.size() - 1 };

    for (std::size_t i = 0; i < u.size(); ++i) {
        using std::swap;

        auto &u_elem1 = u[distribution(generator)];
        auto &v_elem1 = v[distribution(generator)];

        u_elem1.insert(u_elem1.cbegin(), v_elem1.cbegin(), v_elem1.cend());

        auto &u_elem2 = u[distribution(generator)];
        auto &v_elem2 = v[distribution(generator)];

        v_elem2.insert(v_elem2.cbegin(), u_elem2.cbegin(), u_elem2.cend());
    }
}

void global_test(std::size_t num) {
    using AllocT = gregjm::GlobalAllocator<>;

    AllocT alloc;

    for (std::size_t i = 0; i < num; ++i) {
        double_alloc(alloc);
        string_alloc(alloc);
    }
}

void global_std_test(std::size_t num) {
    for (std::size_t i = 0; i < num; ++i) {
        double_alloc();
        string_alloc();
    }
}

// this is really slow
void stack_test(std::size_t num) {
    using AllocT = gregjm::FallbackAllocator<gregjm::StackAllocator<64_KiB>,
                                             gregjm::GlobalAllocator<>>;

    AllocT alloc;

    for (std::size_t i = 0; i < num; ++i) {
        double_alloc(alloc);
        string_alloc(alloc);
    }
}

void pool_test(std::size_t num) {
    using AllocT = gregjm::FallbackAllocator<
        gregjm::PoolAllocator<128_KiB, gregjm::GlobalAllocator<>>,
        gregjm::GlobalAllocator<>
    >;

    AllocT alloc;

    for (std::size_t i = 0; i < num; ++i) {
        double_alloc(alloc);
        string_alloc(alloc);
    }
}

void segregating_test(std::size_t num) {
    using AllocT = gregjm::SegregatingAllocator<
        16, gregjm::StackAllocator<64_KiB>, gregjm::GlobalAllocator<>
    >;

    AllocT alloc;

    for (std::size_t i = 0; i < num; ++i) {
        double_alloc(alloc);
        string_alloc(alloc);
    }
}

void segregating_pool_test(std::size_t num) {
    using AllocT = gregjm::SegregatingAllocator<
        16, gregjm::StackAllocator<64_KiB>,
        gregjm::FallbackAllocator<
            gregjm::PoolAllocator<128_KiB, gregjm::GlobalAllocator<>>,
            gregjm::GlobalAllocator<>
        >
    >;

    AllocT alloc;

    for (std::size_t i = 0; i < num; ++i) {
        double_alloc(alloc);
        string_alloc(alloc);
    }
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
        global_duration += time(global_test, num_tests).count();
    }

    std::cerr << "global tests took " << global_duration << " seconds\n";
}

void run_global_std_tests(const std::size_t num_tests) {
    long double global_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        global_duration += time(global_std_test, num_tests).count();
    }

    std::cerr << "global std tests took " << global_duration << " seconds\n";
}

void run_pool_tests(const std::size_t num_tests) {
    long double pool_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        pool_duration += time(pool_test, num_tests).count();
    }

    std::cerr << "pool tests took " << pool_duration << " seconds\n";
}

void run_segregating_tests(const std::size_t num_tests) {
    long double segregating_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        segregating_duration += time(segregating_test, num_tests).count();
    }

    std::cerr << "segregating tests took " << segregating_duration
        << " seconds\n";
}

void run_segregating_pool_tests(const std::size_t num_tests) {
    long double segregating_pool_duration = 0;

    for (std::size_t i = 0; i < num_tests; ++i) {
        segregating_pool_duration += time(segregating_pool_test, num_tests).count();
    }

    std::cerr << "segregating pool tests took " << segregating_pool_duration
        << " seconds\n";
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    //run_global_tests(NUM_TESTS);
    //run_global_std_tests(NUM_TESTS);
    run_pool_tests(NUM_TESTS);
    //run_segregating_pool_tests(NUM_TESTS);
    //run_segregating_tests(NUM_TESTS);
    //wait();
}
