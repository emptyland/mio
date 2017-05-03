#include "parser.h"
#include "scopes.h"
#include "ast.h"
#include "types.h"
#include "lexer.h"
#include "text-input-stream.h"
#include "text-output-stream.h"
#include <stdarg.h>

namespace mio {

#define CHECK_OK ok); if (!*ok) { return 0; } ((void)0
#define T(str) RawString::Create((str), zone_)

Parser::Parser(TypeFactory *types,
               TextStreamFactory *text_streams,
               Scope *scope,
               Zone *zone)
    : types_(DCHECK_NOTNULL(types))
    , text_streams_(DCHECK_NOTNULL(text_streams))
    , scope_(DCHECK_NOTNULL(scope))
    , zone_(DCHECK_NOTNULL(zone))
    , factory_(new AstNodeFactory(zone))
    , lexer_(new Lexer()) {
}

Parser::~Parser() {
    delete lexer_;
    delete factory_;
}

TextInputStream *Parser::SwitchInputStream(const std::string &key) {
    auto input = text_streams_->GetInputStream(key);
    lexer_->PopScope();
    lexer_->PushScope(input, true);
    lexer_->Next(&ahead_);

    return input;
}

TextInputStream *Parser::PushInputStream(const std::string &key) {
    auto input = text_streams_->GetInputStream(key);
    lexer_->PushScope(input, true);
    lexer_->Next(&ahead_);
    return input;
}

void Parser::PopInputStream() { lexer_->PopScope(); }

ParsingError Parser::last_error() const {
    if (has_error()) {
        return last_error_;
    } else {
        return ParsingError::NoError();
    }
}

void Parser::ClearError() {
    has_error_  = false;
    last_error_ = ParsingError::NoError();
}

Scope *Parser::EnterScope(const std::string &name, int scope_type) {
    auto scope = new (zone_) Scope(scope_,
                                   static_cast<ScopeType>(scope_type),
                                   zone_);
    DCHECK_NOTNULL(scope)->set_name(RawString::Create(name, zone_));
    scope_ = scope;
    return scope;
}

void Parser::EnterScope(Scope *scope) {
    DCHECK_EQ(scope_, DCHECK_NOTNULL(scope)->outter_scope());
    scope_ = scope;
}

Scope *Parser::LeaveScope() {
    scope_ = scope_->outter_scope();
    return scope_;
}

Statement *Parser::ParseStatement(bool *ok) {

    switch (Peek()) {

        case TOKEN_VAL:
            return ParseValDeclaration(false, ok);
    
        case TOKEN_VAR:
            return ParseVarDeclaration(false, ok);

        case TOKEN_WHILE:
            // TODO:
            break;

        case TOKEN_FOR:
            return ParseForeachLoop(ok);

        case TOKEN_EXPORT:
            return ParseDeclaration(ok);

        case TOKEN_NATIVE:
            lexer_->Next(&ahead_);
            return ParseFunctionDefine(false, true, ok);

        case TOKEN_FUNCTION:
            return ParseFunctionDefine(false, false, ok);

        case TOKEN_RETURN:
            return ParserReturn(ok);

        case TOKEN_EOF:
            break;

        default:
            return ParseExpression(false, ok);
    }

    return nullptr;
}

Statement *Parser::ParseDeclaration(bool *ok) {
    bool is_export = Test(TOKEN_EXPORT);
    bool is_native = Test(TOKEN_NATIVE);

    if (Peek() == TOKEN_VAL) {
        return ParseValDeclaration(is_export, ok);
    } else if (Peek() == TOKEN_VAR) {
        return ParseVarDeclaration(is_export, ok);
    } else if (Peek() == TOKEN_FUNCTION) {
        return ParseFunctionDefine(is_export, is_native, ok);
    } else {
        *ok = false;
        ThrowError("unexpected val/var/function, expected: %s",
                   ahead_.ToNameWithText().c_str());
    }
    return nullptr;
}

// function name ( paramters ): type = {}
// function name ( paramters ) = expression
FunctionDefine *Parser::ParseFunctionDefine(bool is_export, bool is_native,
                                            bool *ok) {
    auto position = ahead_.position();

    std::string name;
    auto prototype = ParseFunctionPrototype(false, &name, CHECK_OK);
    auto scope = EnterScope("", FUNCTION_SCOPE);

    Expression *body = nullptr;
    bool is_assignment = false;
    if (!is_native) {
        for (int i = 0; i < prototype->mutable_paramters()->size(); ++i) {
            auto param = prototype->mutable_paramters()->At(i);

            if (scope->FindOrNullLocal(param->param_name())) {
                *ok = false;
                ThrowError("duplicated argument name: %s",
                           param->param_name()->c_str());
                return nullptr;
            }

            auto declaration = factory_->CreateValDeclaration(
                param->param_name()->ToString(),
                false,
                param->param_type(),
                nullptr,
                scope,
                true,
                position);
            scope->Declare(param->param_name(), declaration);
        }

        is_assignment = Test(TOKEN_ASSIGN); // =
        body = ParseExpression(false, CHECK_OK);
    }
    auto literal = factory_->CreateFunctionLiteral(prototype, body,
                                                   scope,
                                                   is_assignment,
                                                   position,
                                                   ahead_.position());
    auto function = factory_->CreateFunctionDefine(name, is_export, is_native,
                                                   literal, scope->outter_scope(),
                                                   position);
    scope->set_name(function->name());
    scope->set_function(function);
    LeaveScope();

    if (!scope_->Declare(function->name(), function)) {
        *ok = false;
        ThrowError("duplicated function name: %s", function->name()->c_str());
        return nullptr;
    }
    return function;
}

ForeachLoop *Parser::ParseForeachLoop(bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_FOR, CHECK_OK);

