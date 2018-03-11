#ifndef GREGJM_FIBONACCI_HEAP_HPP
#define GREGJM_FIBONACCI_HEAP_HPP

#include <cassert> // assert
#include <cstddef> // std::size_t, std::ptrdiff_t
#include <memory> // std::allocator, std::allocator_traits
#include <functional> // std::less, std::invoke
#include <iterator> // std::reverse_iterator
#include <type_traits> // a lot, but mainly std::enable_if_t
#include <initializer_list> // std::initializer_list
#include <utility> // std::move, std::swap, std::forward

namespace gregjm {

template <typename T, typename Compare = std::less<T>,
          typename Allocator = std::allocator<T>>
class FibonacciHeap {
    static_assert(std::is_invocable_v<const Compare&, const T&, const T&>,
                  "must be able to invoke Compare with const T&");
    static_assert(
        std::is_convertible_v<std::invoke_result_t<const Compare&, const T&,
                                                   const T&>,
                              bool>,
        "return type of invoked Compare must be convertible to bool"
    );
    static_assert(
        std::is_same_v<
            T, typename std::allocator_traits<Allocator>::value_type
        >, "Allocator value_type must be T"
    );

    struct Node;

    using SizeT = std::size_t;
    using TraitsT = std::allocator_traits<Allocator>;
    using RebindAllocT = typename TraitsT::template rebind_alloc<Node>;
    using RebindTraitsT = std::allocator_traits<RebindAllocT>;

    struct NodeDeleter {
        NodeDeleter() = default;

        NodeDeleter(RebindAllocT &allocator) noexcept : alloc{ &allocator } { }

        void operator()(Node *const node) {
            assert(alloc);

            RebindTraitsT::destroy(*alloc, node);
            RebindTraitsT::deallocate(*alloc, node, 1);
        }

        RebindAllocT *alloc = nullptr;
    };

    using OwnerT = std::unique_ptr<Node, NodeDeleter>;

    // node in the heap; owns leftmost child and right sibling
    struct Node {
        template <typename ...Args,
                  typename = std::enable_if_t<
                      std::is_constructible_v<T, Args...>
                  >>
        Node(Args &&...args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : data{ std::forward<Args>(args)... } { }

        T data;
        Node *parent = nullptr;
        OwnerT child = nullptr; // this should always be to the leftmost child
        Node *left = nullptr;
        OwnerT right = nullptr;
        SizeT rank = 0;
        bool is_marked = false;
    };

    template <typename R, typename L>
    struct IsComparable {
        static inline constexpr bool value =
            std::is_invocable_v<const Compare&, R, L>
            and std::is_invocable_v<const Compare&, L, R>;
    };

    template <typename R = const T&, typename L = const T&>
    struct IsNothrowComparable {
        static inline constexpr bool value =
            std::is_nothrow_invocable_v<const Compare&, R, L>
            and std::is_nothrow_invocable_v<const Compare&, L, R>;
    };

public:
    class Iterator;

    using value_compare = Compare;
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = SizeT;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename TraitsT::pointer;
    using const_pointer = typename TraitsT::const_pointer;
    using iterator = Iterator;
    using const_iterator = Iterator;

    class Iterator {
        using TraitsT = std::allocator_traits<Allocator>;

    public:
        friend FibonacciHeap;

        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = typename TraitsT::pointer;
        using reference = const T&;
        using iterator_category = std::forward_iterator_tag;

        constexpr Iterator() noexcept = default;

        reference operator*() const {
            assert(current_);

            return current_->data;
        }

        pointer operator->() const noexcept {
            assert(current_);

            return pointer{ &current_->data };
        }

        Iterator& operator++() {
            assert(current_);

            if (current_->child) { // first try descending
                current_ = current_->child.get();
            } else if (current_->right) { // then try the sibling
                current_ = current_->right.get();
            } else if (current_->parent) { // then try finding the next highest sibling
                do {
                    current_ = current_->parent;

                    if (not current_) {
                        return *this;
                    }
                } while (not current_->right);

                current_ = current_->right.get();
            } else {
                current_ = nullptr;
            }

            return *this;
        }

