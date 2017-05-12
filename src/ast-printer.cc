#include "ast-printer.h"
#include "memory-output-stream.h"
#include "text-output-stream.h"
#include "ast.h"
#include "types.h"
#include "scopes.h"
#include "glog/logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string>
//#include <unordered_set>

namespace mio {

const char * const kOperatorTextName[] = {
#define Operator_TEXT_NAME(name, left_proi, right_proi, token) #name,
    DEFINE_OPS(Operator_TEXT_NAME)
#undef  Operator_TEXT_NAME
};

namespace {

class YamlPrinterVisitor : public AstVisitor {
public:
    YamlPrinterVisitor(TextOutputStream *stream, int indent_wide)
        : stream_(DCHECK_NOTNULL(stream))
        , indent_wide_(indent_wide) {}

    // package_name: name
    // with:
    //   - name: a1
    //     alias: .
    //   - name: b1
    //     alias: _
    //   - name: c1
    virtual void VisitPackageImporter(PackageImporter *node) override {
        WriteMapPair("package", node->package_name()->c_str());
    }

    // op: OP_NOT
    // operand: 1
    virtual void VisitUnaryOperation(UnaryOperation *node) override {
        WriteMapPair("op", kOperatorTextName[node->op()]);
        Indent(); WriteMapPair("operand", node->operand());
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
        Indent(); WriteMapPair("lhs", node->lhs());
        Indent(); WriteMapPair("rhs", node->rhs());
    }

    virtual void VisitTypeTest(TypeTest *node) override {
        // TODO:
    }

    virtual void VisitTypeCast(TypeCast *node) override {
        // TODO:
    }