    Match(TOKEN_LPAREN, CHECK_OK);

    auto key_position = ahead_.position();
    std::string key_name;
    Match(TOKEN_ID, &key_name, CHECK_OK);

    std::string value_name;
    int value_position;
    if (Test(TOKEN_COMMA)) {
        value_position = ahead_.position();
        Match(TOKEN_ID, &value_name, CHECK_OK);
    } else {
        value_position = key_position;
        value_name = std::move(key_name);
    }
    if (key_name.compare(value_name) == 0) {
        ThrowError("duplicated val declaration %s", value_name.c_str());
        *ok = false;
        return nullptr;
    }

    Match(TOKEN_IN, CHECK_OK); // in

    auto scope = EnterScope("for-loop", BLOCK_SCOPE);
    auto container = ParseExpression(false, CHECK_OK);

    Match(TOKEN_RPAREN, CHECK_OK);

    auto body = ParseExpression(false, CHECK_OK);

    ValDeclaration *key = nullptr;
    if (!key_name.empty()) {
        key = factory_->CreateValDeclaration(key_name, false, types_->GetUnknown(),
                                             nullptr, scope, false, key_position);
        DCHECK_NOTNULL(scope->Declare(key->name(), key));
    }
    auto value = factory_->CreateValDeclaration(value_name, false,
                                                types_->GetUnknown(), nullptr,
                                                scope, false, value_position);
    DCHECK_NOTNULL(scope->Declare(value->name(), value));

    LeaveScope();
    return factory_->CreateForeachLoop(key, value, container, body, scope, position);
}

Return *Parser::ParserReturn(bool *ok) {
    auto scope = scope_;
    bool inner_func = false;
    while (scope) {
        if (scope->type() == FUNCTION_SCOPE) {
            inner_func = true;
            break;
        }
        scope = scope->outter_scope();
    }

    if (!inner_func) {
        *ok = false;
        ThrowError("return statement is not in function scope.");
        return nullptr;
    }

    auto position = ahead_.position();
    Match(TOKEN_RETURN, CHECK_OK);

    auto expr = ParseExpression(true, CHECK_OK);
    return factory_->CreateReturn(expr, position);
}


// val name: type [ = expression ]
// val name = expression
ValDeclaration *Parser::ParseValDeclaration(bool is_export, bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_VAL, CHECK_OK);

    std::string name;
    Match(TOKEN_ID, &name, CHECK_OK);

    Type *val_type = types_->GetUnknown();
    if (Test(TOKEN_COLON)) {
        val_type = ParseType(CHECK_OK);
    }

    Expression *initializer = nullptr;
    if (Test(TOKEN_ASSIGN)) {
        initializer = ParseExpression(false, CHECK_OK);
    }

    auto val_decl = factory_->CreateValDeclaration(name, is_export, val_type,
                                                   initializer, scope_, false,
                                                   position);
    if (!scope_->Declare(val_decl->name(), val_decl)) {
        *ok = false;
        ThrowError("duplicated val name: %s", val_decl->name()->c_str());

        val_decl->~ValDeclaration();
        zone_->Free(val_decl);
        return nullptr;
    }
    return val_decl;
}

