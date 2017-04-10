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
    M(ValDeclaration) \
    M(VarDeclaration) \
    M(FunctionDefine)

#define DEFINE_EXPRESSION_NODES(M) \
    M(UnaryOperation) \
    M(BinaryOperation) \
    M(TypeTest) \
    M(TypeCast) \
    M(SmiLiteral) \
    M(FloatLiteral) \
    M(MapInitializer) \
    M(Pair) \
    M(Variable) \
    M(Symbol) \
    M(Call) \
    M(FieldAccessing) \
    M(IfOperation) \
    M(Assignment) \
    M(Block) \
    M(FunctionLiteral) \
    M(StringLiteral)

#define DEFINE_AST_NODES(M)    \
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
    class Statement;
        class Expression;
            class Block;
            class Assignment;
            class IfOperation;
            class Variable;
            class Symbol;
            class Call;
            class FieldAccessing;
            class Literal;
                class SmiLiteral;
                class FloatLiteral;
                class StringLiteral;
                class FunctionLiteral;
                class MapInitializer;
                class Pair;
            class UnaryOperation;
            class BinaryOperation;
            class TypeTest;
            class TypeCast;
        class Declaration;
            class ValDeclaration;
            class VarDeclaration;
            class FunctionDefine;
        class PackageImporter;
        class Return;

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
    // TODO:
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
    Scope *scope() const { return scope_; }
    void set_scope(Scope *scope) { scope_ = DCHECK_NOTNULL(scope); }

    Variable *instance() const { return instance_; }
    void set_instance(Variable *instance) { instance_ = DCHECK_NOTNULL(instance); }

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

    void set_type(Type *type) { type_ = DCHECK_NOTNULL(type); }

    Expression *initializer() const { return initializer_; }

    void set_initializer(Expression *initializer) {
        initializer_ = DCHECK_NOTNULL(initializer);
    }

    Type *initializer_type() const { return DCHECK_NOTNULL(initializer_type_); }

    void set_initializer_type(Type *type) {
        initializer_type_ = DCHECK_NOTNULL(type);
    }

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

    void set_type(Type *type) { type_ = DCHECK_NOTNULL(type); }

    Expression *initializer() const { return initializer_; }

    void set_initializer(Expression *initializer) {
        initializer_ = DCHECK_NOTNULL(initializer);
    }

    Type *initializer_type() const { return DCHECK_NOTNULL(initializer_type_); }

    void set_initializer_type(Type *type) {
        initializer_type_ = DCHECK_NOTNULL(type);
    }

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
    FunctionLiteral *function_literal() const { return function_literal_; }

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
    Expression *expression() const { return expression_; }
    void set_expression(Expression *expression) {
        expression_ = DCHECK_NOTNULL(expression);
    }

    bool has_return_value() const { return expression_ != nullptr; }

    DECLARE_AST_NODE(Return)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Return);
private:
    Return(Expression *expression, int position)
    : Statement(position)
    , expression_(expression) {}
    
    Expression *expression_;
}; // class Return


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
    FunctionPrototype *prototype() const { return prototype_; }
    Expression *body() const { return body_; }
    void set_body(Expression *body) { body_ = DCHECK_NOTNULL(body); }

    Scope *scope() const { return scope_; }

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
                    bool is_assignment,
                    int start_position,
                    int end_position)
        : Literal(start_position)
        , prototype_(DCHECK_NOTNULL(prototype))
        , body_(body)
        , scope_(DCHECK_NOTNULL(scope))
        , is_assignment_(is_assignment)
        , end_position_(end_position) {}

    FunctionPrototype *prototype_;
    Expression *body_;
    Scope *scope_;
    bool is_assignment_;
    int end_position_;
}; // class FunctionLiteral


class Pair : public Literal {
public:
    Expression *key() const { return key_; }
    void set_key(Expression *key) { key_ = DCHECK_NOTNULL(key); }

    Expression *value() const { return value_; }
    void set_value(Expression *value) { value_ = DCHECK_NOTNULL(value_); }

//    Type *value_type() const { return DCHECK_NOTNULL(value_type_); }
//    void set_value_type(Type *type) { value_type_ = DCHECK_NOTNULL(type); }

    DECLARE_AST_NODE(Pair)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Pair)
private:
    Pair(Expression *key, Expression *value, int position)
        : Literal(position)
        , key_(DCHECK_NOTNULL(key))
        , value_(DCHECK_NOTNULL(value)) {}

    Expression *key_;
    Expression *value_;
    //Type *value_type_ = nullptr;
}; // class MapPair

class MapInitializer : public Literal {
public:
    Map *map_type() const { return map_type_; }

    RawStringRef annotation() const { return annotation_; }

    ZoneVector<Pair *> *mutable_pairs() { return pairs_; }

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

    Expression *operand() const { return operand_; }
    void set_operand(Expression *node) { operand_ = DCHECK_NOTNULL(node); }

    DECLARE_AST_NODE(UnaryOperation)
    DISALLOW_IMPLICIT_CONSTRUCTORS(UnaryOperation)
private:
    UnaryOperation(Operator op, Expression *operand, int position)
        : Expression(position)
        , op_(op)
        , operand_(DCHECK_NOTNULL(operand)) {}

    Operator op_;
    Expression *operand_;
}; // class UnaryOperation


class BinaryOperation : public Expression {
public:
    DEF_GETTER(Operator, op)
    Expression *lhs() const { return lhs_; }
    void set_lhs(Expression *node) { lhs_ = DCHECK_NOTNULL(node); }

    Expression *rhs() const { return rhs_; }
    void set_rhs(Expression *node) { rhs_ = DCHECK_NOTNULL(node); }

