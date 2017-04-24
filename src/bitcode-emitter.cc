#include "bitcode-emitter.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode.h"
#include "vm-objects.h"
#include "vm-object-factory.h"
#include "vm-function-register.h"
#include "vm-bitcode-disassembler.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"
#include "text-output-stream.h"
#include "base.h"
#include "glog/logging.h"
#include <stack>

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

namespace {

struct PrimitiveKey {
    uint8_t size;
    uint8_t padding0;
    uint8_t data[8];

    static PrimitiveKey FromData(const void *p, int n) {
        PrimitiveKey k;
        auto d = static_cast<const uint8_t *>(p);
        k.size = static_cast<uint8_t>(n);

        k.data[0] = d[0];
        if (k.size == 1) {
            goto out;
        }
        k.data[1] = d[1];
        if (k.size == 2) {
            goto out;
        }
        k.data[2] = d[2];
        k.data[3] = d[3];
        if (k.size == 4) {
            goto out;
        }
        k.data[4] = d[4];
        k.data[5] = d[5];
        k.data[6] = d[6];
        k.data[7] = d[7];
    out:
        return k;
    }
};

struct PrimitiveKeyFallbackHash {
    std::size_t operator()(PrimitiveKey const &k) const {
        std::size_t h = 1315423911;
        for (int i = 0; i < k.size; ++i) {
            h ^= ((h << 5) + k.data[i] + (h >> 2));
        }
        return h;
    }
};

struct PrimitiveKeyFastHash {
    std::size_t operator()(PrimitiveKey const &k) const {
        std::size_t h = 1315423911;
        h ^= ((h << 5) + k.data[0] + (h >> 2));
        if (k.size == 1) {
            return h;
        }
        h ^= ((h << 5) + k.data[1] + (h >> 2));
        if (k.size == 2) {
            return h;
        }
        h ^= ((h << 5) + k.data[2] + (h >> 2));
        h ^= ((h << 5) + k.data[3] + (h >> 2));
        if (k.size == 4) {
            return h;
        }
        h ^= ((h << 5) + k.data[4] + (h >> 2));
        h ^= ((h << 5) + k.data[5] + (h >> 2));
        h ^= ((h << 5) + k.data[6] + (h >> 2));
        h ^= ((h << 5) + k.data[7] + (h >> 2));
        return h;
    }
};

struct PrimitiveKeyEqualTo {
    bool operator() (const PrimitiveKey &lhs, const PrimitiveKey &rhs) const {
        if (lhs.size == rhs.size) {
            return memcmp(lhs.data, rhs.data, lhs.size) == 0;
        }
        return false;
    }
};

} // namespace

typedef std::unordered_map<PrimitiveKey, int,
    PrimitiveKeyFastHash, PrimitiveKeyEqualTo> PrimitiveMap;

struct VMValue {
    BCSegment segment;
    int       offset;
    int       size;

    bool is_void() const { return offset < 0 && size < 0; }

    static VMValue Void() { return { MAX_BC_SEGMENTS, -1, -1, }; }
};

class EmittedScope {
public:
    EmittedScope(EmittedScope **current, FunctionPrototype *prototype,
                 Scope *scope)
        : code_(new MemorySegment)
        , constant_primitive_(new MemorySegment)
        , builder_(code_)
        , current_(DCHECK_NOTNULL(current))
        , saved_(*DCHECK_NOTNULL(current))
        , prototype_(DCHECK_NOTNULL(prototype))
        , scope_(scope) {
        *current_ = this;
    }

    ~EmittedScope() {
        *current_ = saved_;
        delete code_;
        delete constant_primitive_;
    }

    DEF_GETTER(int, p_stack_size)
    DEF_GETTER(int, o_stack_size)
    FunctionPrototype *prototype() const { return prototype_; }
    Scope *scope() const { return scope_; }

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

    VMValue MakeObjectValue() {
        return {
            .segment = BC_LOCAL_OBJECT_SEGMENT,
            .offset  = MakeObjectRoom(),
            .size    = kObjectReferenceSize,
        };
    }

    VMValue MakePrimitiveValue(int size) {
        return {
            .segment = BC_LOCAL_PRIMITIVE_SEGMENT,
            .offset  = MakePrimitiveRoom(size),
            .size    = size,
        };
    }

    VMValue MakeLocalValue(Type *type) {
        return type->is_primitive()
             ? MakePrimitiveValue(type->placement_size())
             : MakeObjectValue();
    }

    VMValue MakeConstantObjectValue(Handle<HeapObject> ob) {
        auto offset = static_cast<int>(constant_objects_.size()) * kObjectReferenceSize;
        constant_objects_.push_back(ob);
        return {
            .segment = BC_FUNCTION_CONSTANT_OBJECT_SEGMENT,
            .offset  = offset,
            .size    = kObjectReferenceSize,
        };
    }

    template<class T>
    VMValue MakeConstantPrimitiveValue(T value) {
        auto key = PrimitiveKey::FromData(&value, static_cast<int>(sizeof(value)));
        auto iter = constant_primitive_map_.find(key);
        if (iter != constant_primitive_map_.end()) {
            return {
                .segment = BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT,
                .offset  = iter->second,
                .size    = sizeof(value),
            };
        }

        auto offset = constant_primitive_->size();
        constant_primitive_->Add(value);
        constant_primitive_map_.emplace(key, offset);
        return {
            .segment = BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT,
            .offset  = offset,
            .size    = sizeof(value),
        };
    }


    Handle<HeapObject> constant_object(int offset) {
        return constant_objects_[offset / kObjectReferenceSize];
    }

