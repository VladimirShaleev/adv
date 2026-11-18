#ifndef ADV_ANALYZER_HPP
#define ADV_ANALYZER_HPP

#include "compiler.hpp"

namespace adv {

class Options;
class Context;

enum class CastResult {
    OK,
    WarningTruncate,
    ErrorForbidden
};

class Analyzer {
public:
    Analyzer(const Options& options, Context& ctx) noexcept;

    [[nodiscard]] adv_result_t build();

private:
    void resolveSymbols();
    void resolveTypes();

    void
    visitStmtsWithRules(adv_node_t* node, std::function<void(adv_node_t*, adv_node_t*)> accept, bool useScopes = false);

    struct FunctionFrameState {
        adv_uint16_t currentSlot;
        adv_uint16_t maxSlots;
    };

    void pushFrame();
    void popFrame() noexcept;
    void saveFrameSlot(class Symbol* sym) noexcept;
    [[nodiscard]] FunctionFrameState& currFrame() noexcept;

    [[nodiscard]] adv_node_t* promoteCastNode(adv_node_t* node, adv_symbol_h targetType);
    [[nodiscard]] CastResult getCastResult(adv_symbol_h from, adv_symbol_h to, bool isExplicitAs) noexcept;
    [[nodiscard]] adv_symbol_h findBaseType(adv_scope_h scope, vm_string_h type) const noexcept;
    [[nodiscard]] static bool isEqualityOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static bool isOrderingOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static bool isComparisonOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static bool isArithmeticOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static bool isBitwiseOp(adv_uint32_t op) noexcept;
    [[nodiscard]] static bool isLogicOp(adv_uint32_t op) noexcept;

    const Options& _options;
    Context& _ctx;
    std::stack<FunctionFrameState> _frameStack{};
};

} // namespace adv

#endif // ADV_ANALYZER_HPP