VarDeclaration *Parser::ParseVarDeclaration(bool is_export, bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_VAR, CHECK_OK);

    std::string name;
    Match(TOKEN_ID, &name, CHECK_OK);

    Type *var_type = types_->GetUnknown();
    if (Test(TOKEN_COLON)) {
        var_type = ParseType(CHECK_OK);
    }

    Expression *initializer = nullptr;
    if (Test(TOKEN_ASSIGN)) {
        initializer = ParseExpression(false, CHECK_OK);
    }

    auto var_decl = factory_->CreateVarDeclaration(name, is_export, var_type,
                                                   initializer, scope_,
                                                   position);
    if (!scope_->Declare(var_decl->name(), var_decl)) {
        *ok = false;
        ThrowError("duplicated var name: %s", var_decl->name()->c_str());

        var_decl->~VarDeclaration();
        zone_->Free(var_decl);
        return nullptr;
    }
    return var_decl;
}

/*
 * package_define_import = package_define [ import_statment ]
 * package_define = "package" id
 * import_statment = "with" "(" import_item { import_item } ")"
 * import_item = string_literal [ "as" import_alias ]
 * import_alias = "_" | id | "."
 */
PackageImporter *Parser::ParsePackageImporter(bool *ok) {
    if (scope_ && !scope_->is_global_scope()) {
        *ok = false;
        ThrowError("pacakge importer must be header in the source file.");
        return nullptr;
    }

    auto position = ahead_.position();
    Match(TOKEN_PACKAGE, CHECK_OK);

    std::string txt;
    Match(TOKEN_ID, &txt, CHECK_OK);
    auto node = factory_->CreatePackageImporter(txt, position);

    if (!Test(TOKEN_WITH)) {
        return node;
    }
    Match(TOKEN_LPAREN, CHECK_OK);

    while (!Test(TOKEN_RPAREN)) {
        Match(TOKEN_STRING_LITERAL, &txt, CHECK_OK);
        auto import_name = txt;

        if (!Test(TOKEN_AS)) {
            node->mutable_import_list()->Put(T(import_name), RawString::kEmpty);
            continue;
        }

        if (Test(TOKEN_DOT)) {
            node->mutable_import_list()->Put(T(import_name), T("."));
        } else {
            Match(TOKEN_ID, &txt, CHECK_OK);
            node->mutable_import_list()->Put(T(import_name), T(txt));
        }
    }

    return node;
}

Expression *Parser::ParseExpression(bool ignore, bool *ok) {
    int rop = 0;
    return ParseExpression(ignore, 0, &rop, ok);
}

Expression *Parser::ParseExpression(bool ignore, int limit, int *rop, bool *ok) {
    switch (Peek()) {
        case TOKEN_LBRACE: // {
            *rop = OP_OTHER;
            return ParseBlock(ok);

        case TOKEN_ID:
        case TOKEN_I8_LITERAL:
        case TOKEN_I16_LITERAL:
        case TOKEN_I32_LITERAL:
        case TOKEN_INT_LITERAL:
        case TOKEN_F32_LITERAL:
        case TOKEN_F64_LITERAL:
        case TOKEN_STRING_LITERAL:
        case TOKEN_MINUS:
        case TOKEN_WAVE:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_FUNCTION:
        case TOKEN_LAMBDA:
        case TOKEN_LPAREN:
        case TOKEN_MAP:
            return ParseOperation(limit, rop, ok);

        case TOKEN_IF:
            *rop = OP_OTHER;
            return ParseIfOperation(ok);

        // TODO:

        default:
            if (ignore) {
                break;
            }
            *ok = false;
            if (Peek() == TOKEN_ERROR) {
                ThrowError("unexpected expression, lexer error: %s",
                           ahead_.text().c_str());
            } else {
                ThrowError("unexpected expression, expected: %s",
                           ahead_.ToNameWithText().c_str());
            }
            break;
    }
    return nullptr;
}

