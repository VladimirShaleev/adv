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

ModuleManager::ModuleManager(Cache& cache) noexcept : _cache(cache) {}

bool ModuleManager::hasModule(std::string_view moduleName) const noexcept {
    if (auto str = _cache.pool.find(moduleName)) {
        return _modules.contains(str.value());
    }
    return false;
}

Module* ModuleManager::getModule(vm_script_h script) noexcept { return _modules.get(script); }

const Module* ModuleManager::getModule(vm_script_h script) const noexcept { return _modules.get(script); }

std::pair<vm_script_h, bool> ModuleManager::loadScript(vm_utf8_t file, std::span<const std::filesystem::path> dirs) {
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
    return loadScript({bytes.data(), bytes.size()}, name);
}

std::pair<vm_script_h, bool> ModuleManager::loadScript(std::span<const vm_char_t> code, std::string_view moduleName) {
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
    std::span bytecode{(const vm_uint8_t*)code.data(), code.size()};
    if (moduleName.empty()) {
        auto name = utils::read<vm_script_export_t>(bytecode).module_name;
        auto data = utils::read<utils::SectionData>(bytecode).data;
        moduleName = utils::readString(name, data);
    }
    if (moduleName.empty()) {
        return {Undefined, false};
    }
    const auto key = _cache.pool.insert(moduleName);
    if (auto [script, loadedModule] = _modules.find(key); loadedModule) {
        const auto oldChecksum = _modules.get(script)->checksum();
        const auto newChecksum = utils::read<vm_script_header_t>(bytecode).checksum;
        if (oldChecksum == newChecksum) {
            return {script, false};
        }
    }
    const auto [script, _] = _modules.emplace(key, bytecode);
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
