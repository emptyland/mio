#include "bitcode-emitter.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode.h"
#include "vm-objects.h"
#include "vm-object-factory.h"
#include "vm-object-extra-factory.h"
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

static const HeapObject * const kNullObject = nullptr;

static const BCInstruction kNop = static_cast<BCInstruction>(0);

struct CastItem {
    BCInstruction cmd;
    int out_size;
};

// 8 | 16 | 32 | 64
const CastItem kNumericCastMatrix[][Numeric::kNumberOfNumericTypes] = {
    //[i8]     i8               i16                 i32                   i64                    f32               f64
    {{kNop,          0}, {BC_sext_i8,    2}, {BC_sext_i8,    4}, {BC_sext_i8,    8}, {BC_sitofp_i8,   4}, {BC_sitofp_i8,  8}},
    //[i16]    i8               i16                 i32                   i64                    f32               f64
    {{BC_trunc_i16,  1}, {kNop,          0}, {BC_sext_i16,   4}, {BC_sext_i16,   8}, {BC_sitofp_i16,  4}, {BC_sitofp_i16, 8}},
    //[i32]    i8               i16                 i32                   i64                    f32               f64
    {{BC_trunc_i32,  1}, {BC_trunc_i32,  2}, {kNop,          0}, {BC_sext_i32,   8}, {BC_sitofp_i32,  4}, {BC_sitofp_i32, 8}},
    //[i64]    i8               i16                 i32                   i64                    f32               f64
    {{BC_trunc_i64,  1}, {BC_trunc_i64,  2}, {BC_trunc_i64,  4}, {kNop,          0}, {BC_sitofp_i64,  4}, {BC_sitofp_i64, 8}},
    //[f32]    i8               i16                 i32                   i64                    f32               f64
    {{BC_fptosi_f32, 1}, {BC_fptosi_f32, 2}, {BC_fptosi_f32, 4}, {BC_fptosi_f32, 8}, {kNop,           0}, {BC_fpext_f32,  8}},
    //[f64]    i8               i16                 i32                   i64                    f32               f64
    {{BC_fptosi_f64, 1}, {BC_fptosi_f64, 2}, {BC_fptosi_f64, 4}, {BC_fptosi_f64, 8}, {BC_fptrunc_f64, 4}, {kNop,          0}},
};

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

    const std::vector<int> &pc_to_position() const {
        DCHECK_EQ(builder_.pc(), pc_to_position_.size());
        return pc_to_position_;
    }

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

    BitCodeBuilder *naked_builder() { return &builder_; }

    BitCodeBuilder *builder(int position) {
        auto pc = static_cast<int>(pc_to_position_.size());
        pc_to_position_.push_back(position);
        DCHECK_EQ(pc, builder_.pc()) << "some hole in position map.";
        return naked_builder();
    }

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
    std::vector<int> pc_to_position_; // pc to position mapping, for debuginfo
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
    virtual void VisitBuiltinCall(BuiltinCall *node) override;
    virtual void VisitValDeclaration(ValDeclaration *node) override;
    virtual void VisitVarDeclaration(VarDeclaration *node) override;
    virtual void VisitIfOperation(IfOperation *node) override;
    virtual void VisitUnaryOperation(UnaryOperation *node) override;
    virtual void VisitAssignment(Assignment *node) override;
    virtual void VisitBinaryOperation(BinaryOperation *node) override;
    virtual void VisitReference(Reference *node) override;
    virtual void VisitStringLiteral(StringLiteral *node) override;
    virtual void VisitSmiLiteral(SmiLiteral *node) override;
    virtual void VisitFloatLiteral(FloatLiteral *node) override;
    virtual void VisitArrayInitializer(ArrayInitializer *node) override;
    virtual void VisitMapInitializer(MapInitializer *node) override;
    virtual void VisitFieldAccessing(FieldAccessing *node) override;
    virtual void VisitTypeTest(TypeTest *node) override;
    virtual void VisitTypeCast(TypeCast *node) override;
    virtual void VisitTypeMatch(TypeMatch *node) override;

    virtual void VisitElement(Element */*node*/) override {
        DLOG(FATAL) << "noreached!";
    }
    virtual void VisitPair(Pair */*node*/) override {
        DLOG(FATAL) << "noreached!";
    }

    BitCodeBuilder *naked_builder() {
        return DCHECK_NOTNULL(current_)->naked_builder();
    }

    BitCodeBuilder *builder(int position) {
        return DCHECK_NOTNULL(current_)->builder(position);
    }

    TypeFactory *types() const { return emitter_->types_; }

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

    VMValue EmitBitShift(BinaryOperation *node,
            std::function<void(BitCodeBuilder *, int, uint16_t, uint16_t, int32_t)> build_imm,
            std::function<void(BitCodeBuilder *, int, uint16_t, uint16_t, int32_t)> build_reg);

    VMValue EmitIntegralAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingAdd(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingSub(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralMul(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingMul(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralDiv(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitFloatingDiv(Type *type, Expression *lhs, Expression *rhs);
    VMValue EmitIntegralCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);
    VMValue EmitFloatingCmp(Type *type, Expression *lhs, Expression *rhs,
                            Operator op);

    void EmitCreateUnion(const VMValue &dest, const VMValue &src, Type *type,
                         int position) {
        builder(position)->oop(OO_UnionOrMerge, dest.offset,  src.offset,
                               TypeInfoIndex(type));
    }

    void EmitFunctionCall(const VMValue &callee, Call *node);
    void EmitMapAccessor(const VMValue &callee, Call *node);
    void EmitArrayAccessorOrMakeSlice(const VMValue &callee, Call *node);

    void EmitMapPut(const VMValue &map, VMValue key, VMValue value, Map *map_ty,
                    Type *val_ty, int position) {
        if (map_ty->value()->IsUnion()) {
            auto tmp = current_->MakeObjectValue();
            EmitCreateUnion(tmp, value, val_ty, position);
            value = tmp;
        }
        builder(position)->oop(OO_MapPut, map.offset, key.offset, value.offset);
    }

    VMValue EmitToString(const VMValue &input, Type *type, int position) {
        auto result = current_->MakeObjectValue();
        auto index = TypeInfoIndex(type);

        builder(position)->oop(OO_ToString, result.offset,  input.offset, index);
        return result;
    }

    VMValue EmitLoadMakeRoom(const VMValue &src, int position);
    VMValue EmitStoreMakeRoom(const VMValue &src, int position);

    void EmitLoad(const VMValue &dest, const VMValue &src, int position);
    void EmitStore(const VMValue &dest, const VMValue &src, int position);
    void EmitMove(const VMValue &dest, const VMValue &src, int position);

    VMValue EmitNumericCastMakeRoomIfNeeded(const VMValue &target, Numeric *from,
                                            Numeric *cast_to, int position);

    VMValue EmitEmptyValue(Type *type, int position);

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
    #define DEFINE_CASE(name, op) \
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

    int GetLastPosition(Statement *stmt) {
        return stmt->IsBlock() ? stmt->AsBlock()->end_position() :
               stmt->position();
    }

    void RegisterGlobalVariable(const std::string &full_name, const Type *type,
                                const VMValue &value, bool readonly);

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
        auto fn = emitter_->object_factory_->CreateNativeFunction(proto->GetSignature().c_str(),
                                                                  nullptr);
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

    auto result = EmitLoadMakeRoom(value, node->position());
    if (ob->IsClosure()) {
        DCHECK(ob->AsClosure()->IsOpen());
        builder(node->position())->close_fn(result.offset);
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
    auto frame_placement = builder(node->position())->debug();

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
        builder(node->end_position())->ret();
    } else {
        auto result = Emit(node->body());
        if (node->is_assignment()) {
            auto size = prototype->return_type()->placement_size();
            if (prototype->return_type()->is_primitive()) {
                VMValue dest;
                dest.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
                dest.size    = size;
                dest.offset  = -size;
                EmitMove(dest, result, GetLastPosition(node->body()));
            } else {
                VMValue dest;
                dest.segment = BC_LOCAL_OBJECT_SEGMENT;
                dest.size    = size;
                dest.offset  = -size;
                EmitMove(dest, result, GetLastPosition(node->body()));
            }
            builder(node->end_position())->ret();
        }
    }

    // refill frame instruction
    naked_builder()->frame(frame_placement, info.p_stack_size(), info.o_stack_size(),
                           object_argument_size);

    Handle<MIOFunction> ob = emitter_
            ->object_factory_
            ->CreateNormalFunction(info.constant_objects(),
                                   info.constant_primitive_data(),
                                   info.constant_primitive_size(),
                                   naked_builder()->code()->offset(0),
                                   naked_builder()->code()->size());
    auto debug_info = emitter_->extra_factory_
            ->CreateFunctionDebugInfo(unit_name_, info.pc_to_position());
    ob->AsNormalFunction()->SetDebugInfo(debug_info);

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
    } else {
        PushValue(VMValue::Void());
    }
}

void EmittingAstVisitor::VisitForeachLoop(ForeachLoop *node) {
    Type *key_type = nullptr;
    if (node->has_key()) {
        key_type = node->key()->type();
    } else {
        if (node->container_type()->IsMap()) {
            key_type = node->container_type()->AsMap()->key();
        } else if (node->container_type()->IsArray() ||
                   node->container_type()->IsSlice()) {
            key_type = types()->GetInt();
        } else {
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
        builder(node->position())->oop(OO_MapFirstKey, container.offset,
                                       key.offset, value.offset);
        auto outter = builder(node->position())->jmp(naked_builder()->pc());
        Emit(node->body());
        builder(node->position())->oop(OO_MapNextKey, container.offset,
                                       key.offset, value.offset);
        builder(node->position())->jmp(outter - naked_builder()->pc() + 1);
        naked_builder()->jmp_fill(outter, naked_builder()->pc() - outter);
    } else if (node->container_type()->IsArray() ||
               node->container_type()->IsSlice()) {
        auto zero = current_->MakeConstantPrimitiveValue(static_cast<mio_int_t>(0));
        EmitLoad(key, zero, node->position());

        auto size = current_->MakeLocalValue(types()->GetI64());
        builder(node->position())->oop(OO_ArraySize, container.offset,
                                       size.offset, 0);

        auto cond = current_->MakeLocalValue(key_type);
        auto retry = builder(node->position())->cmp_i64(CC_LT, cond.offset,
                                                        key.offset,
                                                        size.offset);
        auto outter = builder(node->value()->position())->jz(cond.offset,
                                                             naked_builder()->pc());
        builder(node->value()->position())->oop(OO_ArrayGet, container.offset,
                                                key.offset, value.offset);

        Emit(node->body());
        auto one = current_->MakeConstantPrimitiveValue(static_cast<mio_int_t>(1));
        one = EmitLoadMakeRoom(one, node->position());
        builder(node->position())->add_i64(key.offset, key.offset, one.offset);
        builder(node->position())->jmp(retry - naked_builder()->pc());
        naked_builder()->jz_fill(outter, cond.offset, naked_builder()->pc() - outter);
    }
    PushValue(VMValue::Void());
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
            EmitMove(dest, result, node->position());
        } else {
            VMValue dest;
            dest.segment = BC_LOCAL_OBJECT_SEGMENT;
            dest.size    = size;
            dest.offset  = -size;
            EmitMove(dest, result, node->position());
        }
    }
    builder(node->position())->ret();
    PushValue(VMValue::Void());
}

void EmittingAstVisitor::VisitCall(Call *node) {
    auto expr = Emit(node->expression());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, expr.segment);

    if (node->callee_type()->IsFunctionPrototype()) {
        EmitFunctionCall(expr, node);
    } else if (node->callee_type()->IsMap()) {
        EmitMapAccessor(expr, node);
    } else if (node->callee_type()->IsSlice() || node->callee_type()->IsArray()) {
        EmitArrayAccessorOrMakeSlice(expr, node);
    } else {
        DLOG(FATAL) << "noreached! callee: " << node->callee_type()->ToString();
    }
}

