%pure-parser
%lex-param {void *scanner}
%parse-param {YAParser *parser}

%{
#include <stdint.h>
#include "parse.h"
#include "program.h"
#include "ast.h"
#include "array.h"
#include "logging.h"
#include "y.tab.h"
#include "lex.yy.h"

#define scanner parser->lex

#define LINE yyget_lineno(scanner)

#define YYERROR_VERBOSE 1 // open better error output

void yyerror(YYLTYPE *local, YAParser *p, const char *msg);

//static YANode *YANodeLine(YAParser *p, YANode *node);

%}
%union {
	int64_t  i64;
	double   f64;
	YKStr    str;
	YANode  *node;
    YANode **node_list;
}
%token BYTE_TYPE SHORT_TYPE INT_TYPE LONG_TYPE FLOAT_TYPE DOUBLE_TYPE BOOL_TYPE
%token STRING_TYPE ANY_TYPE UNIT_TYPE
%token MAP ARRAY NIL_VAL NONE_VAL TRUE_VAL FALSE_VAL PACKAGE
%token COLON COMMA LBRACE RBRACE LBRACK RBRACK LPAREN RPAREN ARROW
%token VAL VAR DEF CLASS IMPORT IF WHILE CONTINUE BREAK UNDER MATCH FINALLY
%token REDUCE_TO THROW CATCH CASE RETURN FAIL ELSE TRY DOT DOT2 DOT3 SEMI
%token ELIF FUNC OPTION PROTOCOL NATIVE OVERRIDE SOME TUPLE FOR YIELD EACH_TO
%token BIT_AND BIT_OR BIT_XOR BIT_INV A_RSHIFT L_RSHIFT LSHIFT 

%token <str> NAME STRING_VAL
%token <i64> INTEGRAL_VAL BYTE_VAL SHORT_VAL LONG_VAL
%token <f64> FLOATING_VAL FLOAT_VAL DOUBLE_VAL

%type <str> package symbol

%type <node> file chunkStmt statement breakStmt continueStmt blockExpr type
%type <node> valDecl varDecl valSymbol varSymbol expr arrayType mapType
%type <node> simpleExpr unaryExpr binaryExpr suffixedExpr primaryExpr ifExpr
%type <node> ifClause elseClause whileStmt failClause throwStmt returnStmt
%type <node> matchExpr caseClause typeCaseClause defaultCaseClause tryExpr
%type <node> catchCaseClause finallyClause funcDef funcDecl paramDecl classDef
%type <node> classDecl classMember import arrayCtorExpr mapCtorExpr mapping
%type <node> argument funcType closureExpr optionType methodDef funcDefNormal
%type <node> funcDefNative tupleType tupleCtorExpr optionCtorExpr namedArgument
%type <node> optionCtorPrefix forEach forUntil forStmt iteratedValue

%type <node_list> chunk stmtList caseList catchClause catchCaseList params
%type <node_list> paramList classMembers importList exprList mappings args
%type <node_list> typeList namedArgs


%left ASSIGN INC DEC

%left OR
%left AND

%left EQ NE LT LE GT GE

%left PLUS MINUS BIT_AND BIT_XOR
%left STAR DIVIDE MOD BIT_OR A_RSHIFT L_RSHIFT LSHIFT

%left DOT DOT2

%nonassoc NOT MUST CHECK UPLUS UMINUS IS_INSTANCE_OF AS_INSTANCE_OF BIT_INV

%%
file: package importList chunk {
    YAFile *file = YANewNode(parser->zone, File, LINE);

    file->pkg_name = $1;
    file->u.env    = NULL;
    file->import   = (YAImport **)$2;
    file->chunk    = $3;
    file->name     = parser->file_name;
    
    parser->top = &file->head;

    $$ = &file->head;
}

package: PACKAGE symbol {
    parser->pkg = YAGetOrNewPackage(parser->pgm, parser->path, $2);

    $$ = $2;
}

