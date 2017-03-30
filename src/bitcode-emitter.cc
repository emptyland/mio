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
#include "glog/logging.h"
#include <stack>

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

    BitCodeBuilder *builder() { return &builder_; }

private:
    MemorySegment *code_;
    EmittedScope **current_;
    EmittedScope *saved_;
    int p_stack_size_ = 0;
    int o_stack_size_ = 0;
    FunctionPrototype *prototype_;
    BitCodeBuilder builder_;
};

class EmittingAstVisitor : public DoNothingAstVisitor {
public:
    EmittingAstVisitor(RawStringRef module_name, RawStringRef unit_name,
                       BitCodeEmitter *emitter)
        : module_name_(DCHECK_NOTNULL(module_name))
        , unit_name_(DCHECK_NOTNULL(unit_name))
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

    BitCodeBuilder *builder() { return DCHECK_NOTNULL(current_)->builder(); }

    VMValue Emit(AstNode *node) {
        DCHECK_NOTNULL(node)->Accept(this);
        auto rv = eval_value();
        PopValue();
        return rv;
    }

    VMValue EmitLoadIfNeeded(const VMValue value);

private:
    VMValue MakePlacementRoom(BCSegment seg_kind, Type *type,
                              Expression *initializer);
    VMValue EmitLoadPrimitiveMakeRoom(VMValue src);
    void EmitLoadOrMove(const VMValue &dest, const VMValue &src);

    VMValue EmitLoadMakeRoom(const VMValue &src);

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

    RawStringRef module_name_;
    RawStringRef unit_name_;
    BitCodeEmitter *emitter_;
    EmittedScope *current_ = nullptr;
    std::stack<VMValue> value_stack_;
};

