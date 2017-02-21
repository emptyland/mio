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
    M(PackageImporter)

#define DEFINE_EXPRESSION_NODES(M) \
    M(UnaryOperation) \
    M(BinaryOperation) \
    M(SmiLiteral) \
    M(Symbol) \
    M(Call)

#define DEFINE_AST_NODES(M)    \
    DEFINE_STATEMENT_NODES(M)  \
    DEFINE_EXPRESSION_NODES(M) \

#define DECLARE_AST_NODE(name)                                               \
    friend class AstNodeFactory;                                             \
    virtual void Accept(AstVisitor *v) override;                             \
    virtual AstNode::Type type() const override { return AstNode::k##name; } \


class Statement;
class PackageImporter;
class Expression;
class Symbol;
class Call;
class Literal;
class SmiLiteral;
class UnaryOperation;
class BinaryOperation;


class AstVisitor;
class AstNodeFactory;

class AstNode : public ManagedObject {
public:
#define AstNode_Types_ENUM(node) k##node,
    enum Type {
        DEFINE_AST_NODES(AstNode_Types_ENUM)
        kInvalid = -1,
    };
#undef AstNode_Types_ENUM

    AstNode(int position) : position_(position) {}

    virtual void Accept(AstVisitor *visitor) = 0;
    virtual Type type() const = 0;

    DEF_GETTER(int, position)

#define AstNode_TYPE_ASSERT(node)                                      \
    bool Is##node() const { return type() == k##node; }                \
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
    Statement(int position) : AstNode(position) {}

    DISALLOW_IMPLICIT_CONSTRUCTORS(Statement)
}; // class Statement

class PackageImporter : public AstNode {
public:
    RawStringRef package_name() const { return package_name_; }

    ZoneHashMap<RawStringRef, RawStringRef> *mutable_import_list() {
        return &import_list_;
    }

    DECLARE_AST_NODE(PackageImporter)
    DISALLOW_IMPLICIT_CONSTRUCTORS(PackageImporter)
private:
    PackageImporter(int position, Zone *zone)
        : AstNode(position)
        , import_list_(zone) {}

    RawStringRef package_name_ = RawString::kEmpty;
    ZoneHashMap<RawStringRef, RawStringRef> import_list_;

}; // class PackageImporter

class Expression : public Statement {
public:
    //TODO

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
    DISALLOW_IMPLICIT_CONSTRUCTORS(SmiLiteral);
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


#define DEFINE_SIMPLE_ARITH_OPS(M) \
    M(MUL, 10, 10, TOKEN_STAR) \
    M(DIV, 10, 10, TOKEN_SLASH) \
    M(MOD, 10, 10, TOKEN_PERCENT) \
    M(ADD,  9,  9, TOKEN_PLUS) \
    M(SUB,  9,  9, TOKEN_MINUS)

#define DEFINE_BIT_OPS(M) \
    M(LSHIFT,   7, 7, TOKEN_LSHIFT) \
    M(RSHIFT_L, 7, 7, TOKEN_RSHIFT_L) \
    M(RSHIFT_A, 7, 7, TOKEN_RSHIFT_A) \
    M(BIT_OR,   3, 3, TOKEN_BIT_OR) \
    M(BIT_AND,  4, 4, TOKEN_BIT_AND) \
    M(BIT_XOR,  5, 5, TOKEN_BIT_XOR)

#define DEFINE_LOGIC_OPS(M) \
    M(AND, 2, 2, TOKEN_AND) \
    M(OR,  1, 1, TOKEN_OR) \

#define DEFINE_UNARY_OPS(M) \
    M(MINUS,   11, 11, TOKEN_MINUS) \
    M(NOT,     11, 11, TOKEN_NOT) \
    M(BIT_INV, 11, 11, TOKEN_WAVE)

#define DEFINE_OPS(M) \
    DEFINE_SIMPLE_ARITH_OPS(M) \
    DEFINE_BIT_OPS(M) \
    DEFINE_LOGIC_OPS(M) \
    DEFINE_UNARY_OPS(M)

enum Operator : int {
#define Operator_ENUM(name, left_proi, right_proi, token) OP_##name,
    DEFINE_OPS(Operator_ENUM)
#undef  Operator_ENUM
    MAX_OP,
    OP_NOT_BINARY = -2, // not a binary operator
    OP_NOT_UNARY  = -1, // not a unary operator
};

struct OperatorPriority {
    int left;
    int right;
};

Operator TokenToBinaryOperator(Token token);
Operator TokenToUnaryOperator(Token token);

const OperatorPriority *GetOperatorPriority(Operator op);

class UnaryOperation : public Expression {
public:
    DEF_GETTER(Operator, op)
    Expression *operand() const { return operand_; }

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
    Expression *rhs() const { return rhs_; }

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
}; // class BinaryOperation



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
}; // class Call


////////////////////////////////////////////////////////////////////////////////
// AstVisitor
////////////////////////////////////////////////////////////////////////////////

class AstVisitor {
public:
    AstVisitor() {}
    virtual ~AstVisitor() {}

#define AstVisitor_VISIT_METHOD(name) virtual void Visit##name(name *) = 0;
    DEFINE_AST_NODES(AstVisitor_VISIT_METHOD)
#undef AstVisitor_VISIT_METHOD

    DISALLOW_IMPLICIT_CONSTRUCTORS(AstVisitor)
}; // class AstVisitor


////////////////////////////////////////////////////////////////////////////////
// AstNodeFactory
////////////////////////////////////////////////////////////////////////////////

class AstNodeFactory {
public:
    AstNodeFactory(Zone *zone) : zone_(zone) {}

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

    SmiLiteral *CreateI64SmiLiteral(mio_i64_t value, int position) {
        auto node = new (zone_) SmiLiteral(64, position);
        if (node) {
            node->data_.as_i64 = value;
        }
        return node;
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

private:
    Zone *zone_;
}; // class AstNodeFactory

} // namespace mio

#endif // MIO_AST_H_
