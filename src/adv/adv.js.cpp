#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <adv/adv.h>

using namespace emscripten;

EMSCRIPTEN_DECLARE_VAL_TYPE(String);
EMSCRIPTEN_DECLARE_VAL_TYPE(Bytecode);

class CompilationResult {
public:
    explicit CompilationResult(adv_compilation_result_t result) noexcept : _result(result) {}

    ~CompilationResult() { adv_compilation_result_destroy(_result); }

    [[nodiscard]] bool hasNotes() const noexcept { return !!adv_compilation_result_has_notes(_result); }
    [[nodiscard]] bool hasWarnings() const noexcept { return !!adv_compilation_result_has_warnings(_result); }
    [[nodiscard]] bool hasErrors() const noexcept { return !!adv_compilation_result_has_errors(_result); }

private:
    adv_compilation_result_t _result{};
};

EMSCRIPTEN_BINDINGS(adv) {
    register_type<String>("string");
    register_type<Bytecode>("Bytecode", "ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string");

    enum_<adv_status_t>("Status")
        .value("N1001", ADV_STATUS_N1001)
        .value("W2001", ADV_STATUS_W2001)
        .value("W2002", ADV_STATUS_W2002)
        .value("W2003", ADV_STATUS_W2003)
        .value("W2004", ADV_STATUS_W2004)
        .value("W2005", ADV_STATUS_W2005)
        .value("W2006", ADV_STATUS_W2006)
        .value("E3001", ADV_STATUS_E3001)
        .value("E3002", ADV_STATUS_E3002)
        .value("E3003", ADV_STATUS_E3003)
        .value("E3004", ADV_STATUS_E3004)
        .value("E3005", ADV_STATUS_E3005)
        .value("E3006", ADV_STATUS_E3006)
        .value("E3007", ADV_STATUS_E3007)
        .value("E3008", ADV_STATUS_E3008)
        .value("E3009", ADV_STATUS_E3009)
        .value("E3010", ADV_STATUS_E3010)
        .value("E3011", ADV_STATUS_E3011)
        .value("E3012", ADV_STATUS_E3012)
        .value("E3013", ADV_STATUS_E3013)
        .value("E3014", ADV_STATUS_E3014)
        .value("E3015", ADV_STATUS_E3015)
        .value("E3016", ADV_STATUS_E3016)
        .value("E3017", ADV_STATUS_E3017)
        .value("E3018", ADV_STATUS_E3018)
        .value("E3019", ADV_STATUS_E3019)
        .value("E3020", ADV_STATUS_E3020)
        .value("E3021", ADV_STATUS_E3021)
        .value("E3022", ADV_STATUS_E3022)
        .value("E3023", ADV_STATUS_E3023)
        .value("E3024", ADV_STATUS_E3024)
        .value("E3025", ADV_STATUS_E3025)
        .value("E3026", ADV_STATUS_E3026)
        .value("E3027", ADV_STATUS_E3027)
        .value("E3028", ADV_STATUS_E3028)
        .value("E3029", ADV_STATUS_E3029)
        .value("E3030", ADV_STATUS_E3030)
        .value("E3031", ADV_STATUS_E3031)
        .value("E3032", ADV_STATUS_E3032)
        .value("E3033", ADV_STATUS_E3033)
        .value("E3034", ADV_STATUS_E3034)
        .value("E3035", ADV_STATUS_E3035)
        .value("E3036", ADV_STATUS_E3036)
        .value("E3037", ADV_STATUS_E3037)
        .value("E3038", ADV_STATUS_E3038)
        .value("E3039", ADV_STATUS_E3039)
        .value("E3040", ADV_STATUS_E3040)
        .value("E3041", ADV_STATUS_E3041)
        .value("E3042", ADV_STATUS_E3042)
        .value("E3043", ADV_STATUS_E3043)
        .value("E3044", ADV_STATUS_E3044)
        .value("E3045", ADV_STATUS_E3045);

    value_object<adv_message_t>("Message")
        .field("status", &adv_message_t::status)
        .field(
            "isError",
            +[](const adv_message_t& m) -> bool { return !!m.is_error; },
            +[](adv_message_t& m, bool err) { m.is_error = err ? 1 : 0; });

    class_<CompilationResult>("CompilationResult")
        .property("hasNotes", &CompilationResult::hasNotes)
        .property("hasWarnings", &CompilationResult::hasWarnings)
        .property("hasErrors", &CompilationResult::hasErrors);
}
