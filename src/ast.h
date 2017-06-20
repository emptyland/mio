// mio = Akiyama Mio
#ifndef MIO_AST_H_
#define MIO_AST_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"
#include "token.h"

namespace mio {

#define DEFINE_STATEMENT_NODES(M)  \
    M(PackageImporter) \
    M(Return) \
    M(Break) \
    M(Continue) \
    M(ValDeclaration) \
    M(VarDeclaration) \
    M(FunctionDefine) \
    M(ForLoop) \
    M(ForeachLoop) \
    M(WhileLoop) \
    M(TypeMatch)

#define DEFINE_EXPRESSION_NODES(M) \
    M(UnaryOperation) \
    M(BinaryOperation) \
    M(TypeTest) \
    M(TypeCast) \
    M(SmiLiteral) \
    M(FloatLiteral) \
    M(ArrayInitializer) \
    M(MapInitializer) \
    M(Pair) \
    M(Element) \
    M(Variable) \
    M(Reference) \
    M(Symbol) \
    M(Call) \
    M(BuiltinCall) \
    M(FieldAccessing) \
    M(IfOperation) \
    M(Assignment) \
    M(Block) \
    M(FunctionLiteral) \
    M(StringLiteral)

#define DEFINE_AST_NODES(M)    \
    M(TypeMatchCase)           \
    DEFINE_STATEMENT_NODES(M)  \
    DEFINE_EXPRESSION_NODES(M) \

#define DECLARE_AST_NODE(name)                           \
    friend class AstNodeFactory;                         \
    virtual void Accept(AstVisitor *v) override;         \
    virtual AstNode::NodeType node_type() const override \
        { return AstNode::k##name; }

#define AST_GETTER(field_type, name) \
    field_type *name() const { return name##_; }
#define AST_SETTER(field_type, name) \
    void set_##name(field_type *node) { name##_ = DCHECK_NOTNULL(node); }
#define AST_ACCESS(field_type, name) \
    AST_GETTER(field_type, name) \
    AST_SETTER(field_type, name)

class AstNode;
    class TypeMatchCase;
    class Statement;
        class Expression;
            class Block;
            class Assignment;
            class IfOperation;
            class Variable;
            class Reference;
            class Symbol;
            class Call;
            class BuiltinCall;
            class FieldAccessing;
            class Literal;
                class SmiLiteral;
                class FloatLiteral;
                class StringLiteral;
                class FunctionLiteral;
                class MapInitializer;
                class ArrayInitializer;
                class Element;
                    class Pair;
            class UnaryOperation;
            class BinaryOperation;
            class TypeTest;
            class TypeCast;
            class TypeMatch;
        class Declaration;
            class ValDeclaration;
            class VarDeclaration;
            class FunctionDefine;
        class PackageImporter;
        class Return;
        class Break;
        class Continue;
        class ForLoop;
        class ForeachLoop;
        class WhileLoop;


class Type;
    class FunctionPrototype;
    class Integral;
    class Floating;
    class String;
    class Struct;
    class Array;
    class Void;
    class Union;
    class Map;

class AstVisitor;
class AstNodeFactory;
class Scope;

class AstNode : public ManagedObject {
public:
#define AstNode_Types_ENUM(node) k##node,
    enum NodeType {
        DEFINE_AST_NODES(AstNode_Types_ENUM)
        kInvalid = -1,
    };
#undef AstNode_Types_ENUM

    AstNode(int position) : position_(position) {}

    virtual void Accept(AstVisitor *visitor) = 0;
    virtual NodeType node_type() const = 0;

    DEF_GETTER(int, position)

#define AstNode_TYPE_ASSERT(node)                                      \
    bool Is##node() const { return node_type() == k##node; }           \
    node *As##node() {                                                 \
        return Is##node() ? reinterpret_cast<node *>(this) : nullptr;  \
    }                                                                  \
    const node *As##node() const {                                     \
        return Is##node() ? reinterpret_cast<const node *>(this) : nullptr; \
    }
    DEFINE_AST_NODES(AstNode_TYPE_ASSERT)
#undef AstNode_TYPE_ASSERT

    DISALLOW_IMPLICIT_CONSTRUCTORS(AstNode);
private:
    int position_;
}; // class AstNode;


class Statement : public AstNode {
public:
    DISALLOW_IMPLICIT_CONSTRUCTORS(Statement)

protected:
    Statement(int position) : AstNode(position) {}
}; // class Statement


class PackageImporter : public Statement {
public:
    typedef ZoneHashMap<RawStringRef, RawStringRef> ImportList;

    RawStringRef package_name() const { return package_name_; }

    ImportList *mutable_import_list() {
        return &import_list_;
    }

    DECLARE_AST_NODE(PackageImporter)
    DISALLOW_IMPLICIT_CONSTRUCTORS(PackageImporter)
private:
    PackageImporter(int position, Zone *zone)
        : Statement(position)
        , import_list_(zone) {}

    RawStringRef package_name_ = RawString::kEmpty;
    ImportList import_list_;
    
}; // class PackageImporter


class Declaration : public Statement {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Scope, scope)
    DEF_PTR_PROP_RW_NOTNULL1(Variable, instance)

    virtual Type *type() const = 0;
    virtual RawStringRef name() const = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Declaration)
protected:
    Declaration(Scope *scope, int position)
        : Statement(position)
        , scope_(DCHECK_NOTNULL(scope)) {}

    Scope *scope_;
    Variable *instance_ = nullptr;
}; // class Declaration


