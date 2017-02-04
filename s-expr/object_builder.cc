#include "object_builder.h"
#include <stdlib.h>

namespace mio {

ObjectBuilder::ObjectBuilder() {
}

ObjectBuilder::~ObjectBuilder() {
}

class MallocObjectBuilder : public ObjectBuilder {
public:
    struct Chunk {
        Chunk *next;
        char   data[1];
    };

    MallocObjectBuilder(bool ownership);
    virtual ~MallocObjectBuilder() override;

    virtual Number *NewNumber(NativeNumber value) override;
    virtual Pair *NewPair(Object *car, Pair *cdr) override;
    virtual Id *NewId(const char *z, int16_t len, int16_t tag) override;
    virtual void DeleteObject(const Object *obj) override;
    virtual Boolean *NewBoolean(NativeBoolean value) override;

private:
    template<class T>
    inline T *NewFixedSizeObject(int kind) {
        return this->NewObject<T>(kind, T::kSize);
    }

    template<class T>
    inline T *NewObject(int kind, int size) {
        auto chunk = static_cast<Chunk*>(malloc(sizeof(Chunk) + size));
        auto result = new (chunk->data) T();

        SetObjectProperty<int>(result, Object::kkindOffset, kind);

        chunk->next = top_;
        top_ = chunk;
        return result;
    }

    bool ownership_;
    Chunk *top_ = nullptr;
};

MallocObjectBuilder::MallocObjectBuilder(bool ownership)
    : ownership_(ownership) {
}

MallocObjectBuilder::~MallocObjectBuilder() {
    if (ownership_) {
        while (top_) {
            free(top_);
            top_ = top_->next;
        }
    }
}


Number *MallocObjectBuilder::NewNumber(NativeNumber value) {
    auto result = NewFixedSizeObject<Number>(VALUE_NUMBER);
    SetObjectProperty<NativeNumber>(result, Number::knativeOffset, value);
    return result;
}

Pair *MallocObjectBuilder::NewPair(Object *car, Pair *cdr) {
    auto result = NewFixedSizeObject<Pair>(VALUE_PAIR);
    SetObjectProperty<Object *>(result, Pair::kcarOffset, car);
    SetObjectProperty<Pair *>(result, Pair::kcdrOffset, cdr);
    return result;
}

Id *MallocObjectBuilder::NewId(const char *z, int16_t len, int16_t tag) {
    auto result = NewObject<Id>(VALUE_ID, len + 1); // term zero
    if (!result) {
        return nullptr;
    }
    SetObjectProperty<int16_t>(result, Id::klenOffset, len);
    SetObjectProperty<int16_t>(result, Id::ktagOffset, tag);

    auto data = static_cast<char *>(GetObjectPropertyPointer(result,
                Id::ktxtOffset));
    memcpy(data, z, len);
    data[len] = '\0';
    return result;
}

Boolean *MallocObjectBuilder::NewBoolean(NativeBoolean value) {
    auto result = NewFixedSizeObject<Boolean>(VALUE_BOOLEAN);
    SetObjectProperty<NativeBoolean>(result, Boolean::knativeOffset, value);
    return resutl;
}

void MallocObjectBuilder::DeleteObject(const Object *) {
    NOREACHED() << "delete is not support.";
}

ObjectBuilder *NewMallocObjectBuilder(bool ownership) {
    return new MallocObjectBuilder(ownership);
}

} // namespace mio
