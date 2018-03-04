#ifndef GREGJM_POOL_ALLOCATOR_HPP
#define GREGJM_POOL_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock,
                                     // gregjm::PolymorphicAllocatorAdaptor,
                                     // gregjm::NotOwnedException
#include "stack_allocator.hpp" // gregjm::StackAllocator
#include "dummy_mutex.hpp" // gregjm::DummyMutex

#include <cstddef> // std::size_t, std::ptrdiff_t
#include <cstring> // std::memcpy
#include <algorithm> // std::make_heap, std::push_heap, std::find_if
#include <memory> // std::unique_ptr
#include <mutex> // std::scoped_lock
#include <queue> // std::priority_queue
#include <utility> // std::swap, std::forward
#include <type_traits> // std::is_constructible_v,
                       // std::is_nothrow_constructible_v

namespace gregjm {
namespace detail {

// compare s.t. we get a maxheap based on the remaining size in a heap
struct PoolAllocatorComparator {
    template <std::size_t N, typename D1, std::size_t M = N, typename D2>
    bool operator()(const std::unique_ptr<StackAllocator<N>, D1> &lhs,
                    const std::unique_ptr<StackAllocator<M>, D2> &rhs) const {
        return lhs->max_size() < rhs->max_size();
    }
};

template <typename Allocator>
class PoolAllocatorDeleter {
public:
    PoolAllocatorDeleter() = delete;

    PoolAllocatorDeleter(Allocator &alloc) : alloc_{ &alloc } { }

    template <std::size_t N>
    void operator()(StackAllocator<N> *const pool) {
        pool->~StackAllocator<N>();

        const MemoryBlock block{ pool, sizeof(StackAllocator<N>) };

        alloc_->deallocate(block);
    }

private:
    Allocator *alloc_;
};

} // namespace detail

template <std::size_t PoolSize, typename Allocator, typename Mutex = DummyMutex>
class PoolAllocator final : public PolymorphicAllocator {
    using LockT = std::scoped_lock<Mutex>;
    using DoubleLockT = std::scoped_lock<Mutex, Mutex>;
    using PoolT = StackAllocator<PoolSize>;
    using OwnerT = std::unique_ptr<PoolT,
                                   detail::PoolAllocatorDeleter<Allocator>>;
    using VectorT = std::vector<OwnerT, PolymorphicAllocatorAdaptor<OwnerT>>;
    using IterMutT = typename VectorT::iterator;
    using IterT = typename VectorT::const_iterator;

public:
    PoolAllocator(const PoolAllocator &other) = delete;

    PoolAllocator(PoolAllocator &&other) : mutex_{ } {
        const DoubleLockT lock{ mutex_, other.mutex_ };

        alloc_ = std::move(other.alloc_);
        deleter_ = std::move(other.deleter_);
        pools_ = std::move(other.pools_);
    }

    template <typename ...Args,
              typename = std::enable_if_t<std::is_constructible_v<Allocator,
                                                                  Args...>>>
    PoolAllocator(Args &&...args)
        noexcept(std::is_nothrow_constructible_v<Allocator, Args...>)
        : alloc_{ std::forward<Args>(args)... }
    { }

    PoolAllocator& operator=(const PoolAllocator &other) = delete;

    PoolAllocator& operator=(PoolAllocator &&other) {
        if (this == &other) {
            return *this;
        }

        const LockT lock{ mutex_, other.mutex_ };

        alloc_ = std::move(other.alloc_);
        deleter_ = std::move(other.deleter_);
        pools_ = std::move(other.pools_);

        return *this;
    }

    virtual ~PoolAllocator() = default;

private:
    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        if (size > PoolSize) {
            throw BadAllocationException{ };
        }

        const LockT lock{ mutex_ };

        if (pools_.empty()) {
            return allocate_new(size, alignment);
        }

        try {
            const MemoryBlock block =
                pools_.front()->allocate(size, alignment);

            fix_down();

            return block;
        } catch (const BadAllocationException&) {
            return allocate_new(size, alignment);
        }
    }

    MemoryBlock reallocate_impl(const MemoryBlock block, const std::size_t size,
                                const std::size_t alignment) override {
        const LockT lock{ mutex_ };

        const auto owner_iter = get_owner_iter(block);

        if (owner_iter == pools_.end()) {
            throw NotOwnedException{ };
        }

        auto &owner = **owner_iter;

        try {
            return owner.reallocate(block, size, alignment);
        } catch (const BadAllocationException&) {
            const MemoryBlock new_block = allocate(size, alignment);

            std::memcpy(new_block.memory, block.memory, block.size);

            owner.deallocate(block);
            fix_up(owner_iter);

            return new_block;
        }
    }

    void deallocate_impl(const MemoryBlock block) override {
        const LockT lock{ mutex_ };

        const auto owner_iter = get_owner_iter(block);

        if (owner_iter == pools_.end()) {
            throw NotOwnedException{ };
        }

        auto &owner = **owner_iter;
        
        owner.deallocate(block);
        fix_up(owner_iter);
    }

    void deallocate_all_impl() override {
        const LockT lock{ mutex_ };

        for (const auto &pool_ptr : pools_) {
            pool_ptr->deallocate_all();
        }
    }

