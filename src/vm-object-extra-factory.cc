#include "vm-object-extra-factory.h"

namespace mio {

FunctionDebugInfo *
ObjectExtraFactory::CreateFunctionDebugInfo(RawStringRef unit_name,
                                            int trace_node_size,
                                            const std::vector<int> &p2p) {
    auto placement_size = static_cast<int>(sizeof(FunctionDebugInfo)
                                           + unit_name->size() + 1
                                           + p2p.size() * sizeof(int));

    static const auto p2p_offset =
        reinterpret_cast<intptr_t>(&static_cast<FunctionDebugInfo *>(0)->pc_to_position);

    auto p = static_cast<char *>(allocator_->Allocate(placement_size));
    auto info = reinterpret_cast<FunctionDebugInfo *>(p);
    info->trace_node_size = trace_node_size;
    info->pc_size = static_cast<int>(p2p.size());

    p += p2p_offset;
    memcpy(p, &p2p[0], p2p.size() * sizeof(int));

    p += p2p.size() * sizeof(int);
    info->file_name = p;
    memcpy(p, unit_name->c_str(), unit_name->size());
    p[unit_name->size()] = '\0';
    return info;
}

NativeCodeFragment *
ObjectExtraFactory::CreateNativeCodeFragment(NativeCodeFragment *next,
                                             void **index) {
    auto fragment = static_cast<NativeCodeFragment *>(allocator_->Allocate(sizeof(NativeCodeFragment)));
    fragment->next  = next;
    fragment->index = index;
    return fragment;
}

} // namespace mio
