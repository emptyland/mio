%{
#include "grammar.hh"
#include "parser_utils.h"

//int yywrap();
%}
%option yylineno
%option noyywrap
%option reentrant
%option bison-bridge bison-locations

digit [0-9]
%%

[+-]?[0-9]+ {
    if (ParseIntegral64(yytext, 10, &yylval->i64) < 0) {
        return BAD_TOKEN;
    } else {
        return INTEGRAL_LITERAL;
    }
}

[+-]?[0-9]+\.[0-9]+ {
    yylval->f64 = 0;
    return FLOATING_LITERAL;
}

[A-Za-z_$][A-Za-z_0-9]* {
    return ID;
}

"+" { return PLUS; }
"-" { return MINUS; }
"*" { return STAR; }
"/" { return DIV; }
"=" { return ASSIGN; }

%%

/*int yywrap() {
    return 1;
}*/
