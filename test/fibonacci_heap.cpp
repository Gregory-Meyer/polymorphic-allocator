#include "catch.hpp"

#include "fibonacci_heap.hpp"

#include <string>
#include <vector>

TEST_CASE("fibonacci heaps can be default constructed", "[FibonacciHeap]") {
    gregjm::FibonacciHeap<int> int_heap;
    gregjm::FibonacciHeap<double> double_heap;
    gregjm::FibonacciHeap<std::string> string_heap;
    gregjm::FibonacciHeap<std::vector<int>> vector_heap;
}

TEST_CASE("fibonacci heaps can have elements added to them", "[FibonacciHeap]") {
    GIVEN("an empty minheap of ints") {
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
            REQUIRE(heap.top() == 10);

            heap.push(5);

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 3);
            REQUIRE(heap.top() == 5);
        }
    } GIVEN("an empty maxheap of strings") {
        gregjm::FibonacciHeap<std::string, std::greater<>> heap;

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
    } GIVEN("an empty minheap of doubles") {
        gregjm::FibonacciHeap<double> heap;

        REQUIRE(heap.empty());
        REQUIRE(heap.size() == 0);

        THEN("we can insert from an initializer_list to it") {
            heap.insert({ 5.0, 3.0, 2.0, 1.0, 0.0, 1.0, 2.0 });

            REQUIRE_FALSE(heap.empty());
            REQUIRE(heap.size() == 7);
            REQUIRE(heap.top() == 0.0);
        }
    }
}