    const std::vector<Handle<HeapObject>> &constant_objects() const {
        return constant_objects_;
    }

    BitCodeBuilder *builder() { return &builder_; }
    EmittedScope *prev() const { return saved_; }
    MemorySegment *code() const { return code_; }

    MemorySegment *constant_primitive() const { return constant_primitive_; }

    void *constant_primitive_data() const {
        return constant_primitive_size() ? constant_primitive()->offset(0) : nullptr;
    }

    int constant_primitive_size() const {
        return constant_primitive()->size();
    }

private:
    MemorySegment *code_;
    MemorySegment *constant_primitive_;
    EmittedScope **current_;
    EmittedScope *saved_;
    int p_stack_size_ = 0;
    int o_stack_size_ = 0;
    FunctionPrototype *prototype_;
    BitCodeBuilder builder_;
    Scope *scope_;
    std::vector<Variable *> upvalues_;
    std::vector<Handle<HeapObject>> constant_objects_;
    PrimitiveMap constant_primitive_map_;
};

class EmittingAstVisitor : public DoNothingAstVisitor {
public:
    EmittingAstVisitor(RawStringRef module_name, BitCodeEmitter *emitter)
        : module_name_(DCHECK_NOTNULL(module_name))
        , emitter_(DCHECK_NOTNULL(emitter)) {
    }

    virtual void VisitFunctionDefine(FunctionDefine *node) override;
    virtual void VisitFunctionLiteral(FunctionLiteral *node) override;
    virtual void VisitForeachLoop(ForeachLoop *node) override;
    virtual void VisitReturn(Return *node) override;
    virtual void VisitBlock(Block *node) override;
    virtual void VisitCall(Call *node) override;
    virtual void VisitValDeclaration(ValDeclaration *node) override;
    virtual void VisitVarDeclaration(VarDeclaration *node) override;
    virtual void VisitIfOperation(IfOperation *node) override;
    virtual void VisitUnaryOperation(UnaryOperation *node) override;
    virtual void VisitAssignment(Assignment *node) override;
    virtual void VisitBinaryOperation(BinaryOperation *node) override;
    virtual void VisitVariable(Variable *node) override;
    virtual void VisitStringLiteral(StringLiteral *node) override;
    virtual void VisitSmiLiteral(SmiLiteral *node) override;
    virtual void VisitFloatLiteral(FloatLiteral *node) override;
    virtual void VisitMapInitializer(MapInitializer *node) override;
    virtual void VisitFieldAccessing(FieldAccessing *node) override;
    virtual void VisitTypeTest(TypeTest *node) override;
    virtual void VisitTypeCast(TypeCast *node) override;

    BitCodeBuilder *builder() { return DCHECK_NOTNULL(current_)->builder(); }

    EmittedScope **mutable_current() { return &current_; }

    void set_unit_name(RawStringRef name) { unit_name_ = DCHECK_NOTNULL(name); }

    VMValue Emit(AstNode *node) {
        DCHECK_NOTNULL(node)->Accept(this);
        auto rv = eval_value();
        PopValue();
        return rv;
    }

    VMValue GetOrNewString(RawStringRef raw, Handle<MIOString> *obj) {
        return GetOrNewString(raw->c_str(), raw->size(), obj);
    }

    VMValue GetOrNewString(const std::string &s, Handle<MIOString> *obj) {
        return GetOrNewString(s.c_str(), static_cast<int>(s.size()), obj);
    }

    VMValue GetOrNewString(const char *z, int n, Handle<MIOString> *obj);
private:
    void EmitGlobalFunction(FunctionDefine *node);
    void EmitLocalFunction(FunctionDefine *node);

    VMValue EmitIntegralAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);
    VMValue EmitFloatingCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);

    void EmitCreateUnion(const VMValue &dest, const VMValue &src,
                                             Type *type) {
        builder()->oop(OO_UnionOrMerge, dest.offset,  src.offset,
                       TypeInfoIndex(type));
    }

    void EmitFunctionCall(const VMValue &callee, Call *node);
    void EmitMapAccessor(const VMValue &callee, Call *node);

    void EmitMapPut(const VMValue &map, VMValue key, VMValue value, Map *map_ty,
                    Type *val_ty) {
        if (map_ty->value()->IsUnion()) {
            auto tmp = current_->MakeObjectValue();
            EmitCreateUnion(tmp, value, val_ty);
            value = tmp;
        }
        builder()->oop(OO_MapPut, map.offset, key.offset, value.offset);
    }

    VMValue EmitToString(const VMValue &input, Type *type) {
        auto result = current_->MakeObjectValue();
        auto index = TypeInfoIndex(type);

        builder()->oop(OO_ToString, result.offset,  input.offset, index);
        return result;
    }

    VMValue EmitLoadMakeRoom(const VMValue &src);
    VMValue EmitStoreMakeRoom(const VMValue &src);

    void EmitLoad(const VMValue &dest, const VMValue &src);
    void EmitStore(const VMValue &dest, const VMValue &src);
    void EmitMove(const VMValue &dest, const VMValue &src);

    VMValue EmitEmptyValue(Type *type);

    int GetVariableOffset(Variable *var, Scope *scope);

    int TypeInfoIndex(Type *type) {
        auto iter = emitter_->type_id2index_.find(type->GenerateId());
        DCHECK(iter != emitter_->type_id2index_.end())
                << "has call BitCodeEmitter::Init() ?";
        return iter->second;
    }

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

    VMValue MakeGlobalObjectValue(Handle<HeapObject> ob) {
        VMValue value = {
            .segment = BC_GLOBAL_OBJECT_SEGMENT,
            .offset  = emitter_->o_global_->size(),
            .size    = kObjectReferenceSize,
        };
        emitter_->o_global_->Add(ob.get());
        return value;
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
    if (scope->is_universal()) {
        EmitGlobalFunction(node);
    } else {
        EmitLocalFunction(node);
    }

    PushValue(VMValue::Void());
}

