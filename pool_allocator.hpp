#ifndef GREGJM_POOL_ALLOCATOR_HPP
#define GREGJM_POOL_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock,
                                     // gregjm::PolymorphicAllocatorAdaptor,
                                     // gregjm::NotOwnedException
#include "stack_allocator.hpp" // gregjm::StackAllocator
#include "dummy_mutex.hpp" // gregjm::DummyMutex

#include <cstddef> // std::size_t
#include <algorithm> // std::make_heap, std::push_heap
#include <memory> // std::unique_ptr
#include <mutex> // std::lock_guard
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

        const MemoryBlock block{ pool, sizeof(StackAllocator<N>),
                                 alignof(StackAllocator<N>) };

        alloc_->deallocate(block);
    }

private:
    Allocator *alloc_;
};

} // namespace detail

template <std::size_t PoolSize, typename Allocator, typename Mutex = DummyMutex>
class PoolAllocator final : public PolymorphicAllocator {
    using LockT = std::lock_guard<Mutex>;
    using PoolT = StackAllocator<PoolSize>;
    using OwnerT = std::unique_ptr<PoolT,
                                   detail::PoolAllocatorDeleter<Allocator>>;
    using VectorT = std::vector<OwnerT, PolymorphicAllocatorAdaptor<OwnerT>>;

public:
    PoolAllocator(const PoolAllocator &lhs) = delete;

    template <typename ...Args,
              typename = std::enable_if_t<std::is_constructible_v<Allocator,
                                                                  Args...>>>
    PoolAllocator(Args &&...args)
        noexcept(std::is_nothrow_constructible_v<Allocator, Args...>)
        : alloc_{ std::forward<Args>(args)... }
    { }

    PoolAllocator& operator=(const PoolAllocator &rhs) = delete;

private:
    MemoryBlock allocate_impl(const std::size_t count,
                              const std::size_t alignment) override {
        if (count > PoolSize) {
            throw BadAllocationException{ };
        }

        LockT lock{ mutex_ };

        if (pools_.empty()) {
            return allocate_new(count, alignment);
        }

        try {
            const MemoryBlock block =
                pools_.front()->allocate(count, alignment);

            fix_down();

            return block;
        } catch (const BadAllocationException &e) {
            return allocate_new(count, alignment);
        }
    }

    void deallocate_impl(const MemoryBlock block) override {
        LockT lock{ mutex_ };

        for (const auto &pool_ptr : pools_) {
            if (pool_ptr->owns(block)) {
                pool_ptr->deallocate(block);
                
                return;
            }
        }
        
        throw NotOwnedException{ };
    }

    void deallocate_all_impl() override {
        LockT lock{ mutex_ };

        for (const auto &pool_ptr : pools_) {
            pool_ptr->deallocate_all();
        }
    }

    std::size_t max_size_impl() const override {
        return PoolSize;
    }

    bool owns_impl(const MemoryBlock block) const override {
        LockT lock{ mutex_ };

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

        pools_.emplace_back(pool, detail::PoolAllocatorDeleter{ alloc_ });
        std::push_heap(pools_.begin(), pools_.end(),
                       detail::PoolAllocatorComparator{ });

        return allocated_block;
    }

    // update priority of front heap element
    // assumes we have a lock
    void fix_down() {
        const std::size_t size = pools_.front()->max_size();

        for (auto current = pools_.begin(); current < pools_.end(); ) {
            using std::swap;
            
            const auto left = current + (current - pools_.begin()) + 1;
            const auto right = left + 1;

            if (right < pools_.end()) { // right and left are valid nodes
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
            } else if (left < pools_.end()) {
                if (size < (*left)->max_size()) {
                    swap(*current, *left);
                }

                break;
            } else {
                break;
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