        Iterator operator++(int) {
            const auto copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const Iterator lhs,
                               const Iterator rhs) noexcept {
            return lhs.current_ == rhs.current_;
        }

        friend bool operator!=(const Iterator lhs,
                               const Iterator rhs) noexcept {
            return lhs.current_ != rhs.current_;
        }

    private:
        Iterator(const Node &current) noexcept : current_{ &current } { }

        const Node *current_ = nullptr;
    };
    
    FibonacciHeap() = default;

    // FibonacciHeap(const FibonacciHeap &other);

    FibonacciHeap(FibonacciHeap &&other) = default;

    template <typename =
                  std::enable_if_t<std::is_copy_constructible_v<value_compare>>>
    FibonacciHeap(const value_compare &comparator)
    noexcept(std::is_nothrow_copy_constructible_v<value_compare>)
    : comparator_{ comparator } { }

    template <typename =
                  std::enable_if_t<std::is_move_constructible_v<value_compare>>>
    FibonacciHeap(value_compare &&comparator)
    noexcept(std::is_nothrow_move_constructible_v<value_compare>)
    : comparator_{ std::move(comparator) } { }

    FibonacciHeap(const allocator_type &allocator) noexcept
    : alloc_{ allocator } { }

    FibonacciHeap(allocator_type &&allocator) noexcept
    : alloc_{ std::move(allocator) } { }

    template <typename Iterator,
              typename = std::enable_if_t<std::is_convertible_v<
                  typename std::iterator_traits<Iterator>::value_type, T
              >>>
    FibonacciHeap(const Iterator first, const Iterator last,
                  const value_compare &comparator = value_compare{ },
                  const allocator_type &allocator = allocator_type{ })
    : alloc_{ allocator }, comparator_{ comparator } {
        insert(first, last);
    }

    template <typename Iterator,
              typename = std::enable_if_t<std::is_convertible_v<
                  typename std::iterator_traits<Iterator>::value_type, T
              >>>
    FibonacciHeap(const Iterator first, const Iterator last,
                  const allocator_type &allocator)
    : alloc_{ allocator } {
        insert(first, last);
    }

    FibonacciHeap(const std::initializer_list<value_type> init,
                  const value_compare &comparator = value_compare{ },
                  const allocator_type &allocator = allocator_type{ })
    : FibonacciHeap{ init.begin(), init.end(), comparator, allocator } { }

    FibonacciHeap(const std::initializer_list<value_type> init,
                  const allocator_type &allocator)
    : FibonacciHeap{ init.begin(), init.end(), value_compare{ }, allocator } { }

    // FibonacciHeap& operator=(const FibonacciHeap &other);

    // FibonacciHeap& operator=(FibonacciHeap &&other)
    // noexcept(IS_NOTHROW_MOVE_ASSIGNABLE);

    // FibonacciHeap& operator=(std::initializer_list<value_type> ilist);

    allocator_type get_allocator() const noexcept {
        return allocator_type{ alloc_ };
    }