void EmittingAstVisitor::EmitGlobalFunction(FunctionDefine *node) {
    auto full_name = node->scope()->MakeFullName(node->name());

    Handle<MIOString> name;
    GetOrNewString(full_name, &name);

    auto entry = emitter_->function_register_->FindOrInsert(full_name.c_str());
    entry->set_kind(node->is_native() ? FunctionEntry::NATIVE : FunctionEntry::NORMAL);

    Handle<MIOFunction> ob(nullptr);
    auto value = MakeGlobalObjectValue(ob);
    entry->set_offset(value.offset);

    node->instance()->set_bind_kind(Variable::GLOBAL);
    node->instance()->set_offset(value.offset);

    if (node->is_native()) {
        auto proto = node->function_literal()->prototype();
        int p_size = 0, o_size = 0;
        for (int i = 0; i < proto->mutable_paramters()->size(); ++i) {
            auto param = proto->mutable_paramters()->At(i);

            if (param->param_type()->is_primitive()) {
                p_size += param->param_type()->placement_size();
            } else {
                o_size += param->param_type()->placement_size();
            }
        }
        auto fn = emitter_->object_factory_->CreateNativeFunction("::", nullptr);
        fn->SetPrimitiveArgumentsSize(p_size);
        fn->SetObjectArgumentsSize(o_size);
        fn->SetName(name.get());

        ob = fn;
    } else {
        auto value = Emit(node->function_literal());

        DCHECK_EQ(BC_FUNCTION_CONSTANT_OBJECT_SEGMENT, value.segment);
        ob = static_cast<MIOFunction *>(current_->constant_object(value.offset).get());
        DCHECK(ob->IsNormalFunction()) << ob->GetKind();
    }
    ob->SetName(name.get());

    emitter_->o_global_->Set(value.offset, ob.get());
}

void EmittingAstVisitor::EmitLocalFunction(FunctionDefine *node) {
    auto full_name = node->scope()->MakeFullName(node->name());
    DCHECK(!node->is_native());

    auto value = Emit(node->function_literal());
    DCHECK_EQ(BC_FUNCTION_CONSTANT_OBJECT_SEGMENT, value.segment);

    auto ob = static_cast<MIOFunction *>(current_->constant_object(value.offset).get());

    Handle<MIOString> name;
    GetOrNewString(full_name, &name);
    ob->SetName(name.get());

    auto result = EmitLoadMakeRoom(value);
    if (ob->IsClosure()) {
        DCHECK(ob->AsClosure()->IsOpen());
        builder()->close_fn(result.offset);
    }

    node->instance()->set_bind_kind(Variable::LOCAL);
    node->instance()->set_offset(result.offset);
}

void EmittingAstVisitor::VisitFunctionLiteral(FunctionLiteral *node) {
    EmittedScope info(&current_, node->prototype(), node->scope());

    auto prototype = node->prototype();
    auto scope = node->scope();

    // bind all of upval first.
    for (int i = 0; i < node->up_values_size(); ++i) {
        auto upval = node->up_value(i);
        upval->set_bind_kind(Variable::UP_VALUE);
        upval->set_offset(i * kObjectReferenceSize);
    }

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
    auto object_argument_size = info.o_stack_size();

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
                EmitMove(dest, result);
            } else {
                VMValue dest;
                dest.segment = BC_LOCAL_OBJECT_SEGMENT;
                dest.size    = size;
                dest.offset  = -size;
                EmitMove(dest, result);
            }
            builder()->ret();
        }
    }

    // refill frame instruction
    auto frame = BitCodeBuilder::Make4OpBC(BC_frame,
                                           info.p_stack_size(),
                                           info.o_stack_size(),
                                           0, object_argument_size);
    builder()->code()->Set(frame_placement * sizeof(uint64_t), frame);

    Handle<MIOFunction> ob =
            emitter_->object_factory_->CreateNormalFunction(info.constant_objects(),
                                                            info.constant_primitive_data(),
                                                            info.constant_primitive_size(),
                                                            builder()->code()->offset(0),
                                                            builder()->code()->size());
    if (node->up_values_size() > 0) {
        auto closure = emitter_->object_factory_->CreateClosure(ob, node->up_values_size());
        for (int i = 0; i < node->up_values_size(); ++i) {
            auto upval = node->up_value(i);
            auto offset = GetVariableOffset(upval->link(), info.scope());
            auto desc = closure->GetUpValue(i);

            desc->desc.offset = offset;
            if (upval->type()->is_primitive()) {
                desc->desc.unique_id =
                    static_cast<int32_t>((upval->link()->unique_id() & 0x7fffffff) << 1) | 0;
            } else {
                desc->desc.unique_id =
                    static_cast<int32_t>((upval->link()->unique_id() & 0x7fffffff) << 1) | 1;
            }
        }
        ob = closure;
    }
    PushValue(info.prev()->MakeConstantObjectValue(ob));
}

void EmittingAstVisitor::VisitBlock(Block *node) {
    for (int i = 0; i < node->mutable_body()->size() - 1; ++i) {
        Emit(node->mutable_body()->At(i));
    }
    if (node->mutable_body()->is_not_empty()) {
        node->mutable_body()->last()->Accept(this);
    }
}