void EmittingAstVisitor::VisitBuiltinCall(BuiltinCall *node) {
    switch (node->code()) {
        case BuiltinCall::LEN: {
            auto container = Emit(node->argument(0)->value());
            BCObjectOperatorId op;
            if (node->argument(0)->value_type()->IsArray() ||
                node->argument(0)->value_type()->IsSlice()) {
                op = OO_ArraySize;
            } else if (node->argument(0)->value_type()->IsMap()) {
                op = OO_MapSize;
            } else if (node->argument(0)->value_type()->IsString()) {
                op = OO_StrLen;
            } else {
                DLOG(FATAL) << "noreached!";
            }
            auto len = current_->MakeLocalValue(types()->GetInt());
            builder(node->position())->oop(op, container.offset, 0, len.offset);
            PushValue(len);
        } break;

        case BuiltinCall::ADD: {
            DCHECK(node->argument(0)->value_type()->IsArray());
            auto container = Emit(node->argument(0)->value());

            auto array = node->argument(0)->value_type()->AsArray();
            auto element = node->argument(1);
            VMValue value;
            if (array->element()->IsUnion() &&
                !node->argument(1)->value_type()->IsUnion()) {
                auto tmp = Emit(element->value());
                value = current_->MakeLocalValue(array->element());
                EmitCreateUnion(value, tmp, element->value_type(), element->position());
            } else {
                value = Emit(element->value());
            }
            builder(node->position())->oop(OO_ArrayAdd, container.offset, 0,
                                           value.offset);
            PushValue(VMValue::Void());
        } break;

        case BuiltinCall::DELETE: {
            DCHECK(node->argument(0)->value_type()->IsMap());
            auto container = Emit(node->argument(0)->value());
            auto map = node->argument(0)->value_type()->AsMap();
            auto key = node->argument(1);
            DCHECK_EQ(map->key(), key->value_type());

            auto key_value = Emit(key->value());
            auto rv = current_->MakeLocalValue(types()->GetI1());
            builder(node->position())->oop(OO_MapDelete, container.offset,
                                           key_value.offset, 0);
            PushValue(rv);
        } break;

        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
}

void EmittingAstVisitor::VisitValDeclaration(ValDeclaration *node) {
    if (TraceDeclaration(node)) {
        PushValue(VMValue::Void());
        return;
    }

    VMValue value;
    if (node->scope()->is_universal()) {

        VMValue tmp;
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = EmitEmptyValue(node->type(), node->position());
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, tmp, node->initializer_type(),
                                    node->initializer()->position());
                    tmp = union_ob;
                }
            } else {
                tmp = EmitEmptyValue(node->type(), node->position());
            }
        }
        value = EmitStoreMakeRoom(tmp, node->position());
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::GLOBAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
        RegisterGlobalVariable(node->scope()->MakeFullName(node->name()),
                               node->type(), value, true);
    } else {
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = EmitEmptyValue(node->type(), node->position());
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, value, node->initializer_type(),
                                    node->initializer()->position());
                    value = union_ob;
                }
            } else {
                value = EmitEmptyValue(node->type(), node->position());
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
    if (node->scope()->is_universal()) {

        VMValue tmp;
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
            } else {
                tmp = EmitEmptyValue(node->type(), node->position());
            }
        } else {
            if (node->has_initializer()) {
                tmp = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, tmp, node->initializer_type(),
                                    node->initializer()->position());
                    tmp = union_ob;
                }
            } else {
                tmp = EmitEmptyValue(node->type(), node->position());
            }
        }
        value = EmitStoreMakeRoom(tmp, node->position());
        DCHECK_NOTNULL(node->instance())->set_bind_kind(Variable::GLOBAL);
        DCHECK_NOTNULL(node->instance())->set_offset(value.offset);
        RegisterGlobalVariable(node->scope()->MakeFullName(node->name()),
                               node->type(), value, false);
    } else {
        if (node->type()->is_primitive()) {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
            } else {
                value = EmitEmptyValue(node->type(), node->position());
            }
        } else {
            if (node->has_initializer()) {
                value = Emit(node->initializer());
                if (node->type()->IsUnion()) {
                    auto union_ob = current_->MakeObjectValue();
                    EmitCreateUnion(union_ob, value, node->initializer_type(),
                                    node->initializer()->position());
                    value = union_ob;
                }
            } else {
                value = EmitEmptyValue(node->type(), node->position());
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

    auto outter = builder(node->position())->jz(cond.offset, naked_builder()->pc());
    if (node->has_else()) {
        bool need_union = node->then_type()->GenerateId() !=
                          node->else_type()->GenerateId();

        VMValue val;
        auto then_val = Emit(node->then_statement());
        if (need_union) {
            val.segment = BC_LOCAL_OBJECT_SEGMENT;
            val.offset  = current_->MakeObjectRoom();
            val.size    = kObjectReferenceSize;
            EmitCreateUnion(val, then_val, node->then_type(),
                            GetLastPosition(node->then_statement()));
        }
        auto leave = builder(node->then_statement()->position())->jmp(naked_builder()->pc());
        // fill back outter
        naked_builder()->jz_fill(outter, cond.offset, naked_builder()->pc() - outter);

        auto position = GetLastPosition(node->else_statement());
        auto else_val = Emit(node->else_statement());
        if (need_union) {
            EmitCreateUnion(val, else_val, node->then_type(), position);
        } else {
            if (node->then_type()->IsVoid()) {
                val = VMValue::Void();
            } else if (node->then_type()->is_primitive()) {
                val = then_val;
                EmitMove(val, else_val, position);
            }
        }

        // fill back leave
        naked_builder()->jmp_fill(leave, naked_builder()->pc() - leave);
        PushValue(val);
    } else {
        VMValue val;
        // then
        auto then_val = Emit(node->then_statement());

        val.segment = BC_LOCAL_OBJECT_SEGMENT;
        val.offset  = current_->MakeObjectRoom();
        val.size    = kObjectReferenceSize;
        EmitCreateUnion(val, then_val, node->then_type(),
                        GetLastPosition(node->then_statement()));
        auto leave = builder(GetLastPosition(node->then_statement()))->jmp(naked_builder()->pc());

        // else
        // fill back outter
        naked_builder()->jz_fill(outter, cond.offset, naked_builder()->pc() - outter);
        EmitCreateUnion(val, {}, types()->GetVoid(),
                        GetLastPosition(node->then_statement()));

        // fill back leave
        naked_builder()->jmp_fill(leave, naked_builder()->pc() - leave);
        PushValue(val);
    }
}

void EmittingAstVisitor::VisitUnaryOperation(UnaryOperation *node) {
    switch (node->op()) {
        case OP_NOT: {
            auto operand = Emit(node->operand());
            auto result = current_->MakeLocalValue(types()->GetI1());
            builder(node->position())->logic_not(result.offset, operand.offset);
            PushValue(result);
        } break;

        case OP_BIT_INV: {
            auto operand = Emit(node->operand());
            auto result = current_->MakeLocalValue(node->operand_type());

        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder(node->position())->inv_i##bit(result.offset, operand.offset); \
                break;
            MIO_INT_BYTES_SWITCH(operand.size, DEFINE_CASE)
        #undef DEFINE_CASE
            PushValue(result);
        } break;

        case OP_MINUS: {
            auto operand = Emit(node->operand());
            auto result = current_->MakeLocalValue(node->operand_type());
            auto zero = current_->MakeConstantPrimitiveValue<mio_i64_t>(0);
            zero = EmitLoadMakeRoom(zero, node->position());

            DCHECK(node->operand_type()->is_numeric());
            if (node->operand_type()->IsIntegral()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder(node->position())->sub_i##bit(result.offset, zero.offset, operand.offset); \
                    break;
                MIO_INT_BYTES_SWITCH(node->operand_type()->AsIntegral()->bitwide(), DEFINE_CASE)
            #undef DEFINE_CASE
            } else {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder(node->position())->sub_f##bit(result.offset, zero.offset, operand.offset); \
                    break;
                MIO_FLOAT_BYTES_SWITCH(node->operand_type()->AsFloating()->bitwide(), DEFINE_CASE)
            #undef DEFINE_CASE
            }
            PushValue(result);
        } break;

        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
}

void EmittingAstVisitor::VisitAssignment(Assignment *node) {
    auto rval = Emit(node->rval());

    if (node->target()->IsReference()) {
        auto var = node->target()->AsReference()->variable();
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
            EmitCreateUnion(union_ob, rval, node->rval_type(),
                            node->rval()->position());
            rval = union_ob;
        }
        if (dest.segment == BC_GLOBAL_OBJECT_SEGMENT ||
            dest.segment == BC_GLOBAL_PRIMITIVE_SEGMENT ||
            dest.segment == BC_UP_PRIMITIVE_SEGMENT ||
            dest.segment == BC_UP_OBJECT_SEGMENT) {
            EmitStore(dest, rval, node->target()->position());
        } else if (dest.segment == BC_LOCAL_PRIMITIVE_SEGMENT ||
                   dest.segment == BC_LOCAL_OBJECT_SEGMENT) {
            EmitMove(dest, rval, node->target()->position());
        } else {
            DLOG(FATAL) << "noreached!";
        }
    } else if (node->target()->IsCall()) {
        auto target = node->target()->AsCall();

        if (target->callee_type()->IsMap()) {
            DCHECK_EQ(1, target->argument_size());

            auto map = Emit(target->expression());
            auto key = Emit(target->mutable_arguments()->first()->value());

            EmitMapPut(map, key, rval, target->callee_type()->AsMap(),
                       node->rval_type(), node->position());
        } else if (target->callee_type()->IsArray() ||
                   target->callee_type()->IsSlice()) {
            DCHECK_EQ(1, target->argument_size());

            auto array = Emit(target->expression());
            auto index = Emit(target->argument(0));

            builder(node->position())->oop(OO_ArraySet, array.offset,
                                           index.offset, rval.offset);
        } else {
            DLOG(FATAL) << "noreached!";
        }
    } else if (node->target()->IsFieldAccessing()) {
        auto target = node->target()->AsFieldAccessing();
        DCHECK(target->callee_type()->IsMap());

        auto map_ty = target->callee_type()->AsMap();
        DCHECK(map_ty->key()->IsString());

        auto map = Emit(target->expression());
        auto key = EmitLoadMakeRoom(GetOrNewString(target->field_name(), nullptr),
                                    target->position());

        EmitMapPut(map, key, rval, map_ty, node->rval_type(),
                   node->position());
    } else {
        DLOG(FATAL) << "no type for assignment. " << node->node_type();
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

        case OP_MUL:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitIntegralMul(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitFloatingMul(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            }
            break;

        case OP_DIV:
            if (node->lhs_type()->IsIntegral()) {
                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitIntegralDiv(node->lhs_type(), node->lhs(),
                                          node->rhs()));
            } else if (node->lhs_type()->IsFloating()) {

                DCHECK_EQ(node->lhs_type()->GenerateId(),
                          node->rhs_type()->GenerateId());

                PushValue(EmitFloatingDiv(node->lhs_type(), node->lhs(),
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
                lhs = EmitToString(lhs, node->lhs_type(), node->lhs()->position());
            }
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, lhs.segment);

            VMValue rhs = Emit(node->rhs());
            if (!node->rhs_type()->IsString()) {
                rhs = EmitToString(rhs, node->rhs_type(), node->rhs()->position());
            }
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, rhs.segment);

            VMValue result = current_->MakeObjectValue();
            builder(node->position())->oop(OO_StrCat, result.offset, lhs.offset, rhs.offset);
            PushValue(result);
        } break;

        case OP_BIT_OR: {
            DCHECK_EQ(node->lhs_type(), node->rhs_type());
            DCHECK(node->lhs_type()->IsIntegral());

            auto lhs = Emit(node->lhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, lhs.segment);
            auto rhs = Emit(node->rhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, rhs.segment);

            auto result = current_->MakeLocalValue(node->lhs_type());
        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder(node->position())->or_i##bit(result.offset, lhs.offset, rhs.offset); \
                break;
            MIO_INT_BYTES_SWITCH(lhs.size, DEFINE_CASE)
        #undef DEFINE_CASE
            PushValue(result);
        } break;

        case OP_BIT_AND: {
            DCHECK_EQ(node->lhs_type(), node->rhs_type());
            DCHECK(node->lhs_type()->IsIntegral());

            auto lhs = Emit(node->lhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, lhs.segment);
            auto rhs = Emit(node->rhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, rhs.segment);

            auto result = current_->MakeLocalValue(node->lhs_type());
        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder(node->position())->and_i##bit(result.offset, lhs.offset, rhs.offset); \
                break;
            MIO_INT_BYTES_SWITCH(lhs.size, DEFINE_CASE)
        #undef DEFINE_CASE
            PushValue(result);
        } break;

        case OP_BIT_XOR: {
            DCHECK_EQ(node->lhs_type(), node->rhs_type());
            DCHECK(node->lhs_type()->IsIntegral());

            auto lhs = Emit(node->lhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, lhs.segment);
            auto rhs = Emit(node->rhs());
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, rhs.segment);

            auto result = current_->MakeLocalValue(node->lhs_type());
        #define DEFINE_CASE(byte, bit) \
            case byte: \
                builder(node->position())->xor_i##bit(result.offset, lhs.offset, rhs.offset); \
                break;
            MIO_INT_BYTES_SWITCH(lhs.size, DEFINE_CASE)
        #undef DEFINE_CASE
            PushValue(result);
        } break;

        case OP_LSHIFT:
            PushValue(EmitBitShift(node,
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t imm) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->shl_i##bit##_imm(result, lhs, imm); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   },
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t rhs) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->shl_i##bit(result, lhs, rhs); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   }));
            break;

        case OP_RSHIFT_A:
            PushValue(EmitBitShift(node,
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t imm) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->shr_i##bit##_imm(result, lhs, imm); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   },
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t rhs) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->shr_i##bit(result, lhs, rhs); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   }));
            break;

        case OP_RSHIFT_L:
            PushValue(EmitBitShift(node,
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t imm) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->ushr_i##bit##_imm(result, lhs, imm); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   },
                                   [](BitCodeBuilder *builder, int bitwide,
                                      uint16_t result, uint16_t lhs,
                                      int32_t rhs) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    builder->ushr_i##bit(result, lhs, rhs); \
                    break;
                                       MIO_INT_BYTES_SWITCH(bitwide, DEFINE_CASE)
            #undef DEFINE_CASE
                                   }));
            break;

            // TODO: other operator
        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
}

