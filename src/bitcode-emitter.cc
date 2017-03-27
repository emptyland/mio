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

inline int SizeofType(Type *type) {
    // TODO:
    return 8;
}

class EmittedScope {
public:
    EmittedScope(EmittedScope **current, FunctionPrototype *prototype)
        : current_(DCHECK_NOTNULL(current))
        , saved_(*DCHECK_NOTNULL(current))
        , prototype_(DCHECK_NOTNULL(prototype)) {
        *current_ = this;
    }

    ~EmittedScope() { *current_ = saved_; }

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

private:
    EmittedScope **current_;
    EmittedScope *saved_;
    int p_stack_size_ = 0;
    int o_stack_size_ = 0;
    FunctionPrototype *prototype_;
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
    virtual void VisitVariable(Variable *node) override;

    BitCodeBuilder *builder() { return emitter_->builder_; }

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
    auto entry = emitter_->function_register_->FindOrInsert(scope->MakeFullName(node->name()).c_str());
    entry->set_is_native(node->is_native());

    if (node->is_native()) {
        auto obj = emitter_->object_factory_->CreateNativeFunction("::", nullptr);
        entry->set_offset(emitter_->o_global_->size());
        emitter_->o_global_->Add(obj.get());
    } else {
        auto pc = emitter_->builder_->pc();
        Emit(node->function_literal());
        PopValue();
        emitter_->builder_->BindTo(entry->mutable_label(), pc);

        auto obj = emitter_->object_factory_->CreateNormalFunction(pc);
        entry->set_offset(emitter_->o_global_->size());
        emitter_->o_global_->Add(obj.get());
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
    auto frame_placement = emitter_->builder_->debug();

    for (int i = 0; i < prototype->mutable_paramters()->size(); ++i) {
        auto paramter = prototype->mutable_paramters()->At(i);

        auto var = scope->FindOrNullLocal(paramter->param_name());
        var->set_bind_kind(Variable::LOCAL);
        if (var->type()->is_primitive()) {
            var->set_offset(info.MakePrimitiveRoom(SizeofType(var->type())));
        } else {
            var->set_offset(info.MakeObjectRoom());
        }
    }

    if (prototype->return_type()->IsVoid()) {
        node->body()->Accept(this);
        emitter_->builder_->ret();
    } else {
        auto result = Emit(node->body());
        if (node->is_assignment()) {
            auto size = SizeofType(prototype->return_type());
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
            emitter_->builder_->ret();
        }
    }

    // refill frame instruction
    auto frame = BitCodeBuilder::MakeS2AddrBC(BC_frame, info.p_stack_size(),
                                              info.o_stack_size());
    memcpy(emitter_->builder_->code()->offset(frame_placement), &frame,
           sizeof(frame));

    PushValue(VMValue::Function(frame_placement));
}

void EmittingAstVisitor::VisitBlock(Block *node) {
    // TODO:
}

void EmittingAstVisitor::VisitReturn(Return *node) {
    if (node->has_return_value()) {
        auto result = Emit(node->expression());

        auto size = SizeofType(current_->prototype()->return_type());
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
    emitter_->builder_->ret();
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitCall(Call *node) {
    auto expr = Emit(node->expression());
    if (expr.segment == BC_GLOBAL_OBJECT_SEGMENT) {
        VMValue tmp;

        tmp.segment = BC_LOCAL_OBJECT_SEGMENT;
        tmp.size    = expr.size;
        tmp.offset  = current_->MakeObjectRoom();
        EmitLoadOrMove(tmp, expr);
        expr = tmp;
    }

    VMValue result;
    if (!node->return_type()->IsVoid()) {
        auto result_size = SizeofType(node->return_type());
        if (node->return_type()->is_primitive()) {
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
                emitter_->builder_->load_o(current_->MakeObjectRoom(), value.offset);
                break;

            case BC_LOCAL_OBJECT_SEGMENT:
                emitter_->builder_->mov_o(current_->MakeObjectRoom(), value.offset);
                break;

            default:
                break;
        }
    }

    emitter_->builder_->call_val(p_base, o_base, expr.offset);
    if (!node->return_type()->IsVoid()) {
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
    emitter_->builder_->jz(cond.offset, &outter);

    // TODO:
//    auto then_val = Emit(node->then_statement());
//    if (node->has_else()) {
//
//    } else {
//
//    }
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

        emitter_->builder_->mov_o(dest.offset, src.offset);
        return;
    } else if (src.segment == BC_GLOBAL_OBJECT_SEGMENT) {
        DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, dest.segment);
        if (dest.offset >= 0) {
            emitter_->builder_->load_o(dest.offset, src.offset);
        } else {
            auto tmp = current_->MakeObjectRoom();
            emitter_->builder_->load_o(tmp, src.offset);
            emitter_->builder_->mov_o(dest.offset, tmp);
        }
    }

#define DEFINE_LOAD_OR_MOVE_CASE(posfix) \
    if (src.segment == BC_LOCAL_PRIMITIVE_SEGMENT) { \
        emitter_->builder_->mov##posfix(dest.offset, src.offset); \
    } else { \
        if (dest.offset >= 0) { \
            emitter_->builder_->load##posfix(dest.offset, src.segment, src.offset); \
        } else { \
            auto tmp = current_->MakePrimitiveRoom(src.size); \
            emitter_->builder_->load##posfix(tmp, src.segment, src.offset); \
            emitter_->builder_->mov##posfix(dest.offset, tmp); \
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

VMValue EmittingAstVisitor::EmitLoadPrimitiveMakeRoom(VMValue src) {

    VMValue value;

    value.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
    value.size    = src.size;
    value.offset = current_->MakePrimitiveRoom(src.size);
    EmitLoadOrMove(value, src);
    return value;
}

BitCodeEmitter::BitCodeEmitter(MemorySegment *code,
                               MemorySegment *constants,
                               MemorySegment *p_global,
                               MemorySegment *o_global,
                               TypeFactory *types,
                               ObjectFactory *object_factory,
                               FunctionRegister *function_register)
    : builder_(new BitCodeBuilder(code))
    , constants_(DCHECK_NOTNULL(constants))
    , p_global_(DCHECK_NOTNULL(p_global))
    , o_global_(DCHECK_NOTNULL(o_global))
    , types_(DCHECK_NOTNULL(types))
    , object_factory_(DCHECK_NOTNULL(object_factory))
    , function_register_(DCHECK_NOTNULL(function_register)) {
    // fill zero number area.
    memset(constants_->AlignAdvance(8), 0, 8);
}

BitCodeEmitter::~BitCodeEmitter() {
    delete builder_;
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
