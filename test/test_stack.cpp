#include "polymorphic_allocator.hpp"
#include "stack_allocator.hpp"
#include "reporting_allocator.hpp"
#include "global_allocator.hpp"
#include "fallback_allocator.hpp"
#include "pool_allocator.hpp"

#include <iostream>
#include <vector>
#include <mutex>

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
using AllocT = gregjm::FallbackAllocator<
    gregjm::PoolAllocator<8_MiB, gregjm::GlobalAllocator<>>,
    gregjm::GlobalAllocator<>
>;

// using AllocT = gregjm::GlobalAllocator<std::mutex>;

void double_alloc(gregjm::PolymorphicAllocator &alloc) {
    constexpr std::size_t SIZE = 1 << 20;

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
    constexpr std::size_t SIZE = 1 << 20;
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

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    AllocT alloc{ };

    double_alloc(alloc);

    alloc.deallocate_all();

    string_alloc(alloc);

    alloc.deallocate_all();
}
