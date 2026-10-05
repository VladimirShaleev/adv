#include "module-manager.hpp"

namespace vm {

ModuleManager::ModuleManager() { _modules.emplace_back(); }

std::span<const uint8_t> ModuleManager::getModule(vm_script_h script) const noexcept {
    assert(script.handle < _modules.size());
    const auto& bytecode = _modules[script.handle];
    return {bytecode.data(), bytecode.size()};
}

vm_script_h ModuleManager::loadScript(vm_utf8_t file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open()) {
        return UndefinedScript;
    }
    auto size = stream.tellg();
    stream.seekg(0, std::ios::end);
    size = stream.tellg() - size;
    stream.seekg(0, std::ios::beg);

    std::vector<vm_char_t> bytes;
    bytes.resize(size);
    stream.read(bytes.data(), size);
    stream.close();

    const auto name = std::filesystem::path(file).filename().replace_extension().string();
    return loadScript({bytes.data(), bytes.size()}, name);
}

vm_script_h ModuleManager::loadScript(std::span<const vm_char_t> code, std::string_view moduleName) {
    auto needValidateChecksum = true;
    std::vector<vm_uint8_t> tmpBuffer;
    if (!utils::isBinaryCode(code)) {
        code = tryTranslate(code, tmpBuffer);
        needValidateChecksum = false;
    }
    if (code.empty()) {
        return UndefinedScript;
    }
    if (!utils::validateVersion(code, VM_VERSION)) {
        return UndefinedScript;
    }
    if (needValidateChecksum && !utils::validateChecksum(code)) {
        return UndefinedScript;
    }
    if (moduleName.empty()) {
        moduleName = getModuleName({(const vm_uint8_t*)code.data(), code.size()});
    }
    if (moduleName.empty()) {
        return UndefinedScript;
    }
    if (auto loadedModule = _mapModules.find(moduleName)) {
        return vm_script_h{loadedModule.value().handle};
    }
    const auto script = vm_script_h{_mapModules.insert(moduleName).handle};
    const auto buffer = (const uint8_t*)code.data();
    _modules.emplace_back(buffer, buffer + code.size());
    return script;
}

std::string_view ModuleManager::getModuleName(std::span<const vm_uint8_t> bytecode) noexcept {
    const auto table =
        vm_swap_script_table(utils::aliasCast<vm_script_table_t*>(bytecode.data() + sizeof(vm_script_header_t)));
    if (table.data_size > 0) {
        size_t offset = table.data_offset;
        const auto length = utils::readUint16(bytecode.data(), offset);
        if (length > 0) {
            return {(const char*)(bytecode.data() + offset), length};
        }
    }
    return {};
}

std::span<const vm_char_t>
ModuleManager::tryTranslate(std::span<const vm_char_t> code, std::vector<vm_uint8_t>& buffer) noexcept {
    utils::TranslateResult result{};
    buffer = utils::translateTextToBinary(code, result);
    if (result.errorCode == utils::TranslateResult::NoError) {
        return {(const vm_char_t*)buffer.data(), buffer.size()};
    }
    return {};
}

} // namespace vm
