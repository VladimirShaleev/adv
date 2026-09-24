#ifndef ADV_ANALYZER_HPP
#define ADV_ANALYZER_HPP

#include "compiler.hpp"

constexpr vm_value_type_t VM_VALUE_TYPE_VOID = vm_value_type_t(int(VM_VALUE_TYPE_COUNT_ENUMS) + 1);

namespace adv {

class Options;
class ContextBase;

enum class CastResult {
    OK,
    WarningTruncate,
    ErrorForbidden
};

class Analyzer {
public:
    Analyzer(const Options& options, ContextBase& ctx) noexcept;

    [[nodiscard]] adv_result_t build();

private:
    void resolveSymbols();
    void resolveTypes();

    void visitChildrens(adv_node_t* node, std::function<void(adv_node_t*, adv_node_t*)> accept, bool useScopes = false);

    [[nodiscard]] adv_node_t* promoteNode(adv_node_t* node, vm_value_type_t targetType);
    [[nodiscard]] static bool isComparisonOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static CastResult getCastResult(vm_value_type_t from, vm_value_type_t to, bool isExplicitAs) noexcept;
    [[nodiscard]] static std::string_view getTypeString(vm_value_type_t type) noexcept;
    [[nodiscard]] vm_value_type_t findBaseType(vm_string_h type) const noexcept;

    const Options& _options;
    ContextBase& _ctx;
};

} // namespace adv

#endif // ADV_ANALYZER_HPP
