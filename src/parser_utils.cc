#include <stdint.h>
#include <ctype.h>
#include <assert.h>

static bool IsDigit(char ch, int base) {
    if (base <= 10) {
        return ch >= '0' && ch <= '0' + base - 1;
    }

    char hi_char = 'A' + (base - 11), lo_char = hi_char + 32;
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= hi_char) ||
        (ch >= 'a' && ch <= lo_char);
}

static int ToDigit(char ch, int base) {
    if (ch <= '9') {
        return ch - '0';
    }
    if (ch <= 'Z') {
        return ch - 'A';
    }
    if (ch <= 'z') {
        return ch - 'a';
    }
    assert(!"not a valid digit!");
    return -1;
}

extern "C" int ParseIntegral64(const char *s, int base, int64_t *value) {
    int sign = 0;
    if (s == NULL || *s == '\0') {
        return -1;
    }
    if (base <= 1) {
        return -1;
    }

    if (*s == '-') {
        sign = 1;
        if (*(++s) == '\0') {
            return -1;
        }
    } else if (*s == '+') {
        sign = 0;
        if (*(++s) == '\0') {
           return -1;
        }
    } else if (IsDigit(*s, base)) {
        sign = 0;
    }

    int64_t n = 0;
    while (*s) {
        char ch = *s;
        if (!IsDigit(ch, base)) {
            return -1;
        }
        n *= base;
        n += ToDigit(ch, base);
        ++s;
    }

    *value = n;
    return 0;
}
