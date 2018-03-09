#ifndef GREGJM_FIBONACCI_HEAP_HPP
#define GREGJM_FIBONACCI_HEAP_HPP

#include <cassert> // assert
#include <cstddef> // std::size_t, std::ptrdiff_t
#include <memory> // std::allocator, std::allocator_traits
#include <functional> // std::less, std::invoke
#include <iterator> // std::reverse_iterator
#include <type_traits> // std::is_nothrow_move_assignable_v
#include <initializer_list> // std::initializer_list
#include <utility> // std::move, std::swap, std::forward

namespace gregjm {

template <typename T, typename Compare = std::less<T>,
          typename Allocator = std::allocator<T>,
          typename = std::enable_if_t<std::is_invocable_v<Compare, const T&,
                                                          const T&>>,
          typename = std::enable_if_t<std::is_convertible_v<
              std::invoke_result_t<Compare, const T&, const T&>, bool
          >>,
          typename = std::enable_if_t<std::is_convertible_v<
              typename std::allocator_traits<Allocator>::value_type, T
          >>>
class FibonacciHeap {
    using SizeT = std::size_t;
    using TraitsT = std::allocator_traits<Allocator>;
    using RebindAllocT = typename TraitsT::template rebind_alloc<Node>;
    using RebindTraitsT = std::allocator_traits<RebindAllocT>;

    class Node;

    struct NodeDeleter {
        void operator*(Node *const node) {
            RebindTraitsT::destroy(*alloc, node);
            RebindTraitsT::deallocate(*alloc, node);
        }

        RebindAllocT *alloc;
    };

    using OwnerT = std::unique_ptr<Node, NodeDeleter>;

    // node in the heap; owns leftmost child and right sibling
    struct Node {
        template <typename ...Args,
                  typename = std::enable_if_t<std::is_constructible_v<T,
                                                                      Args...>>
        Node(Args &&...args)
            noexcept(std::is_nothrow_constructible_v<T, Args...>>)
            : data{ std::forward<Args>(args)... }
        { }

        T data;
        Node *parent = nullptr;
        OwnerT child = nullptr; // this should always be to the leftmost child
        Node *left = nullptr;
        OwnerT right = nullptr;
        SizeT rank = 0;
        bool is_marked = false;
    };

public:
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
    
    class iterator;
    using const_iterator = iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    FibonacciHeap& operator=(const FibonacciHeap &other);

    FibonacciHeap& operator=(FibonacciHeap &&other)
            noexcept(std::allocator_traits<RebindAllocT>::is_always_equal::value
                     and std::is_nothrow_move_assignable_v<Compare>::value);

    FibonacciHeap& operator=(std::initializer_list<value_type> ilist);

    allocator_type get_allocator() const {
        return alloc_;
    }

    reference top() {
        assert(root_);

        return *root_;
    }

    const_reference top() const {
        assert(root_);

        return *root_;
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
                  typename std::iterator_traits<Iterator>, value_type
              >>
    void insert(Iterator first, const Iterator last) {
        for (; first != last; ++first) {
            push(*first);
        }
    }

    void insert(const std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    template <typename ...Args>
    void emplace(Args &&...args);

    void pop() {
        assert(root_);

        OwnerT child = std::move(root_->child);
    }

    void swap(FibonacciHeap &other)
        noexcept(std::is_nothrow_swappable_v<RebindAllocT>
                 and std::is_nothrow_swappable_v<OwnerT>
                 and std::is_nothrow_swappable_v<Compare>)
    {
        using std::swap;

        swap(alloc_, other.alloc_);
        swap(root_, other.root_);
        swap(comparator_, other.comparator_);
        swap(size_, other.size_);
    }

    template <typename Function,
              typename = std::enable_if_t<std::is_invocable_v<Function,
                                                              value_type&>>>
    void update(const iterator iter, Function &&f) {
        std::invoke(std::forward<Function>(f), *iter);
        update_priority(iter);
    }

private:
    void push_node(OwnerT node) {
        assert(node);

        if (empty) {
            root_ = std::move(node);

            // TODO: rank
        } else if (lt(root_->data, node->data)) {
            OwnerT old_child = std::move(root_->child);
            node->right = std::move(old_child);
            root_->child = std::move(node);

            // TODO: rank
        } else {
            OwnerT old_root = std::move(root_);
            root_ = std::move(node);
            root_->child = std::move(old_root);

            // TODO: rank
        }

        ++size_;
    }

    void update_priority(const iterator iter);

    NodeDeleter make_deleter() noexcept {
        return NodeDeleter{ &alloc_ };
    }

    template <typename ...Args>
    OwnerT construct_node(Args &&...args) {
        Node *node = RebindTraitsT::allocate(alloc_, 1);
        RebindTraitsT::construct(alloc_, node, std::forward<Args>(args)...);

        return OwnerT{ node, make_deleter() };
    }

    bool eq(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return not std::invoke(comparator_, lhs, rhs)
               and not std::invoke(comparator_, rhs, lhs);
    }

    bool ne(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return std::invoke(comparator_, lhs, rhs)
               or std::invoke(comparator_, rhs, lhs);
    }

    bool lt(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return std::invoke(comparator_, lhs, rhs);
    }

    bool le(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return not std::invoke(comparator_, rhs, lhs);
    }

    bool gt(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return std::invoke(comparator_, rhs, lhs);
    }

    bool ge(const value_type &lhs, const value_type &rhs) const
            noexcept(std::is_nothrow_invocable_v<Compare, const value_type&,
                                                 const value_type&>)
    {
        return not std::invoke(comparator_, lhs, rhs);
    }

    RebindAllocT alloc_;
    OwnerT root_;
    Compare comparator_;
    size_t size_;
};

template <typename T, typename Compare, typename Allocator>
void swap(FibonacciHeap<T, Compare, Allocator> &lhs,
          FibonacciHeap<T, Compare, Allocator> &rhs)
    noexcept(noexcept(lhs.swap(rhs))
{
    lhs.swap(rhs);
}

} // namespace gregjm

#endif