class ValDeclaration : public Declaration {
public:
    virtual RawStringRef name() const override { return name_; }
    virtual Type *type() const override { return type_; }

    DEF_PTR_SETTER_NOTNULL(Type, type)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, initializer)
    DEF_PTR_PROP_RW_NOTNULL2(Type, initializer_type)

    DEF_GETTER(bool, is_argument)
    DEF_GETTER(bool, is_export)

    bool has_initializer() const { return initializer_ != nullptr; }

    DECLARE_AST_NODE(ValDeclaration)
    DISALLOW_IMPLICIT_CONSTRUCTORS(ValDeclaration)
private:
    ValDeclaration(RawStringRef name,
                   bool is_export,
                   Type *type,
                   Expression *initializer,
                   Scope *scope,
                   bool is_argument,
                   int position)
        : Declaration(scope, position)
        , name_(DCHECK_NOTNULL(name))
        , is_export_(is_export)
        , type_(DCHECK_NOTNULL(type))
        , initializer_(initializer)
        , is_argument_(is_argument) {
    }

    RawStringRef name_;
    bool is_export_;
    Type *type_;
    Expression *initializer_;
    Type *initializer_type_ = nullptr;
    bool is_argument_;
}; // class ValDeclaration


class VarDeclaration : public Declaration {
public:
    virtual RawStringRef name() const override { return name_; }
    virtual Type *type() const override { return type_; }

    DEF_PTR_SETTER_NOTNULL(Type, type)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, initializer)
    DEF_PTR_PROP_RW_NOTNULL2(Type, initializer_type)

    DEF_GETTER(bool, is_export)

    bool has_initializer() const { return initializer_ != nullptr; }

    DECLARE_AST_NODE(VarDeclaration)
    DISALLOW_IMPLICIT_CONSTRUCTORS(VarDeclaration)
private:
    VarDeclaration(RawStringRef name,
                   bool is_export,
                   Type *type,
                   Expression *initializer,
                   Scope *scope,
                   int position)
        : Declaration(scope, position)
        , name_(DCHECK_NOTNULL(name))
        , is_export_(is_export)
        , type_(DCHECK_NOTNULL(type))
        , initializer_(initializer) {}

    RawStringRef name_;
    bool is_export_;
    Type *type_;
    Expression *initializer_;
    Type *initializer_type_ = nullptr;
}; // class ValDeclaration



class FunctionDefine : public Declaration {
public:
    DEF_PTR_GETTER(FunctionLiteral, function_literal)

    virtual RawStringRef name() const override { return name_; }
    virtual Type *type() const override;

    DEF_GETTER(int, end_position)
    DEF_GETTER(bool, is_export)
    DEF_GETTER(bool, is_native)

    DECLARE_AST_NODE(FunctionDefine)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionDefine);
private:
    FunctionDefine(RawStringRef name,
                   bool is_export,
                   bool is_native,
                   FunctionLiteral *literal,
                   Scope *scope,
                   int position)
        : Declaration(scope, position)
        , name_(DCHECK_NOTNULL(name))
        , is_export_(is_export)
        , is_native_(is_native)
        , function_literal_(DCHECK_NOTNULL(literal)) {}

    RawStringRef name_;
    bool is_export_;
    bool is_native_;
    FunctionLiteral *function_literal_;
    int end_position_;
}; // class FunctionDefine


class Return : public Statement {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, expression)

    bool has_return_value() const { return expression_ != nullptr; }

    DECLARE_AST_NODE(Return)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Return);
private:
    Return(Expression *expression, int position)
        : Statement(position)
        , expression_(expression) {}
    
    Expression *expression_;
}; // class Return


class Break : public Statement {
public:

    DECLARE_AST_NODE(Break)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Break);
private:
    Break(int position) : Statement(position) {}
}; // class Break


class Continue : public Statement {
public:

    DECLARE_AST_NODE(Continue)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Continue);
private:
    Continue(int position) : Statement(position) {}
}; // class Break


class Loop : public Statement {
public:
    DEF_PTR_PROP_RW(Expression, body)
    DEF_GETTER(int, end_position)

    int begin_position() const { return position(); }

    DISALLOW_IMPLICIT_CONSTRUCTORS(Loop);
protected:
    Loop(Expression *body, int begin_position, int end_position)
        : Statement(begin_position)
        , body_(DCHECK_NOTNULL(body))
        , end_position_(end_position) {}

    Expression *body_;
    int end_position_;
}; // class Loop


class ForLoop : public Loop {
public:
    DEF_PTR_GETTER(ValDeclaration, iterator)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, begin)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, end)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, step)
    DEF_PTR_GETTER(Scope, scope)

    DECLARE_AST_NODE(ForLoop)
    DISALLOW_IMPLICIT_CONSTRUCTORS(ForLoop);
private:
    ForLoop(ValDeclaration *iterator, Expression *begin, Expression *end,
            Expression *step, Expression *body, Scope *scope, int begin_position,
            int end_position)
        : Loop(body, begin_position, end_position)
        , iterator_(DCHECK_NOTNULL(iterator))
        , begin_(DCHECK_NOTNULL(begin))
        , end_(DCHECK_NOTNULL(end))
        , step_(step) {}

    ValDeclaration *iterator_;
    Expression *begin_;
    Expression *end_;
    Expression *step_;
    Scope *scope_;
}; // class ForLoop