Block *Parser::ParseBlock(bool *ok) {
    int position = ahead_.position();

    auto scope = EnterScope("", BLOCK_SCOPE);
    scope_ = scope;

    Match(TOKEN_LBRACE, CHECK_OK);
    auto body = new (zone_) ZoneVector<Statement *>(zone_);

    while (!Test(TOKEN_RBRACE)) {
        auto stmt = ParseStatement(CHECK_OK);
        body->Add(stmt);
    }

    LeaveScope();
    return factory_->CreateBlock(body, scope, position, ahead_.position());
}

// if ( condition) then_statement else else_statement
//
// if (val a = foo(); a == 100) {
//    println(a)
// }
//
// if (val a:int be u) {
//     println(a)
// }
IfOperation *Parser::ParseIfOperation(bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_IF, CHECK_OK);

    Match(TOKEN_LPAREN, CHECK_OK);
    auto expr = ParseExpression(false, CHECK_OK);
    Match(TOKEN_RPAREN, CHECK_OK);

    auto then_stmt = ParseStatement(CHECK_OK);
    Statement *else_stmt = nullptr;
    if (Test(TOKEN_ELSE)) {
        else_stmt = ParseStatement(CHECK_OK);
    }

    auto node = factory_->CreateIfOperation(expr, then_stmt, else_stmt, position);
    return node;
}

// 1 + 2 * i
// node = SmiLiteral(1)
// op = +
// rhs =
// -- node = SmiLiteral(2)
// -- op = *
// -- rhs = Symbol(i)
//
// 2 * i + 1
// node = SmiLiteral(2)
// op = *
// rhs =
// -- node = Symbol(i)
// -- op = +
//
Expression *Parser::ParseOperation(int limit, int *rop, bool *ok) {
    auto position = ahead_.position();
    auto op = TokenToUnaryOperator(Peek());
    if (op != OP_NOT_UNARY) {
        lexer_->Next(&ahead_);

        auto operand = ParseExpression(false, limit, rop, CHECK_OK);
        *rop = op;
        return factory_->CreateUnaryOperation(op, operand, position);
    }

    auto node = ParseSimpleExpression(CHECK_OK);
    if (Test(TOKEN_ASSIGN)) { // = assignment
        if (!node->is_lval()) {
            ThrowError("assigment target is not lval");
            *ok = false;
            return nullptr;
        }
        auto rval = ParseExpression(false, 0, rop, CHECK_OK);
        *rop = OP_OTHER;

        return factory_->CreateAssignment(node, rval, position);
    }

    op = TokenToBinaryOperator(Peek());
    while (op != OP_NOT_BINARY && GetOperatorPriority(op)->left > limit) {
        lexer_->Next(&ahead_);
        position = ahead_.position();

        int next_op;
        auto rhs = ParseExpression(false,
                                   GetOperatorPriority(op)->right,
                                   &next_op,
                                   CHECK_OK);
        node = factory_->CreateBinaryOperation(op, node, rhs, position);

        op = static_cast<Operator>(next_op);
    }
    *rop = op;
    return node;
}

Expression *Parser::ParseSimpleExpression(bool *ok) {
    auto position = ahead_.position();
    switch (Peek()) {
        case TOKEN_TRUE:
            lexer_->Next(&ahead_);
            return factory_->CreateI1SmiLiteral(true, position);

        case TOKEN_FALSE:
            lexer_->Next(&ahead_);
            return factory_->CreateI1SmiLiteral(false, position);

        case TOKEN_I8_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateI8SmiLiteral(ahead_.i8_data(), position);

        case TOKEN_I16_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateI16SmiLiteral(ahead_.i16_data(), position);

        case TOKEN_I32_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateI32SmiLiteral(ahead_.i32_data(), position);

        case TOKEN_INT_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateI64SmiLiteral(ahead_.int_data(), position);

        case TOKEN_F32_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateF32FloatLiteral(ahead_.f32_data(), position);

        case TOKEN_F64_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateF64FloatLiteral(ahead_.f64_data(), position);

        // TODO: other literal

        case TOKEN_STRING_LITERAL: {
            std::string literal;
            Match(TOKEN_STRING_LITERAL, &literal, CHECK_OK);
            return factory_->CreateStringLiteral(literal, position);
        } break;

        case TOKEN_LAMBDA:
            return ParseLambda(ok);

        case TOKEN_FUNCTION:
            return ParseFunctionLiteral(ok);

        case TOKEN_ARRAY:
            return ParseArrayInitializer(ok);

        case TOKEN_MAP:
            return ParseMapInitializer(ok);

        default:
            return ParseSuffixed(ok);
    }
}

