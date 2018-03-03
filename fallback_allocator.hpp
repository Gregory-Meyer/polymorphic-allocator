#ifndef GREGJM_FALLBACK_ALLOCATOR_HPP
#define GREGJM_FALLBACK_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock,
                                     // gregjm::BadAllocationException

#include <algorithm> // std::max
#include <type_traits>
#include <utility> // std::move

namespace gregjm {

template <typename Primary, typename Secondary>
class FallbackAllocator : public PolymorphicAllocator {
public:
    FallbackAllocator() = default;

    FallbackAllocator(Primary primary, Secondary secondary)
        noexcept(std::is_nothrow_move_constructible_v<Primary>
                 and std::is_nothrow_move_constructible_v<Secondary>)
        : primary_{ std::move(primary) },
          secondary_{ std::move(secondary) }
    { }

private:
    MemoryBlock allocate_impl(const std::size_t count,
                              const std::size_t alignment) override {
        MemoryBlock block;

        try {
            block = primary_.allocate(count, alignment);
        } catch (const BadAllocationException &e) {
            // if this throws, we would just rethrow so no need to try-catch
            block = secondary_.allocate(count, alignment);
        }

        return block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        if (primary_.owns(block)) {
            primary_.deallocate(block);
        } else if (secondary_.owns(block)) {
            secondary_.deallocate(block);
        } else {
            throw NotOwnedException{ };
        }
    }

    void deallocate_all_impl() override {
        primary_.deallocate_all();
        secondary_.deallocate_all();
    }

    std::size_t max_size_impl() const override {
        return std::max(primary_.max_size(),
                        secondary_.max_size());
    }

    bool owns_impl(const MemoryBlock block) const override {
        return primary_.owns(block)
               or secondary_.owns(block);
    }

    Primary primary_;
    Secondary secondary_;
};

} // namespace gregjm

#endif