class ForeachLoop : public Loop {
public:
    DEF_PTR_PROP_RW(ValDeclaration, key)
    bool has_key() const { return key_ != nullptr; }

    DEF_PTR_PROP_RW(ValDeclaration, value)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, container)

    DEF_PTR_PROP_RW_NOTNULL2(Type, container_type)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, body)
    DEF_PTR_GETTER(Scope, scope)

    int begin_position() const { return position(); }

    DECLARE_AST_NODE(ForeachLoop)
    DISALLOW_IMPLICIT_CONSTRUCTORS(ForeachLoop);
private:
    ForeachLoop(ValDeclaration *key, ValDeclaration *value,
                Expression *container, Expression *body, Scope *scope,
                int begin_position, int end_position)
        : Loop(body, begin_position, end_position)
        , key_(key)
        , value_(DCHECK_NOTNULL(value))
        , container_(DCHECK_NOTNULL(container))
        , scope_(DCHECK_NOTNULL(scope)) {}

    ValDeclaration *key_;
    ValDeclaration *value_;
    Expression *container_;
    Type *container_type_ = nullptr;
    Scope *scope_;
}; // class ForeachLoop


class WhileLoop : public Loop {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, condition)

    DECLARE_AST_NODE(WhileLoop)
    DISALLOW_IMPLICIT_CONSTRUCTORS(WhileLoop);
private:
    WhileLoop(Expression *condition, Expression *body, int begin_position,
              int end_position)
        : Loop(body, begin_position, end_position)
        , condition_(DCHECK_NOTNULL(condition)) {}

    Expression *condition_;
}; // class WhileLoop


class Expression : public Statement {
public:
    bool is_lval() const;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Expression)
protected:
    Expression(int position) : Statement(position) {}
}; // class Expression

class Literal : public Expression {
public:
    // TODO:
    DISALLOW_IMPLICIT_CONSTRUCTORS(Literal)
protected:
    Literal(int position) : Expression(position) {}

}; // class Literal

class SmiLiteral : public Literal {
public:
    DEF_GETTER(int, bitwide)

    mio_bool_t i1() const  { return data_.as_i1; }
    mio_i8_t   i8() const  { return data_.as_i8; }
    mio_i16_t  i16() const { return data_.as_i16; }
    mio_i32_t  i32() const { return data_.as_i32; }
    mio_i64_t  i64() const { return data_.as_i64; }

    DECLARE_AST_NODE(SmiLiteral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(SmiLiteral)
private:
    SmiLiteral(int bitwide, int position)
        : Literal(position), bitwide_(bitwide) {}

    int bitwide_;
    union {
        mio_bool_t as_i1;
        mio_i8_t   as_i8;
        mio_i16_t  as_i16;
        mio_i32_t  as_i32;
        mio_i16_t  as_i64;
    } data_;

}; // class SmiLiteral


class FloatLiteral : public Literal {
public:
    DEF_GETTER(int, bitwide)

    mio_f32_t f32() const { return data_.as_f32; }
    mio_f64_t f64() const { return data_.as_f64; }

    DECLARE_AST_NODE(FloatLiteral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FloatLiteral);
private:
    FloatLiteral(int bitwide, int position)
        : Literal(position), bitwide_(bitwide) {}

    int bitwide_;
    union {
        mio_f32_t as_f32;
        mio_f64_t as_f64;
    } data_;
}; // class FloatLiteral


class StringLiteral : public Literal {
public:
    RawStringRef data() const { return data_; }

    DECLARE_AST_NODE(StringLiteral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(StringLiteral)
private:
    StringLiteral(RawStringRef data, int position)
        : Literal(position)
        , data_(DCHECK_NOTNULL(data)) {}

    RawStringRef data_;
}; // class StringLiteral


class FunctionLiteral : public Literal {
public:
    DEF_PTR_GETTER(FunctionPrototype, prototype)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, body)
    DEF_PTR_GETTER(Scope, scope)
    DEF_PTR_ZONE_VECTOR_PROP_RO(Variable *, up_value)
    DEF_PTR_ZONE_VECTOR_MUTABLE_GETTER(Variable *, up_value)

    int start_position() const { return position(); }

    bool has_body() const { return body_ != nullptr; }

    DEF_GETTER(int, end_position)
    DEF_GETTER(bool, is_assignment)

    DECLARE_AST_NODE(FunctionLiteral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionLiteral);
private:
    FunctionLiteral(FunctionPrototype *prototype,
                    Expression *body,
                    Scope *scope,
                    ZoneVector<Variable *> *up_values,
                    bool is_assignment,
                    int start_position,
                    int end_position)
        : Literal(start_position)
        , prototype_(DCHECK_NOTNULL(prototype))
        , body_(body)
        , scope_(DCHECK_NOTNULL(scope))
        , up_values_(DCHECK_NOTNULL(up_values))
        , is_assignment_(is_assignment)
        , end_position_(end_position) {}

    FunctionPrototype *prototype_;
    Expression *body_;
    Scope *scope_;
    ZoneVector<Variable *> *up_values_;
    bool is_assignment_;
    int end_position_;
}; // class FunctionLiteral

class ArrayInitializer : public Literal {
public:
    DEF_PTR_GETTER(Array, array_type)
    DEF_GETTER(RawStringRef, annotation)
    DEF_GETTER(int, end_position)
    DEF_PTR_ZONE_VECTOR_PROP_RO(Element *, element)