importList: importList import {
    $$ = YANodeList(parser->zone, $1, $2);
}
| import {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

import: IMPORT STRING_VAL {
    YAImport *node = YANewNode(parser->zone, Import, LINE);

    node->alias = "*";
    node->path = $2;

    $$ = &node->head;
}
| IMPORT symbol STRING_VAL {
    YAImport *node = YANewNode(parser->zone, Import, LINE);
    
    node->alias = $2;
    node->path = $3;
    
    $$ = &node->head;
}
| IMPORT DOT STRING_VAL {
    YAImport *node = YANewNode(parser->zone, Import, LINE);
    
    node->alias = ".";
    node->path = $3;
    
    $$ = &node->head;
}
| IMPORT UNDER STRING_VAL {
    YAImport *node = YANewNode(parser->zone, Import, LINE);
    
    node->alias = "_";
    node->path = $3;

    $$ = &node->head;
}

chunk: chunk chunkStmt {
    $$ = YANodeList(parser->zone, $1, $2);
}
| chunkStmt {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

chunkStmt: classDef {
    YAParseRegisterSymbol(parser, $1);

    $$ = $1;
}
| funcDef {
    YAParseRegisterSymbol(parser, $1);

    $$ = $1;
}
| statement {
    YAParseRegisterSymbol(parser, $1);

    $$ = $1;
}

// class foo {}
// class foo(bar) {}
classDef: classDecl LBRACE classMembers RBRACE {
    YAClassDef *def = YACast($1, ClassDef);

    def->member = $3;

    $$ = &def->head;
}

classDecl: CLASS symbol {
    YAClassDef *def = YANewNode(parser->zone, ClassDef, LINE);

    def->name = $2;
    def->parent.name1 = NULL;
    def->parent.name2 = NULL;
    def->parent.clazz = NULL;

    $$ = &def->head;
}
| CLASS symbol LPAREN symbol RPAREN {
    YAClassDef *def = YANewNode(parser->zone, ClassDef, LINE);

    def->name = $2;
    def->parent.name1 = NULL;
    def->parent.name2 = $4;
    def->parent.clazz = NULL;

    $$ = &def->head;
}
| CLASS symbol LPAREN symbol DOT symbol RPAREN {
    YAClassDef *def = YANewNode(parser->zone, ClassDef, LINE);
    
    def->name = $2;
    def->parent.name1 = $4;
    def->parent.name2 = $6;
    def->parent.clazz = NULL;
    
    $$ = &def->head;
}

classMembers: classMembers classMember {
    $$ = YANodeList(parser->zone, $1, $2);
}
| classMember {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

classMember: varDecl {
    $$ = $1;
}
| valDecl {
    $$ = $1;
}
| methodDef {
    $$ = $1;
}

/*
 * class Foo {
 *   def foo(...) = expr
 *   def foo(...) {}
 *   native def foo(...): Type
 *   protocol def foo(...): Type
 *   override def foo(...): Type
 * }
 */
methodDef: funcDefNormal {
    $$ = $1;
}
| funcDefNative {
	$$ = $1;
}
| OVERRIDE funcDefNormal {
    YAFuncDef *def = YACast($2, FuncDef);

    def->kind = FUNC_OVERRIDE;

    $$ = &def->head;
}
| PROTOCOL DEF symbol funcDecl {
    YAFuncDef *def = YANewNode(parser->zone, FuncDef, LINE);

    def->name   = $3;
	def->kind   = FUNC_PROTOCOL;
    def->decl   = YACast($4, FuncDecl);
    def->assign = 0;
    def->chunk  = NULL;

    $$ = &def->head;
}

/*
 * def foo(...) {}
 * def foo(...): Unit {}
 * def foo(...) = {}
 * def foo(...) = expr
 * def foo(...): Type = expr
 * native def foo(...): Type
 */
funcDef: funcDefNormal {
    $$ = $1;
}
| funcDefNative {
    $$ = $1;
}

funcDefNative: NATIVE DEF symbol funcDecl {
    YAFuncDef *def = YANewNode(parser->zone, FuncDef, LINE);

    def->name   = $3;
	def->kind   = FUNC_NATIVE;
    def->decl   = YACast($4, FuncDecl);
    def->assign = 0;
    def->chunk  = NULL;

    $$ = &def->head;
}

funcDefNormal: DEF symbol funcDecl blockExpr {
    YAFuncDef *def = YANewNode(parser->zone, FuncDef, LINE);
    
    def->name   = $2;
	def->kind   = FUNC_NORMAL;
    def->decl   = YACast($3, FuncDecl);
    def->assign = 0;
    def->chunk  = $4;
    
    $$ = &def->head;
}
| DEF symbol funcDecl ASSIGN expr {
    YAFuncDef *def = YANewNode(parser->zone, FuncDef, LINE);

    def->name   = $2;
	def->kind   = FUNC_NORMAL;
    def->decl   = YACast($3, FuncDecl);
    def->assign = 1;
    def->chunk  = $5;

    $$ = &def->head;
}

funcDecl: LPAREN params RPAREN {
    YAFuncDecl *decl = YANewNode(parser->zone, FuncDecl, LINE);

    decl->clazz = NULL;
    decl->params = $2;
    decl->return_type = NULL;

    $$ = &decl->head;
}
| LPAREN params RPAREN COLON type {
    YAFuncDecl *decl = YANewNode(parser->zone, FuncDecl, LINE);

    decl->clazz = NULL;
    decl->params = $2;
    decl->return_type = YACast($5, Type);
    
    $$ = &decl->head;
}

// def foo(a:Int, b:String, ...)
params: paramList COMMA DOT3 {
    YANode *dot3 = YANewNode(parser->zone, Dot3, LINE);
    $$ = YANodeList(parser->zone, $1, dot3);
}
| paramList {
    $$ = $1;
}
| DOT3 {
    YANode *dot3 = YANewNode(parser->zone, Dot3, LINE);
    $$ = YANodeList(parser->zone, NULL, dot3);
}

paramList: paramList COMMA paramDecl {
    $$ = YANodeList(parser->zone, $1, $3);
}
| paramDecl {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

paramDecl: symbol COLON type {
    YAValDecl *param = YANewNode(parser->zone, ValDecl, LINE);

    param->name = $1;
    param->type = YACast($3, Type);
    param->init = NULL;

    $$ = &param->head;
}

/* variable & value declare */
varDecl: varSymbol {
    $$ = $1;
}
| varSymbol ASSIGN expr {
    YAVarDecl *var = (YAVarDecl *)$1;
    var->init = $3;
    
    $$ = &var->head;
}

varSymbol: VAR symbol {
    YAVarDecl *var = YANewNode(parser->zone, VarDecl, LINE);
    var->name = $2;
    var->type = NULL;
    var->init = NULL;

    $$ = &var->head;
}
| VAR symbol COLON type {
    YAVarDecl *var = YANewNode(parser->zone, VarDecl, LINE);
    var->name = $2;
    var->type = YACast($4, Type);
    var->init = NULL;
    
    $$ = &var->head;
}

valDecl: valSymbol {
    $$ = $1;
}
| valSymbol ASSIGN expr {
    YAValDecl *val = (YAValDecl *)$1;
    val->init = $3;
    
    $$ = &val->head;
}

valSymbol: VAL symbol {
    YAValDecl *val = YANewNode(parser->zone, ValDecl, LINE);
    val->name = $2;
    val->type = NULL;
    val->init = NULL;
    
    $$ = &val->head;
}
| VAL symbol COLON type {
    YAValDecl *val = YANewNode(parser->zone, ValDecl, LINE);
    val->name = $2;
    val->type = YACast($4, Type);
    val->init = NULL;
    
    $$ = &val->head;
}

symbol: NAME {
    $$ = $1;
}

/* types */
type: BYTE_TYPE {
    $$ = &parser->pgm->tags.Byte->head;
}
| SHORT_TYPE {
    $$ = &parser->pgm->tags.Short->head;
}
| INT_TYPE {
    $$ = &parser->pgm->tags.Int->head;
}
| LONG_TYPE {
    $$ = &parser->pgm->tags.Long->head;
}
| FLOAT_TYPE {
    $$ = &parser->pgm->tags.Float->head;
}
| DOUBLE_TYPE {
    $$ = &parser->pgm->tags.Double->head;
}
| STRING_TYPE {
    $$ = &parser->pgm->tags.String->head;
}
| BOOL_TYPE {
    $$ = &parser->pgm->tags.Bool->head;
}
| ANY_TYPE {
    $$ = &parser->pgm->tags.Any->head;
}
| UNIT_TYPE {
    $$ = &parser->pgm->tags.Unit->head;
}
| symbol {
    $$ = YANewSymType(parser->zone, NULL, $1, LINE);
}
| symbol DOT symbol {
    $$ = YANewSymType(parser->zone, $1, $3, LINE);
}
| mapType {
    $$ = $1;
}
| arrayType {
    $$ = $1;
}
| funcType {
    $$ = $1;
}
| optionType {
    $$ = $1;
}
| tupleType {
    $$ = $1;
}

/* Array[Type2] */
arrayType: ARRAY LBRACK type RBRACK {
    YAType *type2 = YACast($3, Type);

    $$ = YANewCollType(parser->zone, NULL, type2, LINE);
}

/* Map[Type1, Type2] */
mapType: MAP LBRACK type COMMA type RBRACK {
    YAType *type1 = YACast($3, Type);
    YAType *type2 = YACast($5, Type);

    $$ = YANewCollType(parser->zone, type1, type2, LINE);
}

/* Func[(params): type] */
funcType: FUNC LBRACK funcDecl RBRACK {
    YAType *type = YANewNode(parser->zone, Type, LINE);

    type->kind = KIND_FUNC;
    type->u.func = YACast($3, FuncDecl);

    $$ = &type->head;
}

/* Option[type] */
optionType: OPTION LBRACK type RBRACK {
    YAType *type = YANewNode(parser->zone, Type, LINE);

    type->kind = KIND_OPTION;
    type->u.opt = YACast($3, Type);

    $$ = &type->head;
}

/* Tuple[type1, type2, ...] */
tupleType: TUPLE LBRACK typeList RBRACK {
    YAType *type = YANewNode(parser->zone, Type, LINE);

    type->kind = KIND_TUPLE;
    type->u.tuple = (YAType **)$3;

    $$ = &type->head;
}

typeList: typeList COMMA type {
    $$ = YANodeList(parser->zone, $1, $3);
}
| type {
    $$ = YANodeList(parser->zone, NULL, $1);
}

/* expressions */
expr: simpleExpr {
    $$ = $1;
}
| unaryExpr {
    $$ = $1;
}
| suffixedExpr {
    $$ = $1;
}
| blockExpr {
    $$ = $1;
}
| binaryExpr {
    $$ = $1;
}
| ifExpr {
    $$ = $1;
}
| matchExpr {
    $$ = $1;
}
| tryExpr {
    $$ = $1;
}
| throwStmt {
    $$ = $1;
}
| breakStmt {
    $$ = $1;
}
| continueStmt {
    $$ = $1;
}
| returnStmt {
    $$ = $1;
}
| whileStmt {
    $$ = $1;
}
| forStmt {
    $$ = $1;
}

blockExpr: LBRACE stmtList RBRACE {
    YABlock *block = YANewNode(parser->zone, Block, LINE);

    block->chunk = $2;
    $$ = &block->head;
}

stmtList: stmtList statement {
    $$ = YANodeList(parser->zone, $1, $2);
}
| statement {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

statement: expr {
    $$ = $1;
}
| varDecl {
    $$ = $1;
}
| valDecl {
    $$ = $1;
}
| statement SEMI {
    $$ = $1;
}

unaryExpr: PLUS expr %prec UPLUS {
    $$ = YANewCompute(parser->zone, AST_UnaryPlus, NULL, $2, LINE);
}
| MINUS expr %prec UMINUS {
    $$ = YANewCompute(parser->zone, AST_UnaryMinus, NULL, $2, LINE);
}
| NOT expr {
    $$ = YANewCompute(parser->zone, AST_Not, NULL, $2, LINE);
}
| BIT_INV expr {
    $$ = YANewCompute(parser->zone, AST_BitInv, NULL, $2, LINE);
}
| expr MUST {
    $$ = YANewCompute(parser->zone, AST_Must, $1, NULL, LINE);
}
| expr CHECK {
    $$ = YANewCompute(parser->zone, AST_IsDefined, $1, NULL, LINE);
}
| expr IS_INSTANCE_OF type {
    $$ = YANewCompute(parser->zone, AST_IsInstanceOf, $1, $3, LINE);
}
| expr AS_INSTANCE_OF type {
    $$ = YANewCompute(parser->zone, AST_AsInstanceOf, $1, $3, LINE);
}

binaryExpr: expr PLUS expr {
    $$ = YANewCompute(parser->zone, AST_Plus, $1, $3, LINE);
}
| expr MINUS expr {
    $$ = YANewCompute(parser->zone, AST_Sub, $1, $3, LINE);
}
| expr DIVIDE expr {
    $$ = YANewCompute(parser->zone, AST_Div, $1, $3, LINE);
}
| expr MOD expr {
    $$ = YANewCompute(parser->zone, AST_Mod, $1, $3, LINE);
}
| expr BIT_AND expr {
    $$ = YANewCompute(parser->zone, AST_BitAnd, $1, $3, LINE);
}
| expr BIT_OR expr {
    $$ = YANewCompute(parser->zone, AST_BitOr, $1, $3, LINE);
}
| expr BIT_XOR expr {
    $$ = YANewCompute(parser->zone, AST_BitXor, $1, $3, LINE);
}
| expr A_RSHIFT expr {
    $$ = YANewCompute(parser->zone, AST_ARShift, $1, $3, LINE);
}
| expr L_RSHIFT expr {
    $$ = YANewCompute(parser->zone, AST_LRShift, $1, $3, LINE);
}
| expr LSHIFT expr {
    $$ = YANewCompute(parser->zone, AST_LShift, $1, $3, LINE);
}
| expr STAR expr {
    $$ = YANewCompute(parser->zone, AST_Mul, $1, $3, LINE);
}
| expr ASSIGN expr {
    $$ = YANewCompute(parser->zone, AST_Assign, $1, $3, LINE);
}
| expr EQ expr {
    $$ = YANewCompute(parser->zone, AST_Eq, $1, $3, LINE);
}
| expr NE expr {
    $$ = YANewCompute(parser->zone, AST_Ne, $1, $3, LINE);
}
| expr LT expr {
    $$ = YANewCompute(parser->zone, AST_Lt, $1, $3, LINE);
}
| expr LE expr {
    $$ = YANewCompute(parser->zone, AST_Le, $1, $3, LINE);
}
| expr GT expr {
    $$ = YANewCompute(parser->zone, AST_Gt, $1, $3, LINE);
}
| expr GE expr {
    $$ = YANewCompute(parser->zone, AST_Ge, $1, $3, LINE);
}
| expr AND expr {
    $$ = YANewCompute(parser->zone, AST_And, $1, $3, LINE);
}
| expr OR expr {
    $$ = YANewCompute(parser->zone, AST_Or, $1, $3, LINE);
}
| expr DOT2 expr {
    $$ = YANewCompute(parser->zone, AST_ToStrCat, $1, $3, LINE);
}

simpleExpr: NIL_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);

    val->type = TYPE_NIL;
    val->u.nil_val = NULL;
    $$ = &val->head;
}
| NONE_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_NONE;
    val->u.nil_val = NULL;
    $$ = &val->head;
}
| TRUE_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_BOOLEAN;
    val->u.bool_val = 1;
    $$ = &val->head;
}
| FALSE_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_BOOLEAN;
    val->u.bool_val = 0;
    $$ = &val->head;
}
| INTEGRAL_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_INTEGRAL;
    val->u.i64_val = $1;
    $$ = &val->head;
}
| BYTE_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_BYTE;
    val->u.i64_val = $1;
    $$ = &val->head;
}
| SHORT_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_SHORT;
    val->u.i64_val = $1;
    $$ = &val->head;
}
| LONG_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_LONG;
    val->u.i64_val = $1;
    $$ = &val->head;
}
| FLOATING_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_FLOATING;
    val->u.f64_val = $1;
    $$ = &val->head;
}
| DOUBLE_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_DOUBLE;
    val->u.f64_val = $1;
    $$ = &val->head;
}
| STRING_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_STRING;
    val->u.str_val = $1;
    $$ = &val->head;
}
| arrayCtorExpr {
    $$ = $1;
}
| mapCtorExpr {
    $$ = $1;
}
| tupleCtorExpr {
    $$ = $1;
}
| optionCtorExpr {
    $$ = $1;
}
| closureExpr {
    $$ = $1;
}