Expression *Parser::ParseSuffixed(bool *ok) {
    auto position = ahead_.position();
    auto node = ParsePrimary(CHECK_OK);

    for (;;) {
        switch (Peek()) {
            // expr ( a1, b1, c1)
            case TOKEN_LPAREN: {
                auto args = new (zone_) ZoneVector<Expression *>(zone_);
                Match(TOKEN_LPAREN, CHECK_OK);
                if (!Test(TOKEN_RPAREN)) {
                    auto arg = ParseExpression(false, CHECK_OK);
                    args->Add(arg);
                    while (Test(TOKEN_COMMA)) {
                        arg = ParseExpression(false, CHECK_OK);
                        args->Add(arg);
                    }
                    Match(TOKEN_RPAREN, CHECK_OK);
                }
                node = factory_->CreateCall(node, args, position);
            } break;

            // expr . field . field
            case TOKEN_DOT: {
                Match(TOKEN_DOT, CHECK_OK);

                std::string field_name;
                Match(TOKEN_ID, &field_name, CHECK_OK);

                node = factory_->CreateFieldAccessing(field_name, node, position);
            } break;

            // expr as type
            case TOKEN_AS: {
                Match(TOKEN_AS, CHECK_OK);

                auto type = ParseType(CHECK_OK);
                return factory_->CreateTypeCast(node, type, position);
            } break;

            // expr is type
            case TOKEN_IS: {
                Match(TOKEN_IS, CHECK_OK);

                auto type = ParseType(CHECK_OK);
                return factory_->CreateTypeTest(node, type, position);
            } break;

            // expr![type]
            case TOKEN_EXCLAMATION: {
                Match(TOKEN_EXCLAMATION, CHECK_OK);

                Match(TOKEN_LBRACK, CHECK_OK);
                auto type = ParseType(CHECK_OK);
                Match(TOKEN_RBRACK, CHECK_OK);
                
                return factory_->CreateTypeCast(node, type, position);
            } break;

            // expr?[type]
            case TOKEN_QUESTION: {
                Match(TOKEN_QUESTION, CHECK_OK);

                Match(TOKEN_LBRACK, CHECK_OK);
                auto type = ParseType(CHECK_OK);
                Match(TOKEN_RBRACK, CHECK_OK);

                return factory_->CreateTypeTest(node, type, position);
            } break;

            case TOKEN_MATCH:
                node = PartialParseTypeMatch(node, CHECK_OK);
                break;

            default:
                return node;
        }
    }
}

Expression *Parser::ParsePrimary(bool *ok) {
    auto position = ahead_.position();
    switch (Peek()) {
         // ( expression )
        case TOKEN_LPAREN: {
            Match(TOKEN_LPAREN, CHECK_OK);
            auto node = ParseExpression(false, CHECK_OK);
            Match(TOKEN_RPAREN, CHECK_OK);
            return node;
        } break;

        // symbol
        // namespace::symbol
        case TOKEN_ID: {
            std::string id, ns;
            Match(TOKEN_ID, &id, CHECK_OK);
            if (Test(TOKEN_NAME_BREAK)) { // ::
                ns = id;
                Match(TOKEN_ID, &id, CHECK_OK);
            }
            return factory_->CreateSymbol(id, ns, position);
        } break;

        default:
            ThrowError("unexpected \'expression\', expected %s",
                       TokenNameWithText(Peek()).c_str());
            break;

    }
    *ok = false;
    return nullptr;
}

