#include "bitcode-emitter.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode.h"
#include "vm-objects.h"
#include "vm-object-factory.h"
#include "vm-function-register.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"
#include "text-output-stream.h"
#include "glog/logging.h"
#include <stack>

#define MIO_SMI_BYTES_TO_BITS(M) \
    M(1, 8) \
    M(2, 16) \
    M(4, 32)

#define MIO_INT_BYTES_TO_BITS(M) \
    MIO_SMI_BYTES_TO_BITS(M) \
    M(8, 64)

#define MIO_FLOAT_BYTES_TO_BITS(M) \
    M(4, 32) \
    M(8, 64)

#define MIO_INT_BYTES_SWITCH(size, M) \
    switch (size) { \
        MIO_INT_BYTES_TO_BITS(M) \
        default: \
            DLOG(FATAL) << "noreached! bad size: " << (size); \
            break; \
    }

#define MIO_SMI_BYTES_SWITCH(size, M) \
    switch (size) { \
        MIO_SMI_BYTES_TO_BITS(M) \
        default: \
            DLOG(FATAL) << "noreached! bad size: " << (size); \
            break; \
    }

#define MIO_FLOAT_BYTES_SWITCH(size, M) \
    switch (size) { \
        MIO_FLOAT_BYTES_TO_BITS(M) \
        default: \
            DLOG(FATAL) << "noreached! bad size: " << (size); \
            break; \
    }

namespace mio {

struct VMValue {
    BCSegment segment;
    int       offset;
    int       size;

    bool is_void() const { return offset < 0 && size < 0; }

    static VMValue Void() { return { MAX_BC_SEGMENTS, -1, -1, }; }
    static VMValue Zero() { return { BC_CONSTANT_SEGMENT, 0, 8 }; }
};

class EmittedScope {
public:
    EmittedScope(EmittedScope **current, FunctionPrototype *prototype)
        : code_(new MemorySegment)
        , builder_(code_)
        , current_(DCHECK_NOTNULL(current))
        , saved_(*DCHECK_NOTNULL(current))
        , prototype_(DCHECK_NOTNULL(prototype)) {
        *current_ = this;
    }

    ~EmittedScope() {
        *current_ = saved_;
        delete code_;
    }

    DEF_GETTER(int, p_stack_size)
    DEF_GETTER(int, o_stack_size)
    FunctionPrototype *prototype() const { return prototype_; }

    int MakePrimitiveRoom(int size) {
        auto base = p_stack_size_;
        p_stack_size_ += AlignDownBounds(kAlignmentSize, size);
        return base;
    }

    int MakeObjectRoom() {
        auto base = o_stack_size_;
        o_stack_size_ += AlignDownBounds(kAlignmentSize, sizeof(HeapObject *));
        return base;
    }

    VMValue MakePrimitiveValue(int size) {
        return {
            .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
            .offset  = MakePrimitiveRoom(size),
            .size    = size,
        };
    }

    BitCodeBuilder *builder() { return &builder_; }

    EmittedScope *prev() const { return saved_; }

    Variable *preallocated() const { return preallocated_; }

    void set_preallocated(Variable *inst) { preallocated_ = inst; }

private:
    MemorySegment *code_;
    EmittedScope **current_;
    EmittedScope *saved_;
    int p_stack_size_ = 0;
    int o_stack_size_ = 0;
    FunctionPrototype *prototype_;
    BitCodeBuilder builder_;
    Variable *preallocated_;
};

class EmittingAstVisitor : public DoNothingAstVisitor {
public:
    EmittingAstVisitor(RawStringRef module_name, BitCodeEmitter *emitter)
        : module_name_(DCHECK_NOTNULL(module_name))
        , emitter_(DCHECK_NOTNULL(emitter)) {
    }

    virtual void VisitFunctionDefine(FunctionDefine *node) override;
    virtual void VisitFunctionLiteral(FunctionLiteral *node) override;
    virtual void VisitReturn(Return *node) override;
    virtual void VisitBlock(Block *node) override;
    virtual void VisitCall(Call *node) override;
    virtual void VisitValDeclaration(ValDeclaration *node) override;
    virtual void VisitVarDeclaration(VarDeclaration *node) override;
    virtual void VisitIfOperation(IfOperation *node) override;
    virtual void VisitUnaryOperation(UnaryOperation *node) override;
    virtual void VisitBinaryOperation(BinaryOperation *node) override;
    virtual void VisitVariable(Variable *node) override;
    virtual void VisitStringLiteral(StringLiteral *node) override;
    virtual void VisitSmiLiteral(SmiLiteral *node) override;
    virtual void VisitFloatLiteral(FloatLiteral *node) override;

