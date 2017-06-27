#ifndef MIO_TRACING_H_
#define MIO_TRACING_H_

#include "managed-allocator.h"
#include "base.h"
#include "glog/logging.h"

namespace mio {

#define DEFINE_TRACE_NODE(M) \
    M(FuncEntry)             \
    M(LoopEntry)             \
    M(LoopEdge)              \
    M(GuardTrue)             \
    M(GuardFalse)


#define DECLARE_TRACE_NODE(name) \
    virtual Kind kind() const override { return k##name; } \
    static name *cast(TraceNode *node) { \
        return DCHECK_NOTNULL(node)->kind() == k##name ? static_cast<name *>(node) : nullptr; \
    } \
    friend class TraceNodeFactory; \
    DISALLOW_IMPLICIT_CONSTRUCTORS(name)

class TraceTree;
class TraceNodeFactory;
class MIONormalFunction;

class TraceNode;
class FuncEntry;
class LoopEntry;
class LoopEdge;
class GuardTrue;
class GuardFalse;

struct TraceBoundle {
    int        pc;
    TraceNode *node;
};

class TraceRecord {
public:
    TraceRecord(ManagedAllocator *allocator);
    ~TraceRecord();

    bool ResizeRecord(int tree_size);

    bool TraceFuncEntry(MIONormalFunction *fn, int pc);
    bool TraceLoopEntry(MIONormalFunction *fn, int id, int pc);
    bool TraceLoopEdge(MIONormalFunction *fn, int linked_id, int id, int pc);
    bool TraceGuardTrue(MIONormalFunction *fn, bool value, int id, int pc);
    bool TraceGuardFalse(MIONormalFunction *fn, bool value, int id, int pc);

    TraceBoundle *GetTraceBoundle(MIONormalFunction *fn, int id);

    DISALLOW_IMPLICIT_CONSTRUCTORS(TraceRecord)
private:
    struct TreeBoundle {
        MIONormalFunction *fn;
        TraceTree         *tree;
    };

    TreeBoundle      *trees_ = nullptr;
    int               tree_size_ = 0;
    TraceNodeFactory *factory_;
}; // class TraceRecord


class TraceTree {
public:
    TraceTree(int node_size) : node_size_(node_size) {}
    ~TraceTree() = default;

    bool Init(ManagedAllocator *allocator);
    void Finialize(ManagedAllocator *allocator);

    inline void InsertTail(int id, TraceNode *node);

    inline TraceBoundle *mutable_node(int i) {
        DCHECK_GE(i, 0);
        DCHECK_LT(i, node_size_);
        return &nodes_[i];
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(TraceTree)
private:
    TraceBoundle *nodes_     = nullptr;
    int           node_size_ = 0;
    TraceNode    *head_      = nullptr;
    TraceNode    *last_      = nullptr;
}; // class TraceTree


class TraceNode {
public:
    enum Kind {
#define DEFINE_ENUM(name) k##name,
        DEFINE_TRACE_NODE(DEFINE_ENUM)
#undef  DEFINE_ENUM
    };

    DEF_GETTER(int, pc)
    DEF_PTR_PROP_RW(TraceNode, next)

    virtual Kind kind() const = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(TraceNode)
protected:
    TraceNode(int pc) : pc_(pc) {}

private:
    int pc_;
    TraceNode *next_ = nullptr;
}; // class TraceNode


class FuncEntry : public TraceNode {
public:
    DEF_GETTER(int, hit)

    void IncrHit() { ++hit_; }

    DECLARE_TRACE_NODE(FuncEntry)
private:
    FuncEntry(int pc) : TraceNode(pc) {}

    int hit_ = 1;
}; // class FuncEntry


class LoopEntry : public TraceNode {
public:
    DEF_GETTER(int, id)
    DEF_GETTER(int, hit)

    void IncrHit() { ++hit_; }

    DECLARE_TRACE_NODE(LoopEntry)
private:
    LoopEntry(int id, int pc) : TraceNode(pc), id_(id) {}

    int id_;
    int hit_ = 1;
}; // class LoopEntry


class LoopEdge : public TraceNode {
public:
    DEF_PTR_GETTER(LoopEntry, entry)

    DECLARE_TRACE_NODE(LoopEdge)
private:
    LoopEdge(LoopEntry *entry, int pc) : TraceNode(pc), entry_(entry) {}

    LoopEntry *entry_;
}; // class LoopEdge


template<bool V>
class GuardNode : public TraceNode {
public:
    DEF_GETTER(int, hit)
    DEF_GETTER(int, pass)

    void IncrHit() { ++hit_; }

    void IncrPass() { ++pass_; }

    int count() const { return hit_ + pass_; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(GuardNode)
protected:
    GuardNode(int pc) : TraceNode(pc) {}

private:
    int hit_ = 0;
    int pass_ = 0;
}; // class GuardNode


class GuardTrue : public GuardNode<true> {
public:
    DECLARE_TRACE_NODE(GuardTrue)
private:
    GuardTrue(int pc) : GuardNode<true>(pc) {}
}; // class GuardTrue


class GuardFalse : public GuardNode<false> {
public:
    DECLARE_TRACE_NODE(GuardFalse)
private:
    GuardFalse(int pc) : GuardNode<false>(pc) {}
}; // class GuardFalse


class TraceNodeFactory {
public:
    TraceNodeFactory(ManagedAllocator *allocator)
        : allocator_(DCHECK_NOTNULL(allocator)) {}

    DEF_PTR_GETTER(ManagedAllocator, allocator)

    TraceTree *CreateTraceTree(int trace_node_size) {
        auto chunk = allocator_->Allocate(sizeof(TraceTree));
        if (!chunk) {
            return nullptr;
        }
        auto tree = new (chunk) TraceTree(trace_node_size);
        if (!tree->Init(allocator_)) {
            return nullptr;
        }
        return tree;
    }

    FuncEntry *CreateFuncEntry(int pc) {
        auto chunk = allocator_->Allocate(sizeof(FuncEntry));
        return !chunk ? nullptr : new (chunk) FuncEntry(pc);
    }

    LoopEntry *CreateLoopEntry(int id, int pc) {
        auto chunk = allocator_->Allocate(sizeof(LoopEntry));
        return !chunk ? nullptr : new (chunk) LoopEntry(id, pc);
    }

    LoopEdge *CreateLoopEdge(LoopEntry *entry, int pc) {
        auto chunk = allocator_->Allocate(sizeof(LoopEdge));
        return !chunk ? nullptr : new (chunk) LoopEdge(entry, pc);
    }

    GuardTrue *CreateGuardTrue(int pc) {
        auto chunk = allocator_->Allocate(sizeof(GuardTrue));
        return !chunk ? nullptr : new (chunk) GuardTrue(pc);
    }

    GuardFalse *CreateGuardFalse(int pc) {
        auto chunk = allocator_->Allocate(sizeof(GuardFalse));
        return !chunk ? nullptr : new (chunk) GuardFalse(pc);
    }
private:
    ManagedAllocator *allocator_;
}; // class TraceNodeFactory


inline void TraceTree::InsertTail(int id, TraceNode *node) {
    DCHECK(nodes_[id].node == nullptr);
    nodes_[id].node = DCHECK_NOTNULL(node);
}

} // namespace mio

#endif // MIO_TRACING_H_
