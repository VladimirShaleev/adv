#ifndef UTILS_FLAT_MAP_HPP
#define UTILS_FLAT_MAP_HPP

#include "utils.hpp"

namespace utils {

template <typename K, IsHandle H, typename V, typename Compare = std::less<K>>
class FlatMap {
public:
    [[nodiscard]] UTILS_FORCE_INLINE V* get(H handle) noexcept {
        if (handle != Undefined) {
            if (auto offset = handleToOffset(handle); offset < _values.size()) {
                return &_values[offset];
            }
        }
        return nullptr;
    }

    [[nodiscard]] UTILS_FORCE_INLINE const V* get(H handle) const noexcept {
        if (handle != Undefined) {
            if (auto offset = handleToOffset(handle); offset < _values.size()) {
                return &_values[offset];
            }
        }
        return nullptr;
    }

    [[nodiscard]] UTILS_FORCE_INLINE std::pair<H, V*> insert(const K& key, const V& value) { return emplace(key, value); }

    template <typename... Args>
    [[nodiscard]] std::pair<H, V*> emplace(const K& key, Args&&... args) {
        const auto pos = lowerBound(key);
        if (pos != _keys.end() && !_compare(key, pos->first)) {
            _values[pos->second] = V(std::forward<Args>(args)...);
            return {offsetToHandle(pos->second), &_values[pos->second]};
        }
        const auto offset = _values.size();
        _values.emplace_back(std::forward<Args>(args)...);
        try {
            _keys.insert(pos, {key, offset});
        } catch (...) {
            _values.pop_back();
            throw;
        }
        return {offsetToHandle(offset), &_values.back()};
    }

    [[nodiscard]] UTILS_FORCE_INLINE std::pair<H, V*> find(const K& key) {
        const auto pos = lowerBound(key);
        if (pos == _keys.end() || _compare(key, pos->first)) {
            return {Undefined, nullptr};
        }
        return {offsetToHandle(pos->second), &_values[pos->second]};
    }

    [[nodiscard]] UTILS_FORCE_INLINE std::pair<H, const V*> find(const K& key) const {
        const auto pos = lowerBound(key);
        if (pos == _keys.end() || _compare(key, pos->first)) {
            return {Undefined, nullptr};
        }
        return {offsetToHandle(pos->second), &_values[pos->second]};
    }

    [[nodiscard]] UTILS_FORCE_INLINE bool contains(const K& key) const noexcept { return find(key).first != Undefined; }

private:
    using KeyEntry = std::pair<K, decltype(H::handle)>;

    [[nodiscard]] UTILS_FORCE_INLINE auto lowerBound(const K& key) noexcept {
        return std::lower_bound(_keys.begin(), _keys.end(), key, [this](const auto& entry, const auto& key) {
            return _compare(entry.first, key);
        });
    }

    [[nodiscard]] UTILS_FORCE_INLINE auto lowerBound(const K& key) const noexcept {
        return std::lower_bound(_keys.begin(), _keys.end(), key, [this](const auto& entry, const auto& key) {
            return _compare(entry.first, key);
        });
    }

    [[nodiscard]] UTILS_FORCE_INLINE static auto handleToOffset(H handle) noexcept { return handle.handle - 1; }
    [[nodiscard]] UTILS_FORCE_INLINE static auto offsetToHandle(size_t offset) noexcept {
        return H{static_cast<decltype(H::handle)>(offset + 1)};
    }

    std::vector<KeyEntry> _keys;
    std::vector<V> _values;
    Compare _compare;
};

} // namespace utils

#endif // UTILS_FLAT_MAP_HPP
