#ifndef MIO_TYPES_H_
#define MIO_TYPES_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"
#include "token.h"
#include <set>

namespace mio {

#define DEFINE_TYPE_NODES(M) \
    M(Map) \
    M(FunctionPrototype) \
    M(Union) \
    M(Integral) \
    M(Floating) \
    M(String) \
    M(Void) \
    M(Unknown)

class Type;
    class Map;
    class FunctionPrototype;
    class Integral;
    class Floating;
    class String;
    class Struct;
    class Array;
    class Void;
    class Union;
    class Unknown;

class TypeFactory;
class TextOutputStream;

#define DECLARE_TYPE(name) \
    virtual Kind type_kind() const override { return k##name; } \
    virtual int ToString(TextOutputStream *stream) const override; \
    virtual int placement_size() const override; \
    friend class TypeFactory;

class Type : public ManagedObject {
public:
    enum Kind: int {
    #define Type_Kind_ENUM_DEFINE(name) k##name,
        DEFINE_TYPE_NODES(Type_Kind_ENUM_DEFINE)
    #undef  Type_Kind_ENUM_DEFINE
        MAX_KIND,
    };

#define Type_TYPE_ASSERT(kind) \
    bool Is##kind () const { return type_kind() == k##kind; } \
    kind *As##kind() { \
        return Is##kind() ? reinterpret_cast<kind *>(this) : nullptr; \
    } \
    const kind *As##kind() const { \
        return Is##kind() ? reinterpret_cast<const kind *>(this) : nullptr; \
    }

    DEFINE_TYPE_NODES(Type_TYPE_ASSERT)

#undef Type_TYPE_ASSERT

    virtual Kind type_kind() const = 0;

    virtual bool CanAcceptFrom(Type *type) const;

    virtual int64_t GenerateId() const;

    virtual int ToString(TextOutputStream *stream) const = 0;

    bool CanBeKey() const;
    bool CanNotBeKey() const { return !CanBeKey(); }

    std::string ToString() const;

    virtual int placement_size() const = 0;

    bool is_numeric() const { return IsIntegral() || IsFloating(); }

    bool is_primitive() const { return is_numeric(); }

    bool is_object() const {
        return IsString() || IsMap() || IsUnion() || IsFunctionPrototype();
    }

    friend class TypeFactory;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Type);
protected:
    Type(int64_t id) : id_(id) {}

    int64_t id_;
}; // class Type


class Void : public Type {
public:

    DECLARE_TYPE(Void)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Void)
private:
    Void(int64_t id) : Type(id) {}
}; // class Void


class Integral : public Type {
public:
    DEF_GETTER(int, bitwide)

    DECLARE_TYPE(Integral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Integral);
private:
    Integral(int bitwide, int64_t id)
        : Type(id)
        , bitwide_(bitwide) {}

    int bitwide_;
}; // class Integral


class Floating : public Type {
public:
    DEF_GETTER(int, bitwide)

    DECLARE_TYPE(Floating)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Floating);
private:
    Floating(int bitwide, int64_t id)
        : Type(id)
        , bitwide_(bitwide) {}

    int bitwide_;
}; // class Floating


class Unknown : public Type {
public:
    DECLARE_TYPE(Unknown)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Unknown);

private:
    Unknown(int64_t id) : Type(id) {}
}; // class Unknown


class String : public Type {
public:
    DECLARE_TYPE(String)
    DISALLOW_IMPLICIT_CONSTRUCTORS(String)

private:
    String(int64_t id) : Type(id) {}
}; // class String


class Paramter : public ManagedObject {
public:
    Type *param_type() const { return param_type_; }
    void set_param_type(Type *type) { param_type_ = DCHECK_NOTNULL(type); }

    RawStringRef param_name() const { return param_name_; }

    bool has_name() const { return param_name_ != RawString::kEmpty; }

    friend class TypeFactory;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Paramter);
private:
    Paramter(RawStringRef name, Type *type)
        : param_type_(DCHECK_NOTNULL(type))
        , param_name_(DCHECK_NOTNULL(name)) {}

    Type *param_type_;
    RawStringRef param_name_;
}; // class Paramter


class Map : public Type {
public:
    Type *key() const   { return key_; }
    void set_key(Type *key) { key_ = DCHECK_NOTNULL(key); }