void EmittingAstVisitor::VisitReference(Reference *node) {
    auto var = node->variable();
    if (var->bind_kind() == Variable::UNBINDED) {
        DCHECK_EQ(MODULE_SCOPE, var->scope()->type());

        auto scope = current_;
        while (scope->prev()) {
            scope = scope->prev();
        }
        auto save = current_;
        current_ = scope;
        Emit(var->declaration());
        current_ = save;
    }
    DCHECK_NE(Variable::UNBINDED, var->bind_kind()) << var->name()->ToString();

    VMValue value;
    if (var->type()->is_primitive()) {
        if (var->bind_kind() == Variable::LOCAL ||
            var->bind_kind() == Variable::ARGUMENT) {

            value.segment = BC_LOCAL_PRIMITIVE_SEGMENT;
            value.size   = var->type()->placement_size();
            value.offset = var->offset();
        } else if (var->bind_kind() == Variable::UP_VALUE) {

            VMValue tmp = {
                .segment = BC_UP_PRIMITIVE_SEGMENT,
                .size    = var->type()->placement_size(),
                .offset  = var->offset(),
            };
            value = EmitLoadMakeRoom(tmp, node->position());
        } else if (var->bind_kind() == Variable::GLOBAL) {

            VMValue tmp = {
                .segment = BC_GLOBAL_PRIMITIVE_SEGMENT,
                .size    = var->type()->placement_size(),
                .offset  = var->offset(),
            };
            value = EmitLoadMakeRoom(tmp, node->position());
        }
    } else {
        if (var->bind_kind() == Variable::LOCAL ||
            var->bind_kind() == Variable::ARGUMENT) {
            value.segment = BC_LOCAL_OBJECT_SEGMENT;
            value.size   = var->type()->placement_size();
            value.offset = var->offset();
        } else if (var->bind_kind() == Variable::UP_VALUE) {

            VMValue tmp = {
                .segment = BC_UP_OBJECT_SEGMENT,
                .size    = value.size,
                .offset  = var->offset(),
            };
            value = EmitLoadMakeRoom(tmp, node->position());
        } else if (var->bind_kind() == Variable::GLOBAL) {
            VMValue tmp = {
                .segment = BC_GLOBAL_OBJECT_SEGMENT,
                .size    = value.size,
                .offset  = var->offset(),
            };
            value = EmitLoadMakeRoom(tmp, node->position());
        }
    }

    PushValue(value);
}

