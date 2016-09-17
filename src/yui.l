%{
#include "parse.h"
#include "y.tab.h"
#include <stdlib.h>

int yywrap();
%}
chars [A-za-z_$0-9]
digit [0-9]
delim [ \f\r\t\v]

%%
"Any" return ANY_TYPE;
"Byte" return BYTE_TYPE;
"Short" return SHORT_TYPE;
"Int" return INT_TYPE;
"Long" return LONG_TYPE;
"Float" return FLOAT_TYPE;
"Double" return DOUBLE_TYPE;
"Bool" return BOOL_TYPE;
"String" return STRING_TYPE;
"Unit" return UNIT_TYPE;
"Option" return OPTION;
"Tuple" return TUPLE;
"Map" return MAP;
"Array" return ARRAY;
"Func" return FUNC;
"Some" return SOME;
"None" return NONE_VAL;

"nil" return NIL_VAL;
"true" return TRUE_VAL;
"false" return FALSE_VAL;

"package" return PACKAGE;
"var" return VAR;
"val" return VAL;
"def" return DEF;
"class" return CLASS;
"import" return IMPORT;
"if" return IF;
"else" return ELSE;
"elif" return ELIF;
"while" return WHILE;
"for" return FOR;
"yield" return YIELD;
"continue" return CONTINUE;
"break" return BREAK;
"match" return MATCH;
"finally" return FINALLY;
"throw" return THROW;
"catch" return CATCH;
"case" return CASE;
"return" return RETURN;
"fail" return FAIL;
"try" return TRY;
"protocol" return PROTOCOL;
"override" return OVERRIDE;
"native" return NATIVE;

":" return COLON;
";" return SEMI;
"..." return DOT3;
".." return DOT2;
"." return DOT;
"," return COMMA;
"_" return UNDER;
"{" return LBRACE;
"}" return RBRACE;
"[" return LBRACK;
"]" return RBRACK;
"(" return LPAREN;
")" return RPAREN;
"->" return ARROW;
"<-" return EACH_TO;
"=>" return REDUCE_TO;

"=" return ASSIGN;
"+" return PLUS;
"-" return MINUS;
"*" return STAR;
"/" return DIVIDE;
"%" return MOD;
"|" return BIT_OR;
"&" return BIT_AND;
"^" return BIT_XOR;
"~" return BIT_INV;
">>" return L_RSHIFT;
">>>" return A_RSHIFT;
"<<" return LSHIFT;
"+=" return INC;
"-=" return DEC;
"==" return EQ;
"!=" return NE;
"<" return LT;
"<=" return LE;
">" return GT;
">=" return GE;
"and" return AND;
"or" return OR;
"not" return NOT;
"!" return MUST;
"?" return CHECK;
"is" return IS_INSTANCE_OF;
"as" return AS_INSTANCE_OF;

[A-Za-z_$][A-Za-z_$0-9]* {
    yylval->str = YKString(yyextra->zone, yytext);
	return NAME;
}

[+-]?[0-9]+[bBsSlL]? {
    return YAAtoI(yytext, &yylval->i64);
}

[+-]?[0-9]+\.[0-9]+[dD]? {
    return YAAtoF(yytext, &yylval->f64);
}

"0x"[0-9a-fA-F]+ {
    return YAXtoI(yytext, &yylval->i64);
}

\"(\\.|[^\\"])*\" {
    yylval->str = YADequoteEscape(yyextra->zone, yytext);
    return STRING_VAL;
}

\/\/.*\n {
    // comment
}

[ \f\r\t\v]+ {
}

\n {
}

%%
int yywrap() {
	return 1;
}
