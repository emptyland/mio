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
    simple_types_[0]  = new (zone_) Integral(1,  TOKEN_BOOL);
    simple_types_[1]  = new (zone_) Integral(8,  TOKEN_I8);
    simple_types_[2]  = new (zone_) Integral(16, TOKEN_I16);
    simple_types_[3]  = new (zone_) Integral(32, TOKEN_I32);
    simple_types_[4]  = new (zone_) Integral(64, TOKEN_I64);
    simple_types_[5]  = new (zone_) Floating(32, TOKEN_F32);
    simple_types_[6]  = new (zone_) Floating(64, TOKEN_F64);
    simple_types_[7]  = new (zone_) Void(TOKEN_VOID);
    simple_types_[8]  = new (zone_) Unknown(-1);
    simple_types_[9]  = new (zone_) String(TOKEN_STRING);
    simple_types_[10] = new (zone_) Error(TOKEN_ERROR_TYPE);
    simple_types_[11] = new (zone_) External(TOKEN_EXTERNAL);

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

int TypeFactory::GetAllType(std::map<int64_t, Type *> *all_type) const {
    int i = 0;
    for (auto type : all_types_) {
        all_type->emplace(type->GenerateId(), type);
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
    return IsIntegral() || IsFloating() || IsString() || IsError();
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
    return (bitwide() + 7) / 8;
}

/*virtual*/
int Integral::ToString(TextOutputStream *stream) const {
    return stream->Printf("i%d", bitwide());
}

////////////////////////////////////////////////////////////////////////////////
/// Floating
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Floating::placement_size() const {
    return (bitwide() + 7) / 8;
}

/*virtual*/
int Floating::ToString(TextOutputStream *stream) const {
    return stream->Printf("f%d", bitwide());
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

/*virtual*/ bool Union::MustBeInitialized() const {
    TypeMap::Iterator iter(types_);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        if (iter->value()->IsVoid()) {
            return false;
        }
    }
    return true;
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

std::string FunctionPrototype::GetSignature() const {
    std::string buf;

    buf.append(1, GetSignature(return_type()));
    buf.append(1, ':');
    for (int i = 0; i < paramter_size(); ++i) {
        auto type = paramter(i)->param_type();
        buf.append(1, GetSignature(type));
    }
    return buf;
}

/*static*/ char FunctionPrototype::GetSignature(const Type *type) {
    char s = 0;
    switch (type->type_kind()) {
    #define DEFINE_CASE(kind, sign) \
        case k##kind: \
        s = sign; \
        break;
            DEFINE_TYPE_NODES(DEFINE_CASE)
    #undef DEFINE_CASE
        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
    if (s == 'I') {
        switch (type->AsIntegral()->bitwide()) {
            case 1:  s = '1'; break;
            case 8:  s = '8'; break;
            case 16: s = '7'; break;
            case 32: s = '5'; break;
            case 64: s = '9'; break;
            default:
                DLOG(FATAL) << "noreached!";
                break;
        }
    } else if (s == 'F') {
        switch (type->AsFloating()->bitwide()) {
            case 32: s = '3'; break;
            case 64: s = '6'; break;
            default:
                DLOG(FATAL) << "noreached!";
                break;
        }
    }
    return s;
}

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
/// Array
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Array::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int Array::ToString(TextOutputStream *stream) const {
    auto rv = stream->Write("array[");
    rv += element_->ToString(stream);
    return rv + stream->Write("]");
}

/*virtual*/ int64_t Array::GenerateId() const {
    TypeDigest digest;
    digest.Step(TOKEN_ARRAY);
    digest.Step(element_->GenerateId());
    return digest.value();
}

////////////////////////////////////////////////////////////////////////////////
/// Slice
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Slice::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int Slice::ToString(TextOutputStream *stream) const {
    auto rv = stream->Write("slice[");
    rv += element_->ToString(stream);
    return rv + stream->Write("]");
}

/*virtual*/ int64_t Slice::GenerateId() const {
    TypeDigest digest;
    digest.Step(TOKEN_SLICE);
    digest.Step(element_->GenerateId());
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

////////////////////////////////////////////////////////////////////////////////
/// Error
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int Error::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int Error::ToString(TextOutputStream *stream) const {
    return stream->Write("error");
}

////////////////////////////////////////////////////////////////////////////////
/// External
////////////////////////////////////////////////////////////////////////////////
/*virtual*/ int External::placement_size() const {
    return kObjectReferenceSize;
}

/*virtual*/
int External::ToString(TextOutputStream *stream) const {
    return stream->Write("external");
}

} // namespace mio
