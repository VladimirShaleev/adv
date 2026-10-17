#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <adv/adv.h>

using namespace emscripten;

EMSCRIPTEN_DECLARE_VAL_TYPE(String);
EMSCRIPTEN_DECLARE_VAL_TYPE(Data);
EMSCRIPTEN_DECLARE_VAL_TYPE(ImportCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(WriteCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrMessage);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrSource);

[[noreturn]] static void throwError(const char* err, const char* message) {
    const auto str = String(val::u8string(message));
    val jsError = val::global(err);
    val error_instance = jsError.new_(str);
    error_instance.throw_();
}

[[noreturn]] static void throwReadOnly() { throwError("TypeError", "Property is read-only"); }

static void check(adv_result_t result) {
    if (result != ADV_RESULT_SUCCESS) {
        const auto message = adv_result_to_string(result);
        switch (result) {
        case ADV_RESULT_COMPILATION_FAILED:
            throwError("SyntaxError", message);
        case ADV_RESULT_ERROR_OUT_OF_MEMORY:
            throwError("RangeError", message);
        case ADV_RESULT_ERROR_INVALID_ARG:
        case ADV_RESULT_ERROR_SOURCE_NOT_FOUND:
            throwError("TypeError", message);
        default:
            throwError("Error", message);
        }
    }
}

[[nodiscard]] static bool isFunction(const val& value) noexcept {
    return val::global("Function")["prototype"].call<bool>("isPrototypeOf", value);
}

[[nodiscard]] static std::vector<vm_uint8_t> readByteBuffer(const val& input) {
    if (input.isNull() || input.isUndefined()) {
        throwError("TypeError", "Input cannot be null or undefined");
    }
    val bytes = val::undefined();
    if (input.isString()) {
        val encoder = val::global("TextEncoder").new_();
        bytes = encoder.call<val>("encode", input);
    } else if (input.instanceof(val::global("ArrayBuffer"))) {
        bytes = val::global("Uint8Array").new_(input);
    } else if (!input["buffer"].isUndefined() && input["byteLength"].isNumber()) {
        bytes = val::global("Uint8Array").new_(input["buffer"], input["byteOffset"], input["byteLength"]);
    } else {
        throwError("TypeError", "Expected String, ArrayBuffer, or TypedArray");
    }
    double rawLength = bytes["length"].as<double>();
    if (!std::isfinite(rawLength) || rawLength < 0) {
        throwError("TypeError", "Invalid byte buffer length");
    }
    const auto length = static_cast<size_t>(rawLength);
    if (length == 0) {
        return std::vector<vm_uint8_t>();
    }
    return emscripten::vecFromJSArray<vm_uint8_t>(bytes);
}

struct Source {
    String name{val::undefined()};
    Data data{val::undefined()};
};

class CompilationResult {
public:
    explicit CompilationResult(adv_compilation_result_t result) noexcept : _result(result) {}

    CompilationResult(const CompilationResult& result) noexcept
        : _result(adv_compilation_result_reference(result._result)) {}

    CompilationResult(CompilationResult&& result) noexcept : _result(result._result) { result._result = nullptr; }

    ~CompilationResult() { adv_compilation_result_destroy(_result); }

    [[nodiscard]] bool hasNotes() const noexcept { return !!adv_compilation_result_has_notes(_result); }
    [[nodiscard]] bool hasWarnings() const noexcept { return !!adv_compilation_result_has_warnings(_result); }
    [[nodiscard]] bool hasErrors() const noexcept { return !!adv_compilation_result_has_errors(_result); }
    [[nodiscard]] ArrMessage getMessages() const {
        std::vector<adv_message_t> messages{};
        adv_uint32_t count{};
        adv_compilation_result_get_messages(_result, &count, nullptr);
        messages.resize(count);
        adv_compilation_result_get_messages(_result, &count, messages.data());
        ArrMessage arr(val::array());
        for (const auto& message : messages) {
            arr.call<void>("push", message);
        }
        return arr;
    }

private:
    adv_compilation_result_t _result{};
};

class Options {
public:
    Options() { check(adv_options_create(&_options)); }

    ~Options() { adv_options_destroy(_options); }

    [[nodiscard]] bool getWarningsAsErrors() const noexcept { return !!adv_options_get_warnings_as_errors(_options); }

    void setWarningsAsErrors(bool enable) noexcept { adv_options_set_warnings_as_errors(_options, enable ? 1 : 0); }

    [[nodiscard]] adv_format_t getOutputFormat() const noexcept { return adv_options_get_output_format(_options); }

    void setOutputFormat(adv_format_t format) noexcept { adv_options_set_output_format(_options, format); }

    [[nodiscard]] ImportCallback getImporter() const noexcept { return _importer; }

    void setImporter(ImportCallback callback) {
        if (callback.isNull() || callback.isUndefined()) {
            _importer = ImportCallback(val::undefined());
            adv_options_set_importer(_options, nullptr, nullptr);
            adv_options_set_release_import(_options, nullptr, nullptr);
            return;
        }
        if (!isFunction(callback)) {
            throwError("TypeError", "Callback must be a function");
        }
        _importer = std::move(callback);
        adv_options_set_importer(_options, &Options::importCallback, this);
        adv_options_set_release_import(_options, &Options::releaseImportCallbackT, this);
    }

