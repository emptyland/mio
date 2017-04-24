/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -L ANSI-C -C -N mio_parse_keyword -K z -t -c -n keywords.gperf  */
/* Computed positions: -k'1,4,$' */

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

#define TOTAL_KEYWORDS 41
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 110
/* maximum key range = 110, duplicates = 0 */

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
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
        6, 111,   1, 111,   7, 111,   2, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111,  50,   6,  31,
       30,   5,  40,  35,  35,  20, 111,  10,   0,  35,
        5,  30,  36, 111,  10,  45,   0,  40,   1,  11,
      111,   5, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
      111, 111, 111, 111, 111, 111
    };
  register unsigned int hval = 0;

  switch (len)
    {
      default:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

const struct mio_keyword *
mio_parse_keyword (register const char *str, register unsigned int len)
{
  static const struct mio_keyword wordlist[] =
    {
      {""},
#line 44 "keywords.gperf"
      {"val", TOKEN_VAL},
      {""}, {""}, {""},
#line 12 "keywords.gperf"
      {"not", TOKEN_NOT},
#line 17 "keywords.gperf"
      {"bool", TOKEN_BOOL},
      {""}, {""}, {""},
#line 18 "keywords.gperf"
      {"true", TOKEN_TRUE},
#line 45 "keywords.gperf"
      {"var", TOKEN_VAR},
      {""}, {""}, {""},
#line 37 "keywords.gperf"
      {"else", TOKEN_ELSE},
#line 38 "keywords.gperf"
      {"while", TOKEN_WHILE},
      {""}, {""}, {""},
#line 24 "keywords.gperf"
      {"int", TOKEN_INT},
#line 23 "keywords.gperf"
      {"i64", TOKEN_I64},
#line 20 "keywords.gperf"
      {"i8", TOKEN_I8},
      {""}, {""},
#line 40 "keywords.gperf"
      {"in", TOKEN_IN},
#line 22 "keywords.gperf"
      {"i32", TOKEN_I32},
#line 21 "keywords.gperf"
      {"i16", TOKEN_I16},
      {""}, {""},
#line 49 "keywords.gperf"
      {"native", TOKEN_NATIVE},
#line 34 "keywords.gperf"
      {"weak", TOKEN_WEAK},
      {""}, {""}, {""},
#line 50 "keywords.gperf"
      {"export", TOKEN_EXPORT},
#line 43 "keywords.gperf"
      {"continue", TOKEN_CONTINUE},
      {""}, {""}, {""},
#line 11 "keywords.gperf"
      {"or", TOKEN_OR},
#line 26 "keywords.gperf"
      {"f64", TOKEN_F64},
      {""}, {""}, {""},
#line 28 "keywords.gperf"
      {"error", TOKEN_ERROR_TYPE},
#line 25 "keywords.gperf"
      {"f32", TOKEN_F32},
      {""}, {""}, {""},
#line 39 "keywords.gperf"
      {"for", TOKEN_FOR},
#line 13 "keywords.gperf"
      {"package", TOKEN_PACKAGE},
      {""}, {""}, {""},
#line 41 "keywords.gperf"
      {"return", TOKEN_RETURN},
#line 47 "keywords.gperf"
      {"lambda", TOKEN_LAMBDA},
      {""}, {""}, {""},
#line 36 "keywords.gperf"
      {"if", TOKEN_IF},
#line 29 "keywords.gperf"
      {"void", TOKEN_VOID},
      {""}, {""}, {""},
#line 16 "keywords.gperf"
      {"is", TOKEN_IS},
#line 42 "keywords.gperf"
      {"break", TOKEN_BREAK},
      {""}, {""}, {""},
#line 48 "keywords.gperf"
      {"def", TOKEN_DEF},
#line 31 "keywords.gperf"
      {"map", TOKEN_MAP},
      {""}, {""}, {""},
#line 30 "keywords.gperf"
      {"union", TOKEN_UNION},
#line 46 "keywords.gperf"
      {"function", TOKEN_FUNCTION},
      {""}, {""}, {""},
#line 10 "keywords.gperf"
      {"and", TOKEN_AND},
#line 14 "keywords.gperf"
      {"with", TOKEN_WITH},
      {""}, {""}, {""},
#line 33 "keywords.gperf"
      {"struct", TOKEN_STRUCT},
      {""}, {""}, {""}, {""},
#line 19 "keywords.gperf"
      {"false", TOKEN_FALSE},
      {""}, {""}, {""}, {""},
#line 15 "keywords.gperf"
      {"as", TOKEN_AS},
      {""}, {""}, {""}, {""},
#line 27 "keywords.gperf"
      {"string", TOKEN_STRING},
      {""}, {""}, {""}, {""},
#line 32 "keywords.gperf"
      {"array", TOKEN_ARRAY},
      {""}, {""}, {""}, {""},
#line 35 "keywords.gperf"
      {"strong", TOKEN_STRONG}
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