void EmittingAstVisitor::VisitFunctionDefine(FunctionDefine *node) {
    auto scope = node->scope();
    auto full_name = scope->MakeFullName(node->name());
    auto entry = emitter_->function_register_->FindOrInsert(full_name.c_str());
    entry->set_is_native(node->is_native());

    if (node->is_native()) {
        auto obj = emitter_->object_factory_->CreateNativeFunction("::", nullptr);
        entry->set_offset(emitter_->o_global_->size());
        emitter_->o_global_->Add(obj.get());
    } else {
        auto value = Emit(node->function_literal());
        entry->set_offset(value.offset);

        DCHECK_EQ(BC_GLOBAL_OBJECT_SEGMENT, value.segment);
        auto func = make_local(emitter_->o_global_->Get<HeapObject *>(value.offset)->AsNormalFunction());
        DCHECK(!func.empty());

        func->SetName(emitter_->object_factory_->GetOrNewString(
            full_name.c_str(),
            static_cast<int>(full_name.size()),
            nullptr).get());
    }
    node->instance()->set_offset(entry->offset());
    node->instance()->set_bind_kind(Variable::GLOBAL);

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
        .offset  = emitter_->o_global_->size(),
        .size    = kObjectReferenceSize,
    };
    emitter_->o_global_->Add(obj.get());
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
    VMValue result;
    if (!proto->return_type()->IsVoid()) {
        auto result_size = proto->return_type()->placement_size();
        if (proto->return_type()->is_primitive()) {
            result = {
                .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
                .offset  = current_->MakePrimitiveRoom(result_size),
                .size    = result_size,
            };
        } else {
            result = {
                .segment = BC_LOCAL_OBJECT_SEGMENT,
                .offset  = current_->MakeObjectRoom(),
                .size    = result_size,
            };
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

    builder()->call_val(p_base, o_base, expr.offset);
    if (!proto->return_type()->IsVoid()) {
        PushValue(result);
    } else {
        PushValue(VMValue::Void());
    }
}

void EmittingAstVisitor::VisitValDeclaration(ValDeclaration *node) {
    if (node->scope()->type() == MODULE_SCOPE ||
        node->scope()->type() == UNIT_SCOPE) {
        if (node->type()->is_primitive()) {
            // TODO: primitive
        } else {
            // TODO: object
        }
    } else {
        // local vals
        if (node->type()->is_primitive()) {

            VMValue value;
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = VMValue::Zero();
            }
            DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::LOCAL);
            DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
        } else {
            // TODO: object
        }
    }
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitIfOperation(IfOperation *node) {
    auto cond = Emit(node->condition());
    DCHECK(cond.segment == BC_LOCAL_PRIMITIVE_SEGMENT);

    CodeLabel outter;
    builder()->jz(cond.offset, &outter);

    if (node->has_else()) {
        CodeLabel leave;
        bool need_union = node->then_type()->id() != node->else_type()->id();

        VMValue val;
        auto then_val = Emit(node->then_statement());
        if (need_union) {
            val.segment = BC_LOCAL_OBJECT_SEGMENT;
            val.offset  = current_->MakeObjectRoom();
            val.size    = kObjectReferenceSize;
            if (node->then_type()->IsVoid()) {
                builder()->create(BC_UnionVoid, val.offset, 0);
            } else if (node->then_type()->is_primitive()) {
                builder()->create(BC_UnionOrMergePrimitive, val.offset, then_val.offset);
            } else {
                builder()->create(BC_UnionOrMergeObject, val.offset, then_val.offset);
            }
        }
        builder()->jmp(&leave);

        builder()->BindNow(&outter);
        auto else_val = Emit(node->else_statement());
        if (need_union) {
            if (node->else_type()->IsVoid()) {
                builder()->create(BC_UnionVoid, val.offset, 0);
            } else if (node->else_type()->is_primitive()) {
                builder()->create(BC_UnionOrMergePrimitive, val.offset, else_val.offset);
            } else {
                builder()->create(BC_UnionOrMergeObject, val.offset, else_val.offset);
            }
        } else {
            if (node->then_type()->IsVoid()) {
                val = VMValue::Void();
            } else if (node->then_type()->is_primitive()) {
                val.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
                val.offset  = current_->MakePrimitiveRoom(then_val.size);
                val.size    = then_val.size;
                EmitLoadOrMove(val, then_val);
            } else {
                val.segment = BC_LOCAL_OBJECT_SEGMENT;
                val.offset  = current_->MakeObjectRoom();
                val.size    = kObjectReferenceSize;
                EmitLoadOrMove(val, else_val);
            }
        }
        builder()->BindNow(&leave);
        PushValue(val);
    } else {
        CodeLabel leave;
        VMValue val;
        // then
        auto then_val = Emit(node->then_statement());
        val.segment = BC_LOCAL_OBJECT_SEGMENT;
        val.offset  = current_->MakeObjectRoom();
        val.size    = kObjectReferenceSize;
        if (node->then_type()->IsVoid()) {
            builder()->create(BC_UnionVoid, val.offset, 0);
        } else if (node->then_type()->is_primitive()) {
            builder()->create(BC_UnionOrMergePrimitive, val.offset, then_val.offset);
        } else {
            builder()->create(BC_UnionOrMergeObject, val.offset, then_val.offset);
        }
        builder()->jmp(&leave);

        // else
        builder()->BindNow(&outter);
        builder()->create(BC_UnionVoid, val.offset, 0);

        builder()->BindNow(&leave);
        PushValue(val);
    }
}

void EmittingAstVisitor::VisitVarDeclaration(VarDeclaration *node) {
    if (node->scope()->type() == MODULE_SCOPE ||
        node->scope()->type() == UNIT_SCOPE) {
        if (node->type()->is_primitive()) {
            // TODO: primitive
        } else {
            // TODO: object
        }
    } else {
        VMValue value;
        // local vals
        if (node->type()->is_primitive()) {

            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                // TODO: zero primitive variable
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                // TODO: zero object
            }
        }

        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::LOCAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
    }
    PushValue(VMValue::Void());
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
    // TODO:
}

void EmittingAstVisitor::VisitVariable(Variable *node) {
    DCHECK_NE(Variable::UNBINDED, node->bind_kind());

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
                .size    = value.size,
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
    VMValue dest = {
        .segment = BC_LOCAL_OBJECT_SEGMENT,
        .offset  = current_->MakeObjectRoom(),
        .size    = kObjectReferenceSize,
    }, src = {
        .segment = BC_GLOBAL_OBJECT_SEGMENT,
        .offset  = 0,
        .size    = kObjectReferenceSize,
    };

    int *offset;
    auto obj = emitter_->object_factory_->GetOrNewString(node->data()->c_str(), node->data()->size(), &offset);
    if (*offset < 0) {
        *offset = emitter_->o_global_->size();
        emitter_->o_global_->Add(obj.get());
    }
    src.offset = *offset;
    EmitLoadOrMove(dest, src);
    PushValue(dest);
}

