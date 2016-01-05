%pure-parser
%locations
%lex-param {void *scanner}
%parse-param {Parser *parser}

%{
#include <stdint.h>
#include "parser.h"
#include "grammar.hh"

#define scanner (parser->lexer())
#define YYERROR_VERBOSE 1

void yyerror(YYLTYPE *local, Parser *p, const char *message);

extern "C" int yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param,
                     void *yyscanner);
%}

%union {
    int64_t i64;
    double  f64;
}

%token ID
%token <i64> INTEGRAL_LITERAL
%token <f64> FLOATING_LITERAL

%left ASSIGN

%left PLUS MINUS
%left STAR DIV

%%

Expression: Operand {
}
| Expression PLUS Expression {
}
| Expression MINUS Expression {
}
| Expression STAR Expression {
}
| Expression DIV Expression {
}

Operand: ID {

}
| INTEGRAL_LITERAL {

}
| FLOATING_LITERAL {

}

%%

void yyerror(YYLTYPE *local, Parser *p, const char *message) {
    return;
}

