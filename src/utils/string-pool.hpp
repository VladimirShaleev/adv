#ifndef UTILS_STRING_POOL_HPP
#define UTILS_STRING_POOL_HPP

#include "utils.hpp"

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

#endif // UTILS_STRING_POOL_HPP
