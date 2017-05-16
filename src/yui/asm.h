#ifndef YUI_ASM_ASM_H
#define YUI_ASM_ASM_H

#include "asm-inl.h"

typedef struct YILabel YILabel;

struct YILabel {
    int pos;
    int near_link_pos;
};

#if defined(__cplusplus)
extern "C" {
#endif

int YILabelPos(const YILabel *l);

#if defined(__cplusplus)
}
#endif

#endif // YUI_ASM_ASM_H