    DECLARE_AST_NODE(ArrayInitializer)
    DISALLOW_IMPLICIT_CONSTRUCTORS(ArrayInitializer)
private:
    ArrayInitializer(Array *type,
                     ZoneVector<Element *> *elements,
                     RawStringRef annotation,
                     int start_position,
                     int end_position)
        : Literal(start_position)
        , array_type_(DCHECK_NOTNULL(type))
        , elements_(DCHECK_NOTNULL(elements))
        , annotation_(DCHECK_NOTNULL(annotation))
        , end_position_(end_position) {}

    Array *array_type_;
    ZoneVector<Element *> *elements_;
    RawStringRef annotation_;
    int start_position_;
    int end_position_;
}; // class ArrayInitializer

class MapInitializer : public Literal {
public:
    DEF_PTR_GETTER(Map, map_type)
    DEF_GETTER(RawStringRef, annotation)
    DEF_GETTER(int, end_position)
    DEF_PTR_ZONE_VECTOR_PROP_RO(Pair *, pair)

    DECLARE_AST_NODE(MapInitializer)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MapInitializer)
private:
    MapInitializer(Map *map_type,
                   ZoneVector<Pair *> *pairs,
                   RawStringRef annotation,
                   int start_position,
                   int end_position)
    : Literal(start_position)
    , map_type_(DCHECK_NOTNULL(map_type))
    , pairs_(DCHECK_NOTNULL(pairs))
    , annotation_(DCHECK_NOTNULL(annotation))
    , end_position_(end_position) {}

    Map *map_type_;
    RawStringRef annotation_;
    ZoneVector<Pair *> *pairs_;
    int end_position_;
}; // MapLiteral

class Element : public Literal {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, value)
    DEF_PTR_PROP_RW_NOTNULL2(Type, value_type)

    DECLARE_AST_NODE(Element)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Element)
protected:
    Element(Expression *value, int position)
        : Literal(position)
        , value_(DCHECK_NOTNULL(value)) {}

    Expression *value_;
    Type *value_type_ = nullptr;
}; // class Element

class Pair : public Element {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, key)

    DECLARE_AST_NODE(Pair)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Pair)
private:
    Pair(Expression *key, Expression *value, int position)
        : Element(value, position)
        , key_(DCHECK_NOTNULL(key)) {}

    Expression *key_;
}; // class MapPair

#define DEFINE_SIMPLE_ARITH_OPS(M) \
    M(MUL, 10, 10, TOKEN_STAR) \
    M(DIV, 10, 10, TOKEN_SLASH) \
    M(MOD, 10, 10, TOKEN_PERCENT) \
    M(ADD,  9,  9, TOKEN_PLUS) \
    M(SUB,  9,  9, TOKEN_MINUS)

#define DEFINE_BIT_OPS(M) \
    M(LSHIFT,   80, 80, TOKEN_LSHIFT) \
    M(RSHIFT_L, 80, 80, TOKEN_RSHIFT_L) \
    M(RSHIFT_A, 80, 80, TOKEN_RSHIFT_A) \
    M(BIT_OR,   40, 40, TOKEN_BIT_OR) \
    M(BIT_AND,  50, 50, TOKEN_BIT_AND) \
    M(BIT_XOR,  60, 60, TOKEN_BIT_XOR)

#define DEFINE_LOGIC_OPS(M) \
    M(AND, 30, 30, TOKEN_AND) \
    M(OR,  20, 20, TOKEN_OR) \

#define DEFINE_CONDITION_OPS(M) \
    M(EQ, 70, 70, TOKEN_EQ) \
    M(NE, 70, 70, TOKEN_NE) \
    M(LT, 70, 70, TOKEN_LT) \
    M(LE, 70, 70, TOKEN_LE) \
    M(GT, 70, 70, TOKEN_GT) \
    M(GE, 70, 70, TOKEN_GE)

#define DEFINE_STRING_OPS(M) \
    M(STRCAT, 10, 10, TOKEN_TWO_DOT)

#define DEFINE_UNARY_OPS(M) \
    M(MINUS,   120, 120, TOKEN_MINUS) \
    M(NOT,     120, 120, TOKEN_NOT) \
    M(BIT_INV, 120, 120, TOKEN_WAVE)

#define DEFINE_OPS(M) \
    DEFINE_SIMPLE_ARITH_OPS(M) \
    DEFINE_BIT_OPS(M) \
    DEFINE_LOGIC_OPS(M) \
    DEFINE_CONDITION_OPS(M) \
    DEFINE_UNARY_OPS(M) \
    DEFINE_STRING_OPS(M) \
    M(OTHER, 120, 120, TOKEN_ERROR)

enum Operator : int {
#define Operator_ENUM(name, left_proi, right_proi, token) OP_##name,
    DEFINE_OPS(Operator_ENUM)
#undef  Operator_ENUM
    MAX_OP,
    OP_NOT_BINARY = -2, // not a binary operator
    OP_NOT_UNARY  = -1, // not a unary operator
}; // enum Operator

struct OperatorPriority {
    int left;
    int right;
}; // struct OperatorPriority

struct OperatorMetadata {
    const char     * name;
    OperatorPriority priority;
    Token            associated_token;
}; // struct OperatorMetadata

extern const OperatorMetadata kOperatorMetadata[MAX_OP];

Operator TokenToBinaryOperator(Token token);
Operator TokenToUnaryOperator(Token token);

