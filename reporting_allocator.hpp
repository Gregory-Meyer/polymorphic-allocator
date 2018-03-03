#ifndef GREGJM_REPORTING_ALLOCATOR_HPP
#define GREGJM_REPORTING_ALLOCATOR_HPP

#include "polymorphic_allocator.hpp" // gregjm::PolymorphicAllocator,
                                     // gregjm::MemoryBlock

#include <iostream> // std::basic_ostream
#include <string> // std::char_traits

namespace gregjm {

template <typename Allocator>
class ReportingAllocator : public PolymorphicAllocator {
public:
    ReportingAllocator() : alloc_{ }, os_{ &std::cout } { }

    ReportingAllocator(std::ostream &os) : os_{ &os } { }

private:
    MemoryBlock allocate_impl(const std::size_t size,
                              const std::size_t alignment) override {
        const MemoryBlock block = alloc_.allocate(size, alignment);

        *os_ << "allocator " << &alloc_ << " allocated a block at "
            << block.memory << " with size " << block.size << " and alignment "
            << block.alignment << '\n';

        return block;
    }

    void deallocate_impl(const MemoryBlock block) override {
        alloc_.deallocate(block);

        *os_ << "allocator " << &alloc_ << " deallocated a block at "
            << block.memory << " with size " << block.size
            << " and alignment " << block.alignment << '\n';
    }

    void deallocate_all_impl() override {
        alloc_.deallocate_all();

        *os_ << "allocator " << &alloc_ << " deallocated all blocks"
            << '\n';
    }

    std::size_t max_size_impl() const override {
        return alloc_.max_size();
    }

    bool owns_impl(const MemoryBlock block) const override {
        return alloc_.owns(block);
    }

    Allocator alloc_;
    std::ostream *os_;
};

} // namespace gregjm

#endif
