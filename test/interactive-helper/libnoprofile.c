#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>

int vprintf(const char *format, va_list ap) {
	static int (*func_fprintf)(const char *format, va_list ap) = NULL;
	if (func_fprintf == NULL)
		func_fprintf = (int (*) (const char *format, va_list ap)) dlsym(RTLD_NEXT, "vprintf");

    va_list ap2;
    va_copy(ap2, ap);
    if (strncmp(format, "profiling:", strlen("profiling:")) == 0)
        return 0;
    int res = func_fprintf(format, ap2);
    va_end(ap2);
    return res;
}
int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vprintf(format, ap);
    va_end(ap);
    return res;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
	static int (*func_vfprintf)(FILE *stream, const char *format, va_list ap) = NULL;
	if (func_vfprintf == NULL)
		func_vfprintf = (int (*) (FILE *stream, const char *format, va_list ap)) dlsym(RTLD_NEXT, "vfprintf");

    va_list ap2;
    va_copy(ap2, ap);
    if (strncmp(format, "profiling:", strlen("profiling:")) == 0)
        return 0;
    int res = func_vfprintf(stream, format, ap2);
    va_end(ap2);
    return res;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vfprintf(stream, format, ap);
    va_end(ap);
    return res;
}
