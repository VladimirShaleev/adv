#include "string-pool.hpp"

namespace utils {

struct StringPool::Impl {
    static constexpr uint32_t EmptySlot = std::numeric_limits<uint32_t>::max();

    void init(size_t slotCapacity, size_t bufferCapacity, size_t hashTableCapacity) {
        buffer.resize(bufferCapacity);
        buffer[0] = '\0';
        bufferSize = 1;

        slots.resize(slotCapacity);
        slotCount = 1;
        slots[0] = {0, 0};

        hashTable.resize(hashTableCapacity, EmptySlot);
    }

    void resize(size_t newCapacity) { buffer.resize(newCapacity); }

    std::string_view get(vm_string_h str) const noexcept {
        if (str.handle < slotCount) {
            const auto& slot = slots[str.handle];
            return std::string_view(&buffer[slot.offset], slot.length);
        }
        return std::string_view();
    }

    vm_string_h insert(std::string_view str) {
        if (str.empty()) {
            return {0};
        }

        if (slotCount * 10 >= hashTable.size() * 7) {
            growHash();
        }

        const auto [findedStr, bucket] = find(str);
        if (findedStr) {
            return findedStr.value();
        }

        const auto newOffset = bufferSize;
        if (newOffset + str.length() + 1 > buffer.size()) {
            resize(std::max(buffer.size() * 2, newOffset + str.length() + 1));
        }
        memcpy(&buffer[newOffset], str.data(), str.length());
        buffer[newOffset + str.length()] = '\0';
        bufferSize += str.length() + 1;

        const auto newSlotIndex = slotCount++;
        if (newSlotIndex >= slots.size()) {
            slots.resize(slots.size() * 2);
        }
        slots[newSlotIndex] = {(uint32_t)newOffset, static_cast<uint32_t>(str.length())};

        hashTable[bucket] = newSlotIndex;
        return {(uint32_t)newSlotIndex};
    }

    std::pair<std::optional<vm_string_h>, size_t> find(std::string_view str) const noexcept {
        if (str.empty()) {
            return std::make_pair(std::make_optional(vm_string_h{0}), 0);
        }

        const auto hash = hashFnv1a(str);

        auto bucket = hash % hashTable.size();

        while (hashTable[bucket] != EmptySlot) {
            const auto slotIndex = hashTable[bucket];
            const auto& slot = slots[slotIndex];

            if (slot.length == str.length()) {
                if (std::memcmp(&buffer[slot.offset], str.data(), str.length()) == 0) {
                    return std::make_pair(std::make_optional(vm_string_h{slotIndex}), bucket);
                }
            }

            bucket = (bucket + 1) % hashTable.size();
        }

        return std::make_pair(std::nullopt, bucket);
    }

    void growHash() {
        const auto oldCapacity = (uint32_t)hashTable.size();
        const auto newCapacity = oldCapacity * 2;
        hashTable.assign(newCapacity, EmptySlot);

        for (uint32_t i = 1; i < slotCount; i++) {
            const auto& slot = slots[i];
            const auto str = &buffer[slot.offset];

            const auto hash = hashFnv1a(std::string_view(str, slot.length));
            auto bucket = hash % newCapacity;

            while (hashTable[bucket] != EmptySlot) {
                bucket = (bucket + 1) % newCapacity;
            }
            hashTable[bucket] = i;
        }
    }

    static uint32_t hashFnv1a(std::string_view str) {
        uint32_t hash = 2'166'136'261u;
        for (size_t i = 0; i < str.length(); ++i) {
            hash ^= (uint8_t)str[i];
            hash *= 16'777'619u;
        }
        return hash;
    }

    struct Slot {
        uint32_t offset;
        uint32_t length;
    };

    std::vector<char> buffer;
    size_t bufferSize{};

    std::vector<Slot> slots;
    size_t slotCount{};

    std::vector<uint32_t> hashTable;
};

StringPool::StringPool(size_t slotCapacity, size_t bufferCapacity, size_t hashTableCapacity)
    : _impl(std::make_shared<Impl>()) {
    _impl->init(slotCapacity, bufferCapacity, hashTableCapacity);
}

StringPool::~StringPool() = default;

std::string_view StringPool::operator[](vm_string_h str) const noexcept { return _impl->get(str); }

std::string_view StringPool::get(vm_string_h str) const noexcept { return _impl->get(str); }

std::optional<vm_string_h> StringPool::find(std::string_view str) const noexcept {
    const auto [result, _] = _impl->find(str);
    return result;
}

vm_string_h StringPool::insert(std::string_view str) { return _impl->insert(str); }

} // namespace utils