void EmittingAstVisitor::VisitForeachLoop(ForeachLoop *node) {
    Type *key_type = nullptr;
    if (node->has_key()) {
        key_type = node->key()->type();
    } else {
        if (node->container_type()->IsMap()) {
            key_type = node->container_type()->AsMap()->key();
        } else { // TODO: other container types
            DLOG(FATAL) << "type can not be foreach.";
        }
    }
    VMValue key = current_->MakeLocalValue(key_type);
    if (node->has_key()) {
        node->key()->instance()->set_bind_kind(Variable::LOCAL);
        node->key()->instance()->set_offset(key.offset);
    }
    VMValue value = current_->MakeLocalValue(node->value()->type());
    node->value()->instance()->set_bind_kind(Variable::LOCAL);
    node->value()->instance()->set_offset(value.offset);

    auto container = Emit(node->container());

    if (node->container_type()->IsMap()) {
        builder()->oop(OO_MapFirstKey, container.offset, key.offset, value.offset);
        auto outter = builder()->jmp(builder()->pc());
        Emit(node->body());
        builder()->oop(OO_MapNextKey, container.offset, key.offset, value.offset);
        builder()->jmp(outter - builder()->pc() + 1);
        builder()->FillPlacement(outter,
                                 BitCodeBuilder::Make3AddrBC(BC_jmp, 0, 0, builder()->pc() - outter));
    } else {
        // TODO: other container types.
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
            EmitMove(dest, result);
        } else {
            VMValue dest;
            dest.segment = BC_LOCAL_OBJECT_SEGMENT;
            dest.size    = size;
            dest.offset  = -size;
            EmitMove(dest, result);
        }
    }
    builder()->ret();
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitCall(Call *node) {
    auto expr = Emit(node->expression());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, expr.segment);

    if (node->callee_type()->IsFunctionPrototype()) {
        EmitFunctionCall(expr, node);
    } else if (node->callee_type()->IsMap()) {
        EmitMapAccessor(expr, node);
    } else {
        DLOG(FATAL) << "noreached! callee: " << node->callee_type()->ToString();
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
                tmp = EmitEmptyValue(node->type());
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, tmp, node->initializer_type());
                    tmp = union_ob;
                }
            } else {
                tmp = EmitEmptyValue(node->type());
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
                value = EmitEmptyValue(node->type());
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, value, node->initializer_type());
                    value = union_ob;
                }
            } else {
                value = EmitEmptyValue(node->type());
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
                tmp = EmitEmptyValue(node->type());
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, tmp, node->initializer_type());
                    tmp = union_ob;
                }
            } else {
                tmp = EmitEmptyValue(node->type());
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
                value = EmitEmptyValue(node->type());
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, value, node->initializer_type());
                    value = union_ob;
                }
            } else {
                value = EmitEmptyValue(node->type());
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
        bool need_union = node->then_type()->GenerateId() !=
                          node->else_type()->GenerateId();

        VMValue val;
        auto then_val = Emit(node->then_statement());
        if (need_union) {
            val.segment = BC_LOCAL_OBJECT_SEGMENT;
            val.offset  = current_->MakeObjectRoom();
            val.size    = kObjectReferenceSize;
            EmitCreateUnion(val, then_val, node->then_type());
        }
        auto leave = builder()->jmp(builder()->pc());
        // bind outter
        builder()->FillPlacement(outter, BitCodeBuilder::Make3AddrBC(BC_jz, 0, cond.offset, builder()->pc() - outter));

        auto else_val = Emit(node->else_statement());
        if (need_union) {
            EmitCreateUnion(val, else_val, node->then_type());
        } else {
            if (node->then_type()->IsVoid()) {
                val = VMValue::Void();
            } else if (node->then_type()->is_primitive()) {
                val = then_val;
                EmitMove(val, else_val);
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
        EmitCreateUnion(val, then_val, node->then_type());
        auto leave = builder()->jmp(builder()->pc());

        // else
        // bind outter
        builder()->FillPlacement(outter, BitCodeBuilder::Make3AddrBC(BC_jz, 0, cond.offset, builder()->pc() - outter));
        EmitCreateUnion(val, {}, emitter_->types_->GetVoid());

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

void EmittingAstVisitor::VisitAssignment(Assignment *node) {
    auto rval = Emit(node->rval());

    if (node->target()->IsVariable()) {
        auto var = node->target()->AsVariable();
        DCHECK(var->is_readwrite());

        if (var->bind_kind() == Variable::UNBINDED) {
            DCHECK_EQ(MODULE_SCOPE, var->scope()->type());
            Emit(var->declaration());
        }
        DCHECK_NE(Variable::UNBINDED, var->bind_kind());

        VMValue dest;
        dest.size   = var->type()->placement_size();
        dest.offset = var->offset();

        DCHECK(!var->type()->IsVoid());
        if (var->type()->is_primitive()) {
            switch (var->bind_kind()) {
                case Variable::GLOBAL:
                    dest.segment = BC_GLOBAL_PRIMITIVE_SEGMENT;
                    break;
                case Variable::LOCAL:
                    dest.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
                    break;
                case Variable::UP_VALUE:
                    dest.segment = BC_UP_PRIMITIVE_SEGMENT;
                    break;
                default:
                    DLOG(FATAL) << "noreached!";
                    break;
            }
        } else {
            switch (var->bind_kind()) {
                case Variable::GLOBAL:
                    dest.segment = BC_GLOBAL_OBJECT_SEGMENT;
                    break;
                case Variable::LOCAL:
                    dest.segment = BC_LOCAL_OBJECT_SEGMENT;
                    break;
                case Variable::UP_VALUE:
                    dest.segment = BC_UP_OBJECT_SEGMENT;
                    break;
                default:
                    DLOG(FATAL) << "noreached!";
                    break;
            }
        }
        if (var->type()->IsUnion()) {
            auto union_ob = current_->MakeObjectValue();
            EmitCreateUnion(union_ob, rval, node->rval_type());
            rval = union_ob;
        }
        if (dest.segment == BC_GLOBAL_OBJECT_SEGMENT ||
            dest.segment == BC_GLOBAL_PRIMITIVE_SEGMENT ||
            dest.segment == BC_UP_PRIMITIVE_SEGMENT ||
            dest.segment == BC_UP_OBJECT_SEGMENT) {
            EmitStore(dest, rval);
        } else if (dest.segment == BC_LOCAL_PRIMITIVE_SEGMENT ||
                   dest.segment == BC_LOCAL_OBJECT_SEGMENT) {
            EmitMove(dest, rval);
        } else {
            DLOG(FATAL) << "noreached!";
        }
    } else if (node->target()->IsCall()) {
        auto target = node->target()->AsCall();
        DCHECK(target->callee_type()->IsMap());
        DCHECK_EQ(1, target->mutable_arguments()->size());

        auto map = Emit(target->expression());
        auto key = Emit(target->mutable_arguments()->first());

        EmitMapPut(map, key, rval, target->callee_type()->AsMap(),
                   node->rval_type());
    } else if (node->target()->IsFieldAccessing()) {
        auto target = node->target()->AsFieldAccessing();
        DCHECK(target->callee_type()->IsMap());

        auto map_ty = target->callee_type()->AsMap();
        DCHECK(map_ty->key()->IsString());

        auto map = Emit(target->expression());
        auto key = EmitLoadMakeRoom(GetOrNewString(target->field_name(), nullptr));

        EmitMapPut(map, key, rval, map_ty, node->rval_type());
    }

    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitBinaryOperation(BinaryOperation *node) {

    switch (node->op()) {
        case OP_ADD:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitIntegralAdd(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitFloatingAdd(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            }
            break;

        case OP_SUB:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitIntegralSub(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

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
                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitIntegralCmp(node->lhs_type(), node->lhs(),
                                          node->rhs(), node->op()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitFloatingCmp(node->lhs_type(), node->lhs(),
                                          node->rhs(), node->op()));
            }
            break;

        case OP_STRCAT: {
            VMValue lhs = Emit(node->lhs());
            if (!node->lhs_type()->IsString()) {
                lhs = EmitToString(lhs, node->lhs_type());
            }
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, lhs.segment);

            VMValue rhs = Emit(node->rhs());
            if (!node->rhs_type()->IsString()) {
                rhs = EmitToString(rhs, node->rhs_type());
            }
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, rhs.segment);

            VMValue result = current_->MakeObjectValue();
            builder()->oop(OO_StrCat, result.offset, lhs.offset, rhs.offset);
            PushValue(result);
        } break;

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
        } else if (node->bind_kind() == Variable::UP_VALUE) {

            VMValue tmp = {
                .segment = BC_UP_PRIMITIVE_SEGMENT,
                .size    = node->type()->placement_size(),
                .offset  = node->offset(),
            };
            value = EmitLoadMakeRoom(tmp);
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
        } else if (node->bind_kind() == Variable::UP_VALUE) {

            VMValue tmp = {
                .segment = BC_UP_OBJECT_SEGMENT,
                .size    = value.size,
                .offset  = node->offset(),
            };
            value = EmitLoadMakeRoom(tmp);
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
    auto dest = current_->MakePrimitiveValue((node->bitwide() + 7) / 8);

    switch (node->bitwide()) {
        case 1:
            builder()->load_i8_imm(dest.offset, node->i1());
            break;
    #define DEFINE_CASE(byte, bit) \
        case bit: \
            builder()->load_i##bit##_imm(dest.offset, node->i##bit ()); \
            break;
        MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
    #undef DEFINE_CASE
        case 64:
            EmitLoad(dest, current_->MakeConstantPrimitiveValue(node->i64()));
            break;
        default:
            DLOG(FATAL) << "noreached! bitwide = " << node->bitwide();
            break;
    }
    PushValue(dest);
}

void EmittingAstVisitor::VisitFloatLiteral(FloatLiteral *node) {
    VMValue src;
    switch (node->bitwide()) {
        case 32:
            src = current_->MakeConstantPrimitiveValue(node->f32());
            break;
        case 64:
            src = current_->MakeConstantPrimitiveValue(node->f64());
            break;
        default:
            DLOG(FATAL) << "noreached! bitwide = " << node->bitwide();
            break;
    }
    PushValue(EmitLoadMakeRoom(src));
}

void EmittingAstVisitor::VisitMapInitializer(MapInitializer *node) {
    auto dest = current_->MakeObjectValue();

    auto type = node->map_type();
    DCHECK(type->key()->CanBeKey());

    builder()->oop(OO_Map, dest.offset,
                   TypeInfoIndex(type->key()),
                   TypeInfoIndex(type->value()));

    for (int i = 0; i < node->mutable_pairs()->size(); ++i) {
        auto pair = node->mutable_pairs()->At(i);
        auto key = Emit(pair->key());

        auto value = Emit(pair->value());
        if (type->value()->IsUnion()) {
            auto tmp = current_->MakeObjectValue();
            EmitCreateUnion(tmp, value, pair->value_type());
            value = tmp;
        }
        builder()->oop(OO_MapPut, dest.offset, key.offset, value.offset);
    }

    PushValue(dest);
}

void EmittingAstVisitor::VisitFieldAccessing(FieldAccessing *node) {
    auto callee_ty = node->callee_type();

    if (callee_ty->IsMap()) {
        auto callee = Emit(node->expression());
        auto key = EmitLoadMakeRoom(GetOrNewString(node->field_name(), nullptr));
        auto result = current_->MakeObjectValue();

        builder()->oop(OO_MapGet, callee.offset, key.offset, result.offset);
        PushValue(result);
    } else {
        DLOG(FATAL) << "noreached! type: " << callee_ty->ToString();
    }
    // TODO: other types
}

void EmittingAstVisitor::VisitTypeTest(TypeTest *node) {
    auto val = Emit(node->expression());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, val.segment);

    auto result = current_->MakePrimitiveValue(1);
    builder()->oop(OO_UnionTest, result.offset, val.offset,
                   TypeInfoIndex(node->type()));
    PushValue(result);
}

void EmittingAstVisitor::VisitTypeCast(TypeCast *node) {
    auto val = Emit(node->expression());

    VMValue result;
    if (node->original()->IsUnion()) {
        DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, val.segment);
        if (node->type()->is_primitive()) {
            result = current_->MakePrimitiveValue(node->type()->placement_size());
        } else {
            result = current_->MakeObjectValue();
        }
        builder()->oop(OO_UnionUnbox, result.offset, val.offset,
                       TypeInfoIndex(node->type()));
    } else if (node->original()->IsIntegral()) {
        // TODO:
    } else if (node->original()->IsFloating()) {
        // TODO:
    } else {
        DLOG(FATAL) << "noreached! type: " << node->original()->ToString()
                    << " can not cast to " << node->type()->ToString();
    }
    PushValue(result);
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

void EmittingAstVisitor::EmitFunctionCall(const VMValue &callee, Call *node) {
    std::vector<VMValue> arguments;
    for (int i = 0; i < node->mutable_arguments()->size(); ++i) {
        auto argument = node->mutable_arguments()->At(i);
        arguments.push_back(Emit(argument));
    }

    auto proto = DCHECK_NOTNULL(node->callee_type()->AsFunctionPrototype());
    VMValue result;
    if (!proto->return_type()->IsVoid()) {
        if (proto->return_type()->is_primitive()) {
            result = current_->MakePrimitiveValue(proto->return_type()->placement_size());
        } else {
            result = current_->MakeObjectValue();
        }
    }

    auto p_base = current_->p_stack_size(), o_base = current_->o_stack_size();
    for (const auto &value : arguments) {
        switch (value.segment) {
            case BC_LOCAL_PRIMITIVE_SEGMENT:
                EmitMove(current_->MakePrimitiveValue(value.size), value);
                break;

            case BC_LOCAL_OBJECT_SEGMENT:
                EmitMove(current_->MakeObjectValue(), value);
                break;

            default:
                DLOG(FATAL) << "bad value segment: " << value.segment;
                break;
        }
    }

    builder()->call_val(p_base, o_base, callee.offset);
    if (proto->return_type()->IsVoid()) {
        PushValue(VMValue::Void());
    } else {
        PushValue(result);
    }
}

void EmittingAstVisitor::EmitMapAccessor(const VMValue &callee, Call *node) {
    auto map = Emit(node->expression());

    DCHECK_EQ(node->mutable_arguments()->size(), 1);
    if (node->mutable_arguments()->size() == 1) {
        auto key = Emit(node->mutable_arguments()->At(0));
        auto result = current_->MakeObjectValue();

        builder()->oop(OO_MapGet, map.offset,  key.offset, result.offset);
        PushValue(result);
    }
}

VMValue EmittingAstVisitor::EmitLoadMakeRoom(const VMValue &src) {
    VMValue dest = { .segment = MAX_BC_SEGMENTS, .size = -1, .offset = -1 };
    switch (src.segment) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
        case BC_UP_PRIMITIVE_SEGMENT:
        case BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT:
            dest = current_->MakePrimitiveValue(src.size);
            break;

        case BC_UP_OBJECT_SEGMENT:
        case BC_FUNCTION_CONSTANT_OBJECT_SEGMENT:
        case BC_GLOBAL_OBJECT_SEGMENT:
            dest = current_->MakeObjectValue();
            break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
    EmitLoad(dest, src);
    return dest;
}

void EmittingAstVisitor::EmitLoad(const VMValue &dest, const VMValue &src) {
    switch (src.segment) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
        case BC_UP_PRIMITIVE_SEGMENT:
        case BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT: {
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, dest.segment);

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            builder()->load_##byte##b(dest.offset, src.segment, src.offset); \
            break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_UP_OBJECT_SEGMENT:
        case BC_FUNCTION_CONSTANT_OBJECT_SEGMENT:
        case BC_GLOBAL_OBJECT_SEGMENT: {
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, dest.segment);
            builder()->load_o(dest.offset, src.segment, src.offset);
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
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

            builder()->store_o(value.offset, value.segment, src.offset);
            return value;
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
    return { .segment = MAX_BC_SEGMENTS, .size = -1, .offset = -1 };
}

void EmittingAstVisitor::EmitStore(const VMValue &dest, const VMValue &src) {
    switch (src.segment) {
        case BC_LOCAL_PRIMITIVE_SEGMENT: {

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            DCHECK(dest.segment == BC_GLOBAL_PRIMITIVE_SEGMENT || \
                   dest.segment == BC_UP_PRIMITIVE_SEGMENT); \
            builder()->store_##byte##b(dest.offset, dest.segment, src.offset); \
            break;
            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_LOCAL_OBJECT_SEGMENT:
            DCHECK(dest.segment == BC_GLOBAL_OBJECT_SEGMENT ||
                   dest.segment == BC_UP_OBJECT_SEGMENT);
            builder()->store_o(dest.offset, dest.segment, src.offset);
            break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
}

void EmittingAstVisitor::EmitMove(const VMValue &dest, const VMValue &src) {
    switch (src.segment) {
        case BC_LOCAL_PRIMITIVE_SEGMENT: {

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            builder()->mov_##byte##b(dest.offset, src.offset); \
            break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_LOCAL_OBJECT_SEGMENT: {
            builder()->mov_o(dest.offset, src.offset);
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
}

VMValue EmittingAstVisitor::EmitEmptyValue(Type *type) {
    DCHECK(!type->IsVoid());

    if (type->is_primitive()) {
        auto result = current_->MakePrimitiveValue(type->placement_size());

        if (type->IsIntegral()) {
            switch (type->placement_size()) {
            #define DEFINE_CASE(byte, bit) \
                case byte: \
                    builder()->load_i##bit##_imm(result.offset, 0);
                MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                case 8: {
                    auto tmp = current_->MakeConstantPrimitiveValue(static_cast<mio_i64_t>(0));
                    EmitLoad(result, tmp);
                } break;
                default:
                    DLOG(FATAL) << "noreached, bad integral size: " << type->placement_size();
                    break;
            }
        } else if (type->IsFloating()) {
            #define DEFINE_CASE(byte, bit) \
                case byte: { \
                    auto tmp = current_->MakeConstantPrimitiveValue(static_cast<mio_f##bit##_t>(0)); \
                    EmitLoad(result, tmp); \
                } break;
            MIO_FLOAT_BYTES_SWITCH(type->placement_size(), DEFINE_CASE)
            #undef DEFINE_CASE
        }
    } else {
        auto result = current_->MakeObjectValue();

        if (type->IsString()) {
            EmitLoad(result, GetOrNewString("", 0, nullptr));
        } else if (type->IsUnion()) {
            EmitCreateUnion(result, {}, emitter_->types_->GetVoid());
        } else if (type->IsMap()) {
            auto map = type->AsMap();
            builder()->oop(OO_Map, result.offset, TypeInfoIndex(map->key()),
                           TypeInfoIndex(map->value()));
        }
        return result;
    }

    // TODO:
    return {};
}

int EmittingAstVisitor::GetVariableOffset(Variable *var, Scope *scope) {
    DCHECK_EQ(FUNCTION_SCOPE, scope->type());
    DCHECK_NE(Variable::UNBINDED, var->bind_kind());

    auto fn_frame = current_;
    DCHECK_EQ(fn_frame->scope(), scope);

    int base = 0, fn_layout = 0;
    for (;;) {
        scope = scope->outter_scope();

        if (scope->type() == FUNCTION_SCOPE) {
            fn_frame = fn_frame->prev();
            ++fn_layout;
            if (var->type()->is_primitive()) {
                base += fn_frame->p_stack_size();
            } else {
                base += fn_frame->o_stack_size();
            }
            DCHECK_EQ(fn_frame->scope(), scope) << scope->name()->ToString();
        }

        if (scope == var->scope()) {
            if (fn_layout == 0) { // variable in same function scope.
                return var->offset();
            }
            fn_frame = fn_frame->prev();
            if (var->type()->is_primitive()) {
                base += (fn_frame->p_stack_size() - var->offset());
            } else {
                base += (fn_frame->o_stack_size() - var->offset());
            }
            return -base;
        }
    }

    return 0;
}

VMValue EmittingAstVisitor::GetOrNewString(const char *z, int n, Handle<MIOString> *rv) {
    auto ob = emitter_->object_factory_->GetOrNewString(z, n);
    auto value = current_->MakeConstantObjectValue(ob);

    if (rv) {
        *rv = std::move(ob);
    }
    return value;
}

Handle<MIOReflectionType>
TypeToReflection(Type *type, ObjectFactory *factory,
                 std::map<int64_t, Handle<MIOReflectionType>> *all) {
    auto tid = type->GenerateId();
    auto iter = all->find(tid);
    if (iter != all->end()) {
        return iter->second;
    }

    Handle<MIOReflectionType> reft;

    switch (type->type_kind()) {

        case Type::kUnknown:
            break;

        case Type::kVoid:
            reft = factory->CreateReflectionVoid(tid);
            break;

        case Type::kIntegral:
            if (type->AsIntegral()->bitwide() == 1) {
                reft = factory->CreateReflectionIntegral(tid, 8);
            } else {
                reft = factory->CreateReflectionIntegral(tid, type->AsIntegral()->bitwide());
            }
            break;

        case Type::kFloating:
            reft = factory->CreateReflectionFloating(tid, type->AsFloating()->bitwide());
            break;

        case Type::kString:
            reft = factory->CreateReflectionString(tid);
            break;

        case Type::kError:
            reft = factory->CreateReflectionError(tid);
            break;

        case Type::kUnion:
            reft = factory->CreateReflectionUnion(tid);
            break;

        case Type::kMap: {
            auto map = type->AsMap();
            auto key = TypeToReflection(map->key(), factory, all);
            auto value = TypeToReflection(map->value(), factory, all);
            reft = factory->CreateReflectionMap(map->GenerateId(), key, value);
        } break;

        case Type::kFunctionPrototype: {
            auto func = type->AsFunctionPrototype();
            auto return_type = TypeToReflection(func->return_type(), factory, all);

            std::vector<Handle<MIOReflectionType>> params;
            for (int i = 0; i < func->mutable_paramters()->size(); ++i) {
                auto ty = func->mutable_paramters()->At(i)->param_type();
                params.emplace_back(TypeToReflection(ty, factory, all));
            }
            reft = factory->CreateReflectionFunction(tid, return_type,
                                                     func->mutable_paramters()->size(), params);
        } break;
            
        default:
            DLOG(FATAL) << "noreached! type: " << type->ToString();
            return make_handle<MIOReflectionType>(nullptr);
    }

    all->emplace(tid, reft);
    return reft;
}

BitCodeEmitter::BitCodeEmitter(MemorySegment *p_global,
                               MemorySegment *o_global,
                               TypeFactory *types,
                               ObjectFactory *object_factory,
                               FunctionRegister *function_register)
    : p_global_(DCHECK_NOTNULL(p_global))
    , o_global_(DCHECK_NOTNULL(o_global))
    , types_(DCHECK_NOTNULL(types))
    , object_factory_(DCHECK_NOTNULL(object_factory))
    , function_register_(DCHECK_NOTNULL(function_register)) {
}

BitCodeEmitter::~BitCodeEmitter() {
}

void BitCodeEmitter::Init() {
    DCHECK(type_id2index_.empty());

    std::map<int64_t, Type *> all_type;
    auto rv = types_->GetAllType(&all_type);
    DCHECK_LT(rv, 0x7fff);

    std::map<int64_t, Handle<MIOReflectionType>> all_obj;
    for (const auto &pair : all_type) {
        if (pair.second->IsUnknown()) {
            continue;
        }
        TypeToReflection(pair.second, object_factory_, &all_obj);
    }

    all_type_base_ = o_global_->size();
    int index = 0;
    for (const auto &pair : all_obj) {
        type_id2index_.emplace(pair.first, index++);
        o_global_->Add(pair.second.get());
    }
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

bool BitCodeEmitter::Run(ParsedModuleMap *all_modules, CompiledInfo *info) {
    DCHECK_NOTNULL(all_modules);
    DLOG(INFO) << "max number of instructions: " << MAX_BC_INSTRUCTIONS;

    auto pair = DCHECK_NOTNULL(all_modules->Get(kMainValue)); // "main"
    auto ok = EmitModule(pair->key(), pair->value(), all_modules);

    if (info) {
        info->all_type_base   = all_type_base_;
        info->void_type_index = type_id2index_[types_->GetVoid()->GenerateId()];
        info->global_primitive_segment_bytes = p_global_->size();
        info->global_object_segment_bytes    = o_global_->size();
    }
    return ok;
}

bool BitCodeEmitter::EmitModule(RawStringRef module_name,
                                ParsedUnitMap *all_units,
                                ParsedModuleMap *all_modules) {
    auto zone = types_->zone();
    auto proto = types_->GetFunctionPrototype(new (zone) ZoneVector<Paramter *>(zone),
                                              types_->GetVoid());

    EmittingAstVisitor visitor(module_name, this);
    EmittedScope info(visitor.mutable_current(), proto, nullptr);

    // placement frame instruction
    auto frame_placement = visitor.builder()->debug();

    ParsedUnitMap::Iterator iter(all_units);
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

    auto builder = info.builder();
    auto main_name = TextOutputStream::sprintf("::%s::main", module_name->c_str());
    auto entry = function_register_->FindOrNull(main_name.c_str());
    if (entry) {
        auto local = info.MakeObjectRoom();
        info.builder()->load_o(local, BC_GLOBAL_OBJECT_SEGMENT, entry->offset());
        info.builder()->call_val(0, 0, local);
    }
    builder->ret();

    // refill frame instruction
    auto frame = BitCodeBuilder::Make4OpBC(BC_frame, info.p_stack_size(),
                                           info.o_stack_size(),
                                           0, 0);
    builder->code()->Set(frame_placement * sizeof(uint64_t), frame);

    auto boot_name = TextOutputStream::sprintf("::%s::bootstrap", module_name->c_str());
    auto ob = object_factory_->CreateNormalFunction(info.constant_objects(),
                                                    info.constant_primitive_data(),
                                                    info.constant_primitive_size(),
                                                    builder->code()->offset(0),
                                                    builder->code()->size());
    Handle<MIOString> inner_name;
    visitor.GetOrNewString(boot_name, &inner_name);
    ob->SetName(inner_name.get());

    auto offset = o_global_->size();
    o_global_->Add(ob.get());
    entry = function_register_->FindOrInsert(boot_name.c_str());
    entry->set_offset(offset);

    return true;
}

bool BitCodeEmitter::ProcessImportList(PackageImporter *pkg,
                                       EmittedScope *info,
                                       ParsedModuleMap *all_modules) {
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
        auto name = TextOutputStream::sprintf("::%s::bootstrap", module_name->c_str());
        auto entry = DCHECK_NOTNULL(function_register_->FindOrNull(name.c_str()));
        auto local = info->MakeObjectRoom();

        info->builder()->load_o(local, BC_GLOBAL_OBJECT_SEGMENT, entry->offset());
        info->builder()->call_val(0, 0, local);
    }
    return true;
}

} // namespace mio
