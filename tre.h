// Public Domain Tiny Regular Expressions Library
// Forked from https://github.com/kokke/tiny-regex-c
//
// Supports:
// ---------
//   '^'        Start anchor, matches start of string
//   '$'        End anchor, matches end of string
// ---------
//   '*'        Asterisk, match zero or more (greedy, *? lazy)
//   '+'        Plus, match one or more (greedy, +? lazy)
//   '{m,n}'    Quantifier, match min. 'm' and max. 'n' (greedy, {m,n}? lazy)
//   '{m}'                  exactly 'm'
//   '{m,}'                 match min 'm' and max. MAX_QUANT
//   '?'        Question, match zero or one (greedy, ?? lazy)
// ---------
//   '.'        Dot, matches any character except newline (\r, \n)
//   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
//   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'}
//   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
//   '\s'       Whitespace, \t \f \r \n \v and spaces
//   '\S'       Non-whitespace
//   '\w'       Alphanumeric, [a-zA-Z0-9_]
//   '\W'       Non-alphanumeric
//   '\d'       Digits, [0-9]
//   '\D'       Non-digits
//   '\X'       Character itself; X in [^sSwWdD] (e.g. '\\' is '\')
// ---------


#ifndef TRE_RE_H_INCLUDE
#define TRE_RE_H_INCLUDE

#ifndef TRE_STATIC
#define TRE_DEF extern
#else
#define TRE_DEF static
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TRE_MAX_NODES    64  // Max number of regex nodes in expression.
#define TRE_MAX_BUFLEN  128  // Max length of character-class buffer in.

//#define TRE_SILENT // disable inclusion of stdio and printing
//#define TRE_DOTANY // dot matches anything including newline

typedef struct tre_node tre_node;
typedef struct tre_comp tre_comp;

// 8 and 16 bytes on x86 and x86_64 resp.
struct tre_node
{
    unsigned char  type;
    union
    {
        unsigned char  ch;  // character itself
        unsigned char *ccl; // for class buffer and quantifier min max
        unsigned short mn[2];
    };
};

struct tre_comp
{
    tre_node nodes[TRE_MAX_NODES];
    unsigned char buffer[TRE_MAX_BUFLEN];
};

// Compile regex string pattern as tre_comp struct tregex
TRE_DEF int tre_compile(const char *pattern, tre_comp *tregex);

// Match tregex in text and return the match start or null if there is no match
// If end is not null set it to the match end
TRE_DEF const char *tre_match(const tre_comp *tregex, const char *text, const char **end);

// Same but compiles pattern then matches
TRE_DEF const char *tre_compile_match(const char *pattern, const char *text, const char **end);

// Print the pattern
TRE_DEF void tre_print(const tre_comp *tregex);

#ifdef __cplusplus
}
#endif

#endif // TRE_H_INCLUDE

//------------------------------------------------------------

#ifdef TRE_IMPLEMENTATION

#define TRE_MAXQUANT  1024  // Max b in {a,b}. must be < ushrt_max
#define TRE_MAXPLUS  40000  // For + and *,  > 32768 for test2

#define TRE_TYPES_X  X(NONE) X(BEGIN) X(END) \
        X(QUANT) X(LQUANT) X(QMARK) X(LQMARK) X(STAR) X(LSTAR) X(PLUS) X(LPLUS) \
        X(DOT) X(CHAR) X(CLASS) X(NCLASS) X(DIGIT) X(NDIGIT) X(ALPHA) X(NALPHA) X(SPACE) X(NSPACE)

#define X(A) TRE_##A,
enum { TRE_TYPES_X };
#undef X

#include "string.h"
#ifndef TRE_SILENT
#include "stdio.h"
#endif

static int tre_err(const char *msg)
{
#ifdef TRE_SILENT
    (void) msg;
#else
    fprintf(stderr, "%s\n", msg);
#endif
    return 0;
}

TRE_DEF const char *tre_compile_match(const char *pattern, const char *text, const char **end)
{
    tre_comp tregex = {0};
    if (!tre_compile(pattern, &tregex))
    {
        tre_err("Compiling pattern failed");
        return 0;
    }

    return tre_match(&tregex, text, end);
}

static const char *matchpattern(const tre_node *nodes, const char *text, const char *tend);

