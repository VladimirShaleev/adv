#ifndef VM_CACHE_HPP
#define VM_CACHE_HPP

#include "vm.hpp"

namespace vm {

struct Cache {
    utils::StringPool pool;
    vm_string_h strTrue;
    vm_string_h strFalse;
    std::array<vm_string_h, 50> strInts;
};

} // namespace vm

#endif // VM_CACHE_HPP
