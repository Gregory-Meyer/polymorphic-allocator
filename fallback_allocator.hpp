#ifndef GREGJM_FALLBACK_ALLOCATOR_HPP
#define GREGJM_FALLBACK_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock,
                                     // gregjm::BadAllocationException

#include <algorithm> // std::max
#include <type_traits> // std::is_nothrow_constructible_v,
                       // std::is_constructible_v, std::enable_if_t
#include <utility> // std::move, std::pair, std::piecewise_construct
#include <tuple> // std::tuple

namespace gregjm {

template <typename Primary, typename Secondary>
class FallbackAllocator final : public PolymorphicAllocator {
    using PairT = std::pair<Primary, Secondary>;
public:
    FallbackAllocator() = default;

    FallbackAllocator(const FallbackAllocator &other) = delete;

    FallbackAllocator(Primary primary, Secondary secondary)
        noexcept(
            std::is_nothrow_constructible_v<PairT, Primary&&, Secondary&&>
        ) : allocs_{ std::move(primary), std::move(secondary) }
    { }

    template <typename ...PrimaryArgs, typename ...SecondaryArgs, typename = std::enable_if_t<std::is_constructible_v<Primary, PrimaryArgs...> and std::is_constructible_v<Secondary, SecondaryArgs...>>>
    FallbackAllocator(const std::tuple<PrimaryArgs...> primary_args,
                      const std::tuple<SecondaryArgs...> secondary_args)
        noexcept(
            std::is_nothrow_constructible_v<PairT, std::piecewise_construct_t,
                                            std::tuple<PrimaryArgs...>,
                                            std::tuple<SecondaryArgs...>>
        ) : allocs_{ std::piecewise_construct, primary_args, secondary_args }
    { }

    FallbackAllocator& operator=(const FallbackAllocator &other) = delete;

private:
    MemoryBlock allocate_impl(const std::size_t count,
                              const std::size_t alignment) override {
        MemoryBlock block;

        try {
            block = primary().allocate(count, alignment);
        } catch (const BadAllocationException &e) {
            // if this throws, we would just rethrow so no need to try-catch
            block = secondary().allocate(count, alignment);
        }

        return block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        if (primary().owns(block)) {
            primary().deallocate(block);
        } else if (secondary().owns(block)) {
            secondary().deallocate(block);
        } else {
            throw NotOwnedException{ };
        }
    }

    void deallocate_all_impl() override {
        primary().deallocate_all();
        secondary().deallocate_all();
    }

    std::size_t max_size_impl() const override {
        return std::max(primary().max_size(),
                        secondary().max_size());
    }

    bool owns_impl(const MemoryBlock block) const override {
        return primary().owns(block) or secondary().owns(block);
    }

    constexpr inline Primary& primary() noexcept {
        return allocs_.first;
    }

    constexpr inline const Primary& primary() const noexcept {
        return allocs_.first;
    }

    constexpr inline Secondary& secondary() noexcept {
        return allocs_.second;
    }

    constexpr inline const Secondary& secondary() const noexcept {
        return allocs_.second;
    }

    PairT allocs_;
};

} // namespace gregjm

#endif