TRE_DEF const char *tre_nmatch(const tre_comp *tregex, const char *text, unsigned tlen, const char **end)
{
    if (!tregex || !text || !tlen)
    {
        tre_err("NULL text or tre_comp");
        return 0;
    }

    const char *tend = text + tlen;
    const char *mend;
    const tre_node *nodes = tregex->nodes;

    if (nodes->type == TRE_BEGIN)
    {
        mend = matchpattern(nodes + 1, text, tend);
        if (mend)
        {
            if (end) { *end = mend; }
            return text;
        }
        return 0;
    }

    do
    {
        mend = matchpattern(nodes, text, tend);
        if (mend)
        {
            //if (!*text) //Fixme: ???
            //    return 0;
            if (end) { *end = mend; }
            return text;
        }
    }
    while (tend > text++);

    return 0;
}

TRE_DEF const char *tre_match(const tre_comp *tregex, const char *text, const char **end)
{
    return tre_nmatch(tregex, text, strlen(text), end);
}

#define TRE_ISMETA(c) ((c=='s')||(c=='S')||(c=='w')||(c=='W')||(c=='d')||(c=='D'))
// s,S,w,W,d,D or esc
#define TRE_METAORESC(c) (TRE_ISMETA(c)||(c=='\\'))

//#define REQUIRE_SPACE(X, S) if(idx > TRE_MAX_BUFLEN - (X)) {return tre_err(S);}
TRE_DEF int tre_ncompile(const char *pattern, unsigned plen, tre_comp *tregex)
{
    if (!tregex || !pattern || !plen)
        return tre_err("NULL/empty string or tre_comp");

    tre_node *tnode = tregex->nodes;
    unsigned char *buf = tregex->buffer;
    unsigned char quable = 0; // is the last node quantifiable
    unsigned char rmax; // max char in a range

    int idx = 0;

    unsigned long val; // for parsing numbers in {m,n}
    unsigned i = 0;    // index into pattern
    unsigned j = 0;    // index into tnode

    while (i < plen && (j + 1 < TRE_MAX_NODES))
    {
        switch (pattern[i])
        {
        // Meta-characters
        case '^': quable = 0; tnode[j].type = TRE_BEGIN; break;
        case '$': quable = 0; tnode[j].type = TRE_END;   break;
        case '.': quable = 1; tnode[j].type = TRE_DOT;   break;
        case '*':
            if (quable == 0) { return tre_err("Non-quantifiable before *"); }
            quable = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LSTAR) : TRE_STAR; break;
        case '+':
            if (quable == 0) { return tre_err("Non-quantifiable before +"); }
            quable = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LPLUS) : TRE_PLUS; break;
        case '?':
            if (quable == 0) { return tre_err("Non-quantifiable before ?"); }
            quable = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LQMARK) : TRE_QMARK; break;

        // Escaped characters
        case '\\':
        {
            quable = 1;
            i++;
            // dangling?
            if (i >= plen) { return tre_err("Dangling \\"); }

            switch (pattern[i])
            {
            // Meta-character:
            case 'd': tnode[j].type = TRE_DIGIT;  break;
            case 'D': tnode[j].type = TRE_NDIGIT; break;
            case 'w': tnode[j].type = TRE_ALPHA;  break;
            case 'W': tnode[j].type = TRE_NALPHA; break;
            case 's': tnode[j].type = TRE_SPACE;  break;
            case 'S': tnode[j].type = TRE_NSPACE; break;

            // Not in [dDwWsS]
            default: tnode[j].type = TRE_CHAR; tnode[j].ch = pattern[i]; break;
            }
        } break;

        // Character class
        case '[':
        {
            quable = 1;

            // Look-ahead to determine if negated
            tnode[j].type = (pattern[i + 1] == '^') ? (i++, TRE_NCLASS) : TRE_CLASS;
            tnode[j].ccl = buf + idx;

            // Copy characters inside [..] to buffer
            while (pattern[++i] != ']' && i < plen)
            {
                if (pattern[i] == '\\')
                {
                    if (i + 1 >= plen)
                        return tre_err("Dangling \\ in class");

                    // needs escaping ?
                    if (TRE_METAORESC(pattern[i + 1]))
                    {
                        if (idx > TRE_MAX_BUFLEN - 3)
                            return tre_err("Buffer overflow at <esc>char in class");
                        buf[idx++] = pattern[i++];
                        buf[idx++] = pattern[i];
                        if (pattern[i + 1] != '\\')
                            continue;
                    }
                    else // skip esc
                    {
                        if (idx > TRE_MAX_BUFLEN - 2)
                            return tre_err("Buffer overflow at [esc]char in class");
                        buf[idx++] = pattern[++i];
                    }
                }
                else
                {
                    if (idx > TRE_MAX_BUFLEN - 2)
                        return tre_err("Buffer overflow at [esc]char in class");
                    buf[idx++] = pattern[i];
                }

                // check range
                if (pattern[i + 1] != '-' || i + 2 >= plen || pattern[i + 2] == ']')
                    continue;
                rmax = (pattern[i + 2] == '\\');
                if (rmax && (i + 3 >= plen || TRE_ISMETA(pattern[i + 3])))
                    continue;

                rmax = rmax ? pattern[i + 3] : pattern[i + 2];
                if (rmax < pattern[i])
                    return tre_err("Incorrect range in class");
                if (idx > TRE_MAX_BUFLEN - 2)
                    return tre_err("Buffer overflow at range - in class");
                buf[idx++] = pattern[++i]; // '-'
            }

            if (pattern[i] != ']')
                return tre_err("Non terminated class");
            // Nul-terminated string
            buf[idx++] = 0;
        } break;

        // Quantifier
        case '{':
        {
            if (quable == 0)
                return tre_err("Non-quantifiable before {m,n}");
            quable = 0;

            // Use a char for each min and max (<= 255)
            //if (idx > TRE_MAX_BUFLEN - 2)
            //    return tre_err("Buffer overflow for quantifier");
            i++;
            val = 0;
            do
            {
                if (i >= plen || pattern[i] < '0' || pattern[i] > '9')
                    return tre_err("Non-digit in quantifier min value");
                val = 10 * val + (pattern[i++] - '0');
            }
            while (pattern[i] != ',' && pattern[i] != '}');

            if (val > TRE_MAXQUANT)
                return tre_err("Quantifier min value too big");
            tnode[j].mn[0] = val;

            if (pattern[i] == ',')
            {
                if (++i >= plen)
                    return tre_err("Unexpected end of string in quantifier");
                if (pattern[i] == '}')
                {
                    val = TRE_MAXQUANT;
                }
                else
                {
                    val = 0;
                    while (pattern[i] != '}')
                    {
                        if (i >= plen || pattern[i] < '0' || pattern[i] > '9')
                            return tre_err("Non-digit in quantifier max value");
                        val = 10 * val + (pattern[i++] - '0');
                    }

                    if (val > TRE_MAXQUANT || val < tnode[j].mn[0])
                        return tre_err("Quantifier max value too big or less than min value");
                }
            }
            tnode[j].type = (i + 1 < plen && pattern[i + 1] == '?') ? (i++, TRE_LQUANT) : TRE_QUANT;
            tnode[j].mn[1] = val;
        } break;

        // Regular characters
        default: quable = 1; tnode[j].type = TRE_CHAR; tnode[j].ch = pattern[i]; break;
        }
        i++;
        j++;
    }
    // 'TRE_NONE' is a sentinel used to indicate end-of-pattern
    tnode[j].type = TRE_NONE;

    return 1;
}

