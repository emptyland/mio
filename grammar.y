%pure-parser
%lex-param {FlexLexer *scanner}
%parse-param {Parser *parser}

%{
#include "parser.h"
#include "grammar.hh"
#include <FlexLexer.h>

#define scanner (parser->lexer())
#define YYERROR_VERBOSE 1

void yyerror(Parser *p, const char *message);

int yylex(YYSTYPE *, void *);
%}

%token ID INTEGRAL


%%
File: ID Statement {
}

Statement: INTEGRAL {
}

%%

void yyerror(Parser *p, const char *message) {
    return;
}

int yylex(YYSTYPE *, void *param) {
    FlexLexer *lexer = static_cast<FlexLexer*>(param);
    return lexer->yylex();
}