void EmittingAstVisitor::VisitStringLiteral(StringLiteral *node) {
    PushValue(EmitLoadMakeRoom(GetOrNewString(node->data(), nullptr),
                               node->position()));
}

void EmittingAstVisitor::VisitSmiLiteral(SmiLiteral *node) {
    auto dest = current_->MakePrimitiveValue((node->bitwide() + 7) / 8);

    switch (node->bitwide()) {
        case 1:
            builder(node->position())->load_i8_imm(dest.offset, node->i1());
            break;
    #define DEFINE_CASE(byte, bit) \
        case bit: \
            builder(node->position())->load_i##bit##_imm(dest.offset, node->i##bit ()); \
            break;
        MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
    #undef DEFINE_CASE
        case 64:
            EmitLoad(dest, current_->MakeConstantPrimitiveValue(node->i64()),
                     node->position());
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
    PushValue(EmitLoadMakeRoom(src, node->position()));
}

void EmittingAstVisitor::VisitArrayInitializer(ArrayInitializer *node) {
    auto dest = current_->MakeObjectValue();

    auto type = node->array_type();
    DCHECK(!type->IsUnknown());

    builder(node->position())->oop(OO_Array, dest.offset,
                                   TypeInfoIndex(type->element()),
                                   node->element_size());
    VMValue index;
    if (node->element_size() >= 0x3fff) {
        auto zero = current_->MakeConstantPrimitiveValue(static_cast<mio_i64_t>(0));
        index = EmitLoadMakeRoom(zero, node->position());
    }
    for (int i = 0; i < node->element_size(); ++i) {
        auto element = node->element(i);
        auto value = Emit(element->value());
        if (type->element()->IsUnion()) {
            auto tmp = current_->MakeObjectValue();
            EmitCreateUnion(tmp, value, element->value_type(),
                            element->position());
            value = tmp;
        }

        if (node->element_size() < 0x3fff) {
            builder(element->position())->oop(OO_ArrayDirectSet,dest.offset, i,
                                              value.offset);
        } else {
            auto one = current_->MakeConstantPrimitiveValue(static_cast<mio_i64_t>(1));
            builder(element->position())->add_i64(index.offset,
                                                  index.offset, one.offset);
            builder(element->position())->oop(OO_ArraySet, dest.offset,
                                              index.offset, value.offset);
        }
    }

    PushValue(dest);
}

void EmittingAstVisitor::VisitMapInitializer(MapInitializer *node) {
    auto dest = current_->MakeObjectValue();

    auto type = node->map_type();
    DCHECK(type->key()->CanBeKey());

    builder(node->position())->oop(OO_Map, dest.offset,
                                   TypeInfoIndex(type->key()),
                                   TypeInfoIndex(type->value()));
    uint32_t weak_flags = 0;
    if (node->annotation()->Contains("weak key")) {
        weak_flags |= MIOHashMap::kWeakKeyFlag;
    }
    if (node->annotation()->Contains("weak value")) {
        weak_flags |= MIOHashMap::kWeakValueFlag;
    }
    if (weak_flags) {
        builder(node->position())->oop(OO_MapWeak, dest.offset, weak_flags, 0);
    }

    for (int i = 0; i < node->pair_size(); ++i) {
        auto pair = node->pair(i);
        auto key = Emit(pair->key());

        auto value = Emit(pair->value());
        if (type->value()->IsUnion()) {
            auto tmp = current_->MakeObjectValue();
            EmitCreateUnion(tmp, value, pair->value_type(), pair->position());
            value = tmp;
        }
        builder(pair->position())->oop(OO_MapPut, dest.offset, key.offset, value.offset);
    }

    PushValue(dest);
}

void EmittingAstVisitor::VisitFieldAccessing(FieldAccessing *node) {
    auto callee_ty = node->callee_type();

    if (callee_ty->IsMap()) {
        auto callee = Emit(node->expression());
        auto key = EmitLoadMakeRoom(GetOrNewString(node->field_name(), nullptr),
                                    node->position());
        auto result = current_->MakeObjectValue();

        builder(node->position())->oop(OO_MapGet, callee.offset, key.offset,
                                       result.offset);
        PushValue(result);
    } else {
        DLOG(FATAL) << "noreached! type: " << callee_ty->ToString();
    }
    // TODO: other types
}

void EmittingAstVisitor::VisitTypeTest(TypeTest *node) {
    auto val = Emit(node->expression());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, val.segment);

    auto result = current_->MakeLocalValue(types()->GetI1());
    builder(node->position())->oop(OO_UnionTest, result.offset, val.offset,
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
        builder(node->position())->oop(OO_UnionUnbox, result.offset, val.offset,
                                       TypeInfoIndex(node->type()));
    } else if (node->original()->is_numeric()) {
        auto input = node->original()->AsNumeric();
        DCHECK(node->type()->is_numeric());
        auto output = node->type()->AsNumeric();

        result = EmitNumericCastMakeRoomIfNeeded(val, input, output,
                                                 node->position());
    } else {
        DLOG(FATAL) << "noreached! type: " << node->original()->ToString()
                    << " can not cast to " << node->type()->ToString();
    }
    PushValue(result);
}

