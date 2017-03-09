#ifndef MIO_TYPES_H_
#define MIO_TYPES_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"
#include "token.h"

namespace mio {

#define DEFINE_TYPE_NODES(M) \
    M(FunctionPrototype) \
    M(Union) \
    M(Integral) \
    M(String) \
    M(Void) \
    M(Unknown)

class Type;
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

#define DECLARE_TYPE(name) \
    virtual bool Is##name() const override { return true; } \
    friend class TypeFactory;

class Type : public ManagedObject {
public:
#define Type_TYPE_ASSERT(kind) \
    virtual bool Is##kind () const { return false; } \
    kind *As##kind() { \
        return Is##kind() ? reinterpret_cast<kind *>(this) : nullptr; \
    } \
    const kind *As##kind() const { \
        return Is##kind() ? reinterpret_cast<const kind *>(this) : nullptr; \
    }

    DEFINE_TYPE_NODES(Type_TYPE_ASSERT)

#undef Type_TYPE_ASSERT

    // TODO: floating
    bool is_numeric() const { return IsIntegral(); }

    int64_t id() const { return id_; }

    bool CanAcceptFrom(Type *type) { return id() == DCHECK_NOTNULL(type)->id(); }

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


class FunctionPrototype : public Type {
public:
    ZoneVector<Paramter *> *mutable_paramters() { return paramters_; }
    Type *return_type() const { return return_type_; }
    void set_return_type(Type *type) { return_type_ = type; }

    void UpdateId();

    DECLARE_TYPE(FunctionPrototype)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionPrototype)
private:
    FunctionPrototype(ZoneVector<Paramter *> *paramters, Type *return_type,
                      int id)
        : Type(id)
        , paramters_(DCHECK_NOTNULL(paramters))
        , return_type_(DCHECK_NOTNULL(return_type)) {}
    
    ZoneVector<Paramter *> *paramters_;
    Type *return_type_;
}; // class FunctionPrototype


class Union : public Type {
public:
    typedef ZoneHashMap<int64_t, Type *> TypeMap;

    TypeMap *mutable_types() { return types_; }

    int GetAllTypes(std::vector<Type *> *all_types) const;

    static int64_t GenerateId(TypeMap *types);

    DECLARE_TYPE(Union)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Union)
private:
    Union(TypeMap *types)
        : Type(GenerateId(DCHECK_NOTNULL(types)))
        , types_(types) {
    }

    // [type_id: type]
    TypeMap *types_;
}; // class Union


class TypeFactory {
public:
    static const int kMaxIntegralTypes = 5;

    TypeFactory(Zone *zone);

    Integral *GetI1() const { return integral_types_[0]; }
    Integral *GetI8() const { return integral_types_[1]; }
    Integral *GetI16() const { return integral_types_[2]; }
    Integral *GetI32() const { return integral_types_[3]; }
    Integral *GetI64() const { return integral_types_[4]; }

    Void *GetVoid() const { return void_type_; }
    Unknown *GetUnknown() const { return unknown_type_; }
    String *GetString() const { return string_type_; }

    FunctionPrototype *GetFunctionPrototype(ZoneVector<Paramter *> *paramters,
                                            Type *return_type);

    Union *GetUnion(ZoneHashMap<int64_t, Type *> *types);

    Paramter *CreateParamter(const std::string &name, Type *type) {
        return new (zone_) Paramter(RawString::Create(name, zone_), type);
    }

private:

    // i1
    // i8
    // i16
    // i32
    // i64
    Integral *integral_types_[kMaxIntegralTypes];
    Void     *void_type_;
    Unknown  *unknown_type_;
    String   *string_type_;

    Zone *zone_;
};

} // namespace mio

#endif // MIO_TYPES_H_