TRE_DEF int tre_compile(const char *pattern, tre_comp *tregex)
{
    return tre_ncompile(pattern, strlen(pattern), tregex);
}

#define TRE_MATCHDIGIT(c) ((c >= '0') && (c <= '9'))
#define TRE_MATCHALPHA(c) ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))
#define TRE_MATCHSPACE(c) ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'))
#define TRE_MATCHALNUM(c) ((c == '_') || TRE_MATCHALPHA(c) || TRE_MATCHDIGIT(c))

static int matchmetachar(char c, char mc)
{
    switch (mc)
    {
    case 'd': return  TRE_MATCHDIGIT(c);
    case 'D': return !TRE_MATCHDIGIT(c);
    case 'w': return  TRE_MATCHALNUM(c);
    case 'W': return !TRE_MATCHALNUM(c);
    case 's': return  TRE_MATCHSPACE(c);
    case 'S': return !TRE_MATCHSPACE(c);
    default:  return (c == mc);
    }
}

// note: compiler makes sure that it is always esc + nonzero (sSwWdD\)
static int matchcharclass(char c, const unsigned char *str)
{
    unsigned char rmax;
    while (*str != '\0')
    {
        if (str[0] == '\\')
        {
            if (matchmetachar(c, str[1])) { return 1; }
            str += 2;
            if (TRE_ISMETA(*str))
                continue;
        }
        else
        {
            if (c == *str) { return 1; }
            str += 1;
        }

        if (*str != '-' || !str[1])
            continue;
        rmax = (str[1] == '\\');
        if (rmax && TRE_ISMETA(str[2]))
            continue;

        rmax = rmax ? str[2] : str[1];
        if (c >= str[-1] && c <= rmax) { return 1; }
        str++;

    }

    return 0;
}


#ifndef TRE_DOTANY
#define TRE_MATCHDOT(c)   ((c != '\n') && (c != '\r'))
#else
#define TRE_MATCHDOT(c)   (1)
#endif