const OperatorPriority *GetOperatorPriority(Operator op);
const char *GetOperatorText(Operator op);

class UnaryOperation : public Expression {
public:
    DEF_GETTER(Operator, op)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, operand)
    DEF_PTR_PROP_RW_NOTNULL2(Type, operand_type)

    DECLARE_AST_NODE(UnaryOperation)
    DISALLOW_IMPLICIT_CONSTRUCTORS(UnaryOperation)
private:
    UnaryOperation(Operator op, Expression *operand, int position)
        : Expression(position)
        , op_(op)
        , operand_(DCHECK_NOTNULL(operand)) {}

    Operator op_;
    Expression *operand_;
    Type *operand_type_ = nullptr;
}; // class UnaryOperation


class BinaryOperation : public Expression {
public:
    DEF_GETTER(Operator, op)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, lhs)
    DEF_PTR_PROP_RW_NOTNULL2(Type, lhs_type)

    DEF_PTR_PROP_RW_NOTNULL1(Expression, rhs)
    DEF_PTR_PROP_RW_NOTNULL2(Type, rhs_type)

    DECLARE_AST_NODE(BinaryOperation)
    DISALLOW_IMPLICIT_CONSTRUCTORS(BinaryOperation)
private:
    BinaryOperation(Operator op, Expression *lhs, Expression *rhs, int position)
        : Expression(position)
        , op_(op)
        , lhs_(DCHECK_NOTNULL(lhs))
        , rhs_(DCHECK_NOTNULL(rhs)) {}

    Operator op_;
    Expression *lhs_;
    Expression *rhs_;
    Type *lhs_type_ = nullptr;
    Type *rhs_type_ = nullptr;
}; // class BinaryOperation


// expr is int
class TypeTest : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, expression)
    DEF_PTR_GETTER(Type, type)

    DECLARE_AST_NODE(TypeTest)
    DISALLOW_IMPLICIT_CONSTRUCTORS(TypeTest)
private:
    TypeTest(Expression *expression, Type *type, int position)
        : Expression(position)
        , expression_(DCHECK_NOTNULL(expression))
        , type_(DCHECK_NOTNULL(type)) {}

    Expression *expression_;
    Type *type_;
}; // class TypeTest


class TypeCast : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, expression)
    DEF_PTR_PROP_RW_NOTNULL2(Type, original)
    DEF_PTR_GETTER(Type, type)

    DECLARE_AST_NODE(TypeCast)
    DISALLOW_IMPLICIT_CONSTRUCTORS(TypeCast)
private:
    TypeCast(Expression *expression, Type *type, int position)
        : Expression(position)
        , expression_(DCHECK_NOTNULL(expression))
        , type_(DCHECK_NOTNULL(type)) {}

    Expression *expression_;
    Type *type_;
    Type *original_ = nullptr;
}; // class TypeCast


class TypeMatch : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, target)
    DEF_PTR_ZONE_VECTOR_PROP_RO(TypeMatchCase *, match_case)

    DECLARE_AST_NODE(TypeMatch)
    DISALLOW_IMPLICIT_CONSTRUCTORS(TypeMatch)
private:
    TypeMatch(Expression *target, ZoneVector<TypeMatchCase *> *match_cases,
              int position)
    : Expression(position)
    , target_(DCHECK_NOTNULL(target))
    , match_cases_(DCHECK_NOTNULL(match_cases)) {
    }
    
    Expression *target_;
    ZoneVector<TypeMatchCase *> *match_cases_;
}; // class ForeachLoop


class TypeMatchCase : public AstNode {
public:
    TypeMatchCase(ValDeclaration *cast_pattern, Expression *body, Scope *scope,
                  int position)
        : AstNode(position)
        , cast_pattern_(cast_pattern)
        , body_(DCHECK_NOTNULL(body))
        , scope_(DCHECK_NOTNULL(scope)) {
    }

    DEF_PTR_GETTER(ValDeclaration, cast_pattern)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, body)
    DEF_PTR_GETTER(Scope, scope)

    bool is_else_case() const { return cast_pattern_ == nullptr; }

    DECLARE_AST_NODE(TypeMatchCase)
    DISALLOW_IMPLICIT_CONSTRUCTORS(TypeMatchCase)
private:
    ValDeclaration *cast_pattern_;
    Expression *body_;
    Scope *scope_;
}; // class TypeMatchCase

class Variable : public Expression {
public:
    enum BindKind {
        UNBINDED,
        GLOBAL,
        LOCAL,
        ARGUMENT,
        UP_VALUE,
    };

    Variable(Declaration *declaration, int64_t unique_id, int position)
        : Expression(position)
        , declaration_(DCHECK_NOTNULL(declaration))
        , link_(nullptr)
        , scope_(nullptr)
        , unique_id_(unique_id) {
    }

    Variable(Variable *link, Scope *scope, int64_t unique_id, int position)
        : Expression(position)
        , declaration_(DCHECK_NOTNULL(link->declaration()))
        , link_(DCHECK_NOTNULL(link))
        , scope_(DCHECK_NOTNULL(scope))
        , unique_id_(unique_id) {
    }

    DEF_PROP_RW(BindKind, bind_kind)
    DEF_PROP_RW(int, offset)

    bool is_read_only() const {
        return link_ == nullptr && (declaration_->IsValDeclaration() ||
                                    declaration_->IsFunctionDefine());
    }

    bool is_readwrite() const { return !is_read_only(); }

