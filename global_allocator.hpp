#ifndef GREGJM_GLOBAL_ALLOCATOR_HPP
#define GREGJM_GLOBAL_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock

#include <climits> // SIZE_MAX
#include <cstdlib> // std::malloc, std::free
#include <mutex> // std::mutex, std::lock_guard
#include <unordered_set> // std::unordered_set

namespace gregjm {

class GlobalAllocator final : public PolymorphicAllocator {
    using LockT = std::lock_guard<std::mutex>;

public:
    GlobalAllocator() = default;

    GlobalAllocator(GlobalAllocator &&other) : blocks_{ }, mutex_{ } {
        LockT self_lock{ mutex_ };
        LockT other_lock{ other.mutex_ };

        blocks_ = std::move(other.blocks_);
    }

    GlobalAllocator& operator=(GlobalAllocator &&other) {
        LockT self_lock{ mutex_ };
        LockT other_lock{ other.mutex_ };

        blocks_ = std::move(other.blocks_);

        return *this;
    }

private:
    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        void *const memory = std::malloc(size);

        if (memory == nullptr) {
            throw BadAllocationException{ };
        }

        const MemoryBlock block{ memory, size, alignment };

        {
            std::lock_guard<std::mutex> lock{ mutex_ };
            blocks_.insert(block);
        }

        return block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        LockT lock{ mutex_ };

        deallocate_locked(block);
    }

    void deallocate_all_impl() override {
        LockT lock{ mutex_ };

        while (not blocks_.empty()) {
            deallocate_locked(*blocks_.begin());
        }
    }

    std::size_t max_size_impl() const override {
        return SIZE_MAX;
    }

    bool owns_impl(const MemoryBlock block) const override {
        LockT lock{ mutex_ };

        return static_cast<bool>(blocks_.count(block));
    }

    void deallocate_locked(const MemoryBlock block) {
        if (blocks_.erase(block) == 0) {
            throw NotOwnedException{ };
        }

        std::free(block.memory);
    }

    std::unordered_set<MemoryBlock> blocks_;
    mutable std::mutex mutex_;
};

} // namespace gregjm

#endif
