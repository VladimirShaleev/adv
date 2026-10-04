#ifndef UTILS_POOL_HPP
#define UTILS_POOL_HPP

#include "utils.hpp"

namespace utils {

template <typename T>
class Pool {
public:
    using handle_type = T::Handle;
    using base_handle_type = decltype(handle_type::handle);
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;

    static constexpr handle_type Undefined = {std::numeric_limits<base_handle_type>::max()};

    explicit Pool(size_type poolSize = 128) : _capacity(0), _head(0), _handles(nullptr), _values(nullptr) {
        reserve(poolSize);
    }

    ~Pool() { clear(); }

    Pool(const Pool&) = delete;

    Pool& operator=(const Pool&) = delete;

    Pool(Pool&& other) noexcept
        : _capacity(other._capacity), _head(other._head), _handles(other._handles), _values(other._values) {
        other._capacity = 0;
        other._head = 0;
        other._handles = nullptr;
        other._values = nullptr;
    }

    Pool& operator=(Pool&& other) noexcept {
        if (this != &other) {
            clear();
            _capacity = other._capacity;
            _head = other._head;
            _handles = other._handles;
            _values = other._values;
            other._capacity = 0;
            other._head = 0;
            other._handles = nullptr;
            other._values = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] size_type size() const noexcept { return _head; }

    [[nodiscard]] size_type capacity() const noexcept { return _capacity; }

    void reserve(size_type newPoolSize) {
        if (newPoolSize <= _capacity) {
            return;
        }
        auto handles = aliasCast<handle_type*>(realloc(_handles, newPoolSize * sizeof(handle_type)));
        if (!handles) {
            throw std::bad_alloc();
        }
        auto values = aliasCast<Value*>(realloc(_values, newPoolSize * sizeof(Value)));
        if (!values) {
            free(handles);
            throw std::bad_alloc();
        }
        std::memset((void*)(values + _capacity), 0, (newPoolSize - _capacity) * sizeof(Value));
        for (base_handle_type i = _capacity; i < newPoolSize; ++i) {
            handles[i] = {i};
        }
        for (size_t i = _capacity; i < newPoolSize; ++i) {
            values[i].handle = Undefined;
        }
        _capacity = newPoolSize;
        _handles = handles;
        _values = values;
    }

    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (uint32_t i = 0; i < _capacity; ++i) {
                if (_values[i].handle.handle != Undefined.handle) {
                    access({i})->~T();
                }
            }
        }
        _head = 0;
        _capacity = 0;
        if (_values) {
            free(_values);
            _values = nullptr;
        }
        if (_handles) {
            free(_handles);
            _handles = nullptr;
        }
    }

    [[nodiscard]] handle_type obtain() {
        checkInit();
        if (_head >= _capacity) {
            reserve(_capacity << 1);
        }
        const auto index = _handles[_head++];
        const auto handle = handle_type{index};
        if constexpr (!std::is_trivially_default_constructible_v<T>) {
            new (access(handle)) T();
        }
        _values[handle.handle].handle = handle;
        return handle;
    }

    void release(handle_type handle) noexcept {
        checkInit();
        checkHandle(handle);
        auto& value = _values[handle.handle];
        if constexpr (!std::is_trivially_destructible_v<T>) {
            value.obj.~T();
        }
        std::memset((void*)&value.obj, 0, sizeof(value_type));
        value.handle = Undefined;
        _handles[--_head] = handle;
    }

    [[nodiscard]] handle_type handle(const value_type* value) const noexcept {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
        constexpr auto offset = offsetof(Value, obj);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        const auto& owner = *aliasCast<const Value*>((const char*)value - offset);
        assert(&owner >= _values && &owner < _values + _capacity);
        return owner.handle;
    }

    [[nodiscard]] pointer access(handle_type handle) noexcept {
        checkInit();
        checkHandle(handle);
        return &_values[handle.handle].obj;
    }

    [[nodiscard]] const_pointer access(handle_type handle) const noexcept {
        checkInit();
        checkHandle(handle);
        return &_values[handle.handle].obj;
    }

    [[nodiscard]] std::pair<handle_type, pointer> obtainAndAccess() {
        const auto handle = obtain();
        return {handle, access(handle)};
    }

private:
    void checkInit() const noexcept { assert(_values && "memory not allocated"); }
    void checkHandle(handle_type handle) const noexcept {
        assert(_head > 0 && "invalid handle");
        assert(handle.handle < _capacity && "invalid handle");
    }

    struct Value {
        handle_type handle;
        value_type obj;
    };

    size_t _capacity;
    size_t _head;
    handle_type* _handles;
    Value* _values;
};

} // namespace utils

#endif // UTILS_POOL_HPP