    bool is_function() const { return declaration()->IsFunctionDefine(); }

    Declaration *declaration() const { return declaration_; }

    Variable *link() const { return link_; }

    Scope *scope() const { return scope_ ? scope_ : declaration()->scope(); }

    Type *type() { return declaration()->type(); }

    RawStringRef name() const { return declaration()->name(); }

    int64_t unique_id() const { return unique_id_; }

    DECLARE_AST_NODE(Variable)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Variable)
private:
    Declaration *declaration_; // for normal declaration
    Variable    *link_;        // for upvalue link to
    Scope       *scope_;
    int64_t      unique_id_;
    BindKind     bind_kind_ = UNBINDED;
    int          offset_    = -1;
}; // class Variable


class Reference : public Expression {
public:
    DEF_PTR_GETTER(Variable, variable)

    DECLARE_AST_NODE(Reference)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Reference)
private:
    Reference(Variable *variable, int position)
        : Expression(position)
        , variable_(DCHECK_NOTNULL(variable)) {}

    Variable *variable_;
}; // class Reference


class Symbol : public Expression {
public:
    DEF_GETTER(RawStringRef, name)
    DEF_GETTER(RawStringRef, name_space)

    bool has_name_space() const { return name_space_ != RawString::kEmpty; }

    DECLARE_AST_NODE(Symbol)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Symbol)
private:
    Symbol(RawStringRef name, RawStringRef name_space, int position)
        : Expression(position)
        , name_(DCHECK_NOTNULL(name))
        , name_space_(DCHECK_NOTNULL(name_space)) {}

    RawStringRef name_;
    RawStringRef name_space_;
}; // class Symbol


class Call : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, expression)
    DEF_PTR_PROP_RW_NOTNULL2(Type, callee_type)
    DEF_PTR_ZONE_VECTOR_PROP_RW(Element *, argument)

    DECLARE_AST_NODE(Call)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Call)
private:
    Call(Expression *expression, ZoneVector<Element *> *arguments,
         int position)
        : Expression(position)
        , expression_(DCHECK_NOTNULL(expression))
        , arguments_(DCHECK_NOTNULL(arguments)) {}

    Expression *expression_;
    ZoneVector<Element *> *arguments_;
    Type *callee_type_ = nullptr;
}; // class Call


#define DEFINE_BUILTIN_OPS(M) \
    M(LEN) \
    M(ADD) \
    M(DELETE)

class BuiltinCall : public Expression {
public:
    enum Function: int {
    #define BuiltinCall_ENUM(name) name,
        DEFINE_BUILTIN_OPS(BuiltinCall_ENUM)
    #undef  BuiltinCall_ENUM
    };

    DEF_GETTER(Function, code)
    DEF_PTR_ZONE_VECTOR_PROP_RW(Element *, argument)

    DECLARE_AST_NODE(BuiltinCall)
    DISALLOW_IMPLICIT_CONSTRUCTORS(BuiltinCall)
private:
    BuiltinCall(Function code, ZoneVector<Element *> *arguments, int position)
        : Expression(position)
        , code_(code)
        , arguments_(DCHECK_NOTNULL(arguments)) {}

    Function code_;
    ZoneVector<Element *> *arguments_;
}; // class Call


class FieldAccessing : public Expression {
public:
    DEF_GETTER(RawStringRef, field_name)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, expression)
    DEF_PTR_PROP_RW_NOTNULL2(Type, callee_type)

    DECLARE_AST_NODE(FieldAccessing)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FieldAccessing)
private:
    FieldAccessing(RawStringRef field_name, Expression *expression,
                   int position)
        : Expression(position)
        , field_name_(DCHECK_NOTNULL(field_name))
        , expression_(DCHECK_NOTNULL(expression)) {
    }

    RawStringRef field_name_;
    Expression *expression_;
    Type *callee_type_ = nullptr;
}; // class FieldAccessing


class IfOperation : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, condition)
    DEF_PTR_PROP_RW_NOTNULL1(Statement, then_statement)
    DEF_PTR_PROP_RW_NOTNULL1(Statement, else_statement)
    DEF_PTR_PROP_RW_NOTNULL2(Type, then_type)
    DEF_PTR_PROP_RW_NOTNULL2(Type, else_type)

    bool has_else() const { return else_statement_ != nullptr; }

    DECLARE_AST_NODE(IfOperation)
    DISALLOW_IMPLICIT_CONSTRUCTORS(IfOperation)
private:
    IfOperation(Expression *condition, Statement *then_statement,
                Statement *else_statement, int position)
        : Expression(position)
        , condition_(DCHECK_NOTNULL(condition))
        , then_statement_(DCHECK_NOTNULL(then_statement))
        , else_statement_(else_statement) {
    }

    Expression *condition_;
    Statement  *then_statement_;
    Type       *then_type_ = nullptr;
    Statement  *else_statement_;
    Type       *else_type_ = nullptr;

}; // class IfOperation

class Assignment : public Expression {
public:
    DEF_PTR_PROP_RW_NOTNULL1(Expression, target)
    DEF_PTR_PROP_RW_NOTNULL1(Expression, rval)
    DEF_PTR_PROP_RW_NOTNULL1(Type, rval_type)

    DECLARE_AST_NODE(Assignment)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Assignment)
