#include "catch.hpp"

#include "fibonacci_heap.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

TEST_CASE("fibonacci heaps can be default constructed", "[FibonacciHeap]") {
    gregjm::FibonacciHeap<int> int_heap;
    gregjm::FibonacciHeap<double> double_heap;
    gregjm::FibonacciHeap<std::string> string_heap;
    gregjm::FibonacciHeap<std::vector<int>> vector_heap;
}

TEST_CASE("fibonacci heaps can have elements added to them", "[FibonacciHeap]") {
    GIVEN("an empty maxheap of ints") {
        gregjm::FibonacciHeap<int> heap;

        REQUIRE(heap.empty());
        REQUIRE(heap.size() == 0);

        THEN("we can push elements onto it") {
            heap.push(15);

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 1);
            REQUIRE(heap.top() == 15);

            heap.push(10);

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 2);
            REQUIRE(heap.top() == 15);

            heap.push(5);

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 3);
            REQUIRE(heap.top() == 15);
        }
    } GIVEN("an empty maxheap of strings") {
        gregjm::FibonacciHeap<std::string> heap;

        REQUIRE(heap.empty());
        REQUIRE(heap.size() == 0);

        THEN("we can emplace elements onto it") {
            heap.emplace();

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 1);
            REQUIRE(heap.top().empty());

            heap.emplace("a");

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 2);
            REQUIRE(heap.top() == "a");

            heap.emplace("b");

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 3);
            REQUIRE(heap.top() == "b");

            heap.emplace("a");

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 4);
            REQUIRE(heap.top() == "b");
        }
    } GIVEN("an empty maxheap of doubles") {
        gregjm::FibonacciHeap<double> heap;

        REQUIRE(heap.empty());
        REQUIRE(heap.size() == 0);

        THEN("we can insert from an initializer_list to it") {
            heap.insert({ 5.0, 3.0, 2.0, 1.0, 0.0, 1.0, 2.0 });

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 7);
            REQUIRE(heap.top() == 5.0);
        }
    }
}

TEST_CASE("iteration of FibonacciHeaps", "[FibonacciHeap]") {
    GIVEN("a FibonacciHeap of some elements") {
        const gregjm::FibonacciHeap<double> heap{ 0.0, 1.0, 2.0, 3.0 };

        THEN("it contains all of the elements") {
            const std::set<double> elements{ 0.0, 1.0, 2.0, 3.0 };
            const std::set<double> collected{ heap.begin(), heap.end() };

            REQUIRE(std::equal(elements.begin(), elements.end(),
                    collected.begin()));
        }
    }
}

bool less(const int lhs, const int rhs) noexcept {
    return lhs < rhs;
}

TEST_CASE("FibonacciHeap custom comparators", "[FibonacciHeap]") {
    GIVEN("a FibonacciHeap with a floating function comparator") {
        using HeapT = gregjm::FibonacciHeap<int, bool(*)(int, int)>;

        const HeapT heap({ 0, 1, 2, 3, 4, 5 }, &less);

        REQUIRE_FALSE(heap.empty());
        REQUIRE(heap.size() == 6);

        THEN("it compares as expected") {
            REQUIRE(heap.top() == 5);
        }
    } GIVEN("a FibonacciHeap with a lambda comparator") {
        auto greater = [](const int lhs, const int rhs) noexcept {
            return lhs > rhs;
        };

        using HeapT = gregjm::FibonacciHeap<int, decltype(greater)>;

        const HeapT heap({ 0, 1, 2, 3, 4, 5 }, std::move(greater));

        REQUIRE_FALSE(heap.empty());
        REQUIRE(heap.size() == 6);

        THEN("it compares as expected") {
            REQUIRE(heap.top() == 0);
        }
    } GIVEN("a FibonacciHeap with a member function comparator") {
        struct IntWrapper {
            int data;

            constexpr IntWrapper(const int x) noexcept : data{ x } { }

            constexpr bool less(const IntWrapper rhs) const noexcept {
                return data < rhs.data;
            }

            constexpr bool greater(const IntWrapper rhs) const noexcept {
                return data > rhs.data;
            }
        };

        using MemberT = decltype(&IntWrapper::less);
        using HeapT = gregjm::FibonacciHeap<IntWrapper, MemberT>;

        const HeapT heap({ 0, 1, 2, 3, 4, 5 }, &IntWrapper::greater);

        REQUIRE_FALSE(heap.empty());
        REQUIRE(heap.size() == 6);

        THEN("it compares as expected") {
            REQUIRE(heap.top().data == 0);
        }
    }
}
