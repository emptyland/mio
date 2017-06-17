#ifndef MIO_NYAA_INSTRUCTIONS_H_
#define MIO_NYAA_INSTRUCTIONS_H_

#include "nyaa-types.h"
#include "raw-string.h"
#include "zone-vector.h"
#include "zone.h"
#include "glog/logging.h"

namespace mio {

#define NAYY_INSTRUCTION_OPS(M) \
    M(Constant)                 \
    M(Add)                      \
    M(Sub)                      \
    M(Mul)                      \
    M(Div)                      \
    M(Mod)                      \
    M(Shl)                      \
    M(Shr)                      \
    M(UShr)                     \
    M(Phi)                      \
    M(Branch)


#define DECLARE_NYAA_INSTRUCTION(name) \
    virtual Opcode opcode() const override { return k##name; } \
    static N##name *cast(NValue *value) { \
        return DCHECK_NOTNULL(value)->opcode() == k##name ? reinterpret_cast<N##name*>(value) : nullptr; \
    } \
    friend class NValueFactory; \
    DISALLOW_IMPLICIT_CONSTRUCTORS(N##name)

class NBasicBlock;
class NUsedListNode;
class NValueFactory;

class TextOutputStream;

////////////////////////////////////////////////////////////////////////////////
/// Base Node
////////////////////////////////////////////////////////////////////////////////

class NValue : public ManagedObject {
public:
    enum Opcode {
    #define DEFINE_ENUM(name) k##name,
        NAYY_INSTRUCTION_OPS(DEFINE_ENUM)
    #undef DEFINE_ENUM
    };

    enum Flag: int {
        kNothing,
        kIsDead,
    };

    NValue(NType type) : type_(type) {}

    DEF_PROP_RW(int, id)
    DEF_PROP_RW(NType, type)
    DEF_PROP_RW(RawStringRef, name)
    DEF_PTR_PROP_RW(NBasicBlock, block)
    DEF_PTR_PROP_RW(NUsedListNode, used_values)

    uint32_t flags() const { return flags_; }
    void set_flag(Flag f) { flags_ |= (1 << f); }
    bool test_flag(Flag f) { return flags_ & (1 << f); }

    virtual int position() const = 0;

    virtual Opcode opcode() const = 0;

    virtual void ToString(TextOutputStream *stream) const = 0;

    virtual int operand_size() const { return 0; }

    virtual NValue *operand(int i) const { return nullptr; }

//    void Kill() { Kill(nullptr); }
//
//    void Kill(Zone *zone);

    DISALLOW_IMPLICIT_CONSTRUCTORS(NValue)
private:
    RawStringRef   name_ = RawString::kEmpty;
    int            id_ = -1;
    uint32_t       flags_ = 0;
    NType          type_;
    NBasicBlock   *block_ = nullptr;
    NUsedListNode *used_values_ = nullptr;

}; // class NValue


class NInstruction : public NValue {
public:
    DEF_PTR_GETTER(NInstruction, next)
    DEF_PTR_GETTER(NInstruction, prev)
    DEF_SETTER(int, position)

    virtual int position() const override { return position_; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(NInstruction)
protected:
    explicit NInstruction(NType type)
        : NValue(type) {}

private:
    NInstruction *next_ = nullptr;
    NInstruction *prev_ = nullptr;
    int position_ = -1;
}; // class NInstruction


template<int N>
class NInstructionTemplate : public NInstruction {
public:
    virtual int operand_size() const override { return N; }

    virtual NValue *operand(int i) const override {
        DCHECK_GE(i, 0);
        DCHECK_LT(i, N);
        return inputs_[i];
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(NInstructionTemplate)
protected:
    explicit NInstructionTemplate(NType type)
        : NInstruction(type) {
        memset(inputs_, 0, sizeof(NValue *) * N);
    }

    void set_operand(int i, NValue *value) {
        DCHECK_GE(i, 0);
        DCHECK_LT(i, N);
        inputs_[i] = DCHECK_NOTNULL(value);
    }

private:
    NValue *inputs_[N];
}; // class NInstructionTemplate


////////////////////////////////////////////////////////////////////////////////
/// Nyaa Instructions
////////////////////////////////////////////////////////////////////////////////

class NPhi : public NValue {
public:
    DEF_ZONE_VECTOR_PROP_RWA(NValue *, input)

    DECLARE_NYAA_INSTRUCTION(Phi)
private:
    explicit NPhi(NType type, Zone *zone)
        : NValue(type)
        , inputs_(DCHECK_NOTNULL(zone)) {}

    ZoneVector<NValue *> inputs_;
}; // class NPhi

class NBranch : public NInstructionTemplate<1> {
public:
    DEF_PTR_GETTER(NBasicBlock, true_target)
    DEF_PTR_GETTER(NBasicBlock, false_target)

    NValue *condition() const { return operand(0); }

    DECLARE_NYAA_INSTRUCTION(Branch)
private:
    NBranch(NType type, NValue *condition, NBasicBlock *true_target,
            NBasicBlock *false_target)
        : NInstructionTemplate(type)
        , true_target_(DCHECK_NOTNULL(true_target))
        , false_target_(DCHECK_NOTNULL(false_target)) {
        set_operand(0, condition);
    }

    NBasicBlock *true_target_;
    NBasicBlock *false_target_;
}; // class NBranch

class NConstant : public NInstructionTemplate<0> {
public:

    DECLARE_NYAA_INSTRUCTION(Constant)
private:
    NConstant(NType type) : NInstructionTemplate(type) {}
}; // class NConstant

class NAdd : public NInstructionTemplate<2> {
public:
    virtual void ToString(TextOutputStream *stream) const override;

    NValue *lhs() const { return operand(0); }
    NValue *rhs() const { return operand(1); }

    DECLARE_NYAA_INSTRUCTION(Add)
private:
    NAdd(NType type, NValue *lhs, NValue *rhs)
        : NInstructionTemplate(type) {
        set_operand(0, lhs);
        set_operand(1, rhs);
    }
}; // class NAdd


////////////////////////////////////////////////////////////////////////////////
/// Toolkit
////////////////////////////////////////////////////////////////////////////////
class NUsedListNode : public ManagedObject {
public:
    NUsedListNode(NUsedListNode *tail, NValue *value, int index)
        : tail_(tail)
        , value_(DCHECK_NOTNULL(value))
        , index_(index) {}

    NUsedListNode *tail() const { return tail_; }
    void set_tail(NUsedListNode *tail) { tail_ = tail; }

    NValue *value() const { return value_; }

    int index() const { return index_; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(NUsedListNode)
private:
    NUsedListNode *tail_;
    NValue        *value_;
    int            index_;
};

} // namespace mio

#endif // MIO_NYAA_INSTRUCTIONS_H_
