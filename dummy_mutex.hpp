#ifndef GREGJM_DUMMY_MUTEX_HPP
#define GREGJM_DUMMY_MUTEX_HPP

namespace gregjm {

class DummyMutex {
public:
    constexpr void lock() const noexcept { }

    constexpr void unlock() const noexcept { } 
};

} // namespace gregjm

#endif
