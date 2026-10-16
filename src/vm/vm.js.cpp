#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <fmt/format.h>
#include <list>
#include <optional>
#include <string>
#include <vector>

#include <vm/vm.h>

using namespace emscripten;

EMSCRIPTEN_DECLARE_VAL_TYPE(String);
EMSCRIPTEN_DECLARE_VAL_TYPE(Bytecode);
EMSCRIPTEN_DECLARE_VAL_TYPE(NativeCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(ImportCallback);
EMSCRIPTEN_DECLARE_VAL_TYPE(LoggerCallback);

static String version() {
    const auto version = vm_version();
    const auto major = (version >> 24) & 0xFF;
    const auto minor = (version >> 16) & 0xFF;
    const auto micro = (version >> 0) & 0xFFFF;
    const auto str = fmt::format("{}.{}.{}", major, minor, micro);
    return String(val::u8string(str.c_str()));
}

class VM {
public:
    VM() {
        if (auto result = vm_create(&_vm); result != VM_RESULT_SUCCESS) {
            const auto message = vm_result_to_string(result);
            switch (result) {
            case VM_RESULT_ERROR_OUT_OF_MEMORY:
                throwError("RangeError", message);
            case VM_RESULT_ERROR_INVALID_ARG:
                throwError("TypeError", message);
            default:
                throwError("Error", message);
            }
        }
    }

    ~VM() { vm_destroy(_vm); }

    void tick(vm_uint32_t elapsedMs) { vm_tick(_vm, elapsedMs); }

    [[nodiscard]] std::optional<vm_script_h> loadScript(const Bytecode& code) {
        const auto buffer = readByteBuffer(code);
        auto result = vm_load_script_from_memory(_vm, (const vm_char_t*)buffer.data(), buffer.size());
        return result.handle != 0 ? std::make_optional(result) : std::nullopt;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<vm_thread_h> runThread(vm_script_h script) {
        auto result = vm_run_thread(_vm, script);
        return result.handle != 0 ? std::make_optional(result) : std::nullopt;
    }

    void shutdownThread(vm_thread_h thread) noexcept { vm_shutdown_thread(_vm, thread); }

    [[nodiscard]] std::optional<vm_native_h> registerNative(const String& name, NativeCallback callback) {
        const auto argName = name.as<std::string>();
        if (callback.isNull() || callback.isUndefined()) {
            vm_register_native(_vm, argName.c_str(), nullptr, nullptr);
            return std::nullopt;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Native callback must be a function");
        }
        auto holder = std::make_unique<NativeCallback>(std::move(callback));
        const auto native = vm_register_native(_vm, argName.c_str(), &VM::nativeCallback, holder.get());
        if (native.handle == 0) {
            throw std::runtime_error("Failed to register VM native function");
        }
        _nativeCallbacks.insert_or_assign(native.handle, std::move(holder));
        return native;
    }

    void unregisterNative(vm_native_h native) noexcept {
        vm_unregister_native(_vm, native);
        _nativeCallbacks.erase(native.handle);
    }

    [[nodiscard]] ImportCallback getImporter() const noexcept { return ImportCallback(val(0)); }

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

    [[nodiscard]] LoggerCallback getLogger() const noexcept { return _logger; }

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

    [[nodiscard]] static vm_bytecode_t* importCallback(vm_t /* vm */, vm_utf8_t module, vm_data_t data) {
        const auto& self = *static_cast<VM*>(data);
        const auto argModule = String(val::u8string(module));
        auto result = self._importer(argModule);
        if (result.isNull() || result.isUndefined()) {
            return nullptr;
        }
        const auto buffer = readByteBuffer(result);
        auto bytecode = new vm_char_t[buffer.size()];
        memcpy(bytecode, buffer.data(), buffer.size());
        return new vm_bytecode_t{bytecode, static_cast<vm_uint32_t>(buffer.size())};
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
        auto str = String(val::u8string(message));
        self._logger(thread, instructionPos, str);
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

    [[nodiscard]] static val toJsValue(vm_t vm, const vm_value_t& value) {
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

    [[nodiscard]] static vm_value_t fromJsValue(vm_t vm, const val& value) {
        if (value.isUndefined()) {
            return {};
        }
        if (value.isTrue() || value.isFalse()) {
            return vm_value_t{.type = VM_VALUE_TYPE_BOOL, .b = value.as<bool>()};
        }
        if (value.isNumber()) {
            const auto number = value.as<double>();
            if (std::isfinite(number) && std::trunc(number) == number &&
                number >= std::numeric_limits<vm_sint32_t>::min() &&
                number <= std::numeric_limits<vm_sint32_t>::max()) {
                return vm_value_t{.type = VM_VALUE_TYPE_INT, .i32 = static_cast<vm_sint32_t>(number)};
            }
            return vm_value_t{.type = VM_VALUE_TYPE_FLOAT, .f32 = static_cast<vm_float32_t>(number)};
        }
        if (value.isString()) {
            const auto text = value.as<std::string>();
            const auto string = vm_intern_string(vm, text.c_str());
            if (string.handle == 0) {
                throwError("RangeError", "Failed to intern VM string");
            }
            return vm_value_t{.type = VM_VALUE_TYPE_STRING, .str = string};
        }
        throwError("TypeError", "Native callbacks must return a boolean, number, string, or undefined");
    }

    [[noreturn]] static void throwError(const char* err, const char* message) {
        const auto str = String(val::u8string(message));
        val jsError = val::global(err);
        val error_instance = jsError.new_(str);
        error_instance.throw_();
    }

    vm_t _vm{};
    std::unordered_map<vm_uint32_t, std::unique_ptr<NativeCallback>> _nativeCallbacks;
    ImportCallback _importer{val::undefined()};
    LoggerCallback _logger{val::undefined()};
};

EMSCRIPTEN_BINDINGS(adv_vm) {
    register_type<String>("string");
    register_type<Bytecode>("Bytecode", "ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string");
    register_type<NativeCallback>(
        "NativeCallback", "(thread: Thread, native: Native, ...args: (number | boolean | string)[]) => void");
    register_type<ImportCallback>("ImportCallback", "(module: string) => Bytecode");
    register_type<LoggerCallback>(
        "LoggerCallback", "(thread: Thread, instructionPos: number, message: string) => void");

    register_optional<vm_script_h>();
    register_optional<vm_thread_h>();
    register_optional<vm_native_h>();

    function("version", &version);

    value_object<vm_script_h>("Script").field("handle", &vm_script_h::handle);
    value_object<vm_thread_h>("Thread").field("handle", &vm_thread_h::handle);
    value_object<vm_native_h>("Native").field("handle", &vm_native_h::handle);

    class_<VM>("VM")
        .constructor<>()
        .function("tick(elapsedMs)", &VM::tick)
        .function("loadScript(code)", &VM::loadScript)
        .function("runThread(script)", &VM::runThread)
        .function("shutdownThread(thread)", &VM::shutdownThread)
        .function("registerNative(name, func)", &VM::registerNative)
        .function("unregisterNative(native)", &VM::unregisterNative)
        .property("importer", &VM::getImporter, &VM::setImporter)
        .property("logger", &VM::getLogger, &VM::setLogger);
}
