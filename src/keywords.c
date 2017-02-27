/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -L ANSI-C -C -N mio_parse_keyword -K z -t -c -n keywords.gperf  */
/* Computed positions: -k'1,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "keywords.gperf"

#include "token-inl.h"
#include <string.h>
#line 5 "keywords.gperf"
struct mio_keyword {
	const char *z;
	int id;
};

#define TOTAL_KEYWORDS 29
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 0
#define MAX_HASH_VALUE 60
/* maximum key range = 61, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      18, 61, 25, 61, 28, 61, 30, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 15, 25, 16,
      23,  5, 10, 25, 13, 30, 61, 25,  0, 61,
      15,  8,  1, 61,  3, 28,  0, 30,  0,  3,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
      61, 61, 61, 61, 61, 61
    };
  return asso_values[(unsigned char)str[len - 1]] + asso_values[(unsigned char)str[0]];
}

const struct mio_keyword *
mio_parse_keyword (register const char *str, register unsigned int len)
{
  static const struct mio_keyword wordlist[] =
    {
#line 31 "keywords.gperf"
      {"val", TOKEN_VAL},
      {""}, {""},
#line 32 "keywords.gperf"
      {"var", TOKEN_VAR},
      {""},
#line 38 "keywords.gperf"
      {"export", TOKEN_EXPORT},
#line 13 "keywords.gperf"
      {"package", TOKEN_PACKAGE},
      {""},
#line 26 "keywords.gperf"
      {"while", TOKEN_WHILE},
      {""},
#line 25 "keywords.gperf"
      {"else", TOKEN_ELSE},
#line 11 "keywords.gperf"
      {"or", TOKEN_OR},
      {""},
#line 27 "keywords.gperf"
      {"for", TOKEN_FOR},
      {""},
#line 12 "keywords.gperf"
      {"not", TOKEN_NOT},
#line 14 "keywords.gperf"
      {"with", TOKEN_WITH},
      {""},
#line 28 "keywords.gperf"
      {"return", TOKEN_RETURN},
      {""},
#line 37 "keywords.gperf"
      {"native", TOKEN_NATIVE},
#line 30 "keywords.gperf"
      {"continue", TOKEN_CONTINUE},
      {""},
#line 35 "keywords.gperf"
      {"void", TOKEN_VOID},
      {""},
#line 33 "keywords.gperf"
      {"function", TOKEN_FUNCTION},
      {""}, {""},
#line 21 "keywords.gperf"
      {"f32", TOKEN_F32},
      {""},
#line 20 "keywords.gperf"
      {"int", TOKEN_INT},
      {""}, {""},
#line 34 "keywords.gperf"
      {"def", TOKEN_DEF},
      {""},
#line 22 "keywords.gperf"
      {"f64", TOKEN_F64},
      {""}, {""},
#line 10 "keywords.gperf"
      {"and", TOKEN_AND},
      {""},
#line 24 "keywords.gperf"
      {"if", TOKEN_IF},
      {""}, {""},
#line 15 "keywords.gperf"
      {"as", TOKEN_AS},
      {""},
#line 36 "keywords.gperf"
      {"union", TOKEN_UNION},
      {""}, {""},
#line 18 "keywords.gperf"
      {"i32", TOKEN_I32},
      {""},
#line 29 "keywords.gperf"
      {"break", TOKEN_BREAK},
      {""}, {""},
#line 23 "keywords.gperf"
      {"string", TOKEN_STRING},
      {""},
#line 19 "keywords.gperf"
      {"i64", TOKEN_I64},
      {""}, {""},
#line 17 "keywords.gperf"
      {"i16", TOKEN_I16},
      {""},
#line 16 "keywords.gperf"
      {"i8", TOKEN_I8}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].z;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
