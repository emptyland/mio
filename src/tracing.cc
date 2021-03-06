#include "tracing.h"
#include "vm-objects.h"

namespace mio {

TraceRecord::TraceRecord(ManagedAllocator *allocator)
    : factory_(new TraceNodeFactory(allocator)) {
}

TraceRecord::~TraceRecord() {
    for (int i = 0; i < tree_size_; ++i) {
        if (trees_[i].tree) {
            trees_[i].tree->Finialize(factory_->allocator());
            factory_->allocator()->Free(trees_[i].tree);
        }
    }
    factory_->allocator()->Free(trees_);
    delete factory_;
}

bool TraceRecord::ResizeRecord(int new_tree_size) {
    auto old_trees = trees_;
    trees_ = static_cast<TreeBoundle *>(factory_->allocator()->Allocate(new_tree_size * sizeof(TreeBoundle)));
    if (!trees_) {
        return false;
    }
    memset(trees_, 0, new_tree_size * sizeof(TreeBoundle));

    auto moved_size = std::min(new_tree_size, tree_size_);
    if (moved_size) {
        memcpy(trees_, old_trees, moved_size * sizeof(TreeBoundle));
    }
    tree_size_ = new_tree_size;
    factory_->allocator()->Free(old_trees);
    return true;
}

bool TraceRecord::TraceFuncEntry(MIOGeneratedFunction *fn, int pc) {
    DCHECK_GE(fn->GetId(), 0);
    DCHECK_LT(fn->GetId(), tree_size_);

    if (!fn->GetDebugInfo()) {
        return true;
    }

    auto fn_idx = fn->GetId();
    if (!trees_[fn_idx].tree) {
        auto tree = factory_->CreateTraceTree(fn->GetDebugInfo()->trace_node_size);
        if (!tree) {
            return false;
        }
        trees_[fn_idx].tree = tree;
        trees_[fn_idx].fn   = fn;
    }

    auto boundle = trees_[fn_idx].tree->mutable_node(0);
    if (boundle->node) {
        DCHECK_NOTNULL(FuncEntry::cast(boundle->node))->IncrHit();
    } else {
        boundle->node = factory_->CreateFuncEntry(pc);
    }
    return boundle->node != nullptr;
}

bool TraceRecord::TraceLoopEntry(MIOGeneratedFunction *fn, int id, int pc, int *hit) {
    auto boundle = GetTraceBoundle(fn, id);
    if (!boundle) {
        return true;
    }
    if (boundle->node) {
        DCHECK_NOTNULL(LoopEntry::cast(boundle->node))->IncrHit();
    } else {
        boundle->node = factory_->CreateLoopEntry(id, pc);
    }
    if (hit && boundle->node) {
        *hit = LoopEntry::cast(boundle->node)->hit();
    }
    return boundle->node != nullptr;
}

bool TraceRecord::TraceLoopEdge(MIOGeneratedFunction *fn, int linked_id, int id,
                                int pc) {
    auto boundle = GetTraceBoundle(fn, id);
    if (!boundle) {
        return true;
    }
    if (!boundle->node) {
        auto linked = trees_[fn->GetId()].tree->mutable_node(linked_id);
        auto entry  = DCHECK_NOTNULL(LoopEntry::cast(linked->node));
        boundle->node = factory_->CreateLoopEdge(entry, pc);
    }
    return boundle->node != nullptr;
}

bool TraceRecord::TraceGuardTrue(MIOGeneratedFunction *fn, bool value, int id, int pc) {
    auto boundle = GetTraceBoundle(fn, id);
    if (!boundle) {
        return true;
    }
    if (boundle->node) {
        if (value) {
            DCHECK_NOTNULL(GuardTrue::cast(boundle->node))->IncrHit();
        } else {
            DCHECK_NOTNULL(GuardTrue::cast(boundle->node))->IncrPass();
        }
    } else {
        boundle->node = factory_->CreateGuardTrue(pc);
    }
    return boundle->node != nullptr;
}

bool TraceRecord::TraceGuardFalse(MIOGeneratedFunction *fn, bool value, int id, int pc) {
    auto boundle = GetTraceBoundle(fn, id);
    if (!boundle) {
        return true;
    }
    if (boundle->node) {
        if (value) {
            DCHECK_NOTNULL(GuardFalse::cast(boundle->node))->IncrPass();
        } else {
            DCHECK_NOTNULL(GuardFalse::cast(boundle->node))->IncrHit();
        }
    } else {
        boundle->node = factory_->CreateGuardFalse(pc);
    }
    return boundle->node != nullptr;
}

TraceBoundle *TraceRecord::GetTraceBoundle(MIOGeneratedFunction *fn, int id) {
    DCHECK_GE(fn->GetId(), 0);
    DCHECK_LT(fn->GetId(), tree_size_);

    if (!fn->GetDebugInfo()) {
        return nullptr;
    }
    return DCHECK_NOTNULL(trees_[fn->GetId()].tree)->mutable_node(id);
}

TraceTree *TraceRecord::GetTraceTreeOrNull(MIOGeneratedFunction *fn) {
    DCHECK_GE(fn->GetId(), 0);
    DCHECK_LT(fn->GetId(), tree_size_);

    if (!fn->GetDebugInfo()) {
        return nullptr;
    }
    return trees_[fn->GetId()].tree;
}

bool TraceTree::Init(ManagedAllocator *allocator) {
    DCHECK_GT(node_size_, 0);
    nodes_ = static_cast<TraceBoundle *>(allocator->Allocate(node_size_ * sizeof(TraceBoundle)));
    if (!nodes_) {
        return false;
    }

    memset(nodes_, 0, node_size_ * sizeof(TraceBoundle));
    return true;
}

void TraceTree::Finialize(ManagedAllocator *allocator) {
    for (int i = 0; i < node_size_; ++i) {
        if (nodes_[i].node) {
            allocator->Free(nodes_[i].node);
        }
    }
}

} // namespace mio
