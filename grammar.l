%{
#include "grammar.hh"

int yywrap();
%}
%option yylineno
%option noyywrap
%option c++

digit [0-9]
%%

[+-]?[0-9]+ {
    return INTEGRAL;
}

[A-Za-z_$][A-Za-z_0-9]* {
    return ID;
}
%%

int yywrap() {
    return 1;
}