    const_iterator begin() const noexcept {
        return Iterator{ *root_ };
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator end() const noexcept {
        return Iterator{ };
    }

    const_iterator cend() const noexcept {
        return end();
    }

    reference top() {
        assert(root_);

        return root_->data;
    }

    const_reference top() const {
        assert(root_);

        return root_->data;
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    size_type size() const noexcept {
        return size_;
    }

    void clear() {
        root_ = nullptr;
    }

    void push(const value_type &value) {
        push_node(construct_node(value));
    }

    void push(value_type &&value) {
        push_node(construct_node(value));
    }

    template <typename Iterator,
              typename = std::enable_if_t<std::is_convertible_v<
                  typename std::iterator_traits<Iterator>::value_type, T
              >>>
    void insert(Iterator first, const Iterator last) {
        for (; first != last; ++first) {
            push(*first);
        }
    }

    void insert(const std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    template <typename ...Args>
    void emplace(Args &&...args) {
        push_node(construct_node(std::forward<Args>(args)...));
    }

    void pop() {
        //static_assert(false, "pop not implemented yet");
        assert(root_);

        OwnerT child = std::move(root_->child);
    }

    void swap(FibonacciHeap &other)
    noexcept(std::is_nothrow_swappable_v<Compare>) {
        using std::swap;

        swap(alloc_, other.alloc_);
        swap(root_, other.root_);
        swap(comparator_, other.comparator_);
        swap(size_, other.size_);
    }

    template <typename Function,
              typename = std::enable_if_t<
                  std::is_invocable_v<Function, T&>
              >>
    void update(const iterator iter, Function &&f)
    noexcept(std::is_nothrow_invocable_v<Function, T&>) {
        std::invoke(std::forward<Function>(f), *iter);
        update_priority(iter);
    }

private:
    void push_node(OwnerT node) {
        assert(node);
        assert(not node->child);
        assert(not node->left);
        assert(not node->right);
        assert(node->rank == 0);

        root_ = meld_trees(std::move(node), std::move(root_));

        ++size_;
    }

    // returns the new root of the melded tree
    OwnerT meld_trees(OwnerT first, OwnerT second) const
    noexcept(IsNothrowComparable<>::value) {
        if (not first and not second) {
            return OwnerT{ };
        } else if (not first and second) {
            return second;
        } else if (first and not second) {
            return first;
        }

        // first and second should be the heads of trees
        assert(not first->parent);
        assert(not first->left);
        assert(not first->right);
        assert(not second->parent);
        assert(not second->left);
        assert(not second->right);

        if (lt(first->data, second->data)) {
            if (second->child) {
                second->child->left = first.get();
                first->right = std::move(second->child);
            }

            first->parent = second.get();
            second->child = std::move(first);
            ++second->rank;

            return second;
        }
        
        // return first as head if equal
        if (first->child) {
            first->child->left = second.get();
            second->right = std::move(first->child);
        }

        second->parent = first.get();
        first->child = std::move(second);
        ++first->rank;

        return first;
    }

    void update_priority(Node *const node) noexcept {
        //static_assert(false, "update_priority not implemented");
    }

    inline NodeDeleter make_deleter() noexcept {
        return NodeDeleter{ alloc_ };
    }

    template <typename ...Args,
              typename = std::enable_if_t<
                  std::is_constructible_v<Node, Args...>
              >>
    OwnerT construct_node(Args &&...args) {
        Node *node = RebindTraitsT::allocate(alloc_, 1);
        RebindTraitsT::construct(alloc_, node, std::forward<Args>(args)...);

        return OwnerT{ node, make_deleter() };
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool eq(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return not std::invoke(comparator_, std::forward<L>(lhs),
                               std::forward<R>(rhs))
               and not std::invoke(comparator_, std::forward<R>(rhs),
                                   std::forward<L>(lhs));
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool ne(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return std::invoke(comparator_, std::forward<L>(lhs),
                           std::forward<R>(rhs))
               or std::invoke(comparator_, std::forward<R>(rhs),
                              std::forward<L>(lhs));
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool lt(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return std::invoke(comparator_, std::forward<L>(lhs),
                           std::forward<R>(rhs));
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool le(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return not std::invoke(comparator_, std::forward<R>(rhs),
                               std::forward<L>(lhs));
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool gt(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return std::invoke(comparator_, std::forward<R>(rhs),
                           std::forward<L>(lhs));
    }

    template <typename L, typename R,
              typename = std::enable_if_t<IsComparable<L, R>::value>>
    inline bool ge(L &&lhs, R &&rhs) const
    noexcept(IsNothrowComparable<L, R>::value) {
        return not std::invoke(comparator_, std::forward<L>(lhs),
                               std::forward<R>(rhs));
    }

    RebindAllocT alloc_{ };
    OwnerT root_;
    value_compare comparator_{ };
    size_t size_ = 0;
};

template <typename T, typename Compare, typename Allocator>
void swap(FibonacciHeap<T, Compare, Allocator> &lhs,
          FibonacciHeap<T, Compare, Allocator> &rhs)
noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

} // namespace gregjm

#endif
