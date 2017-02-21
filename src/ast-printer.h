#ifndef MIO_AST_PRINTER_H_
#define MIO_AST_PRINTER_H_

#include <string>

namespace mio {

class AstNode;

class AstPrinter {
public:
    static void ToYamlString(AstNode *ast, int indent_wide, std::string *buf);

    AstPrinter() = delete;
};

} // namespace mio

#endif // MIO_AST_PRINTER_H_
