#ifndef YUI_ASM_ASM_INL_H
#define YUI_ASM_ASM_INL_H

#define YI_LABEL_INIT() {.pos = 0, .near_link_pos = 0}

#define YILabelIsBound(l)  ((l)->pos < 0)
#define YILabelIsUnused(l) ((l)->pos == 0 && (l)->near_link_pos == 0)
#define YILabelIsLinked(l) ((l)->pos > 0)

#define YILabelIsNearLinked(l) ((l)->near_link_pos > 0)
#define YILabelNearLinkPos(l)  ((l)->near_link_pos - 1)

#define YILabelBindTo(l, for_bind) do { \
    (l)->pos = -(for_bind) - 1; \
    assert(YILabelIsBound(l)); \
} while (0)

#define YILabelLinkTo(l, for_link, is_far) do { \
    if (!(is_far)) { \
        (l)->near_link_pos = (for_link) + 1; \
        assert(YILabelIsNearLinked(l)); \
    } else { \
        (l)->pos = (for_link) + 1; \
        assert(YILabelIsLinked(l)); \
    } \
} while(0)

#endif // YUI_ASM_ASM_INL_H