void EmittingAstVisitor::VisitTypeMatch(TypeMatch *node) {
    TypeMatchCase *else_case = nullptr;
    for (int i = 0; i < node->match_case_size(); ++i) {
        if (node->match_case(i)->is_else_case()) {
            else_case = node->match_case(i);
            break;
        }
    }

    auto target = Emit(node->target());
    DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, target.segment);

    auto cond = current_->MakeLocalValue(types()->GetI1());

    std::vector<int> block_outter;
    int else_outter = -1;
    for (int i = 0; i < node->match_case_size(); ++i) {
        auto match_case = node->match_case(i);
        if (match_case == else_case) {
            continue;
        }
        if (else_outter >= 0) {
            naked_builder()->jz_fill(else_outter, cond.offset,
                                     naked_builder()->pc() - else_outter);
        }

        auto type_index = TypeInfoIndex(match_case->cast_pattern()->type());
        builder(match_case->cast_pattern()->position())
                ->oop(OO_UnionTest, cond.offset, target.offset, type_index);
        else_outter = builder(match_case->position())
                ->jz(cond.offset, naked_builder()->pc());

        auto value = current_->MakeLocalValue(match_case->cast_pattern()->type());
        
        builder(match_case->cast_pattern()->position())
                ->oop(OO_UnionUnbox, value.offset, target.offset, type_index);
        match_case->cast_pattern()->instance()->set_bind_kind(Variable::LOCAL);
        match_case->cast_pattern()->instance()->set_offset(value.offset);

        Emit(match_case->body());
        block_outter.push_back(builder(GetLastPosition(match_case->body()))
                ->jmp(naked_builder()->pc()));
    }

    if (else_outter >= 0) {
        naked_builder()->jz_fill(else_outter, cond.offset,
                                 naked_builder()->pc() - else_outter);
    }
    if (else_case) {
        Emit(else_case->body());
    }
    for (auto pc : block_outter) {
        naked_builder()->jmp_fill(pc, naked_builder()->pc() - pc);
    }
}

