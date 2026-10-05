#include "module-manager.hpp"

namespace vm {

ModuleManager::ModuleManager() { _modules.emplace_back(); }

std::span<const uint8_t> ModuleManager::getModule(vm_script_h script) const noexcept {
    assert(script.handle < _modules.size());
    const auto& bytecode = _modules[script.handle];
    return {bytecode.data(), bytecode.size()};
}

std::pair<vm_script_h, bool> ModuleManager::loadScript(vm_utf8_t file, bool forceLoad) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open()) {
        return {UndefinedScript, false};
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
    return loadScript({bytes.data(), bytes.size()}, forceLoad, name);
}

std::pair<vm_script_h, bool>
ModuleManager::loadScript(std::span<const vm_char_t> code, bool forceLoad, std::string_view moduleName) {
    auto needValidateChecksum = true;
    std::vector<vm_uint8_t> tmpBuffer;
    if (!utils::isBinaryCode(code)) {
        code = tryTranslate(code, tmpBuffer);
        needValidateChecksum = false;
    }
    if (code.empty()) {
        return {UndefinedScript, false};
    }
    if (!utils::validateVersion(code, VM_VERSION)) {
        return {UndefinedScript, false};
    }
    if (needValidateChecksum && !utils::validateChecksum(code)) {
        return {UndefinedScript, false};
    }
    if (moduleName.empty()) {
        moduleName = getModuleName({(const vm_uint8_t*)code.data(), code.size()});
    }
    if (moduleName.empty()) {
        return {UndefinedScript, false};
    }
    if (auto loadedModule = _mapModules.find(moduleName)) {
        const auto script = vm_script_h{loadedModule.value().handle};
        auto reloaded = false;
        if (forceLoad) {
            const auto oldChecksum = utils::read<vm_script_header_t>(getModule(script)).checksum;
            const auto newChecksum =
                utils::read<vm_script_header_t>({(const vm_uint8_t*)code.data(), code.size()}).checksum;
            if (oldChecksum != newChecksum) {
                const auto buffer = (const uint8_t*)code.data();
                _modules[loadedModule.value().handle].assign(buffer, buffer + code.size());
                reloaded = true;
            }
        }
        return {script, reloaded};
    }
    const auto script = vm_script_h{_mapModules.insert(moduleName).handle};
    const auto buffer = (const uint8_t*)code.data();
    _modules.emplace_back(buffer, buffer + code.size());
    return {script, true};
}

void ModuleManager::changeNameToSlot(
    vm_script_h script, vm_uint32_t instructionPos, vm_opcode_t newOpcode, vm_uint32_t slot) noexcept {
    assert(script.handle < _modules.size());
    const auto& bytecode = getModule(script);
    auto text = utils::read<utils::SectionText>(bytecode).text;
    assert(instructionPos < text.size());
    auto ptr = const_cast<uint8_t*>(text.data()) + instructionPos;
    const auto prevOpcode = static_cast<vm_opcode_t>(*ptr);
    switch (prevOpcode) {
    case VM_OPCODE_PUSH_GLOBAL_BY_NAME:
        assert(newOpcode == VM_OPCODE_PUSH_GLOBAL);
        break;
    case VM_OPCODE_POP_GLOBAL_BY_NAME:
        assert(newOpcode == VM_OPCODE_POP_GLOBAL);
        break;
    case VM_OPCODE_CALL_BY_NAME:
        assert(newOpcode == VM_OPCODE_CALL);
        break;
    default:
        assert(!"unreachable code");
        break;
    }
    slot = utils::swapBytes(slot);
    *ptr++ = newOpcode;
    *ptr++ = static_cast<uint8_t>((slot >> 0) & 0xFF);
    *ptr++ = static_cast<uint8_t>((slot >> 8) & 0xFF);
    *ptr++ = static_cast<uint8_t>((slot >> 16) & 0xFF);
    *ptr++ = static_cast<uint8_t>((slot >> 24) & 0xFF);
}

std::string_view ModuleManager::getModuleName(std::span<const vm_uint8_t> bytecode) noexcept {
    auto data = utils::read<utils::SectionData>(bytecode).data;
    return utils::readString(0, data);
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