    std::size_t max_size_impl() const override {
        return PoolSize;
    }

    bool owns_impl(const MemoryBlock block) const override {
        const LockT lock{ mutex_ };

        for (const auto &pool_ptr : pools_) {
            if (pool_ptr->owns(block)) {
                return true;
            }
        }

        return false;
    }

    // creates a new pool and allocates out of it
    // assumes count <= PoolSize
    // assumes we have a lock
    MemoryBlock allocate_new(const std::size_t count,
                             const std::size_t alignment) {
        const MemoryBlock pool_block = alloc_.allocate(sizeof(PoolT),
                                                       alignof(PoolT));

        auto pool = new (pool_block.memory) PoolT{ };

        const MemoryBlock allocated_block =
            pool->allocate(count, alignment);

        pools_.emplace_back(pool,
                            detail::PoolAllocatorDeleter<Allocator>{ alloc_ });
        std::push_heap(pools_.begin(), pools_.end(),
                       detail::PoolAllocatorComparator{ });

        return allocated_block;
    }

    IterMutT get_owner_iter(const MemoryBlock block) {
        return std::find_if(pools_.begin(), pools_.end(),
                            [block](const OwnerT &owner) {
                                return owner->owns(block);
                            });
    }

    bool has_left_child(const IterT element) const noexcept {
        const std::ptrdiff_t signed_size = pools_.size();

        return 2 * (element - pools_.begin()) + 1 < signed_size;
    }

    bool has_right_child(const IterT element) const noexcept {
        const std::ptrdiff_t signed_size = pools_.size();

        return 2 * (element - pools_.begin()) + 2 < signed_size;
    }

    IterMutT left_child(const IterMutT element) noexcept {
        return element + (element - pools_.begin()) + 1;
    }

    IterT left_child(const IterT element) const noexcept {
        return element + (element - pools_.begin()) + 1;
    }

    IterMutT right_child(const IterMutT element) noexcept {
        return element + (element - pools_.begin()) + 2;
    }

    IterT right_child(const IterT element) const noexcept {
        return element + (element - pools_.begin()) + 2;
    }

    IterMutT left_child_or(const IterMutT element) noexcept {
        if (has_left_child(element)) {
            return left_child(element);
        }

        return pools_.end();
    }

    IterT left_child_or(const IterT element) const noexcept {
        if (has_left_child(element)) {
            return left_child(element);
        }

        return pools_.cend();
    }

    IterMutT right_child_or(const IterMutT element) noexcept {
        if (has_right_child(element)) {
            return right_child(element);
        }

        return pools_.end();
    }

    IterT right_child_or(const IterT element) const noexcept {
        if (has_right_child(element)) {
            return right_child(element);
        }

        return pools_.cend();
    }

    // update priority of front heap element
    // assumes we have a lock
    // assumes nonempty
    void fix_down() noexcept(std::is_nothrow_swappable_v<OwnerT>) {
        const std::size_t size = pools_.front()->max_size();

        for (auto current = pools_.begin(); current < pools_.end(); ) {
            using std::swap;

            const auto left = left_child_or(current);
            const auto right = right_child_or(current);

            if (right != pools_.end()) { // right and left are valid nodes
                if ((*left)->max_size() < (*right)->max_size()) {
                    if (size >= (*right)->max_size()) {
                        break;
                    }

                    swap(*right, *current);

                    current = right;
                } else {
                    if (size >= (*left)->max_size()) {
                        break;
                    }

                    swap(*left, *current);

                    current = left;
                }
            } else if (left != pools_.end()) {
                if (size >= (*left)->max_size()) {
                    break;
                }

                swap(*current, *left);
                current = left;
            } else {
                break;
            }
        }
    }

    bool has_parent(const IterT element) const noexcept {
        return element - pools_.begin() > 0;
    }

    IterMutT parent(const IterMutT element) const noexcept {
        return element - (element - pools_.begin() + 2) / 2;
    }

    IterT parent(const IterT element) const noexcept {
        return element - (element - pools_.begin() + 2) / 2;
    }

    IterMutT parent_or(const IterMutT element) noexcept {
        if (has_parent(element)) {
            return parent(element);
        }

        return pools_.end();
    }

    IterT parent_or(const IterT element) const noexcept {
        if (has_parent(element)) {
            return parent(element);
        }

        return pools_.cend();
    }

    // update priority of given heap element
    // assumes we have a lock
    // assumes current is dereferencable
    void fix_up(IterMutT element) noexcept(std::is_nothrow_swappable_v<OwnerT>) {
        const auto size = (*element)->max_size();

        for (; element > pools_.begin(); ) {
            const auto parent = parent_or(element);

            if (parent == pools_.end()) {
                return;
            }

            if ((*parent)->max_size() < size) {
                using std::swap;

                swap(*element, *parent);
                element = parent;
            } else {
                return;
            }
        }
    }

    mutable Mutex mutex_;
    Allocator alloc_;
    detail::PoolAllocatorDeleter<Allocator> deleter_{ alloc_ };
    VectorT pools_{ make_adaptor<OwnerT>(alloc_) };
};

} // namespace gregjm

#endif