Expression *Parser::ParseFunctionLiteral(bool *ok) {
    auto position = ahead_.position();

    auto scope = EnterScope("", FUNCTION_SCOPE);
    auto proto = ParseFunctionPrototype(false, nullptr, CHECK_OK);

    auto is_assignment = Test(TOKEN_ASSIGN);
    auto body = ParseExpression(false, CHECK_OK);


    auto literal = factory_->CreateFunctionLiteral(proto, body, scope,
                                                   is_assignment, position,
                                                   ahead_.position());
    scope->set_name(RawString::sprintf(zone_, "function-%p", literal));
    LeaveScope();
    return literal;
}

// array {val1, val2, ... }
// array[element] { val1, val2, ... }
// array 'damn' { val1, val2, ... }
Expression *Parser::ParseArrayInitializer(bool *ok) {
    auto position = ahead_.position();
    auto array = ParseArrayType(false, CHECK_OK);

    std::string txt;
    RawStringRef annoation = RawString::kEmpty;
    if (Test(TOKEN_STRING_LITERAL, &txt)) {
        annoation = RawString::Create(txt, zone_);
    }

    auto elements = new (zone_) ZoneVector<Expression *>(zone_);
    Match(TOKEN_LBRACE, CHECK_OK);
    do {
        auto element = ParseExpression(false, CHECK_OK);

        elements->Add(element);
    } while (Test(TOKEN_COMMA));
    Match(TOKEN_RBRACE, CHECK_OK);

    return factory_->CreateArrayInitializer(array, elements, annoation,
                                            position, ahead_.position());
}

// map {key<-value, ...}
// map[key, value] {key<-value, ...}
// map 'weak' {key<-value, ...}
Expression *Parser::ParseMapInitializer(bool *ok) {
    auto position = ahead_.position();
    auto map = ParseMapType(false, CHECK_OK);

    std::string txt;
    RawStringRef annoation = RawString::kEmpty;
    if (Test(TOKEN_STRING_LITERAL, &txt)) {
        annoation = RawString::Create(txt, zone_);
    }

    auto pairs = new (zone_) ZoneVector<Pair *>(zone_);
    Match(TOKEN_LBRACE, CHECK_OK);
    do {
        auto pair_pos = ahead_.position();

        auto key = ParseExpression(false, CHECK_OK);
        Match(TOKEN_THIN_LARROW, CHECK_OK);
        auto value = ParseExpression(false, CHECK_OK);

        pairs->Add(factory_->CreatePair(key, value, pair_pos));
    } while (Test(TOKEN_COMMA));
    Match(TOKEN_RBRACE, CHECK_OK);

    return factory_->CreateMapInitializer(map, pairs, annoation, position,
                                          ahead_.position());
}

// lambda (parameters) -> expression
// lambda -> expression
Expression *Parser::ParseLambda(bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_LAMBDA, CHECK_OK);

    auto scope = EnterScope("", FUNCTION_SCOPE);
    auto params = new (zone_) ZoneVector<Paramter *>(zone_);
    if (Test(TOKEN_LPAREN)) {
        std::string name;
        do {
            auto param_pos = ahead_.position();
            Match(TOKEN_ID, &name, CHECK_OK);
            auto param = types_->CreateParamter(name, types_->GetUnknown());
            if (scope->FindOrNullLocal(param->param_name())) {
                *ok = false;
                ThrowError("duplicated argument name: %s",
                           param->param_name()->c_str());
                return nullptr;
            }
            auto declaration = factory_->CreateValDeclaration(name,
                                                              false,
                                                              param->param_type(),
                                                              nullptr,
                                                              scope,
                                                              true,
                                                              param_pos);
            scope->Declare(param->param_name(), declaration);
            params->Add(param);
        } while (Test(TOKEN_COMMA));
        Match(TOKEN_RPAREN, CHECK_OK);
    }
    Match(TOKEN_THIN_RARROW, CHECK_OK);
    auto proto = types_->GetFunctionPrototype(params, types_->GetUnknown());

    auto body = ParseExpression(false, CHECK_OK);
    auto lambda = factory_->CreateFunctionLiteral(proto, body, scope, true,
                                                  position, ahead_.position());
    scope->set_name(RawString::sprintf(zone_, "lambda-%d", position));
    LeaveScope();
    return lambda;
}