    [[nodiscard]] adv_options_t handle() noexcept { return _options; }

private:
    [[nodiscard]] static adv_source_t* importCallback(adv_utf8_t name, adv_uint32_t depth, adv_data_t data) {
        const auto& self = *static_cast<Options*>(data);
        const auto argName = String(val::u8string(name));
        auto result = self._importer(argName, depth);
        if (result.isNull() || result.isUndefined()) {
            return nullptr;
        }
        const auto source = result.as<Source>();
        // const auto buffer = readByteBuffer(result);
        // auto bytecode = new vm_char_t[buffer.size()];
        // memcpy(bytecode, buffer.data(), buffer.size());
        // return new vm_bytecode_t{bytecode, static_cast<vm_uint32_t>(buffer.size())};
        return nullptr;
    }

    static void releaseImportCallbackT(adv_source_t* source, adv_data_t data) {}

    adv_options_t _options{};
    ImportCallback _importer{val::undefined()};
};

class Compiler {
public:
    Compiler() { check(adv_compiler_create(&_compiler)); }

    ~Compiler() { adv_compiler_destroy(_compiler); }

    CompilationResult
    compile(std::optional<String> file, std::optional<ArrSource> sources, std::optional<Options> options) {
        if (!file && !sources && !options) {
            throwError("TypeError", "No sources were specified for compilation");
        }
        std::vector<std::string> strings;
        std::vector<std::vector<adv_uint8_t>> buffers;
        std::vector<adv_source_t> cSources;
        if (sources && !sources->isUndefined() && !sources->isNull()) {
            const auto length = sources.value()["length"].as<adv_sint32_t>();
            if (length > 0) {
                strings.reserve(length);
                buffers.reserve(length);
                cSources.reserve(length);
                for (adv_sint32_t i = 0; i < length; ++i) {
                    val source = sources.value()[i];
                    strings.push_back(source["name"].as<std::string>());
                    buffers.push_back(readByteBuffer(source["data"]));
                    cSources.emplace_back(
                        nullptr,
                        (const adv_char_t*)buffers.back().data(),
                        static_cast<adv_uint32_t>(buffers.back().size()));
                }
                for (size_t i = 0; i < cSources.size(); ++i) {
                    cSources[i].name = strings[i].c_str();
                }
            }
        }
        std::string name{};
        vm_utf8_t cFile{};
        adv_options_t opts{};
        if (file && !file->isUndefined() && !file->isNull()) {
            name = file.value().as<std::string>();
            cFile = name.c_str();
        }
        if (options) {
            opts = options.value().handle();
        }
        adv_compilation_result_t result{};
        const auto code = adv_compiler_compile(
            _compiler, cFile, static_cast<adv_uint32_t>(cSources.size()), cSources.data(), opts, &result);
        if (result == nullptr && code != ADV_RESULT_SUCCESS) {
            check(code);
        }
        return CompilationResult(result);
    }

private:
    adv_compiler_t _compiler{};
};

EMSCRIPTEN_BINDINGS(adv) {
    register_type<String>("string");
    register_type<Data>("Data", "ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string");
    register_type<ImportCallback>("ImportCallback", "(name: string, depth: number) => Source");
    register_type<ArrMessage>("Message[]");
    register_type<ArrSource>("Source[]");

    register_optional<String>();
    register_optional<ArrSource>();
    register_optional<Options>();

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

    enum_<adv_format_t>("Format").value("BINARY", ADV_FORMAT_BINARY).value("TEXT", ADV_FORMAT_TEXT);

    value_object<adv_message_t>("Message")
        .field(
            "status",
            +[](const adv_message_t& m) { return m.status; },
            +[](adv_message_t& m, adv_status_t /* status */) { throwReadOnly(); })
        .field(
            "isError",
            +[](const adv_message_t& m) { return !!m.is_error; },
            +[](adv_message_t& m, bool /* isError */) { throwReadOnly(); })
        .field(
            "message",
            +[](const adv_message_t& m) { return String(val::u8string(m.message)); },
            +[](adv_message_t& m, const String& /* str */) { throwReadOnly(); })
        .field(
            "filename",
            +[](const adv_message_t& m) { return String(val::u8string(m.filename)); },
            +[](adv_message_t& m, const String& /* str */) { throwReadOnly(); })
        .field(
            "line",
            +[](const adv_message_t& m) { return m.line; },
            +[](adv_message_t& m, const adv_uint32_t& /* line */) { throwReadOnly(); })
        .field(
            "column",
            +[](const adv_message_t& m) { return m.column; },
            +[](adv_message_t& m, const adv_uint32_t& /* column */) { throwReadOnly(); });

    value_object<Source>("Source").field("name", &Source::name).field("size", &Source::data);

    class_<CompilationResult>("CompilationResult")
        .property("hasNotes", &CompilationResult::hasNotes)
        .property("hasWarnings", &CompilationResult::hasWarnings)
        .property("hasErrors", &CompilationResult::hasErrors)
        .property("messages", &CompilationResult::getMessages)
        // .property("rootNode", ...)
        // .property("globalScope", ...)
        // .property("globalScope", ...)
        // .function("lookupSymbol(scope, name)", ...)
        // .function("getVisibleSymbolsAt(filename, line, column)", ...)
        // .function("getScope(scope)", ...)
        // .function("getSymbol(symbol)", ...)
        ;

    class_<Options>("Options")
        .constructor<>()
        .property("warningsAsErrors", &Options::getWarningsAsErrors, &Options::setWarningsAsErrors)
        .property("outputFormat", &Options::getOutputFormat, &Options::setOutputFormat)
        .property("importer", &Options::getImporter, &Options::setImporter)
        // .property("writer", )
        ;

    class_<Compiler>("Compiler").constructor<>().function("compile(file, sources, options)", &Compiler::compile);
}
