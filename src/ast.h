// mio = Akiyama Mio
#ifndef MIO_AST_H_
#define MIO_AST_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"
#include "token.h"

namespace mio {

#define DEFINE_STATEMENT_NODES(M)  \
    M(PackageImporter)

#define DEFINE_AST_NODES(M)        \
    DEFINE_STATEMENT_NODES(M)

#define DECLARE_AST_NODE(name)                                               \
    virtual void Accept(AstVisitor *v) override;                             \
    virtual AstNode::Type type() const override { return AstNode::k##name; } \


class PackageImporter;
class Expression;

class AstVisitor;

class AstNode : public ManagedObject {
public:
#define AstNode_Types_ENUM(node) k##node,
    enum Type {
        DEFINE_AST_NODES(AstNode_Types_ENUM)
        kInvalid = -1,
    };
#undef AstNode_Types_ENUM

    AstNode(int position) : position_(position) {}

    virtual void Accept(AstVisitor *visitor) = 0;
    virtual Type type() const = 0;

    DEF_GETTER(int, position)

#define AstNode_TYPE_ASSERT(node)                                      \
    bool Is##node() const { return type() == k##node; }                \
    node *As##node() {                                                 \
        return Is##node() ? reinterpret_cast<node *>(this) : nullptr;  \
    }                                                                  \
    const node *As##node() const {                                     \
        return Is##node() ? reinterpret_cast<const node *>(this) : nullptr; \
    }
    DEFINE_AST_NODES(AstNode_TYPE_ASSERT)
#undef AstNode_TYPE_ASSERT

    DISALLOW_IMPLICIT_CONSTRUCTORS(AstNode);
private:
    int position_;
}; // class AstNode;

class Statement : public AstNode {
public:
    Statement(int position) : AstNode(position) {}

    DISALLOW_IMPLICIT_CONSTRUCTORS(Statement)
}; // class Statement

class PackageImporter : public Statement {
public:
    PackageImporter(int position, Zone *zone)
        : Statement(position)
        , import_list_(zone) {}

    RawStringRef package_name() const { return package_name_; }

    DECLARE_AST_NODE(PackageImporter)
    DISALLOW_IMPLICIT_CONSTRUCTORS(PackageImporter)
private:
    RawStringRef package_name_ = RawString::kEmpty;
    ZoneHashMap<RawStringRef, RawStringRef> import_list_;

}; // class PackageImporter



////////////////////////////////////////////////////////////////////////////////
// AstVisitor
////////////////////////////////////////////////////////////////////////////////
class AstVisitor {
public:
#define AstVisitor_VISIT_METHOD(name) virtual void Visit##name(name *) = 0;
    DEFINE_AST_NODES(AstVisitor_VISIT_METHOD)
#undef AstVisitor_VISIT_METHOD

    DISALLOW_IMPLICIT_CONSTRUCTORS(AstVisitor)
}; // class AstVisitor

} // namespace mio

#endif // MIO_AST_H_
