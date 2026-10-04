#ifndef UTILS_STRING_POOL_HPP
#define UTILS_STRING_POOL_HPP

#include "utils.hpp"

[[nodiscard]] inline bool operator==(vm_string_h lhs, vm_string_h rhs) noexcept { return lhs.handle == rhs.handle; }

[[nodiscard]] inline bool operator!=(vm_string_h lhs, vm_string_h rhs) noexcept { return lhs.handle != rhs.handle; }

[[nodiscard]] inline std::strong_ordering operator<=>(vm_string_h lhs, vm_string_h rhs) noexcept {
    return lhs.handle <=> rhs.handle;
}

namespace utils {

class StringPool final {
public:
    explicit StringPool(size_t slotCapacity = 1024, size_t bufferCapacity = 65536, size_t hashTableCapacity = 4096);
    ~StringPool();

    [[nodiscard]] std::string_view operator[](vm_string_h str) const noexcept;
    [[nodiscard]] std::string_view get(vm_string_h str) const noexcept;
    [[nodiscard]] std::optional<vm_string_h> find(std::string_view str) const noexcept;
    vm_string_h insert(std::string_view str);

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};

} // namespace utils

namespace std {

template <>
struct hash<vm_string_h> {
    [[nodiscard]] size_t operator()(vm_string_h str) const noexcept {
        hash<uint32_t> handleHash;
        return handleHash(str.handle);
    }
};

} // namespace std

#endif // UTILS_STRING_POOL_HPP
