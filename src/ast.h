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
    M(SmiLiteral) \
    M(Variable) \
    M(Symbol) \
    M(Call) \
    M(FieldAccessing) \
    M(IfOperation) \
    M(Assignment) \
    M(Block) \
    M(FunctionLiteral)

#define DEFINE_AST_NODES(M)    \
    DEFINE_STATEMENT_NODES(M)  \
    DEFINE_EXPRESSION_NODES(M) \

#define DECLARE_AST_NODE(name)                           \
    friend class AstNodeFactory;                         \
    virtual void Accept(AstVisitor *v) override;         \
    virtual AstNode::NodeType node_type() const override \
        { return AstNode::k##name; }

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
                class FunctionLiteral;
            class UnaryOperation;
            class BinaryOperation;
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

    virtual Type *type() const = 0;

    virtual RawStringRef name() const = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Declaration)
protected:
    Declaration(Scope *scope, int position)
        : Statement(position)
        , scope_(DCHECK_NOTNULL(scope)) {}

    Scope *scope_;
}; // class Declaration


class ValDeclaration : public Declaration {
public:
    virtual RawStringRef name() const override { return name_; }
    virtual Type *type() const override { return type_; }

    void set_type(Type *type) { type_ = DCHECK_NOTNULL(type); }

    Expression *initializer() const { return initializer_; }

    bool is_argument() const { return is_argument_; }

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
    bool is_argument_;
}; // class ValDeclaration


class VarDeclaration : public Declaration {
public:
    virtual RawStringRef name() const override { return name_; }
    virtual Type *type() const override { return type_; }

    void set_type(Type *type) { type_ = DCHECK_NOTNULL(type); }

    Expression *initializer() const { return initializer_; }

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
}; // class ValDeclaration



class FunctionDefine : public Declaration {
public:
    FunctionLiteral *function_literal() const { return function_literal_; }
    int start_position() const { return position(); }

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
                   FunctionLiteral *function_literal,
                   Scope *scope,
                   int start_position,
                   int end_position)
        : Declaration(scope, start_position)
        , name_(DCHECK_NOTNULL(name))
        , is_export_(is_export)
        , is_native_(is_native)
        , function_literal_(DCHECK_NOTNULL(function_literal))
        , end_position_(end_position) {}

    RawStringRef name_;
    bool is_export_;
    bool is_native_;
    FunctionLiteral *function_literal_;
    int end_position_;
}; // class FunctionDefine


class Return : public Statement {
public:
    Expression *expression() const { return expression_; }

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


class FunctionLiteral : public Literal {
public:
    FunctionPrototype *prototype() const { return prototype_; }
    Expression *body() const { return body_; }
    int start_position() const { return position(); }

    DEF_GETTER(int, end_position)

    DECLARE_AST_NODE(FunctionLiteral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionLiteral);
private:
    FunctionLiteral(FunctionPrototype *prototype, Expression *body,
                    int start_position, int end_position)
        : Literal(start_position)
        , prototype_(DCHECK_NOTNULL(prototype))
        , body_(body)
        , end_position_(end_position) {}