private:
    Assignment(Expression *target, Expression *rval, int position)
        : Expression(position)
        , target_(DCHECK_NOTNULL(target))
        , rval_(DCHECK_NOTNULL(rval)) {
    }

    Expression *target_;
    Expression *rval_;
    Type *rval_type_ = nullptr;
}; // class Assignment

class Block : public Expression {
public:
    DEF_PTR_ZONE_VECTOR_PROP_RW(Statement *, statement)
    DEF_PTR_GETTER(Scope, scope)
    DEF_GETTER(int, end_position)

    int start_position() const { return position(); }

    DECLARE_AST_NODE(Block)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Block)
private:
    Block(ZoneVector<Statement *> *body, Scope *scope, int start_position,
          int end_position)
        : Expression(start_position)
        , statements_(DCHECK_NOTNULL(body))
        , scope_(DCHECK_NOTNULL(scope))
        , end_position_(end_position) {}

    ZoneVector<Statement *> *statements_;
    Scope *scope_;
    int end_position_;
}; // class Block

////////////////////////////////////////////////////////////////////////////////
// AstVisitor
////////////////////////////////////////////////////////////////////////////////

class AstVisitor {
public:
    AstVisitor() = default;
    virtual ~AstVisitor() = default;

#define AstVisitor_VISIT_METHOD(name) virtual void Visit##name(name *) = 0;
    DEFINE_AST_NODES(AstVisitor_VISIT_METHOD)
#undef AstVisitor_VISIT_METHOD

    DISALLOW_IMPLICIT_CONSTRUCTORS(AstVisitor)
}; // class AstVisitor


class DoNothingAstVisitor : public AstVisitor {
public:
    DoNothingAstVisitor() = default;
    virtual ~DoNothingAstVisitor() = default;

#define DoNothingAstVisitor_VISIT_METHOD(name) \
    virtual void Visit##name(name *) override {}
    DEFINE_AST_NODES(DoNothingAstVisitor_VISIT_METHOD)
#undef DoNothingAstVisitor_VISIT_METHOD

    DISALLOW_IMPLICIT_CONSTRUCTORS(DoNothingAstVisitor)
}; // DoNothingAstVisitor


////////////////////////////////////////////////////////////////////////////////
// AstNodeFactory
////////////////////////////////////////////////////////////////////////////////

class AstNodeFactory {
public:
    AstNodeFactory(Zone *zone) : zone_(DCHECK_NOTNULL(zone)) {}

    Return *CreateReturn(Expression *expression, int position) {
        return new (zone_) Return(expression, position);
    }

    Break *CreateBreak(int position) { return new (zone_) Break(position); }

    Continue *CreateContinue(int position) { return new (zone_) Continue(position); }

    ForLoop *CreateForLoop(ValDeclaration *iterator, Expression *init,
                           Expression *end, Expression *step, Expression *body,
                           Scope *scope, int begin_position, int end_position) {
        return new (zone_) ForLoop(iterator, init, end, step, body, scope,
                                   begin_position, end_position);
    }

    ForeachLoop *CreateForeachLoop(ValDeclaration *key, ValDeclaration *value,
                                   Expression *container, Expression *body,
                                   Scope *scope, int begin_position,
                                   int end_position) {
        return new (zone_) ForeachLoop(key, value, container, body, scope,
                                       begin_position, end_position);
    }

    WhileLoop *CreateWhileLoop(Expression *condition, Expression *body,
                               int begin_position, int end_position) {
        return new (zone_) WhileLoop(condition, body, begin_position,
                                     end_position);
    }

    PackageImporter *CreatePackageImporter(const std::string &package_name,
                                           int position) {
        auto node = new (zone_) PackageImporter(position, zone_);
        if (!node) {
            return nullptr;
        }
        node->package_name_ = RawString::Create(package_name, zone_);
        return node;
    }

    UnaryOperation *CreateUnaryOperation(Operator op, Expression *operand,
                                        int position) {
        return new (zone_) UnaryOperation(op, DCHECK_NOTNULL(operand),
                                          position);
    }

    BinaryOperation *CreateBinaryOperation(Operator op, Expression *lhs,
                                           Expression *rhs, int position) {
        return new (zone_) BinaryOperation(op, DCHECK_NOTNULL(lhs),
                                           DCHECK_NOTNULL(rhs), position);
    }

    SmiLiteral *CreateI1SmiLiteral(mio_bool_t value, int position) {
        auto node = new (zone_) SmiLiteral(1, position);
        if (node) {
            node->data_.as_i1 = value;
        }
        return node;
    }

    SmiLiteral *CreateI8SmiLiteral(mio_i8_t value, int position) {
        auto node = new (zone_) SmiLiteral(8, position);
        if (node) {
            node->data_.as_i8 = value;
        }
        return node;
    }

    SmiLiteral *CreateI16SmiLiteral(mio_i16_t value, int position) {
        auto node = new (zone_) SmiLiteral(16, position);
        if (node) {
            node->data_.as_i16 = value;
        }
        return node;
    }

    SmiLiteral *CreateI32SmiLiteral(mio_i32_t value, int position) {
        auto node = new (zone_) SmiLiteral(32, position);
        if (node) {
            node->data_.as_i32 = value;
        }
        return node;
    }

    SmiLiteral *CreateI64SmiLiteral(mio_i64_t value, int position) {
        auto node = new (zone_) SmiLiteral(64, position);
        if (node) {
            node->data_.as_i64 = value;
        }
        return node;
    }

