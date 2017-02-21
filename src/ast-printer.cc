#include "ast-printer.h"
#include "ast.h"
#include "glog/logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string>

namespace mio {

const char * const kOperatorTextName[] = {
#define Operator_TEXT_NAME(name, left_proi, right_proi, token) #name,
    DEFINE_OPS(Operator_TEXT_NAME)
#undef  Operator_TEXT_NAME
};

namespace {

class YamlPrinterVisitor : public AstVisitor {
public:
    YamlPrinterVisitor(std::string *buf, int indent_wide)
        : buf_(DCHECK_NOTNULL(buf))
        , indent_wide_(indent_wide) {}

    // package_name: name
    // with:
    //   - name: a1
    //     alias: .
    //   - name: b1
    //     alias: _
    //   - name: c1
    virtual void VisitPackageImporter(PackageImporter *node) override {
    }

    // op: OP_NOT
    // operand: 1
    virtual void VisitUnaryOperation(UnaryOperation *node) override {
        WriteMapPair("op", kOperatorTextName[node->op()]);
        WriteMapPair("operand", "");
        ++indent_;
        node->operand()->Accept(this);
        --indent_;
    }

    // op: OP_ADD
    // lhs:
    //   op: OP_BIT_INV
    //   operand:
    //     i8: 100
    // rhs:
    //   i8: 110
    virtual void VisitBinaryOperation(BinaryOperation *node) override {
        WriteMapPair("op", kOperatorTextName[node->op()]);
        WriteMapPair("lhs", "");
        ++indent_;
        node->lhs()->Accept(this);
        --indent_;

        WriteMapPair("rhs", "");
        ++indent_;
        node->rhs()->Accept(this);
        --indent_;
    }

    // i1: 1
    // i32: 100
    virtual void VisitSmiLiteral(SmiLiteral *node) override {
        Indent();
        Write("i%d: ", node->bitwide());
        switch (node->bitwide()) {
            case 1:
                Write("%s\n", node->i1() ? "true" : "false");
                break;
            case 8:
                Write("%d\n", node->i8());
                break;
            case 16:
                Write("%d\n", node->i16());
                break;
            case 32:
                Write("%d\n", node->i32());
                break;
            case 64:
                Write("%ld\n", node->i64());
                break;
        }
    }

    // symbol: name
    // symbol: ns::name
    virtual void VisitSymbol(Symbol *node) override {
        if (node->has_name_space()) {
            WriteMapPair("symbol", "%s::%s", node->name()->c_str(),
                         node->name_space()->c_str());
        } else {
            WriteMapPair("symbol", "%s", node->name()->c_str());
        }
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(YamlPrinterVisitor)
private:
    void Write(const char *fmt, ...) {
        va_list ap;
        char buf[128];
        va_start(ap, fmt);
        vsnprintf(buf, arraysize(buf), fmt, ap);
        va_end(ap);
        buf_->append(buf);
    }

    void WriteMapPair(const char *key, const char *fmt, ...) {
        Indent();
        buf_->append(key);
        buf_->append(": ");

        va_list ap;
        char buf[128];
        va_start(ap, fmt);
        vsnprintf(buf, arraysize(buf), fmt, ap);
        va_end(ap);
        buf_->append(buf);
        buf_->append("\n");
    }

    void Indent() {
        buf_->append(indent_wide_ * indent_, ' ');
    }

    std::string *buf_;
    int indent_wide_;
    int indent_ = 0;
};

} // namespace

/*static*/
void AstPrinter::ToYamlString(AstNode *ast, int indent_wide, std::string *buf) {
    auto printer = new YamlPrinterVisitor(buf, indent_wide);
    ast->Accept(printer);
}

} // namespace mio
