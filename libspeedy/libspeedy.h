/* The following functions may differ between Apache and Perl -- this library
 * library is usuable under both.
 */
typedef struct {
    /*
    void (*ls_memcpy)(void *d, void *s, int n);
    void (*ls_memmove)(void *d, void *s, int n);
    void (*ls_bzero)(void *s, int n);
    void *(*ls_malloc)(int n);
    void (*ls_free)(void *s);
    */
    void (*ls_memcpy)();
    void (*ls_memmove)();
    void (*ls_bzero)();
    void *(*ls_malloc)();
    void (*ls_free)();
} LS_funcs;

/*
 * The following global variable must be initialized by the calling program
 * before using any of the library functions
 */
extern LS_funcs speedy_libfuncs;

#include "speedy_libutil.h"
#include "speedy_queue.h"
#include "speedy_frontend.h"