    FloatLiteral *CreateF32FloatLiteral(mio_f32_t value, int position) {
        auto node = new (zone_) FloatLiteral(32, position);
        if (node) {
            node->data_.as_f32 = value;
        }
        return node;
    }

    FloatLiteral *CreateF64FloatLiteral(mio_f64_t value, int position) {
        auto node = new (zone_) FloatLiteral(64, position);
        if (node) {
            node->data_.as_f64 = value;
        }
        return node;
    }

    StringLiteral *CreateStringLiteral(std::string value, int position) {
        return new (zone_) StringLiteral(RawString::Create(value, zone_),
                                         position);
    }

    Pair *CreatePair(Expression *key, Expression *value, int position) {
        return new (zone_) Pair(key, value, position);
    }

    Element *CreateElement(Expression *value, int position) {
        return new (zone_) Element(value, position);
    }

    ArrayInitializer *CreateArrayInitializer(Array *array_type,
                                             ZoneVector<Element *> *elements,
                                             RawStringRef annotation,
                                             int start_position,
                                             int end_position) {
        return new (zone_) ArrayInitializer(array_type,
                                            elements,
                                            annotation,
                                            start_position,
                                            end_position);
    }

    MapInitializer *CreateMapInitializer(Map *map_type,
                                         ZoneVector<Pair *> *pairs,
                                         RawStringRef annotation,
                                         int start_position,
                                         int end_position){
        return new (zone_) MapInitializer(map_type,
                                          pairs,
                                          annotation,
                                          start_position,
                                          end_position);
    }

    Symbol *CreateSymbol(const std::string &name,
                         const std::string &name_space,
                         int position) {
        return new (zone_) Symbol(RawString::Create(name, zone_),
                                  RawString::Create(name_space, zone_),
                                  position);
    }

    Call *CreateCall(Expression *expression, ZoneVector<Element *> *arguments,
                     int position) {
        return new (zone_) Call(expression, arguments, position);
    }

    BuiltinCall *CreateBuiltinCall(BuiltinCall::Function code,
                                   ZoneVector<Element *> *arguments,
                                   int position) {
        return new (zone_) BuiltinCall(code, arguments, position);
    }

    FieldAccessing *CreateFieldAccessing(const std::string &field_name,
                                         Expression *expression,
                                         int position) {
        return new (zone_) FieldAccessing(RawString::Create(field_name, zone_),
                                          expression, position);
    }

    IfOperation *CreateIfOperation(Expression *condition,
                                   Statement *then_statement,
                                   Statement *else_statement,
                                   int position) {
        return new (zone_) IfOperation(condition, then_statement,
                                       else_statement, position);
    }

    Assignment *CreateAssignment(Expression *target, Expression *rval,
                                 int position) {
        return new (zone_) Assignment(target, rval, position);
    }

    Block *CreateBlock(ZoneVector<Statement *> *body, Scope *scope,
                       int start_position, int end_position) {
        return new (zone_) Block(body, scope, start_position, end_position);
    }

    FunctionDefine *CreateFunctionDefine(const std::string &name,
                                         bool is_export,
                                         bool is_native,
                                         FunctionLiteral *literal,
                                         Scope *scope,
                                         int position) {
        return new (zone_) FunctionDefine(RawString::Create(name, zone_),
                                          is_export,
                                          is_native,
                                          literal,
                                          scope,
                                          position);
    }

    FunctionLiteral *CreateFunctionLiteral(FunctionPrototype *prototype,
                                           Expression *body,
                                           Scope *scope,
                                           bool is_assignment,
                                           int start_position,
                                           int end_position) {
        return new (zone_) FunctionLiteral(prototype,
                                           body,
                                           scope,
                                           new (zone_) ZoneVector<Variable *>(zone_),
                                           is_assignment,
                                           start_position,
                                           end_position);
    }

    ValDeclaration *CreateValDeclaration(const std::string &name,
                                         bool is_export,
                                         Type *type,
                                         Expression *initializer,
                                         Scope *scope,
                                         bool is_argument,
                                         int position) {
        return new (zone_) ValDeclaration(RawString::Create(name, zone_),
                                          is_export,
                                          type,
                                          initializer,
                                          scope,
                                          is_argument,
                                          position);
    }

    VarDeclaration *CreateVarDeclaration(const std::string &name,
                                         bool is_export,
                                         Type *type,
                                         Expression *initializer,
                                         Scope *scope,
                                         int position) {
        return new (zone_) VarDeclaration(RawString::Create(name, zone_),
                                          is_export,
                                          type,
                                          initializer,
                                          scope,
                                          position);
    }

    Variable *CreateVariable(Declaration *declaration, int64_t unique_id,
                             int position) {
        return new (zone_) Variable(declaration, unique_id, position);
    }

    Reference *CreateReference(Variable *variable, int position) {
        return new (zone_) Reference(variable, position);
    }

    TypeTest *CreateTypeTest(Expression *expression, Type *type, int position) {
        return new (zone_) TypeTest(expression, type, position);
    }

    TypeCast *CreateTypeCast(Expression *expression, Type *type, int position) {
        return new (zone_) TypeCast(expression, type, position);
    }

    TypeMatch *CreateTypeMatch(Expression *target,
                               ZoneVector<TypeMatchCase *> *match_cases,
                               int position) {
        return new (zone_) TypeMatch(target, match_cases, position);
    }

private:
    Zone *zone_;
}; // class AstNodeFactory

} // namespace mio

#endif // MIO_AST_H_