void EmittingAstVisitor::VisitSmiLiteral(SmiLiteral *node) {
    VMValue dest = {
        .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
        .offset  = 0,
        .size    = 0,
    }, src = {
        .segment = BC_CONSTANT_SEGMENT,
        .offset  = emitter_->constants_->size(),
        .size    = (node->bitwide() + 7) / 8,
    };
    switch (node->bitwide()) {
        case 1:
            emitter_->constants_->Add(node->i1());
            break;

        case 8:
            emitter_->constants_->Add(node->i8());
            break;

        case 16:
            emitter_->constants_->Add(node->i16());
            break;

        case 32:
            emitter_->constants_->Add(node->i32());
            break;

        case 64:
            emitter_->constants_->Add(node->i64());
            break;

        default:
            DLOG(FATAL) << "noreached! bitwide = " << node->bitwide();
            break;
    }

    dest.size = src.size;
    dest.offset = current_->MakePrimitiveRoom(dest.size);
    EmitLoadOrMove(dest, src);

    PushValue(dest);
}

VMValue EmittingAstVisitor::MakePlacementRoom(BCSegment seg_kind,
                                              Type *type,
                                              Expression *initializer) {
    DCHECK(type->is_primitive());
    MemorySegment *segment = nullptr;
    if (seg_kind == BC_CONSTANT_SEGMENT) {
        segment = emitter_->constants_;
    } else if (seg_kind == BC_GLOBAL_PRIMITIVE_SEGMENT) {
        segment = emitter_->p_global_;
    } else {
        DLOG(FATAL) << "noreached! segment = " << seg_kind;
    }

    if (type->IsIntegral()) {
        if (initializer->IsSmiLiteral()) {
            auto smi = initializer->AsSmiLiteral();

            VMValue value;
            value.segment = seg_kind;
            value.offset  = emitter_->constants_->size();

            switch (smi->bitwide()) {
                case 1:
                    segment->Add<mio_bool_t>(smi->i1());
                    break;
                case 8:
                    segment->Add<mio_i8_t>(smi->i8());
                    break;
                case 16:
                    segment->Add<mio_i16_t>(smi->i16());
                    break;
                case 32:
                    segment->Add<mio_i32_t>(smi->i32());
                    break;
                case 64:
                    segment->Add<mio_i64_t>(smi->i64());
                    break;
            }
            return value;
        }
    } else {
        // TODO floating
    }
    return VMValue();
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

#define DEFINE_LOAD_OR_MOVE_CASE(posfix) \
    if (src.segment == BC_LOCAL_PRIMITIVE_SEGMENT) { \
        builder()->mov##posfix(dest.offset, src.offset); \
    } else { \
        if (dest.offset >= 0) { \
            builder()->load##posfix(dest.offset, src.segment, src.offset); \
        } else { \
            auto tmp = current_->MakePrimitiveRoom(src.size); \
            builder()->load##posfix(tmp, src.segment, src.offset); \
            builder()->mov##posfix(dest.offset, tmp); \
        } \
    }

    DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, dest.segment);
    switch (src.size) {
        case 1:
            DEFINE_LOAD_OR_MOVE_CASE(_1b)
            break;

        case 2:
            DEFINE_LOAD_OR_MOVE_CASE(_2b)
            break;

        case 4:
            DEFINE_LOAD_OR_MOVE_CASE(_4b)
            break;

        case 8:
            DEFINE_LOAD_OR_MOVE_CASE(_8b)
            break;

#undef DEFINE_LOAD_OR_MOVE_CASE
        default:
            DLOG(FATAL) << "noreached! src.size = " << src.size;
            break;
    }
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

            switch (src.size) {
                case 1:
                    builder()->load_1b(value.offset, src.segment, src.offset);
                    break;
                case 2:
                    builder()->load_2b(value.offset, src.segment, src.offset);
                    break;
                case 4:
                    builder()->load_4b(value.offset, src.segment, src.offset);
                    break;
                case 8:
                    builder()->load_8b(value.offset, src.segment, src.offset);
                    break;
                default:
                    DLOG(FATAL) << "noreached! bad size: " << src.size;
                    break;
            }
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
    EmittingAstVisitor visitor(module_name, unit_name, this);

    for (int i = 0; i < stmts->size(); ++i) {
        stmts->At(i)->Accept(&visitor);
    }
    return true;
}

} // namespace mio