    BitCodeBuilder *builder() { return DCHECK_NOTNULL(current_)->builder(); }

    EmittedScope **mutable_current() { return &current_; }

    void set_unit_name(RawStringRef name) { unit_name_ = DCHECK_NOTNULL(name); }

    VMValue Emit(AstNode *node) {
        DCHECK_NOTNULL(node)->Accept(this);
        auto rv = eval_value();
        PopValue();
        return rv;
    }

    VMValue EmitLoadIfNeeded(const VMValue value);

    VMValue GetOrNewString(RawStringRef raw, Local<MIOString> *obj) {
        return GetOrNewString(raw->c_str(), raw->size(), obj);
    }

    VMValue GetOrNewString(const std::string &s, Local<MIOString> *obj) {
        return GetOrNewString(s.c_str(), static_cast<int>(s.size()), obj);
    }

    VMValue GetOrNewString(const char *z, int n, Local<MIOString> *obj);
private:
    VMValue EmitIntegralAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);
    VMValue EmitFloatingCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);

    VMValue EmitLoadPrimitiveMakeRoom(VMValue src);

    void EmitLoadOrMove(const VMValue &dest, const VMValue &src);

    VMValue EmitLoadMakeRoom(const VMValue &src);

    VMValue EmitStoreMakeRoom(const VMValue &src);

    VMValue GetEmptyObject(Type *type);

    VMValue eval_value() const {
        DCHECK(!value_stack_.empty());
        return value_stack_.top();
    }

    void PopValue() {
        DCHECK(!value_stack_.empty());
        value_stack_.pop();
    }

    void PushValue(const VMValue &value) {
        value_stack_.push(value);
    }

    bool TraceDeclaration(Declaration *decl) {
        if (emitter_->emitted_.find(decl) != emitter_->emitted_.end()) {
            return true;
        }
        emitter_->emitted_.insert(decl);
        return false;
    }

    BCComparator Operator2Comparator(Operator op) {
        switch (op) {
    #define DEFINE_CASE(name) \
        case OP_##name: return CC_##name;

            VM_COMPARATOR(DEFINE_CASE)
    #undef DEFINE_CASE
        default:
            DLOG(FATAL) << "noreached! bad op: " << op;
            break;
        }
        return MAX_CC_COMPARATORS;
    }

    RawStringRef module_name_;
    RawStringRef unit_name_ = RawString::kEmpty;
    BitCodeEmitter *emitter_;
    EmittedScope *current_ = nullptr;
    std::stack<VMValue> value_stack_;
};

