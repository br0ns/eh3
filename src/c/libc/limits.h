/* Copy'n'paste from glibc's limits.h. */
#ifndef _LIMITS_H
#define _LIMITS_H

/* Maximum length of any multibyte character in any locale.
   We define this value here since the gcc header does not define
   the correct value.  */
#define MB_LEN_MAX  16

#include <bits/wordsize.h>

/* These assume 8-bit `char's, 16-bit `short int's,
   and 32-bit `int's and `long int's.  */

/* Number of bits in a `char'.  */
# define CHAR_BIT  8

/* Minimum and maximum values a `signed char' can hold.  */
# define SCHAR_MIN (-128)
# define SCHAR_MAX 127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0.)  */
# define UCHAR_MAX 255

/* Minimum and maximum values a `char' can hold.  */
# ifdef __CHAR_UNSIGNED__
#  define CHAR_MIN 0
#  define CHAR_MAX UCHAR_MAX
# else
#  define CHAR_MIN SCHAR_MIN
#  define CHAR_MAX SCHAR_MAX
# endif

/* Minimum and maximum values a `signed short int' can hold.  */
# define SHRT_MIN  (-32768)
# define SHRT_MAX  32767

/* Maximum value an `unsigned short int' can hold.  (Minimum is 0.)  */
# define USHRT_MAX 65535

/* Minimum and maximum values a `signed int' can hold.  */
# define INT_MIN   (-INT_MAX - 1)
# define INT_MAX   2147483647

/* Maximum value an `unsigned int' can hold.  (Minimum is 0.)  */
# define UINT_MAX  4294967295U

/* Minimum and maximum values a `signed long int' can hold.  */
# if __WORDSIZE == 64
#  define LONG_MAX 9223372036854775807L
# else
#  define LONG_MAX 2147483647L
# endif
# define LONG_MIN  (-LONG_MAX - 1L)

/* Maximum value an `unsigned long int' can hold.  (Minimum is 0.)  */
# if __WORDSIZE == 64
#  define ULONG_MAX    18446744073709551615UL
# else
#  define ULONG_MAX    4294967295UL
# endif

# ifdef __USE_ISOC99

/* Minimum and maximum values a `signed long long int' can hold.  */
#  define LLONG_MAX    9223372036854775807LL
#  define LLONG_MIN    (-LLONG_MAX - 1LL)

/* Maximum value an `unsigned long long int' can hold.  (Minimum is 0.)  */
#  define ULLONG_MAX   18446744073709551615ULL

# endif /* ISO C99 */

#endif /* limits.h  */
