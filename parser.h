#ifndef PARSER_H_
#define PARSER_H_

class FlexLexer;

class Parser {
public:
    FlexLexer *lexer() { return lexer_; }

private:
    FlexLexer *lexer_;
};

#endif // PARSER_H_