void EmittingAstVisitor::VisitFunctionDefine(FunctionDefine *node) {
    if (TraceDeclaration(node)) {
        PushValue(VMValue::Void());
        return;
    }

    auto scope = node->scope();
    auto full_name = scope->MakeFullName(node->name());
    auto entry = emitter_->function_register_->FindOrInsert(full_name.c_str());
    entry->set_is_native(node->is_native());
    entry->set_offset(emitter_->o_global_->size());
    emitter_->o_global_->Add(static_cast<MIOFunction *>(nullptr));

    node->instance()->set_offset(entry->offset());
    node->instance()->set_bind_kind(Variable::GLOBAL);

    Local<MIOString> name;
    GetOrNewString(full_name, &name);
    if (node->is_native()) {
        auto obj = emitter_->object_factory_->CreateNativeFunction("::", nullptr);
        emitter_->o_global_->Set(entry->offset(), obj.get());

        obj->SetName(name.get());
    } else {
        current_->set_preallocated(node->instance());
        auto value = Emit(node->function_literal());
        current_->set_preallocated(nullptr);

        DCHECK_EQ(BC_GLOBAL_OBJECT_SEGMENT, value.segment);
        auto func = make_local(emitter_->o_global_->Get<HeapObject *>(value.offset)->AsNormalFunction());
        DCHECK(!func.empty());

        func->SetName(name.get());
    }

    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitFunctionLiteral(FunctionLiteral *node) {
    EmittedScope info(&current_, node->prototype());

    auto prototype = node->prototype();
    auto scope = node->scope();

    // placement frame instruction
    auto frame_placement = builder()->debug();

    for (int i = 0; i < prototype->mutable_paramters()->size(); ++i) {
        auto paramter = prototype->mutable_paramters()->At(i);

        auto var = scope->FindOrNullLocal(paramter->param_name());
        var->set_bind_kind(Variable::LOCAL);
        if (var->type()->is_primitive()) {
            var->set_offset(info.MakePrimitiveRoom(var->type()->placement_size()));
        } else {
            var->set_offset(info.MakeObjectRoom());
        }
    }

    if (prototype->return_type()->IsVoid()) {
        node->body()->Accept(this);
        builder()->ret();
    } else {
        auto result = Emit(node->body());
        if (node->is_assignment()) {
            auto size = prototype->return_type()->placement_size();
            if (prototype->return_type()->is_primitive()) {
                VMValue dest;
                dest.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
                dest.size    = size;
                dest.offset  = -size;
                EmitLoadOrMove(dest, result);
            } else {
                VMValue dest;
                dest.segment = BC_LOCAL_OBJECT_SEGMENT;
                dest.size    = size;
                dest.offset  = -size;
                EmitLoadOrMove(dest, result);
            }
            builder()->ret();
        }
    }

    // refill frame instruction
    auto frame = BitCodeBuilder::MakeS2AddrBC(BC_frame, info.p_stack_size(),
                                              info.o_stack_size());
    memcpy(builder()->code()->offset(frame_placement * sizeof(uint64_t)),
           &frame,
           sizeof(frame));

    auto obj = emitter_->object_factory_->CreateNormalFunction(builder()->code()->offset(0),
                                                               builder()->code()->size());
    VMValue value = {
        .segment = BC_GLOBAL_OBJECT_SEGMENT,
        .size    = kObjectReferenceSize,
    };
    if (current_->prev() && current_->prev()->preallocated()) {
        value.offset = current_->prev()->preallocated()->offset();
        emitter_->o_global_->Set(value.offset, obj.get());
    } else {
        value.offset = emitter_->o_global_->size();
        emitter_->o_global_->Add(obj.get());
    }
    PushValue(value);
}

void EmittingAstVisitor::VisitBlock(Block *node) {
    for (int i = 0; i < node->mutable_body()->size() - 1; ++i) {
        Emit(node->mutable_body()->At(i));
    }
    if (node->mutable_body()->is_not_empty()) {
        node->mutable_body()->last()->Accept(this);
    }
}

void EmittingAstVisitor::VisitReturn(Return *node) {
    if (node->has_return_value()) {
        auto result = Emit(node->expression());

        auto size = current_->prototype()->return_type()->placement_size();
        if (current_->prototype()->return_type()->is_primitive()) {
            VMValue dest;
            dest.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
            dest.size    = size;
            dest.offset  = -size;
            EmitLoadOrMove(dest, result);
        } else {
            VMValue dest;
            dest.segment = BC_LOCAL_OBJECT_SEGMENT;
            dest.size    = size;
            dest.offset  = -size;
            EmitLoadOrMove(dest, result);
        }
    }
    builder()->ret();
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitCall(Call *node) {
    auto expr = Emit(node->expression());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, expr.segment);

    auto proto = DCHECK_NOTNULL(node->callee_type()->AsFunctionPrototype());
    if (!proto->return_type()->IsVoid()) {
        auto result_size = proto->return_type()->placement_size();
        if (proto->return_type()->is_primitive()) {
            if (current_->p_stack_size() == 0) {
                current_->MakePrimitiveRoom(result_size);
            }
        } else {
            if (current_->o_stack_size() == 0) {
                current_->MakeObjectRoom();
            }
        }
    }

    std::vector<VMValue> arguments;
    for (int i = 0; i < node->mutable_arguments()->size(); ++i) {
        auto argument = node->mutable_arguments()->At(i);
        arguments.push_back(Emit(argument));
    }
    auto p_base = current_->p_stack_size(), o_base = current_->o_stack_size();
    for (const auto &value : arguments) {
        switch (value.segment) {
            case BC_CONSTANT_SEGMENT:
            case BC_GLOBAL_PRIMITIVE_SEGMENT:
            case BC_LOCAL_PRIMITIVE_SEGMENT:
                EmitLoadPrimitiveMakeRoom(value);
                break;

            case BC_GLOBAL_OBJECT_SEGMENT:
                builder()->load_o(current_->MakeObjectRoom(), value.offset);
                break;

            case BC_LOCAL_OBJECT_SEGMENT:
                builder()->mov_o(current_->MakeObjectRoom(), value.offset);
                break;

            default:
                break;
        }
    }

    VMValue result;
    if (!proto->return_type()->IsVoid()) {
        auto result_size = proto->return_type()->placement_size();
        if (proto->return_type()->is_primitive()) {
            result = {
                .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
                .offset  = p_base - result_size,
                .size    = result_size,
            };
        } else {
            result = {
                .segment = BC_LOCAL_OBJECT_SEGMENT,
                .offset  = o_base - result_size,
                .size    = result_size,
            };
        }
    }
    builder()->call_val(p_base, o_base, expr.offset);
    if (proto->return_type()->IsVoid()) {
        PushValue(VMValue::Void());
    } else {
        PushValue(result);
    }
}

void EmittingAstVisitor::VisitValDeclaration(ValDeclaration *node) {
    if (TraceDeclaration(node)) {
        PushValue(VMValue::Void());
        return;
    }

    // local vals
    VMValue value;
    if (node->scope()->type() == MODULE_SCOPE ||
        node->scope()->type() == UNIT_SCOPE) {

        VMValue tmp;
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = VMValue::Zero();
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = GetEmptyObject(node->type());
            }
        }
        value = EmitStoreMakeRoom(tmp);
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::GLOBAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
    } else {
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = VMValue::Zero();
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = GetEmptyObject(node->type());
            }
        }
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::LOCAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
    }
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitVarDeclaration(VarDeclaration *node) {
    if (TraceDeclaration(node)) {
        PushValue(VMValue::Void());
        return;
    }

    // local vars
    VMValue value;
    if (node->scope()->type() == MODULE_SCOPE ||
        node->scope()->type() == UNIT_SCOPE) {

        VMValue tmp;
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = VMValue::Zero();
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = GetEmptyObject(node->type());
            }
        }
        value = EmitStoreMakeRoom(tmp);
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::GLOBAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
    } else {
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = VMValue::Zero();
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = GetEmptyObject(node->type());
            }
        }
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::LOCAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
    }
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitIfOperation(IfOperation *node) {
    auto cond = Emit(node->condition());
    DCHECK(cond.segment == BC_LOCAL_PRIMITIVE_SEGMENT);

    auto outter = builder()->jz(cond.offset, builder()->pc());

    if (node->has_else()) {
        bool need_union = node->then_type()->id() != node->else_type()->id();

        VMValue val;
        auto then_val = Emit(node->then_statement());
        if (need_union) {
            val.segment = BC_LOCAL_OBJECT_SEGMENT;
            val.offset  = current_->MakeObjectRoom();
            val.size    = kObjectReferenceSize;
            if (node->then_type()->IsVoid()) {
                builder()->oop(OO_UnionVoid, val.offset, 0, 0);
            } else if (node->then_type()->is_primitive()) {
                builder()->oop(OO_UnionOrMergePrimitive, val.offset, then_val.offset, 0);
            } else {
                builder()->oop(OO_UnionOrMergeObject, val.offset, then_val.offset, 0);
            }
        }
        auto leave = builder()->jmp(builder()->pc());
        // bind outter
        builder()->FillPlacement(outter, BitCodeBuilder::Make3AddrBC(BC_jz, 0, cond.offset, builder()->pc() - outter));

        auto else_val = Emit(node->else_statement());
        if (need_union) {
            if (node->else_type()->IsVoid()) {
                builder()->oop(OO_UnionVoid, val.offset, 0, 0);
            } else if (node->else_type()->is_primitive()) {
                builder()->oop(OO_UnionOrMergePrimitive, val.offset, else_val.offset, 0);
            } else {
                builder()->oop(OO_UnionOrMergeObject, val.offset, else_val.offset, 0);
            }
        } else {
            if (node->then_type()->IsVoid()) {
                val = VMValue::Void();
            } else if (node->then_type()->is_primitive()) {
                val = then_val;
                EmitLoadOrMove(val, else_val);
            }
        }

        // bind leave
        builder()->FillPlacement(leave, BitCodeBuilder::Make3AddrBC(BC_jmp, 0, 0, builder()->pc() - leave));
        PushValue(val);
    } else {
        VMValue val;
        // then
        auto then_val = Emit(node->then_statement());
        val.segment = BC_LOCAL_OBJECT_SEGMENT;
        val.offset  = current_->MakeObjectRoom();
        val.size    = kObjectReferenceSize;
        if (node->then_type()->IsVoid()) {
            builder()->oop(OO_UnionVoid, val.offset, 0, 0);
        } else if (node->then_type()->is_primitive()) {
            builder()->oop(OO_UnionOrMergePrimitive, val.offset, then_val.offset, 0);
        } else {
            builder()->oop(OO_UnionOrMergeObject, val.offset, then_val.offset, 0);
        }
        auto leave = builder()->jmp(builder()->pc());

        // else
        // bind outter
        builder()->FillPlacement(outter, BitCodeBuilder::Make3AddrBC(BC_jz, 0, cond.offset, builder()->pc() - outter));
        builder()->oop(OO_UnionVoid, val.offset, 0, 0);

        // bnd leave
        builder()->FillPlacement(leave, BitCodeBuilder::Make3AddrBC(BC_jmp, 0, 0, builder()->pc() - leave));
        PushValue(val);
    }
}

