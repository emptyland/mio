#include "types.h"
#include "memory-output-stream.h"

namespace mio {

namespace {

class TypeDigest {
public:
    static const int64_t kInitialValue = 1315423911;

    TypeDigest() = default;

    int64_t value() const { return value_; }

    void Step(int64_t atomic) {
        value_ ^= ((value_ << 5) + atomic + (value_ >> 2));
    }

private:
    int64_t value_ = kInitialValue;
};

}

////////////////////////////////////////////////////////////////////////////////
/// TypeFactory
////////////////////////////////////////////////////////////////////////////////

TypeFactory::TypeFactory(Zone *zone)
    : zone_(DCHECK_NOTNULL(zone)) {
    // i1
    // i8
    // i16
    // i32
    // i64
    // f32
    // f64
    // void
    // unknown
    // string
    simple_types_[0] = new (zone_) Integral(1,  TOKEN_BOOL);
    simple_types_[1] = new (zone_) Integral(8,  TOKEN_I8);
    simple_types_[2] = new (zone_) Integral(16, TOKEN_I16);
    simple_types_[3] = new (zone_) Integral(32, TOKEN_I32);
    simple_types_[4] = new (zone_) Integral(64, TOKEN_I64);
    simple_types_[5] = new (zone_) Floating(32, TOKEN_F32);
    simple_types_[6] = new (zone_) Floating(64, TOKEN_F64);
    simple_types_[7] = new (zone_) Void(TOKEN_VOID);
    simple_types_[8] = new (zone_) Unknown(-1);
    simple_types_[9] = new (zone_) String(TOKEN_STRING);

    for (auto type : simple_types_) {
        Record(type);
    }
}

Union *TypeFactory::MergeToFlatUnion(Type **types, int n) {
    auto *ut = new (zone_) Union::TypeMap(zone_);
    for (int i = 0; i < n; ++i) {

        if (types[i]->IsUnion()) {
            auto u = types[i]->AsUnion();

            Union::TypeMap::Iterator iter(u->mutable_types());
            for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
                ut->Put(iter->key(), iter->value());
            }
        } else {
            ut->Put(types[i]->GenerateId(), types[i]);
        }
    }
    return GetUnion(ut);
}

int TypeFactory::GetAllTypeId(std::set<int64_t> *all_id) const {
    int i = 0;
    for (auto type : all_types_) {
        all_id->insert(type->GenerateId());
        ++i;
    }
    return i;
}

////////////////////////////////////////////////////////////////////////////////
/// Type
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ bool Type::CanAcceptFrom(mio::Type *type) const {
    return GenerateId() == DCHECK_NOTNULL(type)->GenerateId();
}

/*virtual*/ int64_t Type::GenerateId() const {
    return id_;
}

bool Type::CanBeKey() const {
    return IsIntegral() || IsFloating() || IsString();
}

std::string Type::ToString() const {
    std::string buf;
    MemoryOutputStream stream(&buf);
    ToString(&stream);
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
/// Integral
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ int Integral::placement_size() const {
    return (bitwide_ + 7) / 8;
}

/*virtual*/
int Integral::ToString(TextOutputStream *stream) const {
    return stream->Printf("i%d", bitwide_);
}

////////////////////////////////////////////////////////////////////////////////
/// Floating
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Floating::placement_size() const {
    return (bitwide_ + 7) / 8;
}

/*virtual*/
int Floating::ToString(TextOutputStream *stream) const {
    return stream->Printf("f%d", bitwide_);
}

////////////////////////////////////////////////////////////////////////////////
/// String
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ int String::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int String::ToString(TextOutputStream *stream) const {
    return stream->Write("string");
}

////////////////////////////////////////////////////////////////////////////////
/// Void
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ int Void::placement_size() const {
    DLOG(FATAL) << "void type has no placement size.";
    return 0;
}

/*virtual*/
int Void::ToString(TextOutputStream *stream) const {
    return stream->Write("void");
}

////////////////////////////////////////////////////////////////////////////////
/// Unknown
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ int Unknown::placement_size() const {
    DLOG(FATAL) << "unknown has no placement size.";
    return 0;
}

/*virtual*/
int Unknown::ToString(TextOutputStream *stream) const {
    return stream->Write("unknown");
}

////////////////////////////////////////////////////////////////////////////////
/// Union
////////////////////////////////////////////////////////////////////////////////

/*virtual*/ int Union::placement_size() const {
    return kObjectReferenceSize;
}

int Union::GetAllTypes(std::vector<Type *> *all_types) const {
    all_types->clear();

    TypeMap::Iterator iter(types_);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        all_types->push_back(iter->value());
    }
    return static_cast<int>(all_types->size());
}

/*virtual*/
int Union::ToString(TextOutputStream *stream) const {
    auto rv = stream->Write("[");

    int i = 0;
    Union::TypeMap::Iterator iter(types_);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        if (i++ != 0) {
            rv += stream->Write(",");
        }
        rv += iter->value()->ToString(stream);
    }
    return rv + stream->Write("]");
}

/*virtual*/ bool Union::CanAcceptFrom(Type *type) const {
    if (GenerateId() == type->GenerateId()) {
        return true;
    }
    return types_->Exist(type->GenerateId());
}

/*virtual*/ int64_t Union::GenerateId() const {
    TypeDigest digest;
    digest.Step(TOKEN_UNION);
    if (types_->is_empty()) {
        return digest.value();
    }

    std::vector<int64_t> primary;
    TypeMap::Iterator iter(types_);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        primary.push_back(iter->value()->GenerateId());
    }
    std::sort(primary.begin(), primary.end());
    for (auto val : primary) {
        digest.Step(val);
    }
    return digest.value();
}

////////////////////////////////////////////////////////////////////////////////
/// FunctionPrototype
////////////////////////////////////////////////////////////////////////////////

/*virtual*/
int FunctionPrototype::ToString(TextOutputStream *stream) const {
    auto rv = stream->Write("function (");
    for (int i = 0; i < paramters_->size(); ++i) {
        if (i != 0) {
            rv += stream->Write(",");
        }

        auto param = paramters_->At(i);
        rv += stream->Write(param->param_name());
        rv += stream->Write(":");
        rv += param->param_type()->ToString(stream);
    }
    rv += stream->Write("): ");
    rv += return_type_->ToString(stream);
    return rv;
}

/*virtual*/ int FunctionPrototype::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/ int64_t FunctionPrototype::GenerateId() const {
    TypeDigest digest;

    digest.Step(TOKEN_FUNCTION);
    for (int i = 0; i < paramters_->size(); ++i) {
        digest.Step(paramters_->At(i)->param_type()->GenerateId());
    }
    digest.Step(return_type()->GenerateId() << 4);
    return digest.value();
}

////////////////////////////////////////////////////////////////////////////////
/// Map
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Map::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int Map::ToString(TextOutputStream *stream) const {
    auto rv = stream->Write("map[");
    rv += key_->ToString(stream);
    rv += stream->Write(",");
    rv += value_->ToString(stream);
    return rv + stream->Write("]");
}

/*virtual*/ int64_t Map::GenerateId() const {
    TypeDigest digest;
    digest.Step(TOKEN_MAP);
    digest.Step(key_->GenerateId());
    digest.Step(value_->GenerateId());
    return digest.value();
}

} // namespace mio
