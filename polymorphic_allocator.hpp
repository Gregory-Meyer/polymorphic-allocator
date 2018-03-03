#ifndef GREGJM_POLYMORPHIC_ALLOCATOR_HPP
#define GREGJM_POLYMORPHIC_ALLOCATOR_HPP

#include <cstddef> // std::size_t
#include <functional> // std::hash
#include <stdexcept> // std::exception

namespace gregjm {

struct MemoryBlock {
    void *memory;
    std::size_t size;
    std::size_t alignment;
};

bool operator==(const MemoryBlock lhs, const MemoryBlock rhs) noexcept {
    return (lhs.memory == rhs.memory) and (lhs.size == rhs.size);
}

bool operator!=(const MemoryBlock lhs, const MemoryBlock rhs) noexcept {
    return (lhs.memory != rhs.memory) or (lhs.size != rhs.size);
}

class PolymorphicAllocator {
public:
    inline MemoryBlock allocate(const std::size_t count,
                                const std::size_t alignment) {
        return allocate_impl(count, alignment);
    }

    inline void deallocate(const MemoryBlock block) {
        deallocate_impl(block);
    }

    inline void deallocate_all() {
        deallocate_all_impl();
    }

    inline std::size_t max_size() const {
        return max_size_impl();
    }

    inline bool owns(const MemoryBlock block) const {
        return owns_impl(block);
    }

private:
    virtual MemoryBlock allocate_impl(std::size_t count,
                                      std::size_t alignment) = 0;

    virtual void deallocate_impl(MemoryBlock block) = 0;

    virtual void deallocate_all_impl() = 0;

    virtual std::size_t max_size_impl() const = 0;

    virtual bool owns_impl(MemoryBlock block) const = 0;
};

template <typename T>
class PolymorphicAllocatorAdaptor {
public:
    using value_type = T;

    explicit PolymorphicAllocatorAdaptor(
        PolymorphicAllocator &alloc
    ) noexcept : alloc_{ &alloc } { }

    T* allocate(const std::size_t count) {
        const MemoryBlock block = alloc_->allocate(sizeof(T) * count,
                                                       alignof(T));

        return reinterpret_cast<T*>(block.memory);
    }

    void deallocate(T *const memory, const std::size_t count) {
        const MemoryBlock block{ reinterpret_cast<void*>(memory),
                                 sizeof(T) * count, alignof(T) };

        alloc_->deallocate(block);
    }

    std::size_t max_size() const {
        return alloc_->max_size();
    }

    PolymorphicAllocator& allocator() noexcept {
        return *alloc_;
    }

    const PolymorphicAllocator& allocator() const noexcept {
        return *alloc_;
    }

    void set_allocator(PolymorphicAllocator &allocator) noexcept {
        alloc_ = &allocator;
    }

    friend bool operator==(const PolymorphicAllocatorAdaptor &lhs,
                           const PolymorphicAllocatorAdaptor &rhs) noexcept {
        return lhs.alloc_ == rhs.alloc_;
    }

    friend bool operator!=(const PolymorphicAllocatorAdaptor &lhs,
                           const PolymorphicAllocatorAdaptor &rhs) noexcept {
        return lhs.alloc_ != rhs.alloc_;
    }

private:
    PolymorphicAllocator *alloc_;
};

template <typename T>
PolymorphicAllocatorAdaptor<T>
make_adaptor(PolymorphicAllocator &alloc) noexcept {
    return PolymorphicAllocatorAdaptor<T>{ alloc };
}

class NotOwnedException final : public std::exception {
public:
    NotOwnedException() noexcept : std::exception{ } { }

    virtual ~NotOwnedException() = default;

    const char* what() const noexcept override {
        return "NotOwnedException";
    }
};

class BadAllocationException final : public std::exception {
public:
    BadAllocationException() noexcept : std::exception{ } { }

    virtual ~BadAllocationException() = default;

    const char* what() const noexcept override {
        return "BadAllocationException";
    }
};

} // namespace gregjm

namespace std {

template <>
struct hash<gregjm::MemoryBlock> {
    std::size_t operator()(const gregjm::MemoryBlock block) const noexcept {
        const std::hash<void*> pointer_hasher;
        const std::size_t hash = pointer_hasher(block.memory);

        // from boost::hash_combine
        const std::hash<std::size_t> size_hasher;
        return hash ^ (size_hasher(block.size) + 0x9e3779b9 + (hash << 6)
                       + (hash >> 2));
    }
};

} // namespace std

#endif