Type *Parser::ParseType(bool *ok) {
    switch (Peek()) {
        case TOKEN_VOID:
            lexer_->Next(&ahead_);
            return types_->GetVoid();

        case TOKEN_BOOL:
            lexer_->Next(&ahead_);
            return types_->GetI1();

        case TOKEN_I8:
            lexer_->Next(&ahead_);
            return types_->GetI8();

        case TOKEN_I16:
            lexer_->Next(&ahead_);
            return types_->GetI16();

        case TOKEN_I32:
            lexer_->Next(&ahead_);
            return types_->GetI32();

        case TOKEN_INT:
            lexer_->Next(&ahead_);
            return types_->GetI64();

        case TOKEN_I64:
            lexer_->Next(&ahead_);
            return types_->GetI64();

        case TOKEN_F32:
            lexer_->Next(&ahead_);
            return types_->GetF32();

        case TOKEN_F64:
            lexer_->Next(&ahead_);
            return types_->GetF64();

        case TOKEN_STRING:
            lexer_->Next(&ahead_);
            return types_->GetString();

        case TOKEN_FUNCTION:
            return ParseFunctionPrototype(true, nullptr, ok);

        case TOKEN_LBRACK:
            return ParseUnionType(ok);

        case TOKEN_SLICE:
        case TOKEN_ARRAY:
            return ParseArrayOrSliceType(ok);

        case TOKEN_MAP:
            return ParseMapType(true, ok);

            // TODO:
        default:
            *ok = false;
            ThrowError("unexpected type, expected %s",
                       ahead_.ToNameWithText().c_str());
            break;
    }
    return nullptr;
}

Union *Parser::ParseUnionType(bool *ok) {
    auto types = new (zone_) Union::TypeMap(zone_);

    Match(TOKEN_LBRACK, CHECK_OK);
    do {
        auto type = ParseType(CHECK_OK);
        types->Put(type->GenerateId(), type);
    }while (Test(TOKEN_COMMA));
    Match(TOKEN_RBRACK, CHECK_OK);

    return types_->GetUnion(types);
}


// function (a:int, b:int): void
// function name: void
FunctionPrototype *Parser::ParseFunctionPrototype(bool strict,
                                                  std::string *name,
                                                  bool *ok) {
    Match(TOKEN_FUNCTION, CHECK_OK);

    if (name) {
        Match(TOKEN_ID, name, CHECK_OK);
    }

    auto params = new (zone_) ZoneVector<Paramter *>(zone_);
    if (Test(TOKEN_LPAREN)) {
        std::string param_name;
        do {
            Match(TOKEN_ID, &param_name, CHECK_OK);
            auto param_type = static_cast<Type *>(types_->GetUnknown());
            if (strict) {
                Match(TOKEN_COLON, CHECK_OK);
                param_type = ParseType(CHECK_OK);
            } else {
                if (Test(TOKEN_COLON)) {
                    param_type = ParseType(CHECK_OK);
                }
            }

            params->Add(types_->CreateParamter(param_name, param_type));
        } while (Test(TOKEN_COMMA));
        Match(TOKEN_RPAREN, CHECK_OK);
    }
    auto return_type = static_cast<Type *>(types_->GetUnknown());
    if (strict) {
        Match(TOKEN_COLON, CHECK_OK);
        return_type = ParseType(CHECK_OK);
    } else {
        if (Test(TOKEN_COLON)) {
            return_type = ParseType(CHECK_OK);
        }
    }

    return types_->GetFunctionPrototype(params, return_type);
}

Array *Parser::ParseArrayOrSliceType(bool *ok) {
    if (ahead_.token_code() == TOKEN_ARRAY) {
        return ParseArrayType(true, ok);
    } else {
        return ParseSliceType(ok);
    }
}

Array *Parser::ParseArrayType(bool strict, bool *ok) {
    Match(TOKEN_ARRAY, CHECK_OK);

    Type *element = types_->GetUnknown();

    if (strict) {
        Match(TOKEN_LBRACK, CHECK_OK);
        element = ParseType(CHECK_OK);
        Match(TOKEN_RBRACK, CHECK_OK);
    } else {
        if (Test(TOKEN_LBRACK)) {
            element = ParseType(CHECK_OK);
            Match(TOKEN_RBRACK, CHECK_OK);
        }
    }
    return types_->GetArray(element);
}