void EmittingAstVisitor::VisitUnaryOperation(UnaryOperation *node) {
    auto value = Emit(node->operand());
    switch (node->op()) {
        case OP_MINUS:
            // TODO:
            break;

        case OP_NOT:
            // TODO:
            break;

        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
    PushValue(value);
}

void EmittingAstVisitor::VisitBinaryOperation(BinaryOperation *node) {

    switch (node->op()) {
        case OP_ADD:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitIntegralAdd(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitFloatingAdd(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            }
            break;

        case OP_SUB:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitIntegralSub(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitFloatingSub(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            }
            break;

        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitIntegralCmp(node->lhs_type(), node->lhs(),
                                          node->rhs(), node->op()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->id(), node->rhs_type()->id());

                PushValue(EmitFloatingCmp(node->lhs_type(), node->lhs(),
                                          node->rhs(), node->op()));
            }
            break;

            // TODO: other operator
        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
}

void EmittingAstVisitor::VisitVariable(Variable *node) {
    if (node->bind_kind() == Variable::UNBINDED) {
        DCHECK_EQ(MODULE_SCOPE, node->scope()->type());
        Emit(node->declaration());
    }
    DCHECK_NE(Variable::UNBINDED, node->bind_kind()) << node->name()->ToString();

    VMValue value;
    if (node->type()->is_primitive()) {
        if (node->bind_kind() == Variable::LOCAL ||
            node->bind_kind() == Variable::ARGUMENT) {
            value.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
            value.size   = node->type()->placement_size();
            value.offset = node->offset();
        } else if (node->bind_kind() == Variable::GLOBAL) {
            VMValue tmp = {
                .segment = BC_GLOBAL_PRIMITIVE_SEGMENT,
                .size    = node->type()->placement_size(),
                .offset  = node->offset(),
            };
            value = EmitLoadMakeRoom(tmp);
        }
    } else {
        if (node->bind_kind() == Variable::LOCAL ||
            node->bind_kind() == Variable::ARGUMENT) {
            value.segment = BC_LOCAL_OBJECT_SEGMENT;
            value.size   = node->type()->placement_size();
            value.offset = node->offset();
        } else if (node->bind_kind() == Variable::GLOBAL) {
            VMValue tmp = {
                .segment = BC_GLOBAL_OBJECT_SEGMENT,
                .size    = value.size,
                .offset  = node->offset(),
            };
            value = EmitLoadMakeRoom(tmp);
        }
    }

    PushValue(value);
}

void EmittingAstVisitor::VisitStringLiteral(StringLiteral *node) {
    PushValue(EmitLoadMakeRoom(GetOrNewString(node->data(), nullptr)));
}

void EmittingAstVisitor::VisitSmiLiteral(SmiLiteral *node) {
    VMValue dest = {
        .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
        .offset  = 0,
        .size    = (node->bitwide() + 7) / 8,
    };
    dest.offset = current_->MakePrimitiveRoom(dest.size);

    switch (node->bitwide()) {
    #define DEFINE_CASE(byte, bit) \
        case bit: \
            builder()->load_i##bit##_imm(dest.offset, node->i##bit ()); \
            break;
        MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
    #undef DEFINE_CASE
        case 64: {
            VMValue src = {
                .segment = BC_CONSTANT_SEGMENT,
                .offset  = emitter_->constants_->size(),
                .size    = (node->bitwide() + 7) / 8,
            };
            emitter_->constants_->Add(node->i64());
            EmitLoadOrMove(dest, src);
        } break;
        default:
            DLOG(FATAL) << "noreached! bitwide = " << node->bitwide();
            break;
    }
    PushValue(dest);
}

void EmittingAstVisitor::VisitFloatLiteral(FloatLiteral *node) {
    VMValue src = {
        .segment = BC_CONSTANT_SEGMENT,
        .offset  = emitter_->constants_->size(),
        .size    = (node->bitwide() + 7) / 8,
    };
    switch (node->bitwide()) {
        case 32:
            emitter_->constants_->Add(node->f32());
            break;
        case 64:
            emitter_->constants_->Add(node->f64());
            break;
        default:
            DLOG(FATAL) << "noreached! bitwide = " << node->bitwide();
            break;
    }
    PushValue(EmitLoadMakeRoom(src));
}

VMValue EmittingAstVisitor::EmitIntegralAdd(Type *type, Expression *lhs,
                                            Expression *rhs) {
    SmiLiteral *smi = nullptr;
    Expression *op = nullptr;
    if (lhs->IsSmiLiteral()) {
        smi = lhs->AsSmiLiteral();
        op  = rhs;
    } else if (rhs->IsSmiLiteral()) {
        smi = rhs->AsSmiLiteral();
        op  = lhs;
    }

    if (smi && smi->bitwide() != 64) {
        auto val = Emit(op);
        auto result = current_->MakePrimitiveValue(val.size);

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            builder()->add_i##bit##_imm(result.offset, val.offset, smi->i##bit ()); \
            break;

        MIO_SMI_BYTES_SWITCH(val.size, DEFINE_CASE)
    #undef DEFINE_CASE
        return result;
    } else {
        auto val1 = Emit(lhs);
        auto val2 = Emit(rhs);
        DCHECK_EQ(val1.size, val2.size);
        auto result = current_->MakePrimitiveValue(val1.size);

        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder()->add_i##bit (result.offset, val1.offset, val2.offset); \
                break;

            MIO_INT_BYTES_SWITCH(val1.size, DEFINE_CASE)
        #undef DEFINE_CASE
        return result;
    }
}

VMValue EmittingAstVisitor::EmitFloatingAdd(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
        case byte: \
            builder()->add_f##bit (result.offset, val1.offset, val2.offset); \
            break;

    MIO_FLOAT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitIntegralSub(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
        case byte: \
            builder()->sub_i##bit (result.offset, val1.offset, val2.offset); \
            break;

    MIO_INT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitFloatingSub(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder()->sub_f##bit (result.offset, val1.offset, val2.offset); \
        break;

    MIO_FLOAT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitIntegralCmp(Type *type, Expression *lhs,
                                            Expression *rhs, Operator op) {
    auto comparator = Operator2Comparator(op);
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder()->cmp_i##bit (comparator, result.offset, val1.offset, val2.offset); \
        break;

    MIO_INT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitFloatingCmp(Type *type, Expression *lhs,
                                            Expression *rhs, Operator op) {
    auto comparator = Operator2Comparator(op);
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder()->cmp_f##bit (comparator, result.offset, val1.offset, val2.offset); \
        break;

    MIO_FLOAT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

void EmittingAstVisitor::EmitLoadOrMove(const VMValue &dest, const VMValue &src) {
    if (src.segment == BC_LOCAL_OBJECT_SEGMENT) {
        DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, dest.segment);

        builder()->mov_o(dest.offset, src.offset);
        return;
    } else if (src.segment == BC_GLOBAL_OBJECT_SEGMENT) {
        DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, dest.segment);
        if (dest.offset >= 0) {
            builder()->load_o(dest.offset, src.offset);
        } else {
            auto tmp = current_->MakeObjectRoom();
            builder()->load_o(tmp, src.offset);
            builder()->mov_o(dest.offset, tmp);
        }
    }

#define DEFINE_CASE(byte, bit) \
    case byte: \
        if (src.segment == BC_LOCAL_PRIMITIVE_SEGMENT) { \
            builder()->mov_##byte##b(dest.offset, src.offset); \
        } else { \
            if (dest.offset >= 0) { \
                builder()->load_##byte##b(dest.offset, src.segment, src.offset); \
            } else { \
                auto tmp = current_->MakePrimitiveRoom(src.size); \
                builder()->load_##byte##b(tmp, src.segment, src.offset); \
                builder()->mov_##byte##b(dest.offset, tmp); \
            } \
        } \
        break;

    DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, dest.segment);
    MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
#undef DEFINE_CASE
}

VMValue EmittingAstVisitor::EmitLoadMakeRoom(const VMValue &src) {
    switch (src.segment) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
        case BC_CONSTANT_SEGMENT: {
            VMValue value = {
                .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
                .offset  = current_->MakePrimitiveRoom(src.size),
                .size    = src.size,
            };

        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder()->load_##byte##b(value.offset, src.segment, src.offset); \
                break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
        #undef DEFINE_CASE
            return value;
        } break;

        case BC_GLOBAL_OBJECT_SEGMENT: {
            VMValue value = {
                .segment = BC_LOCAL_OBJECT_SEGMENT,
                .offset  = current_->MakeObjectRoom(),
                .size    = src.size,
            };
            builder()->load_o(value.offset, src.offset);
            return value;
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
    return { .segment = MAX_BC_SEGMENTS, .size = -1, .offset = -1 };
}

VMValue EmittingAstVisitor::EmitStoreMakeRoom(const VMValue &src) {
    switch (src.segment) {
        case BC_LOCAL_PRIMITIVE_SEGMENT: {
            VMValue value = {
                .segment = BC_GLOBAL_PRIMITIVE_SEGMENT,
                .offset  = emitter_->p_global_->size(),
                .size    = src.size,
            };
            emitter_->p_global_->AlignAdvance(src.size);

        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder()->store_##byte##b(value.offset, src.segment, src.offset); \
                break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
        #undef DEFINE_CASE

            return value;
        } break;

        case BC_LOCAL_OBJECT_SEGMENT: {
            VMValue value = {
                .segment = BC_GLOBAL_OBJECT_SEGMENT,
                .offset  = emitter_->o_global_->size(),
                .size    = src.size,
            };
            emitter_->o_global_->AlignAdvance(kObjectReferenceSize);

            builder()->store_o(value.offset, src.offset);
            return value;
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
    return { .segment = MAX_BC_SEGMENTS, .size = -1, .offset = -1 };
}


VMValue EmittingAstVisitor::GetEmptyObject(Type *type) {
    if (type->IsString()) {
        return GetOrNewString("", 0, nullptr);
    } else if (type->IsUnion()) {
        // TODO:
    }

    // TODO:
    return {};
}

VMValue EmittingAstVisitor::GetOrNewString(const char *z, int n, Local<MIOString> *rv) {
    VMValue value = {
        .segment = BC_GLOBAL_OBJECT_SEGMENT,
        .offset  = 0,
        .size    = kObjectReferenceSize,
    };

    int *offset;
    auto obj = emitter_->object_factory_->GetOrNewString(z, n, &offset);
    if (*offset < 0) {
        *offset = emitter_->o_global_->size();
        emitter_->o_global_->Add(obj.get());
    }
    value.offset = *offset;

    if (rv) {
        *rv = std::move(obj);
    }
    return value;
}

VMValue EmittingAstVisitor::EmitLoadPrimitiveMakeRoom(VMValue src) {

    VMValue value;

    value.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
    value.size    = src.size;
    value.offset = current_->MakePrimitiveRoom(src.size);
    EmitLoadOrMove(value, src);
    return value;
}

BitCodeEmitter::BitCodeEmitter(MemorySegment *constants,
                               MemorySegment *p_global,
                               MemorySegment *o_global,
                               TypeFactory *types,
                               ObjectFactory *object_factory,
                               FunctionRegister *function_register)
    : constants_(DCHECK_NOTNULL(constants))
    , p_global_(DCHECK_NOTNULL(p_global))
    , o_global_(DCHECK_NOTNULL(o_global))
    , types_(DCHECK_NOTNULL(types))
    , object_factory_(DCHECK_NOTNULL(object_factory))
    , function_register_(DCHECK_NOTNULL(function_register)) {
    // fill zero number area.
    memset(constants_->AlignAdvance(8), 0, 8);
}

BitCodeEmitter::~BitCodeEmitter() {
}

bool BitCodeEmitter::Run(RawStringRef module_name, RawStringRef unit_name,
                         ZoneVector<Statement *> *stmts) {
    EmittingAstVisitor visitor(module_name, this);

    visitor.set_unit_name(unit_name);
    for (int i = 0; i < stmts->size(); ++i) {
        stmts->At(i)->Accept(&visitor);
    }
    return true;
}

bool BitCodeEmitter::Run(CompiledModuleMap *all_modules) {
    DLOG(INFO) << "max number of instructions: " << MAX_BC_INSTRUCTIONS;

    auto pair = DCHECK_NOTNULL(all_modules->Get(kMainValue)); // "main"
    return EmitModule(pair->key(), pair->value(), all_modules);
}

bool BitCodeEmitter::EmitModule(RawStringRef module_name,
                                CompiledUnitMap *all_units,
                                CompiledModuleMap *all_modules) {
    auto zone = types_->zone();
    auto proto = types_->GetFunctionPrototype(new (zone) ZoneVector<Paramter *>(zone),
                                              types_->GetVoid());

    EmittingAstVisitor visitor(module_name, this);
    EmittedScope info(visitor.mutable_current(), proto);

    // placement frame instruction
    auto frame_placement = visitor.builder()->debug();

    CompiledUnitMap::Iterator iter(all_units);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        visitor.set_unit_name(iter->key());

        auto stmts = iter->value();
        if (stmts->is_not_empty()) {
            if (!ProcessImportList(stmts->first()->AsPackageImporter(), &info,
                                   all_modules)) {
                return false;
            }
        }
        for (int i = 1; i < stmts->size(); ++i) {
            stmts->At(i)->Accept(&visitor);
        }
    }

    // refill frame instruction
    auto frame = BitCodeBuilder::MakeS2AddrBC(BC_frame, info.p_stack_size(),
                                              info.o_stack_size());

    auto builder = info.builder();
    memcpy(builder->code()->offset(frame_placement * sizeof(uint64_t)),
           &frame,
           sizeof(frame));
    auto main_name = TextOutputStream::sprintf("::%s::main", module_name->c_str());
    auto entry = function_register_->FindOrNull(main_name.c_str());
    if (entry) {
        auto local = info.MakeObjectRoom();
        info.builder()->load_o(local, entry->offset());
        info.builder()->call_val(0, 0, local);
    }
    builder->ret();

    auto boot_name = TextOutputStream::sprintf("::%s::bootstrap", module_name->c_str());
    auto obj = object_factory_->CreateNormalFunction(builder->code()->offset(0),
                                                     builder->code()->size());
    Local<MIOString> inner_name;
    visitor.GetOrNewString(boot_name, &inner_name);
    obj->SetName(inner_name.get());

    auto offset = o_global_->size();
    o_global_->Add(obj.get());
    entry = function_register_->FindOrInsert(boot_name.c_str());
    entry->set_offset(offset);

    return true;
}

bool BitCodeEmitter::ProcessImportList(PackageImporter *pkg,
                                       EmittedScope *info,
                                       CompiledModuleMap *all_modules) {
    DCHECK_NOTNULL(pkg);

    PackageImporter::ImportList::Iterator iter(pkg->mutable_import_list());
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        auto pair = DCHECK_NOTNULL(all_modules->Get(iter->key()));
        auto module_name = pair->key();
        if (imported_.find(module_name->ToString()) != imported_.end()) {
            return true;
        }
        imported_.insert(module_name->ToString());

        if (!EmitModule(module_name, pair->value(), all_modules)) {
            return false;
        }
        auto name = TextOutputStream::sprintf("%s::bootstrap", module_name->c_str());
        auto entry = function_register_->FindOrInsert(name.c_str());
        auto local = info->MakeObjectRoom();

        info->builder()->load_o(local, entry->offset());
        info->builder()->call_val(0, 0, local);
    }
    return true;
}

} // namespace mio