    Type *value() const { return value_; }
    void set_value(Type *value) { value_ = DCHECK_NOTNULL(value); }

    DECLARE_TYPE(Map)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Map)

    virtual int64_t GenerateId() const override;
private:
    Map(Type *key, Type *value)
        : Type(0)
        , key_(key)
        , value_(value) { id_ = GenerateId(); }

    Type *key_;
    Type *value_;
}; // class Map


class FunctionPrototype : public Type {
public:
    ZoneVector<Paramter *> *mutable_paramters() { return paramters_; }
    Type *return_type() const { return return_type_; }
    void set_return_type(Type *type) { return_type_ = type; }

    virtual int64_t GenerateId() const override;

    DECLARE_TYPE(FunctionPrototype)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionPrototype)
private:
    FunctionPrototype(ZoneVector<Paramter *> *paramters, Type *return_type)
        : Type(0)
        , paramters_(DCHECK_NOTNULL(paramters))
        , return_type_(DCHECK_NOTNULL(return_type)) { id_ = GenerateId(); }
    
    ZoneVector<Paramter *> *paramters_;
    Type *return_type_;
}; // class FunctionPrototype


class Union : public Type {
public:
    typedef ZoneHashMap<int64_t, Type *> TypeMap;

    TypeMap *mutable_types() { return types_; }

    int GetAllTypes(std::vector<Type *> *all_types) const;

    bool CanBe(Type *type) { return types_->Exist(type->GenerateId()); }

    virtual bool CanAcceptFrom(Type *type) const override;

    virtual int64_t GenerateId() const override;

    DECLARE_TYPE(Union)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Union)
private:
    Union(TypeMap *types)
        : Type(0)
        , types_(types) { id_ = GenerateId(); }

    // [type_id: type]
    TypeMap *types_;
}; // class Union


class TypeFactory {
public:
    static const int kNumberOfIntegralTypes = 5;
    static const int kNumberOfFloatingTypes = 2;
    static const int kMaxSimpleTypes = kNumberOfIntegralTypes
                                     + kNumberOfFloatingTypes
                                     + 1  // String
                                     + 1  // Void
                                     + 1; // Unknown

    TypeFactory(Zone *zone);

    Zone *zone() const { return zone_; }

    Integral *GetI1() const { return simple_types_[0]->AsIntegral(); }
    Integral *GetI8() const { return simple_types_[1]->AsIntegral(); }
    Integral *GetI16() const { return simple_types_[2]->AsIntegral(); }
    Integral *GetI32() const { return simple_types_[3]->AsIntegral(); }
    Integral *GetI64() const { return simple_types_[4]->AsIntegral(); }

    Floating *GetF32() const { return simple_types_[5]->AsFloating(); }
    Floating *GetF64() const { return simple_types_[6]->AsFloating(); }

    Void *GetVoid() const { return simple_types_[7]->AsVoid(); }
    Unknown *GetUnknown() const { return simple_types_[8]->AsUnknown(); }
    String *GetString() const { return simple_types_[9]->AsString(); }

    FunctionPrototype *GetFunctionPrototype(ZoneVector<Paramter *> *paramters,
                                            Type *return_type) {
        return Record(new (zone_) FunctionPrototype(paramters, return_type));
    }

    Union *GetUnion(ZoneHashMap<int64_t, Type *> *types) {
        return Record(new (zone_) Union(types));
    }

    Map *GetMap(Type *key, Type *value) {
        return Record(new (zone_) Map(key, value));
    }

    // [t1, t2] and [t2, t3, t4] merge to
    // [t1, t2, t3, t4]
    Union *MergeToFlatUnion(Type **types, int n);

    Paramter *CreateParamter(const std::string &name, Type *type) {
        return new (zone_) Paramter(RawString::Create(name, zone_), type);
    }

    int GetAllTypeId(std::set<int64_t> *all_id) const;

private:
    template<class T>
    T *Record(T *type) {
        all_types_.push_back(type);
        return type;
    }

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
    Type *simple_types_[kMaxSimpleTypes];
    Zone *zone_;
    std::vector<Type *> all_types_;
};

} // namespace mio

#endif // MIO_TYPES_H_
