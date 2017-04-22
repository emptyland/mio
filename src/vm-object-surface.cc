#include "vm-object-surface.h"

namespace mio {

MIOHashMapSurface::MIOHashMapSurface(MIOHashMap *core, ManagedAllocator *allocator)
    : core_(DCHECK_NOTNULL(core))
    , key_size_(core->GetKey()->GetTypePlacementSize())
    , value_size_(core->GetValue()->GetTypePlacementSize())
    , allocator_(DCHECK_NOTNULL(allocator)) {
    if (core_->GetKey()->IsPrimitive()) {
        hash_     = NativeBaseLibrary::PrimitiveHash;
        equal_to_ = NativeBaseLibrary::PrimitiveEqualTo;
    } else {
        hash_     = NativeBaseLibrary::StringHash;
        equal_to_ = NativeBaseLibrary::StringEqualTo;
    }
}

MIOPair *MIOHashMapSurface::GetOrInsertRoom(const void *key, bool *insert) {
    auto code = hash_(key, key_size_);
    auto slot = core_->GetSlot(code % core_->GetSlotSize());

    auto node = slot->head;
    while (node) {
        if (equal_to_({node->GetKey(), key_size_}, {key, key_size_})) {
            return node;
        }
        node = node->GetNext();
    }
    auto factor = static_cast<float>(core_->GetSize()) /
                  static_cast<float>(core_->GetSlotSize());
    if (factor > kRehashTopFactor) {
        Rehash(1.7f);
        slot = core_->GetSlot(code % core_->GetSlotSize());
    } else if (factor < kRehashBottomFactor && core_->GetSize() > kMinSloSize) {
        Rehash(0.7f);
        slot = core_->GetSlot(code % core_->GetSlotSize());
    }

    *insert = true;
    node = static_cast<MIOPair *>(allocator_->Allocate(MIOPair::kMIOPairOffset));
    memcpy(node->GetKey(), key, key_size_);
    node->SetNext(slot->head);
    slot->head = node;
    core_->SetSize(core_->GetSize() + 1);
    return node;
}

MIOPair *MIOHashMapSurface::GetRoom(const void *key) {
    auto code = hash_(key, key_size_);
    auto slot = core_->GetSlot(code % core_->GetSlotSize());

    auto node = slot->head;
    while (node) {
        if (equal_to_({node->GetKey(), key_size_}, {key, key_size_})) {
            break;
        }
        node = node->GetNext();
    }
    return node;
}

void MIOHashMapSurface::Rehash(float scalar) {
    auto new_slot_size = static_cast<int>(scalar * core_->GetSlotSize());
    auto new_slots = static_cast<uint8_t *>(allocator_->Allocate(new_slot_size * MIOPair::kHeaderOffset));
    memset(new_slots, 0, new_slot_size * MIOPair::kHeaderOffset);

    for (int i = 0; i < core_->GetSlotSize(); ++i) {
        auto node = core_->GetSlot(i)->head;
        while (node) {
            auto offset = (hash_(node->GetKey(), key_size_) % new_slot_size) * MIOPair::kHeaderOffset;
            auto new_slot = reinterpret_cast<MIOPair*>(new_slots + offset);
            auto next = node->GetNext();

            node->SetNext(new_slot->GetNext());
            new_slot->SetNext(node);
            node = next;
        }
    }
    allocator_->Free(core_->GetSlot(0));
    HeapObjectSet(core_.get(), MIOHashMap::kSlotsOffset, new_slots);
    core_->SetSlotSize(new_slot_size);
}

} // namespace mio
