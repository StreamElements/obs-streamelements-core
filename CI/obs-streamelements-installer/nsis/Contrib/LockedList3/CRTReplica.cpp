#include <Windows.h>
#include <errno.h>
#include <ctype.h>

extern "C" void* __cdecl memset(void *p, int c, size_t z)
{
	BYTE *pb = reinterpret_cast<BYTE*>(p);
	for(size_t i=0; i<z; ++i, ++pb)
		(*pb) = c;
	return p;
}

extern "C" void* __cdecl memcpy(void *pt, const void *ps, size_t z)
{
	BYTE *ptc = reinterpret_cast<BYTE*>(pt);
	const BYTE *psc = reinterpret_cast<const BYTE*>(ps);
	for(size_t i=0; i<z; ++i)
		(*ptc++) = (*psc++);
	return pt;
}

#define __ascii_tolower(c)      ( (((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c) )
#define __ascii_toupper(c)      ( (((c) >= 'a') && ((c) <= 'z')) ? ((c) - 'a' + 'A') : (c) )
#define __ascii_iswalpha(c)     ( ('A' <= (c) && (c) <= 'Z') || ( 'a' <= (c) && (c) <= 'z'))
#define __ascii_iswdigit(c)     ( '0' <= (c) && (c) <= '9')
#define __ascii_isspace(c)      ( (c) == ' ' || (c) == '\t')
#define __ascii_towlower(c)     ( (((c) >= L'A') && ((c) <= L'Z')) ? ((c) - L'A' + L'a') : (c) )
#define __ascii_towupper(c)     ( (((c) >= L'a') && ((c) <= L'z')) ? ((c) - L'a' + L'A') : (c) )

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
/*#include <limits.h>
#include <ctype.h>
#include <stdlib.h>*/

/*
 * Convert a unicode string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 *
 * @implemented
 */
