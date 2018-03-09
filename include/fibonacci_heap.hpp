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

    static inline constexpr bool IS_NOTHROW_COMPARABLE =
        std::is_nothrow_invocable_v<const Compare&, const T&, const T&>;

    static inline constexpr bool IS_NOTHROW_SWAPPABLE =
        std::is_nothrow_swappable_v<RebindAllocT>
        and std::is_nothrow_swappable_v<OwnerT>
        and std::is_nothrow_swappable_v<Compare>;

    static inline constexpr bool ALLOC_IS_NOTHROW_CONSTRUCTIBLE =
        std::is_nothrow_constructible_v<Allocator, const RebindAllocT&>;

    static inline constexpr bool IS_NOTHROW_MOVE_ASSIGNABLE =
        std::is_nothrow_move_assignable_v<RebindAllocT>
        and std::is_nothrow_move_assignable_v<Compare>;

    static inline constexpr bool IS_NOTHROW_DEFAULT_CONSTRUCTIBLE =
        std::is_nothrow_default_constructible_v<RebindAllocT>
        and std::is_nothrow_default_constructible_v<OwnerT>
        and std::is_nothrow_default_constructible_v<Compare>;

    template <typename Iterator>
    struct IsValidIterator {
        static inline constexpr bool value = std::is_convertible_v<
            typename std::iterator_traits<Iterator>::value_type, T
        >;
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
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer =
        typename std::allocator_traits<Allocator>::const_pointer;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using reverse_iterator = std::reverse_iterator<Iterator>;
    using const_reverse_iterator = std::reverse_iterator<Iterator>;

    class Iterator {
    public:
        friend FibonacciHeap;

        using difference_type = FibonacciHeap::difference_type;
        using value_type = FibonacciHeap::value_type;
        using pointer = FibonacciHeap::const_pointer;
        using reference = FibonacciHeap::const_reference;
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

            if (current_->child) {
                current_ = current_->child;
            } else if (current_->right) {
                if (current_->parent) {
                    current_ = current_->parent;
                } else {
                    current_ = nullptr;
                }
            } else if (current_->parent) {
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
    
    FibonacciHeap() noexcept(IS_NOTHROW_DEFAULT_CONSTRUCTIBLE) = default;

    // FibonacciHeap& operator=(const FibonacciHeap &other);

    // FibonacciHeap& operator=(FibonacciHeap &&other)
    // noexcept(IS_NOTHROW_MOVE_ASSIGNABLE);

    // FibonacciHeap& operator=(std::initializer_list<value_type> ilist);

    allocator_type get_allocator() const
    noexcept(ALLOC_IS_NOTHROW_CONSTRUCTIBLE) {
        return allocator_type{ alloc_ };
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
              typename = std::enable_if_t<IsValidIterator<Iterator>::value>>
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
        static_assert(false, "pop not implemented yet");
        assert(root_);

        OwnerT child = std::move(root_->child);
    }

    void swap(FibonacciHeap &other) noexcept(IS_NOTHROW_SWAPPABLE) {
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

        if (empty()) {
            root_ = std::move(node);
        } else if (lt(root_->data, node->data)) {
            ++root_->rank;

            if (root_->child) {
                node->right = std::move(root_->child);
                node->right->left = node.get();
            }

            root_->child = std::move(node);
            root_->child->parent = root_.get();
        } else {
            OwnerT old_root = std::move(root_);
            root_ = std::move(node);
            root_->child = std::move(old_root);
            root_->child->parent = root_.get();

            root_->rank = root_->child->rank + 1;
        }

        ++size_;
    }

    void update_priority(Node *const node) noexcept {
        static_assert(false, "update_priority not implemented");
    }

    inline NodeDeleter make_deleter() noexcept {
        return NodeDeleter{ alloc_ };
    }

    template <typename ...Args>
    OwnerT construct_node(Args &&...args) {
        Node *node = RebindTraitsT::allocate(alloc_, 1);
        RebindTraitsT::construct(alloc_, node, std::forward<Args>(args)...);

        return OwnerT{ node, make_deleter() };
    }

    inline bool eq(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return not std::invoke(comparator_, lhs, rhs)
               and not std::invoke(comparator_, rhs, lhs);
    }

    inline bool ne(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return std::invoke(comparator_, lhs, rhs)
               or std::invoke(comparator_, rhs, lhs);
    }

    inline bool lt(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return std::invoke(comparator_, lhs, rhs);
    }

    inline bool le(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return not std::invoke(comparator_, rhs, lhs);
    }

    inline bool gt(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return std::invoke(comparator_, lhs, rhs);
    }

    inline bool ge(const value_type &lhs, const value_type &rhs) const
    noexcept(IS_NOTHROW_COMPARABLE) {
        return not std::invoke(comparator_, lhs, rhs);
    }

    RebindAllocT alloc_{ };
    OwnerT root_;
    Compare comparator_{ };
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
