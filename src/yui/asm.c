#include "asm.h"
#include <assert.h>

int YILabelPos(const YILabel *l) {
    if (l->pos < 0)
        return -l->pos - 1;
    if (l->pos > 0)
        return l->pos - 1;
    assert(!"noreached!");
    return 0;
}
