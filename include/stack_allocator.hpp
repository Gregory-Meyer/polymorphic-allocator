#ifndef GREGJM_STACK_ALLOCATOR_HPP
#define GREGJM_STACK_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock
#include "dummy_mutex.hpp" // gregjm::DummyMutex

#include <cstdint> // std::uint8_t
#include <cstring> // std::memcpy
#include <array> // std::array
#include <mutex> // std::scoped_lock

#include <iostream>

namespace gregjm {

template <std::size_t N, typename Mutex = DummyMutex>
class StackAllocator final : public PolymorphicAllocator {
    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        const std::scoped_lock lock{ mutex_ };

        return allocate_locked(size, alignment);
    }

    MemoryBlock reallocate_impl(const MemoryBlock block, const std::size_t size,
                                const std::size_t alignment) override {
        const std::scoped_lock lock{ mutex_ };

        if (not owns_locked(block)) {
            throw NotOwnedException{ };
        }

        const MemoryBlock realloc_block = allocate_locked(size, alignment);
        std::memcpy(realloc_block.memory, block.memory, block.size);
        deallocate_locked(block);

        return realloc_block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        const std::scoped_lock lock{ mutex_ };

        deallocate_locked(block);
    }

    void deallocate_all_impl() override {
        const std::scoped_lock lock{ mutex_ };

        stack_pointer_ = begin();
    }

    std::size_t max_size_impl() const override {
        const std::scoped_lock lock{ mutex_ };

        return max_size_locked();
    }

    bool owns_impl(const MemoryBlock block) const override {
        const std::scoped_lock lock{ mutex_ };

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

    // assumes resources are locked 
    void align_top(const std::size_t alignment) {
        auto sp = reinterpret_cast<std::uintptr_t>(stack_pointer_);
        sp += alignment - (sp % alignment);
        stack_pointer_ = reinterpret_cast<void*>(sp);
    }

    // assumes resources are locked
    void push(const std::size_t size) {
        auto sp = reinterpret_cast<std::uint8_t*>(stack_pointer_);
        sp += size;

        stack_pointer_ = sp;
    }

    // assumes resources are locked
    void pop(const std::size_t size) {
        auto sp = reinterpret_cast<std::uint8_t*>(stack_pointer_);
        sp -= size;

        stack_pointer_ = sp;
    }

    MemoryBlock allocate_locked(const std::size_t size,
                                const std::size_t alignment) {
        if (size + alignment - (size % alignment) > max_size_locked()) {
            throw BadAllocationException{ };
        }

        align_top(alignment);
        const MemoryBlock block{ stack_pointer_, size, alignment };
        push(size);

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
    }

    std::size_t max_size_locked() const {
        return reinterpret_cast<const uint8_t*>(end())
               - reinterpret_cast<const std::uint8_t*>(stack_pointer_);
    }

    bool owns_locked(const MemoryBlock block) const {
        return block.memory >= begin() and block.memory < stack_pointer_;
    }

    alignas(64) std::array<std::uint8_t, N> memory_;
    mutable Mutex mutex_;
    void *stack_pointer_ = begin();
};

} // namespace gregjm

#endif
