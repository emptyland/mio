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

void MIOHashMapSurface::CleanAll() {
    for (int i = 0; i < core_->GetSlotSize(); ++i) {
        while (core_->GetSlot(i)->head) {
            auto node = core_->GetSlot(i)->head;
            core_->GetSlot(i)->head = node->GetNext();
            allocator_->Free(node);
        }
    }
    core_->SetSize(0);
}

MIOPair *MIOHashMapSurface::GetNextRoom(const void *key) {
    if (key) {
        int index;
        auto pair = GetRoom(key, &index);
        if (!pair) {
            return nullptr;
        }
        if (!pair->GetNext() && index == core_->GetSlotSize() - 1) {
            return nullptr;
        }
        pair = pair->GetNext();
        if (pair) {
            return pair;
        }
        for (int i = index + 1; i < core_->GetSlotSize(); ++i) {
            pair = core_->GetSlot(i)->head;
            if (pair) {
                break;
            }
        }
        return pair;
    } else {
        MIOPair *pair = nullptr;
        for (int i = 0; i < core_->GetSlotSize(); ++i) {
            pair = core_->GetSlot(i)->head;
            if (pair) {
                break;
            }
        }
        return pair;
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
    if (key_slot_factor() > kRehashTopFactor) {
        Rehash(1.7f);
        slot = core_->GetSlot(code % core_->GetSlotSize());
    }

    *insert = true;
    node = static_cast<MIOPair *>(allocator_->Allocate(MIOPair::kMIOPairOffset));
    if (!node) {
        return nullptr;
    }
    memcpy(node->GetKey(), key, key_size_);
    node->SetNext(slot->head);
    slot->head = node;
    core_->SetSize(core_->GetSize() + 1);
    return node;
}

MIOPair *MIOHashMapSurface::GetRoom(const void *key, int *index) {
    auto code = hash_(key, key_size_);
    auto slot = core_->GetSlot(code % core_->GetSlotSize());

    auto node = slot->head;
    while (node) {
        if (equal_to_({node->GetKey(), key_size_}, {key, key_size_})) {
            break;
        }
        node = node->GetNext();
    }
    if (node && index) {
        *index = code % core_->GetSlotSize();
    }
    return node;
}

bool MIOHashMapSurface::RawDelete(const void *key) {
    auto code = hash_(key, key_size_);
    auto slot = core_->GetSlot(code % core_->GetSlotSize());

    auto prev = reinterpret_cast<MIOPair *>(slot);
    auto node = slot->head;
    while (node) {
        if (equal_to_({node->GetKey(), key_size_}, {key, key_size_})) {
            break;
        }
        prev = node;
        node = node->GetNext();
    }

    if (node) {
        prev->SetNext(node->GetNext());
        allocator_->Free(node);
        core_->SetSize(core_->GetSize() - 1);

        if (key_slot_factor() < kRehashBottomFactor && core_->GetSize() > kMinSloSize) {
            Rehash(0.7f);
        }
        return true;
    }
    return false;
}

void MIOHashMapSurface::Rehash(float scalar) {
    auto new_slot_size = static_cast<int>(scalar * core_->GetSlotSize());
    if (new_slot_size < kMinSloSize) {
        new_slot_size = kMinSloSize;
    }

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

MIOArraySurface::MIOArraySurface(Handle<HeapObject> ob,
                                 ManagedAllocator *allocator)
    : allocator_(DCHECK_NOTNULL(allocator)) {
    DCHECK(ob->IsSlice() || ob->IsVector());
    if (ob->IsSlice()) {
        slice_ = ob->AsSlice();
        core_  = slice_->GetVector();
        begin_ = slice_->GetRangeBegin();
        size_  = slice_->GetRangeSize();
    } else {
        core_  = ob->AsVector();
        begin_ = 0;
        size_  = core_->GetSize();
    }
    element_size_ = core_->GetElement()->GetTypePlacementSize();
}

void *MIOArraySurface::AddRoom(mio_int_t size, bool *ok) {
    DCHECK(slice_.empty()) << "slice can not be added!";

    if (core_->GetSize() + size > core_->GetCapacity()) {
        auto new_capacity = core_->GetCapacity() * MIOVector::kCapacityScale;
        auto new_data = allocator_->Allocate(new_capacity * element_size_);
        if (!new_data) {
            *ok = false;
            return nullptr;
        }
        memcpy(new_data, core_->GetData(), core_->GetSize() * element_size_);
        core_->SetData(new_data);
        core_->SetCapacity(new_capacity);
    }
    auto new_room = RawGet(core_->GetSize());
    core_->SetSize(static_cast<int>(core_->GetSize() + size));
    return new_room;
}

} // namespace mio