    FunctionPrototype *prototype_;
    Expression *body_;
    int end_position_;
}; // class FunctionLiteral


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

#define DEFINE_CONDITION_OPS(M) \
    M(EQ, 6, 6, TOKEN_EQ) \
    M(NE, 6, 6, TOKEN_NE) \
    M(LT, 6, 6, TOKEN_LT) \
    M(LE, 6, 6, TOKEN_LE) \
    M(GT, 6, 6, TOKEN_GT) \
    M(GE, 6, 6, TOKEN_GE)

#define DEFINE_STRING_OPS(M) \
    M(STRCAT, 0, 0, TOKEN_TWO_DOT)

#define DEFINE_UNARY_OPS(M) \
    M(MINUS,   11, 11, TOKEN_MINUS) \
    M(NOT,     11, 11, TOKEN_NOT) \
    M(BIT_INV, 11, 11, TOKEN_WAVE)

#define DEFINE_OPS(M) \
    DEFINE_SIMPLE_ARITH_OPS(M) \
    DEFINE_BIT_OPS(M) \
    DEFINE_LOGIC_OPS(M) \
    DEFINE_CONDITION_OPS(M) \
    DEFINE_UNARY_OPS(M) \
    DEFINE_STRING_OPS(M) \
    M(OTHER, 11, 11, TOKEN_ERROR)

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



class Variable : public Expression {
public:
//    enum BindType {
//        GLOBAL,
//        LOCAL,
//        ARGUMENT,
//        UP_VALUE,
//    };
    Variable(Declaration *declaration, int position)
        : Expression(position)
        , declaration_(DCHECK_NOTNULL(declaration)) {
    }


    bool is_read_only() const {
        return declaration_->IsValDeclaration() ||
               declaration_->IsVarDeclaration();
    }

    bool is_readwrite() const { return !is_read_only(); }

    bool is_function() const { return declaration_->IsFunctionDefine(); }

    Declaration *declaration() const { return declaration_; }

    Scope *scope() const { return declaration_->scope(); }

    Type *type() { return declaration_->type(); }

    RawStringRef name() const { return declaration_->name(); }

    DECLARE_AST_NODE(Variable)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Variable)
private:
    Declaration *declaration_;
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


class FieldAccessing : public Expression {
public:
    RawStringRef field_name() const { return field_name_; }
    Expression *expression() const { return expression_; }

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
}; // class FieldAccessing


class IfOperation : public Expression {
public:
    Expression *condition() const { return condition_; }
    Statement  *then_statement() const { return then_statement_; }
    Statement  *else_statement() const { return else_statement_; }

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
    Statement  *else_statement_;

}; // class IfOperation

class Assignment : public Expression {
public:
    Expression *target() const { return target_; }
    Expression *rval() const { return rval_; }

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
}; // class Assignment

class Block : public Expression {
public:
    ZoneVector<Statement *> *mutable_body() { return body_; }

    int number_of_statements() const { return body_->size(); }
    int start_position() const { return position(); }

    DEF_GETTER(int, end_position)

    DECLARE_AST_NODE(Block)
    DISALLOW_IMPLICIT_CONSTRUCTORS(Block)
private:
    Block(ZoneVector<Statement *> *body, int start_position, int end_position)
        : Expression(start_position)
        , body_(DCHECK_NOTNULL(body))
        , end_position_(end_position) {}

    ZoneVector<Statement *> *body_;
    int end_position_;
}; // class Block

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


class DoNothingAstVisitor : public AstVisitor {
public:
    DoNothingAstVisitor() {}
    virtual ~DoNothingAstVisitor() {}

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

    Block *CreateBlock(ZoneVector<Statement *> *body, int start_position,
                       int end_position) {
        return new (zone_) Block(body, start_position, end_position);
    }

    FunctionDefine *CreateFunctionDefine(const std::string &name,
                                         bool is_export,
                                         bool is_native,
                                         FunctionLiteral *function_literal,
                                         Scope *scope,
                                         int start_position,
                                         int end_position) {
        return new (zone_) FunctionDefine(RawString::Create(name, zone_),
                                          is_export,
                                          is_native,
                                          function_literal,
                                          scope,
                                          start_position,
                                          end_position);
    }

    FunctionLiteral *CreateFunctionLiteral(FunctionPrototype *prototype,
                                           Expression *body,
                                           int start_position,
                                           int end_position) {
        return new (zone_) FunctionLiteral(prototype, body, start_position,
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

    Variable *CreateVariable(Declaration *declaration,
                             int position) {
        return new (zone_) Variable(declaration, position);
    }

private:
    Zone *zone_;
}; // class AstNodeFactory

} // namespace mio

#endif // MIO_AST_H_
