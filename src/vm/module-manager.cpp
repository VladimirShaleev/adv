#include "module-manager.hpp"

namespace vm {

Module::Module(std::span<const uint8_t> bytecode) {
    if (!bytecode.empty()) {
        _bytecode.assign(bytecode.begin(), bytecode.end());
        auto text = utils::read<utils::SectionText>(_bytecode).text;
        _text = {const_cast<uint8_t*>(text.data()), text.size()};
        _data = utils::read<utils::SectionData>(_bytecode).data;
    }
}

std::string_view Module::name() const noexcept {
    auto name = utils::read<vm_script_export_t>(_bytecode).module_name;
    return utils::readString(name, _data);
}

vm_uint32_t Module::checksum() const noexcept { return utils::read<vm_script_header_t>(_bytecode).checksum; }

void Module::modifyOpcode(vm_uint32_t instructionPos, vm_opcode_t newOpcode, vm_uint32_t slot) noexcept {
    assert(instructionPos < _text.size());
    auto ptr = const_cast<uint8_t*>(_text.data()) + instructionPos;
    const auto prevOpcode = static_cast<vm_opcode_t>(*ptr);
    switch (prevOpcode) {
    case VM_OPCODE_IMPORT:
        assert(newOpcode == VM_OPCODE_NO_OPERATION);
        break;
    case VM_OPCODE_PUSH_STRING_FROM_DATA:
        assert(newOpcode == VM_OPCODE_PUSH_STRING);
        break;
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
    _changes.emplace_back(instructionPos, newOpcode, slot);
}

ModuleManager::ModuleManager() { _modules.emplace_back(std::span<const uint8_t>{}); }

bool ModuleManager::hasModule(std::string_view moduleName) const noexcept {
    return _mapModules.find(moduleName).has_value();
}

Module& ModuleManager::getModule(vm_script_h script) noexcept {
    assert(script.handle < _modules.size());
    return _modules[script.handle];
}

const Module& ModuleManager::getModule(vm_script_h script) const noexcept {
    assert(script.handle < _modules.size());
    return _modules[script.handle];
}

std::pair<vm_script_h, bool>
ModuleManager::loadScript(vm_utf8_t file, std::span<const std::filesystem::path> dirs, bool forceLoad) {
    const auto fullpath = utils::findFile(file, dirs);
    std::ifstream stream(fullpath, std::ios::binary);
    if (!stream.is_open()) {
        return {Undefined, false};
    }
    auto size = stream.tellg();
    stream.seekg(0, std::ios::end);
    size = stream.tellg() - size;
    stream.seekg(0, std::ios::beg);

    std::vector<vm_char_t> bytes;
    bytes.resize(size);
    stream.read(bytes.data(), size);
    stream.close();

    const auto name = utils::getModuleName(fullpath);
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
        return {Undefined, false};
    }
    if (!utils::validateVersion(code, VM_VERSION)) {
        return {Undefined, false};
    }
    if (needValidateChecksum && !utils::validateChecksum(code)) {
        return {Undefined, false};
    }
    if (moduleName.empty()) {
        auto name = utils::read<vm_script_export_t>({(const vm_uint8_t*)code.data(), code.size()}).module_name;
        auto data = utils::read<utils::SectionData>({(const vm_uint8_t*)code.data(), code.size()}).data;
        moduleName = utils::readString(name, data);
    }
    if (moduleName.empty()) {
        return {Undefined, false};
    }
    if (auto loadedModule = _mapModules.find(moduleName)) {
        const auto script = vm_script_h{loadedModule.value().handle};
        auto reloaded = false;
        if (forceLoad) {
            const auto oldChecksum = getModule(script).checksum();
            const auto newChecksum =
                utils::read<vm_script_header_t>({(const vm_uint8_t*)code.data(), code.size()}).checksum;
            if (oldChecksum != newChecksum) {
                const auto buffer = (const uint8_t*)code.data();
                _modules[loadedModule.value().handle] = std::move(Module({buffer, buffer + code.size()}));
                reloaded = true;
            }
        }
        return {script, reloaded};
    }
    const auto script = vm_script_h{_mapModules.insert(moduleName).handle};
    const auto buffer = (const uint8_t*)code.data();
    _modules.emplace_back(std::span{buffer, buffer + code.size()});
    return {script, true};
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
