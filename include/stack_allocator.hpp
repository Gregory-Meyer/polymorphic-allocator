#ifndef GREGJM_STACK_ALLOCATOR_HPP
#define GREGJM_STACK_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock
#include "dummy_mutex.hpp" // gregjm::DummyMutex

#include <cstdint> // std::uint8_t
#include <cstring> // std::memcpy
#include <array> // std::array
#include <algorithm> // std::min
#include <mutex> // std::scoped_lock

#include <iostream>

namespace gregjm {

template <std::size_t N, typename Mutex = DummyMutex>
class StackAllocator final : public PolymorphicAllocator {
public:
    virtual ~StackAllocator() = default;

private:
    using LockT = std::scoped_lock<Mutex>;

    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        const LockT lock{ mutex_ };

        return allocate_locked(size, alignment);
    }

    MemoryBlock reallocate_impl(const MemoryBlock block, const std::size_t size,
                                const std::size_t alignment) override {
        const LockT lock{ mutex_ };

        if (not owns_locked(block)) {
            throw NotOwnedException{ };
        }

        if (reinterpret_cast<std::uint8_t*>(block.memory) + block.size
            == stack_pointer_) {
            const MemoryBlock realloc_block{ block.memory, size };

            if (size < block.size) {
                pop(block.size - size);
            } else {
                push(size - block.size);
            }

            return realloc_block;
        }

        const auto min_size = std::min(size, block.size);
        const MemoryBlock realloc_block = allocate_locked(size, alignment);
        std::memcpy(realloc_block.memory, block.memory, min_size);
        deallocate_locked(block);

        return realloc_block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        const LockT lock{ mutex_ };

        deallocate_locked(block);
    }

    void deallocate_all_impl() override {
        const LockT lock{ mutex_ };

        stack_pointer_ = begin();
    }

    std::size_t max_size_impl() const override {
        const LockT lock{ mutex_ };

        return max_size_locked();
    }

    bool owns_impl(const MemoryBlock block) const override {
        const LockT lock{ mutex_ };

        return owns_locked(block);
    }

    constexpr void* begin() noexcept {
        return reinterpret_cast<void*>(memory_.data());
    }

    constexpr const void* begin() const noexcept {
        return reinterpret_cast<const void*>(memory_.data());
    }

    constexpr const void* cbegin() const noexcept {
        return begin();
    }

    constexpr void* end() noexcept {
        return reinterpret_cast<void*>(memory_.data() + memory_.size());
    }

    constexpr const void* end() const noexcept {
        return reinterpret_cast<const void*>(memory_.data() + memory_.size());
    }

    constexpr const void* cend() const noexcept {
        return end();
    }

    std::size_t aligned_offset(const std::size_t size, const std::size_t alignment) {
        const auto modulo = size % alignment;

        if (modulo == 0) {
            return 0;
        }

        return alignment - modulo;
    }

    // assumes resources are locked 
    void align_top(const std::size_t alignment) {
        auto sp = reinterpret_cast<std::uintptr_t>(stack_pointer_);
        max_size_ -= aligned_offset(static_cast<std::size_t>(sp), alignment);
        sp += aligned_offset(static_cast<std::size_t>(sp), alignment);
        stack_pointer_ = reinterpret_cast<void*>(sp);
    }

    // assumes resources are locked
    void push(const std::size_t size) {
        auto sp = reinterpret_cast<std::uint8_t*>(stack_pointer_);
        max_size_ -= size;
        sp += size;

        stack_pointer_ = sp;
    }

    // assumes resources are locked
    void pop(const std::size_t size) {
        auto sp = reinterpret_cast<std::uint8_t*>(stack_pointer_);
        max_size_ += size;
        sp -= size;

        stack_pointer_ = sp;
    }

    MemoryBlock allocate_locked(const std::size_t size,
                                const std::size_t alignment) {
        if (size + aligned_offset(size, alignment) > max_size_locked()) {
            throw BadAllocationException{ };
        }

        align_top(alignment);
        const MemoryBlock block{ stack_pointer_, size };
        push(size);

        ++allocated_;

        return block;
    }

    void deallocate_locked(const MemoryBlock block) {
        if (not owns_locked(block)) {
            throw NotOwnedException{ };
        }

        const std::uint8_t *const memory =
            reinterpret_cast<std::uint8_t*>(block.memory) + block.size;

        if (reinterpret_cast<const void*>(memory) == stack_pointer_) {
            pop(block.size);
        }

        --allocated_;

        if (allocated_ == 0) {
            stack_pointer_ = begin();
            max_size_ = N;
        }
    }

    std::size_t max_size_locked() const {
        return max_size_;
    }

    bool owns_locked(const MemoryBlock block) const {
        return block.memory >= begin() and block.memory < stack_pointer_;
    }


    alignas(64) std::array<std::uint8_t, N> memory_;
    mutable Mutex mutex_;
    void *stack_pointer_ = begin();
    std::size_t allocated_ = 0;
    std::size_t max_size_ = N;
};

} // namespace gregjm

#endif