VMValue EmittingAstVisitor::EmitBitShift(BinaryOperation *node,
    std::function<void(BitCodeBuilder *, int, uint16_t, uint16_t, int32_t)> build_imm,
    std::function<void(BitCodeBuilder *, int, uint16_t, uint16_t, int32_t)> build_reg) {
    DCHECK(node->lhs_type()->IsIntegral());
    DCHECK(node->rhs_type()->IsIntegral());

    auto lhs = Emit(node->lhs());
    DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, lhs.segment);

    auto result = current_->MakeLocalValue(node->lhs_type());
    if (node->rhs()->IsSmiLiteral()) {
        auto smi = node->rhs()->AsSmiLiteral();

        int32_t imm32 = 0;
        #define DEFINE_CASE(byte, bit) case bit: \
            imm32 = static_cast<int32_t>(smi->i##bit()); \
            break;
        MIO_INT_BYTES_SWITCH(smi->bitwide(), DEFINE_CASE)
        #undef DEFINE_CASE

        build_imm(builder(node->position()),
                  node->lhs_type()->AsIntegral()->bitwide(),
                  result.offset, lhs.offset, imm32);
    } else {
        auto rhs = Emit(node->rhs());
        DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, lhs.segment);
        rhs = EmitNumericCastMakeRoomIfNeeded(rhs,
                                              node->rhs_type()->AsNumeric(),
                                              types()->GetInt(),
                                              node->rhs()->position());
        build_reg(builder(node->position()),
                  node->lhs_type()->AsIntegral()->bitwide(),
                  result.offset, lhs.offset, rhs.offset);
    }
    return result;
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
            builder(lhs->position())->add_i##bit##_imm(result.offset, val.offset, smi->i##bit ()); \
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
                builder(lhs->position())->add_i##bit (result.offset, val1.offset, val2.offset); \
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
            builder(lhs->position())->add_f##bit (result.offset, val1.offset, val2.offset); \
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
            builder(lhs->position())->sub_i##bit (result.offset, val1.offset, val2.offset); \
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
        builder(lhs->position())->sub_f##bit (result.offset, val1.offset, val2.offset); \
        break;

    MIO_FLOAT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitIntegralMul(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->mul_i##bit (result.offset, val1.offset, val2.offset); \
        break;

    MIO_INT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitFloatingMul(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->mul_f##bit (result.offset, val1.offset, val2.offset); \
        break;

    MIO_FLOAT_BYTES_SWITCH(result.size, DEFINE_CASE)
    
#undef DEFINE_CASE
    
    return result;
}

VMValue EmittingAstVisitor::EmitIntegralDiv(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->div_i##bit (result.offset, val1.offset, val2.offset); \
        break;

    MIO_INT_BYTES_SWITCH(result.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

VMValue EmittingAstVisitor::EmitFloatingDiv(Type *type, Expression *lhs, Expression *rhs) {
    auto val1 = Emit(lhs);
    auto val2 = Emit(rhs);
    DCHECK_EQ(val1.size, val2.size);
    auto result = current_->MakePrimitiveValue(val1.size);

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->div_f##bit (result.offset, val1.offset, val2.offset); \
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
    auto result = current_->MakeLocalValue(types()->GetI1());

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->cmp_i##bit (comparator, result.offset, val1.offset, val2.offset); \
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
    auto result = current_->MakeLocalValue(types()->GetI1());

#define DEFINE_CASE(byte, bit) \
    case byte: \
        builder(lhs->position())->cmp_f##bit (comparator, result.offset, val1.offset, val2.offset); \
        break;

    MIO_FLOAT_BYTES_SWITCH(val1.size, DEFINE_CASE)

#undef DEFINE_CASE

    return result;
}

void EmittingAstVisitor::EmitFunctionCall(const VMValue &callee, Call *node) {
    auto proto = DCHECK_NOTNULL(node->callee_type()->AsFunctionPrototype());

    std::vector<VMValue> args;
    for (int i = 0; i < node->mutable_arguments()->size(); ++i) {
        auto arg = node->mutable_arguments()->At(i);
        if (proto->paramter(i)->param_type()->IsUnion() &&
            !arg->value_type()->IsUnion()) {
            auto uni = current_->MakeObjectValue();
            EmitCreateUnion(uni, Emit(arg->value()), arg->value_type(),
                            arg->position());
            args.push_back(uni);
        } else {
            args.push_back(Emit(arg->value()));
        }
    }

    VMValue result;
    if (!proto->return_type()->IsVoid()) {
        if (proto->return_type()->is_primitive()) {
            result = current_->MakePrimitiveValue(proto->return_type()->placement_size());
        } else {
            result = current_->MakeObjectValue();
        }
    }

    auto p_base = current_->p_stack_size(), o_base = current_->o_stack_size();
    for (int i = 0; i < node->argument_size(); ++i) {
        auto value = args[i];
        switch (value.segment) {
            case BC_LOCAL_PRIMITIVE_SEGMENT:
                EmitMove(current_->MakePrimitiveValue(value.size), value,
                         node->argument(i)->position());
                break;

            case BC_LOCAL_OBJECT_SEGMENT:
                EmitMove(current_->MakeObjectValue(), value,
                         node->argument(i)->position());
                break;

            default:
                DLOG(FATAL) << "bad value segment: " << value.segment;
                break;
        }
    }

    builder(node->position())->call_val(p_base, o_base, callee.offset);
    if (proto->return_type()->IsVoid()) {
        PushValue(VMValue::Void());
    } else {
        PushValue(result);
    }
}

void EmittingAstVisitor::EmitMapAccessor(const VMValue &callee, Call *node) {
    auto map = Emit(node->expression());

    DCHECK_EQ(node->mutable_arguments()->size(), 1);
    auto key = Emit(node->argument(0)->value());
    auto result = current_->MakeObjectValue();

    builder(node->position())->oop(OO_MapGet, map.offset,  key.offset, result.offset);
    PushValue(result);
}

void EmittingAstVisitor::EmitArrayAccessorOrMakeSlice(const VMValue &callee,
                                                      Call *node) {
    DCHECK(node->argument_size() > 0 &&
           node->argument_size() <= 2) << node->argument_size();
    auto array_ty = static_cast<Array*>(node->callee_type());

    auto array = Emit(node->expression());
    if (node->argument_size() == 1) {
        auto index = Emit(node->argument(0)->value());
        auto element = current_->MakeLocalValue(array_ty->element());
        builder(node->position())->oop(OO_ArrayGet, array.offset, index.offset,
                                       element.offset);
        PushValue(element);
    } else {
        auto begin = Emit(node->argument(0)->value());
        auto size  = Emit(node->argument(1)->value());
        auto slice = current_->MakeObjectValue();
        builder(node->position())->oop(OO_Slice, array.offset, begin.offset,
                                       size.offset);
        PushValue(slice);
    }
}

VMValue EmittingAstVisitor::EmitLoadMakeRoom(const VMValue &src, int position) {
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
    EmitLoad(dest, src, position);
    return dest;
}

void EmittingAstVisitor::EmitLoad(const VMValue &dest, const VMValue &src,
                                  int position) {
    switch (src.segment) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
        case BC_UP_PRIMITIVE_SEGMENT:
        case BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT: {
            DCHECK_EQ(BC_LOCAL_PRIMITIVE_SEGMENT, dest.segment);

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            builder(position)->load_##byte##b(dest.offset, src.segment, src.offset); \
            break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_UP_OBJECT_SEGMENT:
        case BC_FUNCTION_CONSTANT_OBJECT_SEGMENT:
        case BC_GLOBAL_OBJECT_SEGMENT: {
            DCHECK_EQ(BC_LOCAL_OBJECT_SEGMENT, dest.segment);
            builder(position)->load_o(dest.offset, src.segment, src.offset);
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
}

VMValue EmittingAstVisitor::EmitStoreMakeRoom(const VMValue &src, int position) {
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
                builder(position)->store_##byte##b(value.offset, value.segment, src.offset); \
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
            emitter_->o_global_->Add(kNullObject);

            builder(position)->store_o(value.offset, value.segment, src.offset);
            return value;
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
    return { .segment = MAX_BC_SEGMENTS, .size = -1, .offset = -1 };
}

void EmittingAstVisitor::EmitStore(const VMValue &dest, const VMValue &src,
                                   int position) {
    switch (src.segment) {
        case BC_LOCAL_PRIMITIVE_SEGMENT: {

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            DCHECK(dest.segment == BC_GLOBAL_PRIMITIVE_SEGMENT || \
                   dest.segment == BC_UP_PRIMITIVE_SEGMENT); \
            builder(position)->store_##byte##b(dest.offset, dest.segment, src.offset); \
            break;
            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_LOCAL_OBJECT_SEGMENT:
            DCHECK(dest.segment == BC_GLOBAL_OBJECT_SEGMENT ||
                   dest.segment == BC_UP_OBJECT_SEGMENT);
            builder(position)->store_o(dest.offset, dest.segment, src.offset);
            break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
}

void EmittingAstVisitor::EmitMove(const VMValue &dest, const VMValue &src,
                                  int position) {
    switch (src.segment) {
        case BC_LOCAL_PRIMITIVE_SEGMENT: {

    #define DEFINE_CASE(byte, bit) \
        case byte: \
            builder(position)->mov_##byte##b(dest.offset, src.offset); \
            break;

            MIO_INT_BYTES_SWITCH(src.size, DEFINE_CASE)
    #undef DEFINE_CASE
        } break;

        case BC_LOCAL_OBJECT_SEGMENT: {
            builder(position)->mov_o(dest.offset, src.offset);
        } break;

        default:
            DLOG(FATAL) << "noreached! bad segment: " << src.segment;
            break;
    }
}

VMValue
EmittingAstVisitor::EmitNumericCastMakeRoomIfNeeded(const VMValue &value,
                                                    Numeric *from,
                                                    Numeric *cast_to,
                                                    int position) {
    auto item = kNumericCastMatrix[from->order()][cast_to->order()];
    if (item.cmd != kNop) {
        auto result = current_->MakeLocalValue(cast_to);
        builder(position)->Emit3Addr(item.cmd, result.offset, item.out_size,
                                     value.offset);
        return result;
    } else {
        return value;
    }
}

VMValue EmittingAstVisitor::EmitEmptyValue(Type *type, int position) {
    DCHECK(!type->IsVoid());

    if (type->is_primitive()) {
        auto result = current_->MakePrimitiveValue(type->placement_size());

        if (type->IsIntegral()) {
            switch (type->placement_size()) {
            #define DEFINE_CASE(byte, bit) \
                case byte: \
                    builder(position)->load_i##bit##_imm(result.offset, 0);
                MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                case 8: {
                    auto tmp = current_->MakeConstantPrimitiveValue(static_cast<mio_i64_t>(0));
                    EmitLoad(result, tmp, position);
                } break;
                default:
                    DLOG(FATAL) << "noreached, bad integral size: " << type->placement_size();
                    break;
            }
        } else if (type->IsFloating()) {
            #define DEFINE_CASE(byte, bit) \
                case byte: { \
                    auto tmp = current_->MakeConstantPrimitiveValue(static_cast<mio_f##bit##_t>(0)); \
                    EmitLoad(result, tmp, position); \
                } break;
            MIO_FLOAT_BYTES_SWITCH(type->placement_size(), DEFINE_CASE)
            #undef DEFINE_CASE
        }
    } else {
        auto result = current_->MakeObjectValue();

        if (type->IsString()) {
            EmitLoad(result, GetOrNewString("", 0, nullptr), position);
        } else if (type->IsUnion()) {
            EmitCreateUnion(result, {}, types()->GetVoid(), position);
        } else if (type->IsMap()) {
            auto map = type->AsMap();
            builder(position)->oop(OO_Map, result.offset, TypeInfoIndex(map->key()),
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

void EmittingAstVisitor::RegisterGlobalVariable(const std::string &full_name,
                                                const Type *type,
                                                const VMValue &value, bool readonly) {
    auto name = emitter_->object_factory_->GetOrNewString(full_name.c_str());
    auto code = (value.offset << 2) & ~0x3;
    if (readonly) {
        code |= 0x1;
    }
    if (type->is_object()) {
        code |= 0x2;
    }
    auto ok = emitter_->all_var_->Put(name, code);
    DCHECK(ok) << full_name;
    if (ok) {
        auto gc = static_cast<GarbageCollector *>(emitter_->object_factory_);
        gc->WriteBarrier(emitter_->all_var_->core().get(), name.get());
    }
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
            reft = factory->CreateReflectionIntegral(tid, type->AsIntegral()->bitwide());
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

        case Type::kExternal:
            reft = factory->CreateReflectionExternal(tid);
            break;

        case Type::kArray: {
            auto array = type->AsArray();
            auto element = TypeToReflection(array->element(), factory, all);
            reft = factory->CreateReflectionArray(array->GenerateId(), element);
        } break;

        case Type::kSlice: {
            auto array = type->AsSlice();
            auto element = TypeToReflection(array->element(), factory, all);
            reft = factory->CreateReflectionSlice(array->GenerateId(), element);
        } break;

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
                               ObjectExtraFactory *extra_factory,
                               FunctionRegister *function_register)
    : p_global_(DCHECK_NOTNULL(p_global))
    , o_global_(DCHECK_NOTNULL(o_global))
    , types_(DCHECK_NOTNULL(types))
    , object_factory_(DCHECK_NOTNULL(object_factory))
    , extra_factory_(DCHECK_NOTNULL(extra_factory))
    , function_register_(DCHECK_NOTNULL(function_register)) {
}

BitCodeEmitter::~BitCodeEmitter() {
    delete all_var_;
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

    auto idx = type_id2index_[types_->GetString()->GenerateId()];
    auto key = o_global_->Get<MIOReflectionType *>(all_type_base_ + idx * kObjectReferenceSize);
    idx = type_id2index_[types_->GetI32()->GenerateId()];
    auto value = o_global_->Get<MIOReflectionType *>(all_type_base_ + idx * kObjectReferenceSize);
    auto core = object_factory_->CreateHashMap(0, 17, make_handle(key), make_handle(value));
    all_var_ = new MIOHashMapStub<Handle<MIOString>, mio_i32_t>(core.get(), object_factory_->allocator());
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
    printf("max number of instructions: %d\n", MAX_BC_INSTRUCTIONS);

    auto pair = DCHECK_NOTNULL(all_modules->Get(kMainValue)); // "main"
    auto ok = EmitModule(pair->key(), pair->value(), all_modules);

    if (info) {
        info->all_type_base   = all_type_base_;
        info->void_type_index  = type_id2index_[types_->GetVoid()->GenerateId()];
        info->error_type_index = type_id2index_[types_->GetError()->GenerateId()];
        info->global_primitive_segment_bytes = p_global_->size();
        info->global_object_segment_bytes    = o_global_->size();
        info->all_var = all_var_->core().get();
    }
    return ok;
}

bool BitCodeEmitter::EmitModule(RawStringRef module_name,
                                ParsedUnitMap *all_units,
                                ParsedModuleMap *all_modules) {
    //printf("process module name: %s\n", module_name->c_str());

    auto zone = types_->zone();
    auto proto = types_->GetFunctionPrototype(new (zone) ZoneVector<Paramter *>(zone),
                                              types_->GetVoid());

    EmittingAstVisitor visitor(module_name, this);
    EmittedScope info(visitor.mutable_current(), proto, nullptr);

    // placement frame instruction
    auto frame_placement = visitor.builder(0)->debug();

    ParsedUnitMap::Iterator iter(all_units);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        auto stmts = iter->value();
        if (stmts->is_not_empty()) {
            if (!ProcessImportList(stmts->first()->AsPackageImporter(), &info,
                                   all_modules)) {
                return false;
            }
        }

        visitor.set_unit_name(iter->key());
        for (int i = 1; i < stmts->size(); ++i) {
            stmts->At(i)->Accept(&visitor);
        }
    }

    //auto builder = info.builder();
    auto main_name = TextOutputStream::sprintf("::%s::main", module_name->c_str());
    auto entry = function_register_->FindOrNull(main_name.c_str());
    if (entry) {
        auto local = info.MakeObjectRoom();
        info.builder(0)->load_o(local, BC_GLOBAL_OBJECT_SEGMENT, entry->offset());
        info.builder(0)->call_val(0, 0, local);
    }
    info.builder(0)->ret();

    // refill frame instruction
    info.naked_builder()->frame(frame_placement, info.p_stack_size(), info.o_stack_size(), 0);

    auto boot_name = TextOutputStream::sprintf("::%s::bootstrap", module_name->c_str());
    auto ob = object_factory_->CreateNormalFunction(info.constant_objects(),
                                                    info.constant_primitive_data(),
                                                    info.constant_primitive_size(),
                                                    info.naked_builder()->code()->offset(0),
                                                    info.naked_builder()->code()->size());
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
            continue;
        }
        imported_.insert(module_name->ToString());

        if (!EmitModule(module_name, pair->value(), all_modules)) {
            return false;
        }
        auto name = TextOutputStream::sprintf("::%s::bootstrap", module_name->c_str());
        auto entry = DCHECK_NOTNULL(function_register_->FindOrNull(name.c_str()));
        auto local = info->MakeObjectRoom();

        info->builder(0)->load_o(local, BC_GLOBAL_OBJECT_SEGMENT, entry->offset());
        info->builder(0)->call_val(0, 0, local);
    }
    return true;
}

} // namespace mio