static int matchone(const tre_node *tnode, char c)
{
    switch (tnode->type)
    {
    case TRE_CHAR:   return (tnode->ch == c);
    case TRE_DOT:    return  TRE_MATCHDOT(c);
    case TRE_CLASS:  return  matchcharclass(c, tnode->ccl);
    case TRE_NCLASS: return !matchcharclass(c, tnode->ccl);
    case TRE_DIGIT:  return  TRE_MATCHDIGIT(c);
    case TRE_NDIGIT: return !TRE_MATCHDIGIT(c);
    case TRE_ALPHA:  return  TRE_MATCHALNUM(c);
    case TRE_NALPHA: return !TRE_MATCHALNUM(c);
    case TRE_SPACE:  return  TRE_MATCHSPACE(c);
    case TRE_NSPACE: return !TRE_MATCHSPACE(c);
    default: return 0; // return tre_err("Stray ^ or $");
    }
}

#undef TRE_MATCHDIGIT
#undef TRE_MATCHALPHA
#undef TRE_MATCHSPACE
#undef TRE_MATCHALNUM
#undef TRE_MATCHDOT

static const char *matchquant_lazy(const tre_node *nodes, const char *text, const char *tend,
                                   unsigned min, unsigned max)
{
    const char *end;
    max = max - min + 1;
    while (min && text < tend && matchone(nodes, *text)) { text++; min--; }
    if (min) { return 0; }

    do
    {
        end = matchpattern(nodes + 2, text, tend);
        if (end) { return end; }
        max--;
    }
    while (max && text < tend && matchone(nodes, *text++));

    return 0;
}

static const char *matchquant(const tre_node *nodes, const char *text, const char *tend,
                              unsigned min, unsigned max)
{
    const char *end, *start = text;
    while (max && text < tend && matchone(nodes, *text)) { text++; max--; }

    while (text - start >= min)
    {
        end = matchpattern(nodes + 2, text--, tend);
        if (end) { return end; }
    }

    return 0;
}

// Iterative matching
static const char *matchpattern(const tre_node *nodes, const char *text, const char *tend)
{
    do
    {
        if (nodes[0].type == TRE_NONE)
        {
            return text;
        }
        if ((nodes[0].type == TRE_END) && nodes[1].type == TRE_NONE)
        {
            return (text == tend) ? text : 0;
        }

        switch (nodes[1].type)
        {
        case TRE_QMARK:
            return matchquant(nodes, text, tend, 0, 1);
        case TRE_LQMARK:
            return matchquant_lazy(nodes, text, tend, 0, 1);
        case TRE_QUANT:
            return matchquant(nodes, text, tend, nodes[1].mn[0], nodes[1].mn[1]);
        case TRE_LQUANT:
            return matchquant_lazy(nodes, text, tend, nodes[1].mn[0], nodes[1].mn[1]);
        case TRE_STAR:
            return matchquant(nodes, text, tend, 0, TRE_MAXPLUS);
        case TRE_LSTAR:
            return matchquant_lazy(nodes, text, tend, 0, TRE_MAXPLUS);
        case TRE_PLUS:
            return matchquant(nodes, text, tend, 1, TRE_MAXPLUS);
        case TRE_LPLUS:
            return matchquant_lazy(nodes, text, tend, 1, TRE_MAXPLUS);
            // default: break; // w/e
        }
    }
    while (text < tend && matchone(nodes++, *text++));

    return 0;
}

void tre_print(const tre_comp *tregex)
{
#ifdef TRE_SILENT
    (void) tregex;
#else
#define X(A) #A,
    static const char *tre_typenames[] = { TRE_TYPES_X };
#undef X

    if (!tregex)
    {
        printf("NULL compiled regex detected\n");
        return;
    }

    const tre_node *tnode = tregex->nodes;
    int i;
    for (i = 0; i < TRE_MAX_NODES; ++i)
    {
        if (tnode[i].type == TRE_NONE)
            break;

        printf("type: %s", tre_typenames[tnode[i].type]);
        if (tnode[i].type == TRE_CLASS || tnode[i].type == TRE_NCLASS)
        {
            printf(" \"%s\"", tnode[i].ccl);
        }
        else if (tnode[i].type == TRE_QUANT || tnode[i].type == TRE_LQUANT)
        {
            printf(" {%d,%d}", tnode[i].mn[0], tnode[i].mn[1]);
        }
        else if (tnode[i].type == TRE_CHAR)
        {
            printf(" '%c'", tnode[i].ch);
        }
        printf("\n");
    }
#endif // TRE_SILENT
}

#undef TRE_TYPES_X

#endif // TRE_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - MIT License
Copyright (c) 2018 kokke, monolifed
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
*/