arrayCtorExpr: arrayType LPAREN exprList RPAREN {
    YAArrayCtor *ctor = YANewNode(parser->zone, ArrayCtor, LINE);

    ctor->type = YACast($1, Type);
    ctor->init = $3;
    ctor->args = NULL;

    $$ = &ctor->head;
}
| arrayType LPAREN namedArgs RPAREN {
    YAArrayCtor *ctor = YANewNode(parser->zone, ArrayCtor, LINE);

    ctor->type = YACast($1, Type);
    ctor->init = NULL;
    ctor->args = (YAArg**)$3;

    $$ = &ctor->head;
}
| ARRAY LPAREN exprList RPAREN {
    YAType *type = YANewNode(parser->zone, Type, LINE);

    type->kind = KIND_ARRAY;
    type->u.coll.type1 = NULL;
    type->u.coll.type2 = NULL;

    YAArrayCtor *ctor = YANewNode(parser->zone, ArrayCtor, LINE);

    ctor->type = type;
    ctor->init = $3;
    ctor->args = NULL;

    $$ = &ctor->head;
}

exprList: exprList COMMA expr {
    $$ = YANodeList(parser->zone, $1, $3);
}
| expr {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

namedArgs: namedArgs COMMA namedArgument {
    $$ = YANodeList(parser->zone, $1, $3);
}
| namedArgument {
    $$ = YANodeList(parser->zone, NULL, $1);
}

namedArgument: symbol ASSIGN expr {
    YAArg *arg = YANewNode(parser->zone, Arg, LINE);
    
    arg->name  = $1;
    arg->value = $3;
    
    $$ = &arg->head;
}

mapCtorExpr: mapType LPAREN mappings RPAREN {
    YAMapCtor *ctor = YANewNode(parser->zone, MapCtor, LINE);
    
    ctor->type = YACast($1, Type);
    ctor->init = (YAPair **)$3;

    $$ = &ctor->head;
}
| MAP LPAREN mappings RPAREN {
    YAType *type = YANewNode(parser->zone, Type, LINE);
    
    type->kind = KIND_MAP;
    type->u.coll.type1 = NULL;
    type->u.coll.type2 = NULL;

    YAMapCtor *ctor = YANewNode(parser->zone, MapCtor, LINE);

    ctor->type = type;
    ctor->init = (YAPair **)$3;

    $$ = &ctor->head;
}

mappings: mappings COMMA mapping {
    $$ = YANodeList(parser->zone, $1, $3);
}
| mapping {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

mapping: expr ARROW expr {
    $$ = YANewCompute(parser->zone, AST_Pair, $1, $3, LINE);
}

tupleCtorExpr: tupleType LPAREN exprList RPAREN {
    YATupleCtor *ctor = YANewNode(parser->zone, TupleCtor, LINE);

    ctor->type = YACast($1, Type);
    ctor->init = $3;

    $$ = &ctor->head;
}
| TUPLE LPAREN exprList RPAREN {
    YAType *type = YANewKindType(parser->zone, KIND_TUPLE, LINE);

    type->u.tuple = NULL;

    YATupleCtor *ctor = YANewNode(parser->zone, TupleCtor, LINE);
    
    ctor->type = type;
    ctor->init = $3;

    $$ = &ctor->head;
}

optionCtorExpr: optionCtorPrefix LPAREN expr RPAREN {
    YAOptionCtor *ctor = YANewNode(parser->zone, OptionCtor, LINE);

    ctor->type = YACast($1, Type);
    ctor->some = $3;

    $$ = &ctor->head;
}
| optionType LPAREN RPAREN {
    YAOptionCtor *ctor = YANewNode(parser->zone, OptionCtor, LINE);
    
    ctor->type = YACast($1, Type);
    ctor->some = NULL;
    
    $$ = &ctor->head;
}

optionCtorPrefix: SOME {
    YAType *type = YANewKindType(parser->zone, KIND_OPTION, LINE);

    type->u.opt = NULL;

    $$ = &type->head;
}
| OPTION {
    YAType *type = YANewKindType(parser->zone, KIND_OPTION, LINE);
    
    type->u.opt = NULL;

    $$ = &type->head;
}
| optionType {
    $$ = $1;
}

closureExpr: funcDecl blockExpr {
    YAClosure *def = YANewNode(parser->zone, Closure, LINE);

    def->name = NULL;
    def->kind = FUNC_NORMAL;
    def->decl = YACast($1, FuncDecl);
    def->assign = 0;
    def->chunk = $2;

    $$ = &def->head;
}
| funcDecl ASSIGN expr {
    YAClosure *def = YANewNode(parser->zone, Closure, LINE);

    def->name = NULL;
    def->kind = FUNC_NORMAL;
    def->decl = YACast($1, FuncDecl);
    def->assign = 1;
    def->chunk = $3;
    
    $$ = &def->head;
}

suffixedExpr: primaryExpr {
    $$ = $1;
}
| suffixedExpr DOT symbol {
    YASymbol *sym = YANewNode(parser->zone, Symbol, LINE);
    
    sym->name = $3;

    $$ = YANewCompute(parser->zone, AST_Dot, $1, &sym->head, LINE);
}
| suffixedExpr DOT INTEGRAL_VAL {
    YALiteralVal *val = YANewNode(parser->zone, LiteralVal, LINE);
    
    val->type = TYPE_INTEGRAL;
    val->u.i64_val = $3;

    $$ = YANewCompute(parser->zone, AST_DotInt, $1, &val->head, LINE);
}
| suffixedExpr LPAREN args RPAREN {
    YACall *call = YANewNode(parser->zone, Call, LINE);

    call->caller = $1;
    call->args   = (YAArg **)$3;

    $$ = &call->head;
}


primaryExpr: LPAREN expr RPAREN{
    $$ = $2;
}
| symbol {
    YASymbol *sym = YANewNode(parser->zone, Symbol, LINE);

    sym->name = $1;
    $$ = &sym->head;
}


args: args COMMA argument {
    $$ = YANodeList(parser->zone, $1, $3);
}
| argument {
    $$ = YANodeList(parser->zone, NULL, $1);
}
| {
    $$ = NULL;
}

argument: expr {
    YAArg *arg = YANewNode(parser->zone, Arg, LINE);

    arg->name  = NULL;
    arg->value = $1;

    $$ = &arg->head;
}
| namedArgument {
    $$ = $1;
}

/* if expression */
ifExpr: ifClause elseClause {
    YAIf *node = YANewNode(parser->zone, If, LINE);

    node->type = NULL;
    node->if_clause = YACast($1, Pair);
    node->else_clause = $2;
    
    $$ = &node->head;
}
| ifClause {
    YAIf *node = YANewNode(parser->zone, If, LINE);

    node->type = NULL;
    node->if_clause = YACast($1, Pair);
    node->else_clause = NULL;

    $$ = &node->head;
}

ifClause: IF LPAREN expr RPAREN expr {
    $$ = YANewCompute(parser->zone, AST_Pair, $3, $5, LINE);
}

elseClause: ELSE expr {
    $$ = $2;
}

/* match expression */
matchExpr: expr MATCH LBRACE caseList RBRACE {
    YAMatch *match = YANewNode(parser->zone, Match, LINE);

    match->type = NULL;
    match->expr = $1;
    match->case_clause = (YAPair **)$4;

    $$ = &match->head;
}

caseList: caseList caseClause {
    $$ = YANodeList(parser->zone, $1, $2);
}
| caseClause {
    $$ = YANodeList(parser->zone, NULL, $1);
}

caseClause: CASE expr REDUCE_TO expr {
    $$ = YANewCompute(parser->zone, AST_Pair, $2, $4, LINE);
}
| typeCaseClause {
    $$ = $1;
}
| defaultCaseClause {
    $$ = $1;
}

typeCaseClause: CASE symbol COLON type REDUCE_TO expr {
    YAValDecl *val = YANewNode(parser->zone, ValDecl, LINE);
    val->name = $2;
    val->type = YACast($4, Type);
    val->init = NULL;

    $$ = YANewCompute(parser->zone, AST_Pair, &val->head, $6, LINE);
}

defaultCaseClause: CASE UNDER REDUCE_TO expr {
    $$ = YANewCompute(parser->zone, AST_Pair, NULL, $4, LINE);
}

/* try-catch-finally expression */
tryExpr: TRY expr catchClause finallyClause {
    YATry *node = YANewNode(parser->zone, Try, LINE);

    node->try_clause = $2;
    node->catch_clause = (YAPair **)$3;
    node->finally_clause = $4;

    $$ = &node->head;
}
| TRY expr catchClause {
    YATry *node = YANewNode(parser->zone, Try, LINE);
    
    node->try_clause = $2;
    node->catch_clause = (YAPair **)$3;
    node->finally_clause = NULL;
    
    $$ = &node->head;
}
| TRY expr finallyClause {
    YATry *node = YANewNode(parser->zone, Try, LINE);
    
    node->try_clause = $2;
    node->catch_clause = NULL;
    node->finally_clause = $3;
    
    $$ = &node->head;
}

catchClause: CATCH LBRACE catchCaseList RBRACE {
    $$ = $3;
}

catchCaseList: catchCaseList catchCaseClause {
    $$ = YANodeList(parser->zone, $1, $2);
}
| catchCaseClause {
    $$ = YANodeList(parser->zone, NULL, $1);
}

catchCaseClause: typeCaseClause {
    $$ = $1;
}
| defaultCaseClause {
    $$ = $1;
}

finallyClause: FINALLY expr {
    $$ = $2;
}
| {
    $$ = NULL;
}

/* Statements */
/* throw-stmt */
throwStmt: THROW expr {
    $$ = YANewCompute(parser->zone, AST_Throw, NULL, $2, LINE);
}

/* return-stmt */
returnStmt: RETURN expr {
    $$ = YANewCompute(parser->zone, AST_Return, NULL, $2, LINE);
}
| RETURN {
    $$ = YANewCompute(parser->zone, AST_Return, NULL, NULL, LINE);
}

/* break-continue-stmt */
breakStmt: BREAK {
    $$ = YANewNode(parser->zone, Break, LINE);
}
continueStmt: CONTINUE {
    $$ = YANewNode(parser->zone, Continue, LINE);
}

/* while-stmt */
whileStmt: WHILE LPAREN expr RPAREN expr failClause {
    YAWhile *node = YANewNode(parser->zone, While, LINE);

    node->cond = $3;
    node->body = $5;
    node->fail = $6;

    $$ = &node->head;
}

failClause: FAIL expr {
    $$ = $2;
}
| {
    $$ = NULL;
}

/* for-stmt */
forStmt: forUntil YIELD expr {
    YAForUntil *node = YACast($1, ForUntil);

    node->yield = 1;
    node->body  = $3;

    $$ = $1;
}
| forUntil expr {
    YAForUntil *node = YACast($1, ForUntil);
    
    node->yield = 0;
    node->body  = $2;
    
    $$ = $1;
}
| forEach YIELD expr {
    YAForEach *node = YACast($1, ForEach);
    
    node->yield = 1;
    node->body  = $3;
    
    $$ = $1;
}
| forEach expr {
    YAForEach *node = YACast($1, ForEach);
    
    node->yield = 0;
    node->body  = $2;
    
    $$ = $1;
}

// for (i <- 0, 100)
forUntil: FOR LPAREN iteratedValue EACH_TO expr COMMA expr RPAREN {
    YAForUntil *node = YANewNode(parser->zone, ForUntil, LINE);

    node->val   = YACast($3, ValDecl);
    node->init  = $5;
    node->until = $7;
    node->step  = NULL;
    node->yield = 0;
    node->type  = NULL;
    node->body  = NULL;

    $$ = &node->head;
}
// for (i <- 100, 0, -1)
| FOR LPAREN iteratedValue EACH_TO expr COMMA expr COMMA expr RPAREN {
    YAForUntil *node = YANewNode(parser->zone, ForUntil, LINE);
    
    node->val   = YACast($3, ValDecl);
    node->init  = $5;
    node->until = $7;
    node->step  = $9;
    node->yield = 0;
    node->type  = NULL;
    node->body  = NULL;
    
    $$ = &node->head;
}

// for (k, v <- gen)
forEach: FOR LPAREN iteratedValue COMMA iteratedValue EACH_TO expr RPAREN {
    YAForEach *node = YANewNode(parser->zone, ForEach, LINE);

    node->val1  = YACast($3, ValDecl);
    node->val2  = YACast($5, ValDecl);
    node->expr  = $7;
    node->yield = 0;
    node->type  = NULL;
    node->body  = NULL;

    $$ = &node->head;
}

iteratedValue: symbol {
    YAValDecl *val = YANewNode(parser->zone, ValDecl, LINE);

    val->name = $1;
    val->type = NULL;
    val->init = NULL;

    $$ = &val->head;
}
| UNDER {
    $$ = NULL;
}


%%

void yyerror(YYLTYPE *local, YAParser *p, const char *msg) {
    int line = yyget_lineno(p->lex);
    DLOG(Debug, &p->log, "ERR %s:%d %s", p->file_name, line, msg);
    YAParseRaise(p, line, local->first_column, msg);
}

/*static YANode *YANodeLine(YAParser *p, YANode *node) {
    node->column = 0;
    node->line   = yyget_lineno(p->lex);
    return node;
}*/
