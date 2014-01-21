#ifndef INCLUDED_FILE_H
#define INCLUDED_FILE_H

#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

extern int f_read(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int f_write(void *ptr, size_t size, size_t nmemb, FILE *stream);

#endif /* INCLUDED_FILE_H */
