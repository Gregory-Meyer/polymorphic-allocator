#ifndef GREGJM_DUMMY_MUTEX_HPP
#define GREGJM_DUMMY_MUTEX_HPP

#include <chrono>

namespace gregjm {

class DummyMutex {
public:
    constexpr inline void lock() const noexcept { }

    constexpr inline bool try_lock() const noexcept {
        return true;
    }

    template <typename Rep, typename Period>
    constexpr inline bool try_lock_for(
        const std::chrono::duration<Rep, Period>&
    ) const noexcept {
        return true;
    }

    template <typename Clock, typename Duration>
    constexpr inline bool try_lock_until(
        const std::chrono::time_point<Clock, Duration>&
    ) const noexcept {
        return true;
    }

    constexpr inline void unlock() const noexcept { } 
};

} // namespace gregjm

#endif