unsigned long wcstoul(const wchar_t *nptr, wchar_t **endptr, int base)
{
  const wchar_t *s = nptr;
  unsigned long acc;
  int c;
  unsigned long cutoff;
  int neg = 0, any, cutlim;

  /*
  * See strtol for comments as to the logic used.
  */
  do {
    c = *s++;
  } while (__ascii_isspace(c));
  if (c == '-')
  {
    neg = 1;
    c = *s++;
  }
  else if (c == L'+')
    c = *s++;
  if ((base == 0 || base == 16) &&
    c == L'0' && (*s == L'x' || *s == L'X'))
  {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == L'0' ? 8 : 10;
  cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
  cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
  for (acc = 0, any = 0;; c = *s++)
  {
    if (__ascii_iswdigit(c))
      c -= L'0';
    else if (__ascii_iswalpha(c))
      c -= __ascii_towupper(c) ? L'A' - 10 : L'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
      any = -1;
    else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (any < 0)
  {
    acc = ULONG_MAX;
  }
  else if (neg)
    acc = -acc;
  if (endptr != 0)
    *endptr = any ? (wchar_t *)s - 1 : (wchar_t *)nptr;
  return acc;
}

//#define FL_UNSIGNED   1       /* strtoul called */
//#define FL_NEG        2       /* negative sign found */
//#define FL_OVERFLOW   4       /* overflow occured */
//#define FL_READDIGIT  8       /* we've read at least one correct digit */
//
//static unsigned long __cdecl strtoxl(const char *nptr, const char **endptr, int ibase, int flags)
//{
//        const char *p;
//        char c;
//        unsigned long number;
//        unsigned digval;
//        unsigned long maxval;
//
//        /* validation section */
//        if (endptr != NULL)
//        {
//            /* store beginning of string in endptr */
//            *endptr = (char *)nptr;
//        }
//		if(nptr == NULL)
//			return EINVAL;
//		if(!(ibase == 0 || (2 <= ibase && ibase <= 36)))
//			return EINVAL;
//
//        p = nptr;                       /* p is our scanning pointer */
//        number = 0;                     /* start with zero */
//
//        c = *p++;                       /* read char */
//        while ( __ascii_isspace((int)(unsigned char)c) )
//            c = *p++;               /* skip whitespace */
//
//        if (c == '-') {
//            flags |= FL_NEG;        /* remember minus sign */
//            c = *p++;
//        }
//        else if (c == '+')
//            c = *p++;               /* skip sign */
//
//        if (ibase < 0 || ibase == 1 || ibase > 36) {
//            /* bad base! */
//            if (endptr)
//                /* store beginning of string in endptr */
//                *endptr = nptr;
//            return 0L;              /* return 0 */
//        }
//        else if (ibase == 0) {
//            /* determine base free-lance, based on first two chars of
//               string */
//            if (c != '0')
//                ibase = 10;
//            else if (*p == 'x' || *p == 'X')
//                ibase = 16;
//            else
//                ibase = 8;
//        }
//
//        if (ibase == 0) {
//            /* determine base free-lance, based on first two chars of
//               string */
//            if (c != '0')
//                ibase = 10;
//            else if (*p == 'x' || *p == 'X')
//                ibase = 16;
//            else
//                ibase = 8;
//        }
//
//        if (ibase == 16) {
//            /* we might have 0x in front of number; remove if there */
//            if (c == '0' && (*p == 'x' || *p == 'X')) {
//                ++p;
//                c = *p++;       /* advance past prefix */
//            }
//        }
//
//        /* if our number exceeds this, we will overflow on multiply */
//        maxval = ULONG_MAX / ibase;
//
//
//        for (;;) {      /* exit in middle of loop */
//            /* convert c to value */
//            if ( __ascii_iswdigit((int)(unsigned char)c) )
//                digval = c - '0';
//            else if ( __ascii_iswalpha((int)(unsigned char)c) )
//                digval = __ascii_toupper(c) - 'A' + 10;
//            else
//                break;
//            if (digval >= (unsigned)ibase)
//                break;          /* exit loop if bad digit found */
//
//            /* record the fact we have read one digit */
//            flags |= FL_READDIGIT;
//
//            /* we now need to compute number = number * base + digval,
//               but we need to know if overflow occured.  This requires
//               a tricky pre-check. */
//
//            if (number < maxval || (number == maxval &&
//                        (unsigned long)digval <= ULONG_MAX % ibase)) {
//                /* we won't overflow, go ahead and multiply */
//                number = number * ibase + digval;
//            }
//            else {
//                /* we would have overflowed -- set the overflow flag */
//                flags |= FL_OVERFLOW;
//                if (endptr == NULL) {
//                    /* no need to keep on parsing if we
//                       don't have to return the endptr. */
//                    break;
//                }
//            }
//
//            c = *p++;               /* read next digit */
//        }
//
//        --p;                            /* point to place that stopped scan */
//
//        if (!(flags & FL_READDIGIT)) {
//            /* no number there; return 0 and point to beginning of
//               string */
//            if (endptr)
//                /* store beginning of string in endptr later on */
//                p = nptr;
//            number = 0L;            /* return 0 */
//        }
//        else if ( (flags & FL_OVERFLOW) ||
//                ( !(flags & FL_UNSIGNED) &&
//                  ( ( (flags & FL_NEG) && (number > -LONG_MIN) ) ||
//                    ( !(flags & FL_NEG) && (number > LONG_MAX) ) ) ) )
//        {
//            /* overflow or signed overflow occurred */
//            //errno = ERANGE;
//            if ( flags & FL_UNSIGNED )
//                number = ULONG_MAX;
//            else if ( flags & FL_NEG )
//                number = (unsigned long)(-LONG_MIN);
//            else
//                number = LONG_MAX;
//        }
//
//        if (endptr != NULL)
//            /* store pointer to char that stopped the scan */
//            *endptr = p;
//
//        if (flags & FL_NEG)
//            /* negate result if there was a neg sign */
//            number = (unsigned long)(-(long)number);
//
//        return number;                  /* done. */
//}
//
//extern "C" unsigned long __cdecl strtoul (const char *nptr, char **endptr, int ibase)
//{
//	return strtoxl(nptr, (const char **)endptr, ibase, FL_UNSIGNED);
//}