    Type *lhs_type() const { return DCHECK_NOTNULL(lhs_type_); }
    void set_lhs_type(Type *type) { lhs_type_ = DCHECK_NOTNULL(type); }

    Type *rhs_type() const { return DCHECK_NOTNULL(rhs_type_); }
    void set_rhs_type(Type *type) { rhs_type_ = DCHECK_NOTNULL(type); }

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
    Expression *expression() const { return expression_; }
    void set_expression(Expression *expr) { expression_ = DCHECK_NOTNULL(expr); }

    Type *type() const { return type_; }

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
    Expression *expression() const { return expression_; }
    void set_expression(Expression *expr) { expression_ = DCHECK_NOTNULL(expr); }

    Type *original() const { return DCHECK_NOTNULL(original_); }
    void set_original(Type *type) { original_ = DCHECK_NOTNULL(type); }

    Type *type() const { return type_; }

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
        , unique_id_(unique_id) {
    }

    DEF_PROP_RW(BindKind, bind_kind)
    DEF_PROP_RW(int, offset)

    bool is_read_only() const {
        return declaration_->IsValDeclaration() ||
               declaration_->IsFunctionDefine();
    }

    bool is_readwrite() const { return !is_read_only(); }

    bool is_function() const { return declaration_->IsFunctionDefine(); }

    Declaration *declaration() const { return declaration_; }

    Scope *scope() const { return declaration_->scope(); }

    Type *type() { return declaration_->type(); }

    RawStringRef name() const { return declaration_->name(); }

    int64_t unique_id() const { return unique_id_; }

    DECLARE_AST_NODE(Variable)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Variable)
private:
    Declaration *declaration_;
    int64_t      unique_id_;
    BindKind     bind_kind_ = UNBINDED;
    int          offset_    = -1;
}; // class Variable



class Symbol : public Expression {
public:
    RawStringRef name() const { return name_; }
    RawStringRef name_space() const { return name_space_; }

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
    Expression *expression() const { return expression_; }
    void set_expression(Expression *expression) {
        expression_ = DCHECK_NOTNULL(expression);
    }

    Type *callee_type() const { return DCHECK_NOTNULL(callee_type_); }
    void set_callee_type(Type *type) { callee_type_ = DCHECK_NOTNULL(type); }

    ZoneVector<Expression *> *mutable_arguments() { return arguments_; }

    DECLARE_AST_NODE(Call)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Call)
private:
    Call(Expression *expression, ZoneVector<Expression *> *arguments,
         int position)
        : Expression(position)
        , expression_(DCHECK_NOTNULL(expression))
        , arguments_(DCHECK_NOTNULL(arguments)) {}

    Expression *expression_;
    ZoneVector<Expression *> *arguments_;
    Type *callee_type_ = nullptr;
}; // class Call


class FieldAccessing : public Expression {
public:
    RawStringRef field_name() const { return field_name_; }
    Expression *expression() const { return expression_; }
    void set_expression(Expression *expr) { expression_ = DCHECK_NOTNULL(expr); }

    Type *callee_type() const { return DCHECK_NOTNULL(callee_type_); }
    void set_callee_type(Type *type) { callee_type_ = DCHECK_NOTNULL(type); }

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
    Expression *condition() const { return condition_; }
    void set_condition(Expression *condition) {
        condition_ = DCHECK_NOTNULL(condition);
    }

    Statement *then_statement() const { return then_statement_; }
    void set_then_statement(Statement *stmt) {
        then_statement_ = DCHECK_NOTNULL(stmt);
    }
    Statement *else_statement() const { return else_statement_; }
    void set_else_statement(Statement *stmt) {
        else_statement_ = DCHECK_NOTNULL(stmt);
    }

    bool has_else() const { return else_statement_ != nullptr; }

    Type *then_type() const { return DCHECK_NOTNULL(then_type_); }
    void set_then_type(Type *type) { then_type_ = DCHECK_NOTNULL(type); }

    Type *else_type() const { return DCHECK_NOTNULL(else_type_); }
    void set_else_type(Type *type) { else_type_ = DCHECK_NOTNULL(type); }

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
    Expression *target() const { return target_; }
    void set_target(Expression *target) { target_ = DCHECK_NOTNULL(target); }

    Expression *rval() const { return rval_; }
    void set_rval(Expression *rval) { rval_ = DCHECK_NOTNULL(rval); }

    Type *rval_type() const { return DCHECK_NOTNULL(rval_type_); }
    void set_rval_type(Type *type) { rval_type_ = DCHECK_NOTNULL(type); }

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
    ZoneVector<Statement *> *mutable_body() { return body_; }

    Scope *scope() const { return scope_; }

    int number_of_statements() const { return body_->size(); }
    int start_position() const { return position(); }

    DEF_GETTER(int, end_position)

    DECLARE_AST_NODE(Block)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Block)
private:
    Block(ZoneVector<Statement *> *body, Scope *scope, int start_position,
          int end_position)
        : Expression(start_position)
        , body_(DCHECK_NOTNULL(body))
        , scope_(DCHECK_NOTNULL(scope))
        , end_position_(end_position) {}

    ZoneVector<Statement *> *body_;
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

    Call *CreateCall(Expression *expression, ZoneVector<Expression *> *arguments,
                     int position) {
        return new (zone_) Call(expression, arguments, position);
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

    TypeTest *CreateTypeTest(Expression *expression, Type *type, int position) {
        return new (zone_) TypeTest(expression, type, position);
    }

    TypeCast *CreateTypeCast(Expression *expression, Type *type, int position) {
        return new (zone_) TypeCast(expression, type, position);
    }

private:
    Zone *zone_;
}; // class AstNodeFactory

} // namespace mio

#endif // MIO_AST_H_
