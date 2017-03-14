#ifndef MIO_AST_PRINTER_H_
#define MIO_AST_PRINTER_H_

#include "compiler.h"
#include <string>

namespace mio {

class AstNode;
class TextOutputStream;

class AstPrinter {
public:
    static void ToYamlString(AstNode *ast, int indent_wide, std::string *buf);

    static void ToYamlString(AstNode *ast, int indent_wide,
                             TextOutputStream *stream);

    static void ToYamlString(CompiledUnitMap *all_units,
                             int indent_wide,
                             TextOutputStream *stream);

    static void ToYamlString(CompiledUnitMap *all_units,
                             int indent_wide,
                             std::string *buf);
    AstPrinter() = delete;
};

} // namespace mio

#endif // MIO_AST_PRINTER_H_
