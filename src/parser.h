#ifndef PARSER_H_
#define PARSER_H_

typedef void *yyscan_t;

class Parser {
public:
    yyscan_t lexer() { return lexer_; }

private:
    yyscan_t lexer_;
};

#endif // PARSER_H_
