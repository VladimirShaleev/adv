#ifndef UTILS_OBJECT_HPP
#define UTILS_OBJECT_HPP

#include "utils.hpp"

namespace utils {

template <typename T>
struct Result {};

class Object {
public:
    virtual ~Object() = default;

    void reference() noexcept { ++_refCount; }

    void destroy() noexcept {
        if (--_refCount == 0) {
            delete this;
        }
    }

    template <typename T>
    [[nodiscard]] UTILS_FORCE_INLINE T* as() noexcept {
        static_assert(std::is_base_of_v<Object, T>, "T must inheritance from Object");
        return aliasCast<T*>(this);
    }

    template <typename R, typename D, typename T, typename... Args>
    [[nodiscard]] static R create(T*& obj, Args&&... args) noexcept {
        static_assert(std::is_base_of_v<T, D>, "D must inheritance from T");
        try {
            obj = new (std::nothrow) D(std::forward<Args>(args)...);
            return obj ? Result<R>::Success : Result<R>::OutOfMemory;
        } catch (...) {
            return Result<R>::Error;
        }
    }

private:
    uint32_t _refCount{1};
};

} // namespace utils

#endif // UTILS_OBJECT_HPP
