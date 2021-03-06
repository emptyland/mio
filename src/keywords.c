/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/gperf -L ANSI-C -C -N mio_parse_keyword -K z -t -c -n keywords.gperf  */
/* Computed positions: -k'1-4' */

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

#define TOTAL_KEYWORDS 52
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 0
#define MAX_HASH_VALUE 125
/* maximum key range = 126, duplicates = 0 */

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
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126,  60,
        1,   0,  41, 126,  55,  50,  11, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126,  30, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126,  10,  25,   1,
       15,   5,   5, 126,   0,   0,  50, 126,   5,   1,
       35,   1,  10, 126,  30,   0,  30,   6,  40,   2,
       51,  41, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
      126, 126, 126, 126, 126, 126, 126
    };
  register unsigned int hval = 0;

  switch (len)
    {
      default:
        hval += asso_values[(unsigned char)str[3]+1];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

const struct mio_keyword *
mio_parse_keyword (register const char *str, register unsigned int len)
{
  static const struct mio_keyword wordlist[] =
    {
#line 16 "keywords.gperf"
      {"is", TOKEN_IS},
#line 26 "keywords.gperf"
      {"i32", TOKEN_I32},
      {""},
#line 44 "keywords.gperf"
      {"while", TOKEN_WHILE},
      {""},
#line 42 "keywords.gperf"
      {"if", TOKEN_IF},
#line 29 "keywords.gperf"
      {"f32", TOKEN_F32},
      {""}, {""}, {""},
#line 15 "keywords.gperf"
      {"as", TOKEN_AS},
#line 24 "keywords.gperf"
      {"i8", TOKEN_I8},
      {""}, {""}, {""},
#line 43 "keywords.gperf"
      {"else", TOKEN_ELSE},
      {""},
#line 54 "keywords.gperf"
      {"lambda", TOKEN_LAMBDA},
      {""}, {""},
#line 36 "keywords.gperf"
      {"slice", TOKEN_SLICE},
#line 35 "keywords.gperf"
      {"map", TOKEN_MAP},
#line 40 "keywords.gperf"
      {"weak", TOKEN_WEAK},
      {""}, {""},
#line 55 "keywords.gperf"
      {"def", TOKEN_DEF},
#line 13 "keywords.gperf"
      {"package", TOKEN_PACKAGE},
      {""},
#line 17 "keywords.gperf"
      {"bool", TOKEN_BOOL},
      {""},
#line 60 "keywords.gperf"
      {"delete", TOKEN_DELETE},
#line 11 "keywords.gperf"
      {"or", TOKEN_OR},
#line 14 "keywords.gperf"
      {"with", TOKEN_WITH},
      {""}, {""},
#line 47 "keywords.gperf"
      {"in", TOKEN_IN},
#line 45 "keywords.gperf"
      {"for", TOKEN_FOR},
      {""}, {""}, {""},
#line 59 "keywords.gperf"
      {"add", TOKEN_ADD},
      {""}, {""},
#line 50 "keywords.gperf"
      {"continue", TOKEN_CONTINUE},
      {""},
#line 58 "keywords.gperf"
      {"len", TOKEN_LEN},
#line 33 "keywords.gperf"
      {"void", TOKEN_VOID},
      {""}, {""}, {""},
#line 19 "keywords.gperf"
      {"false", TOKEN_FALSE},
#line 34 "keywords.gperf"
      {"union", TOKEN_UNION},
      {""}, {""}, {""},
#line 51 "keywords.gperf"
      {"val", TOKEN_VAL},
#line 46 "keywords.gperf"
      {"match", TOKEN_MATCH},
      {""}, {""}, {""},
#line 10 "keywords.gperf"
      {"and", TOKEN_AND},
#line 53 "keywords.gperf"
      {"function", TOKEN_FUNCTION},
      {""}, {""}, {""},
#line 28 "keywords.gperf"
      {"int", TOKEN_INT},
#line 12 "keywords.gperf"
      {"not", TOKEN_NOT},
      {""}, {""}, {""},
#line 41 "keywords.gperf"
      {"strong", TOKEN_STRONG},
#line 18 "keywords.gperf"
      {"true", TOKEN_TRUE},
      {""}, {""}, {""},
#line 32 "keywords.gperf"
      {"error", TOKEN_ERROR_TYPE},
#line 57 "keywords.gperf"
      {"export", TOKEN_EXPORT},
      {""}, {""}, {""},
#line 52 "keywords.gperf"
      {"var", TOKEN_VAR},
#line 22 "keywords.gperf"
      {"inf32", TOKEN_INF32},
      {""}, {""}, {""},
#line 49 "keywords.gperf"
      {"break", TOKEN_BREAK},
#line 61 "keywords.gperf"
      {"typeof", TOKEN_TYPEOF},
      {""}, {""}, {""},
#line 23 "keywords.gperf"
      {"inf64", TOKEN_INF64},
#line 39 "keywords.gperf"
      {"external", TOKEN_EXTERNAL},
      {""}, {""}, {""},
#line 37 "keywords.gperf"
      {"array", TOKEN_ARRAY},
#line 27 "keywords.gperf"
      {"i64", TOKEN_I64},
      {""}, {""}, {""},
#line 38 "keywords.gperf"
      {"struct", TOKEN_STRUCT},
#line 30 "keywords.gperf"
      {"f64", TOKEN_F64},
      {""}, {""}, {""},
#line 48 "keywords.gperf"
      {"return", TOKEN_RETURN},
      {""}, {""}, {""}, {""},
#line 31 "keywords.gperf"
      {"string", TOKEN_STRING},
#line 20 "keywords.gperf"
      {"NaN32", TOKEN_NAN32},
      {""}, {""}, {""},
#line 25 "keywords.gperf"
      {"i16", TOKEN_I16},
      {""}, {""}, {""}, {""},
#line 21 "keywords.gperf"
      {"NaN64", TOKEN_NAN64},
      {""}, {""}, {""}, {""},
#line 56 "keywords.gperf"
      {"native", TOKEN_NATIVE}
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
