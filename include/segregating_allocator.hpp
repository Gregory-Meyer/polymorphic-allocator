#ifndef GREGJM_SEGREGATING_ALLOCATOR_HPP
#define GREGJM_SEGREGATING_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp"

#include <cstddef> // std::size_t
#include <cstring> // std::memcpy
#include <type_traits> // std::is_nothrow_constructible_v,
                       // std::is_constructible_v, std::enable_if_t
#include <utility> // std::pair

namespace gregjm {

template <std::size_t N, typename Little, typename Big>
class SegregatingAllocator final : public PolymorphicAllocator {
    using PairT = std::pair<Little, Big>;

public:
    SegregatingAllocator() = default;

    SegregatingAllocator(const SegregatingAllocator &other) = delete;

    SegregatingAllocator(Little little, Big big)
        noexcept(
            std::is_nothrow_constructible_v<PairT, Little&&, Big&&>
        ) : allocs_{ std::move(primary), std::move(secondary) }
    { }

    template <typename ...LittleArgs, typename ...BigArgs,
              typename = std::enable_if_t<
                  std::is_constructible_v<Little, LittleArgs...>
                  and std::is_constructible_v<Big, BigArgs...>
              >>
    SegregatingAllocator(const std::tuple<LittleArgs...> little_args,
                         const std::tuple<BigArgs...> big_args)
        noexcept(
            std::is_nothrow_constructible_v<PairT, std::piecewise_construct_t,
                                            std::tuple<LittleArgs...>,
                                            std::tuple<BigArgs...>>
        ) : allocs_{ std::piecewise_construct, little_args, big_args }
    { }

    SegregatingAllocator& operator=(const SegregatingAllocator &other) = delete;

    virtual ~SegregatingAllocator() = default;

private:
    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        if (size <= N) {
            return little().allocate(size, alignment);
        }

        return big().allocate(size, alignment);
    }

    MemoryBlock reallocate_impl(const MemoryBlock block, const std::size_t size,
                                const std::size_t alignment) override {
        if (block.size <= N) {
            return reallocate_from_little(block, size, alignment);
        }
        
        return reallocate_from_big(block, size, alignment);
    }

    void deallocate_impl(const MemoryBlock block) override {
        if (block.size <= N) {
            little().deallocate(block);
        } else {
            big().deallocate(block);
        }
    }

    void deallocate_all_impl() override {
        big().deallocate_all();
        little().deallocate_all();
    }

    std::size_t max_size_impl() const override {
        // you might have an allocator where this is necessary
        return std::max(N, big().max_size());
    }

    bool owns_impl(const MemoryBlock block) const override {
        if (block.size <= N) {
            return little().owns(block);
        }

        return big().owns(block);
    }

    MemoryBlock reallocate_from_little(const MemoryBlock block,
                                       const std::size_t size,
                                       const std::size_t alignment) {
        if (size > N) {
            const MemoryBlock new_block = big().allocate(size, alignment);

            // we know that block.size <= N, so block.size < size
            std::memcpy(new_block.memory, block.memory, block.size);

            little().deallocate(block);

            return new_block;
        }

        return little().reallocate(block, size, alignment);
    }

    MemoryBlock reallocate_from_big(const MemoryBlock block,
                                    const std::size_t size,
                                    const std::size_t alignment) {
        if (size <= N) {
            const MemoryBlock new_block = little().allocate(size, alignment);

            // we know that block.size > N, so size < block.size
            std::memcpy(new_block.memory, block.memory, size);

            little().deallocate(block);

            return new_block;
        }

        return big().reallocate(block, size, alignment);
    }

    constexpr inline Little& little() noexcept {
        return allocs_.first;
    }

    constexpr inline const Little& little() const noexcept {
        return allocs_.first;
    }

    constexpr inline Big& big() noexcept {
        return allocs_.second;
    }

    constexpr inline const Big& big() const noexcept {
        return allocs_.second;
    }

    PairT allocs_;
};

} // namespace gregjm

#endif