    // i1: 1
    // i32: 100
    virtual void VisitSmiLiteral(SmiLiteral *node) override {
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

    virtual void VisitFloatLiteral(FloatLiteral *node) override {
        Write("i%d: ", node->bitwide());
        switch (node->bitwide()) {
            case 32:
                Write("%f\n", node->f32());
                break;
            case 64:
                Write("%f\n", node->f64());
                break;
        }
    }

    // string: literal
    virtual void VisitStringLiteral(StringLiteral *node) override {
        WriteMapPair("string", node->data()->c_str());
    }

    virtual void VisitArrayInitializer(ArrayInitializer *node) override {
        WriteMapPair("type", node->array_type()->Type::ToString().c_str());
        Indent(); WriteMapPair("elements", node->mutable_elements());
    }

    virtual void VisitElement(Element *node) override {
        WriteMapPair("value", node->value());
    }

    virtual void VisitMapInitializer(MapInitializer *node) override {
        WriteMapPair("type", node->map_type()->Type::ToString().c_str());
        Indent(); WriteMapPair("pairs", node->mutable_pairs());
    }

    virtual void VisitPair(Pair *node) override {
        WriteMapPair("key", node->key());
        Indent(); WriteMapPair("value", node->value());
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

    // expression:
    //   expr
    // arguments:
    //   - op: ADD
    //     lhs:
    //       i8: 1
    //     rhs:
    //       i8: 1
    //   - i8: -127
    virtual void VisitCall(Call *node) override {
        WriteMapPair("expression", node->expression());
        Indent(); WriteMapPair("arguments", node->mutable_arguments());
    }

    // expression:
    //   expr
    // field_name: name
    virtual void VisitFieldAccessing(FieldAccessing *node) override {
        WriteMapPair("expression", node->expression());
        Indent(); WriteMapPair("field_name", node->field_name()->c_str());
    }

    // target:
    //   symbol: ns:name
    // rval:
    //   i64: 1
    virtual void VisitAssignment(Assignment *node) override {
        WriteMapPair("taget", node->target());
        Indent(); WriteMapPair("rval", node->rval());
    }

    virtual void VisitIfOperation(IfOperation *node) override {
        WriteMapPair("if", node->condition());
        Indent(); WriteMapPair("then", node->then_statement());
        if (node->has_else()) {
            Indent(); WriteMapPair("else", node->else_statement());
        }
    }

    virtual void VisitForeachLoop(ForeachLoop *node) override {
        WriteMapPair("container", node->container());
        if (node->has_key()) {
            Indent(); WriteMapPair("key", node->key());
        }
        Indent(); WriteMapPair("value", node->value());
        Indent(); WriteMapPair("body", node->body());
    }

    virtual void VisitTypeMatch(TypeMatch *node) override {
        WriteMapPair("target", node->target());
        Indent(); WriteMapPair("cases", node->mutable_match_cases());
    }

    virtual void VisitTypeMatchCase(TypeMatchCase *node) override {
        if (node->is_else_case()) {
            WriteMapPair("else", node->body());
        } else {
            WriteMapPair("case", node->cast_pattern());
            Indent(); WriteMapPair("body", node->body());
        }
    }

    virtual void VisitReturn(Return *node) override {
        if (node->has_return_value()) {
            WriteMapPair("return", node->expression());
        } else {
            WriteMapPair("return", "void");
        }
    }

    virtual void VisitValDeclaration(ValDeclaration *node) override {
        WriteMapPair("declare_val", node->name()->c_str());
        Indent(); WriteMapPair("export", node->is_export() ? "yes" : "no");
        Indent(); WriteMapPair("type", node->type()->ToString().c_str());
        if (node->has_initializer()) {
            Indent(); WriteMapPair("init", node->initializer());
        }
    }

    virtual void VisitVarDeclaration(VarDeclaration *node) override {
        WriteMapPair("declare_var", node->name()->c_str());
        Indent(); WriteMapPair("export", node->is_export() ? "yes" : "no");
        Indent(); WriteMapPair("type", node->type()->ToString().c_str());
        if (node->has_initializer()) {
            Indent(); WriteMapPair("init", node->initializer());
        }
    }

    virtual void VisitVariable(Variable *node) override {
        auto name = node->scope()->MakeFullName(node->name());
        WriteMapPair("var", name.c_str());
        if (node->link()) {
            Indent(); WriteMapPair("link", node->link());
        }
    }

    virtual void VisitReference(Reference *node) override {
        VisitVariable(node->variable());
    }

    virtual void VisitBuiltinCall(BuiltinCall *node) override {
        WriteMapPair("code", "%d", node->code());
        WriteMapPair("arguments", node->mutable_arguments());
    }

    // block:
    //   - node 1
    //   - node 2
    //   - node 3
    virtual void VisitBlock(Block *node) override {
        if (node->mutable_body()->size() == 0) {
            WriteMapPair("block", "-EMPTY-");
        } else {
            WriteMapPair("block", node->mutable_body());
        }
    }

    virtual void VisitFunctionLiteral(FunctionLiteral *node) override {
        WriteMapPair("prototype", node->prototype()->Type::ToString().c_str());
        Indent(); WriteMapPair("assignment", node->is_assignment() ? "yes" : "no");
        if (node->up_values_size() > 0) {
            Indent(); WriteMapPair("up_values", node->mutable_up_values());
        }
        if (node->has_body()) {
            Indent(); WriteMapPair("body", node->body());
        }
    }

    virtual void VisitFunctionDefine(FunctionDefine *node) override {
        WriteMapPair("function_def", node->name()->c_str());
        Indent(); WriteMapPair("export", node->is_export() ? "yes" : "no");
        Indent(); WriteMapPair("native", node->is_native() ? "yes" : "no");
        Indent(); WriteMapPair("literal", node->function_literal());
    }

    template<class T>
    void WriteMapPair(const char *key, ZoneVector<T*> *array) {
        WriteMapPair(key, "");
        ++indent_;
        for (int i = 0; i < array->size(); i++) {
            WriteArrayElement(array->At(i));
        }
        --indent_;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(YamlPrinterVisitor)
private:
    void Write(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        stream_->Vprintf(fmt, ap);
        va_end(ap);
    }

    void WriteMapPair(const char *key, const char *fmt, ...) {
        stream_->Write(key);
        stream_->Write(": ");

        va_list ap;
        va_start(ap, fmt);
        stream_->Vprintf(fmt, ap);
        va_end(ap);
        stream_->Write("\n");
    }

    void WriteMapPair(const char *key, AstNode *node) {
        WriteMapPair(key, "");
        ++indent_;
        Indent(); node->Accept(this);
        --indent_;
    }

    void WriteArrayElement(AstNode *node) {
        Indent();
        stream_->Write("- ");
        ++indent_;
        node->Accept(this);
        --indent_;
    }

    void Indent() {
        std::string buf(indent_wide_ * indent_, ' ');
        stream_->Write(buf.data(), static_cast<int>(buf.size()));
    }

    TextOutputStream *stream_;
    int indent_wide_;
    int indent_ = 0;
};

} // namespace

/*static*/
void AstPrinter::ToYamlString(AstNode *ast, int indent_wide, std::string *buf) {
    MemoryOutputStream stream(buf);
    ToYamlString(ast, indent_wide, &stream);
}

/*static*/
void AstPrinter::ToYamlString(AstNode *ast, int indent_wide, TextOutputStream *stream) {
    YamlPrinterVisitor printer(stream, indent_wide);
    ast->Accept(&printer);
}

/*static*/ void AstPrinter::ToYamlString(ParsedUnitMap *all_units,
                                         int indent_wide,
                                         TextOutputStream *stream) {
    YamlPrinterVisitor printer(stream, indent_wide);
    ParsedUnitMap::Iterator iter(all_units);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        printer.WriteMapPair(iter->key()->c_str(), iter->value());
    }
}

/*static*/ void AstPrinter::ToYamlString(ParsedUnitMap *all_units,
                                         int indent_wide,
                                         std::string *buf) {
    MemoryOutputStream stream(buf);
    ToYamlString(all_units, indent_wide, &stream);
}

} // namespace mio
