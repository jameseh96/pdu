/**
 * Something (presumably a dep) has been compiled -D_FORTIFY_SOURCE=1 and
 * expects to find `__snprintf_chk`.
 *
 * If using musl (e.g., on alpine linux) this is not present, and will build but
 * fail to link when loaded:
 *
 * >>> import pypdu
 * Traceback (most recent call last):
 *  File "<stdin>", line 1, in <module>
 * ImportError: Error relocating
 * /pdu/build/lib.linux-x86_64-3.8/pypdu.cpython-38-x86_64-linux-gnu.so:
 * __snprintf_chk: symbol not found
 *
 */

#include <cstdarg>

extern "C" {

/* Write formatted output into S, according to the format
   string FORMAT, writing no more than MAXLEN characters.  */
int __snprintf_chk(char* s,
                   size_t maxlen,
                   int flag,
                   size_t slen,
                   const char* format,
                   ...) {
    if (slen < maxlen) {
        throw 1;
    }
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = (vsnprintf)(s, maxlen, format, ap);
    va_end(ap);
    return ret;
}
}
