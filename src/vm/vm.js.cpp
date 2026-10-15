#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <fmt/format.h>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vm/vm.h>

using namespace emscripten;

EM_JS(void, throwJsError, (const char* message), { throw new Error(UTF8ToString(message)); });

EMSCRIPTEN_DECLARE_VAL_TYPE(String);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrBool);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrInt8);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrUint8);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrInt16);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrUint16);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrInt32);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrUint32);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrInt64);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrUint64);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrFloat32);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrFloat64);
EMSCRIPTEN_DECLARE_VAL_TYPE(ArrString);
EMSCRIPTEN_DECLARE_VAL_TYPE(NativeCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(ImportCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(LoggerCallback);

// clang-format off
template <typename> struct ArrItem;
template <> struct ArrItem<ArrBool> { using type = bool; static constexpr char typed[] = ""; };
template <> struct ArrItem<ArrInt8> { using type = vm_sint8_t; static constexpr char typed[] = "Int8Array"; };
template <> struct ArrItem<ArrUint8> { using type = vm_uint8_t; static constexpr char typed[] = "Uint8Array"; };
template <> struct ArrItem<ArrInt16> { using type = vm_sint16_t; static constexpr char typed[] = "Int16Array"; };
template <> struct ArrItem<ArrUint16> { using type = vm_uint16_t; static constexpr char typed[] = "Uint16Array"; };
template <> struct ArrItem<ArrInt32> { using type = vm_sint32_t; static constexpr char typed[] = "Int32Array"; };
template <> struct ArrItem<ArrUint32> { using type = vm_uint32_t; static constexpr char typed[] = "Uint32Array"; };
template <> struct ArrItem<ArrInt64> { using type = vm_sint64_t; static constexpr char typed[] = "BigInt64Array"; };
template <> struct ArrItem<ArrUint64> { using type = vm_uint64_t; static constexpr char typed[] = "BigUint64Array"; };
template <> struct ArrItem<ArrFloat32> { using type = vm_float32_t; static constexpr char typed[] = "Float32Array"; };
template <> struct ArrItem<ArrFloat64> { using type = vm_float64_t; static constexpr char typed[] = "Float64Array"; };
template <> struct ArrItem<ArrString> { using type = String; static constexpr char typed[] = ""; };
// clang-format on

template <typename, typename>
struct JsConverter;

template <typename T, typename S>
inline T jsconvert(const S& obj) {
    return JsConverter<T, S>::convert(obj);
}

template <typename T, typename S>
struct JsConverter<T, std::span<S>> {
    static T convert(std::span<S> obj) {
        using ItemT = typename ArrItem<T>::type;
        if constexpr (std::size(ArrItem<T>::typed) > 1) {
            val view{typed_memory_view(obj.size(), obj.data())};
            T arr(val::global(ArrItem<T>::typed).new_(obj.size()));
            arr.template call<void>("set", view);
            return arr;
        } else {
            T arr(val::array());
            for (auto& value : obj) {
                arr.template call<void>("push", jsconvert<ItemT>(value));
            }
            return arr;
        }
    }
};

template <>
struct JsConverter<String, std::string> {
    static String convert(const std::string& obj) { return String(val::u8string(obj.c_str())); }
};

template <>
struct JsConverter<String, std::span<const char>> {
    static String convert(std::span<const char> obj) {
        std::string str(obj.data(), obj.size());
        return String(val::u8string(str.c_str()));
    }
};

// this is for representing a byte array, not a string.
// the 'String' type is used to represent strings.
template <>
struct JsConverter<std::string, std::span<const char>> {
    static std::string convert(std::span<const char> obj) { return std::string(obj.data(), obj.size()); }
};

template <typename T, typename S>
struct JsConverter<std::optional<T>, S> {
    static std::optional<T> convert(const S obj) { return obj ? std::make_optional(jsconvert<T>(*obj)) : std::nullopt; }
};

template <typename T>
struct JsConverter<T, T> {
    static T convert(const T& obj) { return obj; }
};

template <>
struct JsConverter<String, vm_char_t> {
    static String convert(vm_char_t obj) {
        vm_char_t str[2] = {obj};
        return String(val::u8string(str));
    }
};

template <>
struct JsConverter<String, vm_utf8_t> {
    static String convert(vm_utf8_t obj) { return String(val::u8string(obj)); }
};

struct CContext {
    template <typename T>
    T* allocate() {
        const auto size = allocData<T>();
        auto& buffer = buffers.back();
        auto result = new (buffer.data.get() + buffer.offset) T();
        buffer.offset += size;
        return result;
    }

    template <typename T>
    T* allocateArray(size_t count) {
        const auto size = allocData<T>(count);
        auto& buffer = buffers.back();
        auto result = new (buffer.data.get() + buffer.offset) T[count]();
        buffer.offset += size;
        return result;
    }

    template <typename T>
    size_t allocData(size_t count = 1) {
        static_assert(
            std::is_trivially_default_constructible_v<T> && std::is_trivially_copyable_v<T>, "T is not trivial type");
        constexpr auto mask = 7;
        auto size = (sizeof(T) * count + mask) & ~mask;
        if (buffers.empty() || buffers.back().offset + size > buffers.back().size) {
            buffers.push_back({});
            auto allocSize = std::max(size, Buffer::defaultSize);
            buffers.back().size = allocSize;
            buffers.back().data = std::unique_ptr<char[]>(new char[allocSize]);
        }
        return size;
    }

    struct Buffer {
        static constexpr size_t defaultSize = 1024;
        size_t offset{};
        size_t size{};
        std::unique_ptr<char[]> data{};
    };

    std::list<Buffer> buffers;
};

template <typename>
struct ArrSize {};

template <typename T, typename S>
struct CConverter {
    static T* convert(CContext& ctx, S& obj) {
        if constexpr (std::is_integral_v<T>) {
            auto vec = convertJSArrayToNumberVector<T>(obj);
            auto result = ctx.allocateArray<T>(vec.size());
            memcpy(result, vec.data(), sizeof(T) * vec.size());
            return result;
        } else if constexpr (std::is_same_v<T, typename ArrItem<S>::type>) {
            using ItemS = typename ArrItem<S>::type;
            auto vec = vecFromJSArray<ItemS>(obj);
            auto result = ctx.allocateArray<T>(vec.size());
            memcpy(result, vec.data(), sizeof(T) * vec.size());
            return result;
        } else {
            using ItemS = typename ArrItem<S>::type;
            auto vec = vecFromJSArray<ItemS>(obj);
            auto result = ctx.allocateArray<T>(vec.size());
            for (size_t i = 0; i < vec.size(); ++i) {
                auto value = CConverter<T, ItemS>::convert(ctx, vec[i]);
                if constexpr (std::is_same_v<T, vm_utf8_t>) {
                    result[i] = value;
                } else {
                    result[i] = *value;
                }
            }
            return result;
        }
    }
};

template <typename T, typename S>
struct CConverter<ArrSize<T>, S> {
    static T* convert(CContext& ctx, S& obj) {
        auto result = ctx.allocate<T>();
        *result = obj["length"].template as<T>();
        return result;
    }
};

template <typename T>
struct CConverter<ArrSize<T>, String> {
    static T* convert(CContext& ctx, const String& obj) {
        auto result = ctx.allocate<T>();
        *result = obj["length"].template as<T>();
        return result;
    }
};

// this is for representing a byte array, not a string.
// the 'String' type is used to represent strings.
template <typename T>
struct CConverter<ArrSize<T>, std::string> {
    static T* convert(CContext& ctx, const std::string& obj) {
        auto result = ctx.allocate<T>();
        *result = (T)obj.length();
        return result;
    }
};

template <typename T, typename S>
inline auto cconvert(CContext& ctx, const S& obj) {
    return CConverter<T, S>::convert(ctx, const_cast<S&>(obj));
}

template <typename T>
struct CConverter<T, T> {
    static T* convert(CContext& ctx, T& obj) { return &obj; }
};

template <typename T, typename S>
struct CConverter<T, std::optional<S>> {
    static auto convert(CContext& ctx, std::optional<S> obj) { return obj ? cconvert<T>(ctx, obj.value()) : nullptr; }
};

template <>
struct CConverter<vm_utf8_t, String> {
    static vm_utf8_t convert(CContext& ctx, String& obj) {
        auto str = obj.as<std::string>();
        auto result = ctx.allocateArray<char>(str.length() + 1);
        memcpy(result, str.c_str(), str.length());
        result[str.length()] = '\0';
        return result;
    }
};

template <>
struct CConverter<vm_char_t, String> {
    static vm_char_t* convert(CContext& ctx, String& obj) {
        auto str = obj.as<std::string>();
        auto result = ctx.allocateArray<char>(str.length() + 1);
        memcpy(result, str.c_str(), str.length());
        result[str.length()] = '\0';
        return result;
    }
};

// this is for representing a byte array, not a string.
// the 'String' type is used to represent strings.
template <>
struct CConverter<vm_data_t, std::string> {
    static vm_data_t convert(CContext& ctx, std::string& obj) {
        auto result = ctx.allocateArray<char>(obj.length() + 1);
        memcpy(result, obj.c_str(), obj.length());
        result[obj.length()] = '\0';
        return (vm_data_t)result;
    }
};

constexpr vm_uint32_t invalidHandle = std::numeric_limits<vm_uint32_t>::max();

std::vector<vm_uint8_t> readByteBuffer(const val& input) {
    if (input.isNull() || input.isUndefined()) {
        throw std::invalid_argument("Expected a Uint8Array, ArrayBuffer, or byte array");
    }

    const auto inputLength = input["length"];
    const auto byteLength = input["byteLength"];
    if (inputLength.isUndefined() && byteLength.isUndefined()) {
        throw std::invalid_argument("Expected a Uint8Array, ArrayBuffer, or byte array");
    }

    const auto sourceLength = inputLength.isUndefined() ? byteLength.as<double>() : inputLength.as<double>();
    if (!std::isfinite(sourceLength) || sourceLength < 0 || std::trunc(sourceLength) != sourceLength) {
        throw std::invalid_argument("VM script byte buffer length must be a non-negative integer");
    }
    if (sourceLength > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("VM script byte buffer is too large");
    }

    const auto bytes = val::global("Uint8Array").new_(input);
    return emscripten::convertJSArrayToNumberVector<vm_uint8_t>(bytes);
}

bool isFunction(const val& value) { return val::global("Function")["prototype"].call<bool>("isPrototypeOf", value); }

val toJsValue(vm_t vm, const vm_value_t& value) {
    switch (value.type) {
    case VM_VALUE_TYPE_BOOL:
        return val(value.b != 0);
    case VM_VALUE_TYPE_INT:
        return val(value.i32);
    case VM_VALUE_TYPE_FLOAT:
        return val(value.f32);
    case VM_VALUE_TYPE_STRING:
        return val::u8string(vm_get_string(vm, value.str, nullptr));
    case VM_VALUE_TYPE_UNKNOWN:
    default:
        return val::undefined();
    }
}

static void check(vm_result_t result) {
    if (result != VM_RESULT_SUCCESS) {
        const auto message = vm_result_to_string(result);
        throwJsError(message);
    }
}

vm_value_t fromJsValue(vm_t vm, const val& value) {
    if (value.isUndefined()) {
        return {};
    }
    if (value.isTrue() || value.isFalse()) {
        return vm_value_t{.type = VM_VALUE_TYPE_BOOL, .b = value.as<bool>()};
    }
    if (value.isNumber()) {
        const auto number = value.as<double>();
        if (std::isfinite(number) && std::trunc(number) == number &&
            number >= std::numeric_limits<vm_sint32_t>::min() && number <= std::numeric_limits<vm_sint32_t>::max()) {
            return vm_value_t{.type = VM_VALUE_TYPE_INT, .i32 = static_cast<vm_sint32_t>(number)};
        }
        return vm_value_t{.type = VM_VALUE_TYPE_FLOAT, .f32 = static_cast<vm_float32_t>(number)};
    }
    if (value.isString()) {
        const auto text = value.as<std::string>();
        const auto string = vm_intern_string(vm, text.c_str());
        if (string.handle == invalidHandle) {
            throw std::runtime_error("Failed to intern VM string");
        }
        return vm_value_t{.type = VM_VALUE_TYPE_STRING, .str = string};
    }
    throw std::invalid_argument("Native callbacks must return a boolean, number, string, or undefined");
}

class VM {
public:
    VM() noexcept { check(vm_create(&_vm)); }

    VM(vm_t vm) noexcept : _vm(vm) {}

    ~VM() {
        vm_set_logger(_vm, nullptr, nullptr);
        vm_set_importer(_vm, nullptr, nullptr);
        vm_set_release_import(_vm, nullptr, nullptr);
        vm_destroy(_vm);
    }

    void tick(vm_uint32_t elapsedMs) { vm_tick(_vm, elapsedMs); }

    std::optional<vm_script_h> loadScriptFromFile(const String& file) {
        CContext ctx;
        auto argFile = cconvert<vm_utf8_t>(ctx, file);
        auto result = vm_load_script_from_file(_vm, argFile);
        return result.handle != 0 ? std::make_optional(result) : std::nullopt;
    }

    std::optional<vm_script_h> loadScriptFromMemory(const std::string& code) {
        auto result = vm_load_script_from_memory(_vm, code.data(), code.size());
        return result.handle != 0 ? std::make_optional(result) : std::nullopt;
    }

    std::optional<vm_thread_h> runThread(vm_script_h script) {
        auto result = vm_run_thread(_vm, script);
        return result.handle != 0 ? std::make_optional(result) : std::nullopt;
    }

    void shutdownThread(vm_thread_h thread) { vm_shutdown_thread(_vm, thread); }

    std::optional<vm_native_h> registerNative(const String& name, NativeCallback callback) {
        CContext ctx;
        auto argName = cconvert<vm_utf8_t>(ctx, name);
        if (callback.isNull() || callback.isUndefined()) {
            vm_register_native(_vm, argName, nullptr, nullptr);
            return std::nullopt;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Native callback must be a function");
        }
        auto holder = std::make_unique<NativeCallback>(std::move(callback));
        const auto native = vm_register_native(_vm, argName, &VM::nativeCallback, holder.get());
        if (native.handle == invalidHandle) {
            throw std::runtime_error("Failed to register VM native function");
        }
        _nativeCallbacks.insert_or_assign(native.handle, std::move(holder));
        return native;
    }

    void unregisterNative(vm_native_h native) {
        vm_unregister_native(_vm, native);
        _nativeCallbacks.erase(native.handle);
    }

    ImportCallback getImporter() const { return ImportCallback(val(0)); }

    void setImporter(ImportCallback callback) {
        if (callback.isNull() || callback.isUndefined()) {
            _importer = ImportCallback(val::undefined());
            vm_set_importer(_vm, nullptr, nullptr);
            vm_set_release_import(_vm, nullptr, nullptr);
            return;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Importer must be a function or undefined");
        }
        _importer = std::move(callback);
        vm_set_importer(_vm, &VM::importCallback, this);
        vm_set_release_import(_vm, &VM::releaseImportCallbackT, this);
    }

    LoggerCallback getLogger() const { return _logger; }

    void setLogger(LoggerCallback callback) {
        if (callback.isNull() || callback.isUndefined()) {
            _logger = LoggerCallback(val::undefined());
            vm_set_logger(_vm, nullptr, nullptr);
            return;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Logger must be a function or undefined");
        }
        _logger = std::move(callback);
        vm_set_logger(_vm, &VM::loggerCallback, this);
    }

private:
    static void nativeCallback(vm_t vm, vm_thread_h thread, vm_native_h native, vm_uint16_t argCount, vm_data_t data) {
        const auto& callback = *static_cast<val*>(data);
        auto args = val::array();
        args.call<void>("push", thread);
        args.call<void>("push", native);
        for (vm_uint16_t i = 0; i < argCount; ++i) {
            args.call<void>("push", toJsValue(vm, vm_thread_get_arg(vm, thread, i)));
        }
        const auto result = val::global("Reflect").call<val>("apply", callback, val::undefined(), args);
        if (!result.isUndefined()) {
            const auto value = fromJsValue(vm, result);
            vm_thread_set_result(vm, thread, 0, &value);
        }
    }

    static vm_bytecode_t* importCallback(vm_t /* vm */, vm_utf8_t module, vm_data_t data) {
        const auto& self = *static_cast<VM*>(data);
        const auto argModule = jsconvert<String>(module);
        auto result = self._importer(argModule);
        if (result.isNull() || result.isUndefined()) {
            return nullptr;
        }
        const auto str = result.as<std::string>();
        char* buffer = new char[str.size()];
        memcpy(buffer, str.data(), str.size());
        return new vm_bytecode_t{buffer, static_cast<vm_uint32_t>(str.size())};
    }

    static void releaseImportCallbackT(vm_t /* vm */, vm_bytecode_t* bytecode, vm_data_t /* data */) {
        if (bytecode) {
            delete[] bytecode->data;
            delete bytecode;
        }
    }

    static void
    loggerCallback(vm_t vm, vm_thread_h thread, vm_uint32_t instructionPos, vm_utf8_t message, vm_data_t data) {
        const auto& self = *static_cast<VM*>(data);
        auto arg0 = val::object();
        arg0.set("handle", thread.handle);
        auto str = jsconvert<String>(message);
        self._logger(thread, instructionPos, str);
    }

    vm_t _vm{};
    std::unordered_map<vm_uint32_t, std::unique_ptr<NativeCallback>> _nativeCallbacks;
    ImportCallback _importer{val::undefined()};
    LoggerCallback _logger{val::undefined()};
};
template <>
struct JsConverter<VM, vm_t> {
    static VM convert(vm_t& obj) { return VM(obj); }
};

// template <>
// struct CConverter<vm_t, VM> {
//     static vm_t convert(CContext& ctx, VM& obj) {
//         return obj.handle();
//     }
// };

static String version() {
    const auto version = vm_version();
    const auto major = (version >> 24) & 0xFF;
    const auto minor = (version >> 16) & 0xFF;
    const auto micro = (version >> 0) & 0xFFFF;
    const auto str = fmt::format("{}.{}.{}", major, minor, micro);
    return jsconvert<String>(str);
}

EMSCRIPTEN_BINDINGS(adv_vm) {
    register_type<String>("string");
    register_type<ArrBool>("boolean[]");
    register_type<ArrInt8>("Int8Array");
    register_type<ArrUint8>("Uint8Array");
    register_type<ArrInt16>("Int16Array");
    register_type<ArrUint16>("Uint16Array");
    register_type<ArrInt32>("Int32Array");
    register_type<ArrUint32>("Uint32Array");
    register_type<ArrInt64>("BigInt64Array");
    register_type<ArrUint64>("BigUint64Array");
    register_type<ArrFloat32>("Float32Array");
    register_type<ArrFloat64>("Float64Array");
    register_type<ArrString>("string[]");
    register_type<NativeCallback>(
        "NativeCallback",
        "(thread: Thread, native: Native, ...args: (number | boolean | string | undefined)[]) => void");
    register_type<ImportCallback>("ImportCallback", "(module: string) => EmbindString");
    register_type<LoggerCallback>(
        "LoggerCallback", "(thread: Thread, instructionPos: number, message: string) => void");

    register_optional<vm_script_h>();
    register_optional<vm_thread_h>();
    register_optional<vm_native_h>();

    function("version", &version);

    enum_<vm_opcode_t>("Opcode")
        .value("NO_OPERATION", VM_OPCODE_NO_OPERATION)
        .value("IMPORT", VM_OPCODE_IMPORT)
        .value("PUSH_TRUE", VM_OPCODE_PUSH_TRUE)
        .value("PUSH_FALSE", VM_OPCODE_PUSH_FALSE)
        .value("PUSH_INT", VM_OPCODE_PUSH_INT)
        .value("PUSH_FLOAT", VM_OPCODE_PUSH_FLOAT)
        .value("PUSH_STRING_FROM_DATA", VM_OPCODE_PUSH_STRING_FROM_DATA)
        .value("PUSH_STRING", VM_OPCODE_PUSH_STRING)
        .value("PUSH_GLOBAL_BY_NAME", VM_OPCODE_PUSH_GLOBAL_BY_NAME)
        .value("PUSH_GLOBAL", VM_OPCODE_PUSH_GLOBAL)
        .value("PUSH_LOCAL", VM_OPCODE_PUSH_LOCAL)
        .value("POP_GLOBAL_BY_NAME", VM_OPCODE_POP_GLOBAL_BY_NAME)
        .value("POP_GLOBAL", VM_OPCODE_POP_GLOBAL)
        .value("POP_LOCAL", VM_OPCODE_POP_LOCAL)
        .value("POP", VM_OPCODE_POP)
        .value("BOOL_TO_INT", VM_OPCODE_BOOL_TO_INT)
        .value("BOOL_TO_FLOAT", VM_OPCODE_BOOL_TO_FLOAT)
        .value("BOOL_TO_STRING", VM_OPCODE_BOOL_TO_STRING)
        .value("INT_TO_BOOL", VM_OPCODE_INT_TO_BOOL)
        .value("INT_TO_FLOAT", VM_OPCODE_INT_TO_FLOAT)
        .value("INT_TO_STRING", VM_OPCODE_INT_TO_STRING)
        .value("FLOAT_TO_BOOL", VM_OPCODE_FLOAT_TO_BOOL)
        .value("FLOAT_TO_INT", VM_OPCODE_FLOAT_TO_INT)
        .value("FLOAT_TO_STRING", VM_OPCODE_FLOAT_TO_STRING)
        .value("STRING_TO_BOOL", VM_OPCODE_STRING_TO_BOOL)
        .value("STRING_TO_INT", VM_OPCODE_STRING_TO_INT)
        .value("STRING_TO_FLOAT", VM_OPCODE_STRING_TO_FLOAT)
        .value("BITWISE_NOT", VM_OPCODE_BITWISE_NOT)
        .value("LOGICAL_NOT", VM_OPCODE_LOGICAL_NOT)
        .value("NUMERIC_NEGATION", VM_OPCODE_NUMERIC_NEGATION)
        .value("BITWISE_AND", VM_OPCODE_BITWISE_AND)
        .value("BITWISE_OR", VM_OPCODE_BITWISE_OR)
        .value("BITWISE_XOR", VM_OPCODE_BITWISE_XOR)
        .value("LOGIC_SHIFT_LEFT", VM_OPCODE_LOGIC_SHIFT_LEFT)
        .value("LOGIC_SHIFT_RIGHT", VM_OPCODE_LOGIC_SHIFT_RIGHT)
        .value("ARITHMETIC_SHIFT_LEFT", VM_OPCODE_ARITHMETIC_SHIFT_LEFT)
        .value("ARITHMETIC_SHIFT_RIGHT", VM_OPCODE_ARITHMETIC_SHIFT_RIGHT)
        .value("ADD", VM_OPCODE_ADD)
        .value("SUBTRACT", VM_OPCODE_SUBTRACT)
        .value("MULTIPLY", VM_OPCODE_MULTIPLY)
        .value("DIVISION", VM_OPCODE_DIVISION)
        .value("MOD", VM_OPCODE_MOD)
        .value("EQUAL", VM_OPCODE_EQUAL)
        .value("NOT_EQUAL", VM_OPCODE_NOT_EQUAL)
        .value("LESS", VM_OPCODE_LESS)
        .value("LESS_EQUAL", VM_OPCODE_LESS_EQUAL)
        .value("GREAT", VM_OPCODE_GREAT)
        .value("GREAT_EQUAL", VM_OPCODE_GREAT_EQUAL)
        .value("CALL_NATIVE_BY_NAME", VM_OPCODE_CALL_NATIVE_BY_NAME)
        .value("CALL_NATIVE", VM_OPCODE_CALL_NATIVE)
        .value("CALL_BY_NAME", VM_OPCODE_CALL_BY_NAME)
        .value("CALL", VM_OPCODE_CALL)
        .value("RETURN", VM_OPCODE_RETURN)
        .value("JUMP", VM_OPCODE_JUMP)
        .value("JUMP_IF_FALSE", VM_OPCODE_JUMP_IF_FALSE)
        .value("YIELD", VM_OPCODE_YIELD)
        .value("WAIT", VM_OPCODE_WAIT)
        .value("HALT", VM_OPCODE_HALT);

    enum_<vm_value_type_t>("ValueType")
        .value("UNKNOWN", VM_VALUE_TYPE_UNKNOWN)
        .value("BOOL", VM_VALUE_TYPE_BOOL)
        .value("INT", VM_VALUE_TYPE_INT)
        .value("FLOAT", VM_VALUE_TYPE_FLOAT)
        .value("STRING", VM_VALUE_TYPE_STRING);

    value_object<vm_script_h>("Script").field("handle", &vm_script_h::handle);
    value_object<vm_thread_h>("Thread").field("handle", &vm_thread_h::handle);
    value_object<vm_native_h>("Native").field("handle", &vm_native_h::handle);

    class_<VM>("VM")
        .constructor<>()
        .function("tick(elapsedMs)", &VM::tick)
        .function("loadScriptFromFile(file)", &VM::loadScriptFromFile)
        .function("loadScriptFromMemory(code)", &VM::loadScriptFromMemory)
        .function("runThread(script)", &VM::runThread)
        .function("shutdownThread(thread)", &VM::shutdownThread)
        .function("registerNative(name, func)", &VM::registerNative)
        .function("unregisterNative(native)", &VM::unregisterNative)
        .property("importer", &VM::getImporter, &VM::setImporter)
        .property("logger", &VM::getLogger, &VM::setLogger);
}