Slice *Parser::ParseSliceType(bool *ok) {
    Match(TOKEN_SLICE, CHECK_OK);
    Match(TOKEN_LBRACK, CHECK_OK);
    auto element = ParseType(CHECK_OK);
    Match(TOKEN_RBRACK, CHECK_OK);
    return types_->GetSlice(element);
}

Map *Parser::ParseMapType(bool strict, bool *ok) {
    Match(TOKEN_MAP, CHECK_OK);

    Type *key = types_->GetUnknown(), *value = types_->GetUnknown();

    if (strict) {
        Match(TOKEN_LBRACK, CHECK_OK);
        key = ParseType(CHECK_OK);
        Match(TOKEN_COMMA, CHECK_OK);
        value = ParseType(CHECK_OK);
        Match(TOKEN_RBRACK, CHECK_OK);
    } else {
        if (Test(TOKEN_LBRACK)) {
            key = ParseType(CHECK_OK);
            Match(TOKEN_COMMA, CHECK_OK);
            value = ParseType(CHECK_OK);
            Match(TOKEN_RBRACK, CHECK_OK);
        }
    }

    return types_->GetMap(key, value);
}

TypeMatch *Parser::PartialParseTypeMatch(Expression *target, bool *ok) {
    Match(TOKEN_MATCH, CHECK_OK);

    Match(TOKEN_LBRACE, CHECK_OK);

    auto match_cases = new (zone_) ZoneVector<TypeMatchCase *>(zone_);
    int else_counter = 0;
    while (!Test(TOKEN_RBRACE)) {
        auto scope = EnterScope("match-case", BLOCK_SCOPE);
        ValDeclaration *cast_pattern = nullptr;

        auto match_case_position = ahead_.position();
        if (Test(TOKEN_ELSE)) {
            if (++else_counter > 1) {
                ThrowError("too much else-case in type match.");
                *ok = false;
                return nullptr;
            }
        } else {
            // name: type -> body
            auto cast_pattern_position = ahead_.position();
            std::string name;
            Match(TOKEN_ID, &name, CHECK_OK);
            Match(TOKEN_COLON, CHECK_OK);
            auto type = ParseType(CHECK_OK);

            cast_pattern = factory_->CreateValDeclaration(name, false, type,
                                                          nullptr, scope, false,
                                                          cast_pattern_position);
            scope->Declare(cast_pattern->name(), cast_pattern);
        }
        Match(TOKEN_THIN_RARROW, CHECK_OK); // ->
        auto body = ParseExpression(false, CHECK_OK);
        auto match_case = new (zone_) TypeMatchCase(cast_pattern, body,
                                                    scope, match_case_position);
        match_cases->Add(match_case);
        LeaveScope();
    }

    return factory_->CreateTypeMatch(target, match_cases, target->position());
}

void Parser::Match(Token code, std::string *txt, bool *ok) {
    DCHECK_NE(TOKEN_ERROR, code);

    if (ahead_.token_code() != code) {
        *ok = false;
        if (ahead_.is_error()) {
            ThrowError("unexpected: %s, lexer error: %s",
                       TokenNameWithText(code).c_str(),
                       ahead_.text().c_str());
        } else {
            ThrowError("unexpected: %s, expected: %s",
                       TokenNameWithText(code).c_str(),
                       ahead_.ToNameWithText().c_str());
        }
    }
    if (txt) {
        txt->assign(ahead_.text());
    }
    lexer_->Next(&ahead_);
}

bool Parser::Test(Token code, std::string *txt) {
    if (ahead_.token_code() != code) {
        return false;
    }

    if (txt) {
        txt->assign(ahead_.text());
    }
    lexer_->Next(&ahead_);
    return true;
}

void Parser::ThrowError(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    VThrowError(fmt, ap);
    va_end(ap);
}

void Parser::VThrowError(const char *fmt, va_list ap) {
    has_error_ = true;
    last_error_.column    = lexer_->current()->column;
    last_error_.line      = lexer_->current()->line;
    last_error_.position  = lexer_->current()->position;
    last_error_.file_name = lexer_->input_stream()->file_name();

    last_error_.message = TextOutputStream::vsprintf(fmt, ap);
}

} // namespace mio
