#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "vm/vm.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using emscripten::enum_;
using emscripten::class_;
using emscripten::val;

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

    const auto sourceLength = inputLength.isUndefined()
        ? byteLength.as<double>()
        : inputLength.as<double>();
    if (!std::isfinite(sourceLength) ||
        sourceLength < 0 ||
        std::trunc(sourceLength) != sourceLength) {
        throw std::invalid_argument("VM script byte buffer length must be a non-negative integer");
    }
    if (sourceLength > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("VM script byte buffer is too large");
    }

    const auto bytes = val::global("Uint8Array").new_(input);
    return emscripten::convertJSArrayToNumberVector<vm_uint8_t>(bytes);
}

bool isFunction(const val& value) {
    return val::global("Function")["prototype"].call<bool>("isPrototypeOf", value);
}

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

vm_value_t fromJsValue(vm_t vm, const val& value) {
    if (value.isUndefined()) {
        return {};
    }
    if (value.isTrue() || value.isFalse()) {
        return vm_value_t{.type = VM_VALUE_TYPE_BOOL, .b = value.as<bool>()};
    }
    if (value.isNumber()) {
        const auto number = value.as<double>();
        if (std::isfinite(number) &&
            std::trunc(number) == number &&
            number >= std::numeric_limits<vm_sint32_t>::min() &&
            number <= std::numeric_limits<vm_sint32_t>::max()) {
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
        return vm_value_t{
            .type = VM_VALUE_TYPE_STRING,
            .str = string};
    }
    throw std::invalid_argument("Native callbacks must return a boolean, number, string, or undefined");
}

class Vm {
public:
    Vm() {
        const auto result = vm_create(&_vm);
        if (result != VM_RESULT_SUCCESS) {
            throw std::runtime_error(vm_result_to_string(result));
        }
    }

    ~Vm() {
        vm_set_logger(_vm, nullptr, nullptr);
        vm_set_importer(_vm, nullptr, nullptr);
        vm_set_release_import(_vm, nullptr, nullptr);
        vm_destroy(_vm);
    }

    Vm(const Vm&) = delete;
    Vm& operator=(const Vm&) = delete;

    void tick(vm_uint32_t elapsedMs) {
        vm_tick(_vm, elapsedMs);
    }

    vm_uint32_t loadScript(const val& input) {
        const auto bytes = readByteBuffer(input);
        const auto script = vm_load_script_from_memory(
            _vm,
            reinterpret_cast<const vm_char_t*>(bytes.data()),
            static_cast<vm_uint32_t>(bytes.size()));
        if (script.handle == invalidHandle) {
            throw std::runtime_error("Failed to load VM script from memory");
        }
        return script.handle;
    }

    vm_uint32_t runThread(vm_uint32_t script) {
        const auto thread = vm_run_thread(_vm, vm_script_h{script});
        if (thread.handle == invalidHandle) {
            throw std::runtime_error("Failed to start VM script thread");
        }
        return thread.handle;
    }

    void shutdownThread(vm_uint32_t thread) {
        vm_shutdown_thread(_vm, vm_thread_h{thread});
    }

    std::string getString(vm_uint32_t string) const {
        vm_uint32_t length = 0;
        const auto text = vm_get_string(_vm, vm_string_h{string}, &length);
        return std::string(text, length);
    }

    vm_uint32_t internString(const std::string& text) {
        return vm_intern_string(_vm, text.c_str()).handle;
    }

    vm_uint32_t registerNative(const std::string& name, val callback) {
        if (!isFunction(callback)) {
            throw std::invalid_argument("Native callback must be a function");
        }

        auto holder = std::make_unique<val>(std::move(callback));
        const auto native = vm_register_native(
            _vm,
            name.c_str(),
            &Vm::nativeCallback,
            holder.get());
        if (native.handle == invalidHandle) {
            throw std::runtime_error("Failed to register VM native function");
        }
        _nativeCallbacks.insert_or_assign(native.handle, std::move(holder));
        return native.handle;
    }

    void unregisterNative(vm_uint32_t native) {
        vm_unregister_native(_vm, vm_native_h{native});
        _nativeCallbacks.erase(native);
    }

    void setLogger(val callback) {
        if (callback.isNull() || callback.isUndefined()) {
            _logger = val::undefined();
            vm_set_logger(_vm, nullptr, nullptr);
            return;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Logger must be a function or undefined");
        }
        _logger = std::move(callback);
        vm_set_logger(_vm, &Vm::loggerCallback, this);
    }

    void setImporter(val callback) {
        if (callback.isNull() || callback.isUndefined()) {
            _importer = val::undefined();
            vm_set_importer(_vm, nullptr, nullptr);
            vm_set_release_import(_vm, nullptr, nullptr);
            return;
        }
        if (!isFunction(callback)) {
            throw std::invalid_argument("Importer must be a function or undefined");
        }
        _importer = std::move(callback);
        vm_set_importer(_vm, &Vm::importCallback, this);
        vm_set_release_import(_vm, &Vm::releaseImport, nullptr);
    }

private:
    static void nativeCallback(vm_t vm,
                               vm_thread_h thread,
                               vm_native_h,
                               vm_uint16_t argCount,
                               vm_data_t data) {
        const auto& callback = *static_cast<val*>(data);
        auto args = val::array();
        for (vm_uint16_t i = 0; i < argCount; ++i) {
            args.call<void>("push", toJsValue(vm, vm_thread_get_arg(vm, thread, i)));
        }
        const auto result = val::global("Reflect").call<val>(
            "apply",
            callback,
            val::undefined(),
            args);
        if (!result.isUndefined()) {
            const auto value = fromJsValue(vm, result);
            vm_thread_set_result(vm, thread, 0, &value);
        }
    }

    static void loggerCallback(vm_t,
                               vm_thread_h thread,
                               vm_uint32_t instructionPos,
                               vm_utf8_t message,
                               vm_data_t data) {
        const auto& self = *static_cast<Vm*>(data);
        auto entry = val::object();
        entry.set("thread", thread.handle);
        entry.set("instruction", instructionPos);
        entry.set("message", val::u8string(message));
        auto args = val::array();
        args.call<void>("push", entry);
        val::global("Reflect").call<val>(
            "apply",
            self._logger,
            val::undefined(),
            args);
    }

    static vm_bytecode_t* importCallback(vm_t,
                                         vm_utf8_t module,
                                         vm_data_t data) {
        const auto& self = *static_cast<Vm*>(data);
        auto args = val::array();
        // args.call<void>("push", module);
        const auto result = val::global("Reflect").call<val>(
            "apply",
            self._importer,
            val::undefined(),
            args);
        if (result.isNull() || result.isUndefined()) {
            return nullptr;
        }

        const auto bytes = readByteBuffer(result);
        auto dataCopy = std::make_unique<vm_char_t[]>(bytes.size());
        if (!bytes.empty()) {
            std::memcpy(dataCopy.get(), bytes.data(), bytes.size());
        }
        return new vm_bytecode_t{
            .data = dataCopy.release(),
            .size = static_cast<vm_uint32_t>(bytes.size())};
    }

    static void releaseImport(vm_t, vm_bytecode_t* bytecode, vm_data_t) {
        delete[] bytecode->data;
        delete bytecode;
    }

    vm_t _vm{};
    std::unordered_map<vm_uint32_t, std::unique_ptr<val>> _nativeCallbacks;
    val _logger{val::undefined()};
    val _importer{val::undefined()};
};

} // namespace

EMSCRIPTEN_BINDINGS(adv_vm) {
    enum_<vm_value_type_t>("ValueType")
        .value("Unknown", VM_VALUE_TYPE_UNKNOWN)
        .value("Bool", VM_VALUE_TYPE_BOOL)
        .value("Int", VM_VALUE_TYPE_INT)
        .value("Float", VM_VALUE_TYPE_FLOAT)
        .value("String", VM_VALUE_TYPE_STRING);

    class_<Vm>("Vm")
        .constructor<>()
        .function("tick", &Vm::tick)
        .function("loadScript", &Vm::loadScript)
        .function("runThread", &Vm::runThread)
        .function("shutdownThread", &Vm::shutdownThread)
        .function("getString", &Vm::getString)
        .function("internString", &Vm::internString)
        .function("registerNative", &Vm::registerNative)
        .function("unregisterNative", &Vm::unregisterNative)
        .function("setLogger", &Vm::setLogger)
        .function("setImporter", &Vm::setImporter);
}
