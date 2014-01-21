#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "int.h"
#include "file.h"


/** FIXME: this is not portable and the interface is ugly - rewrite. **/
int f_read(void *outptr, size_t size, size_t nmemb, FILE *stream)
{
  unsigned char * tmp;
  size_t count;
  unsigned int num, pos;

  tmp = malloc(size*nmemb);
  if (tmp==NULL)
  {
    fprintf(stderr, "malloc failed\n");
    return 0;
  }

  count = fread(tmp, size, nmemb, stream);
  if (count!=nmemb)
    fprintf(stderr, "short read\n");
  pos = 0;
    
  switch (size)
  {
    default:
    case 1:
      for (num=0; num<count; num++)
      {
        ((unsigned char *)outptr)[num] = buff_to_uint8(tmp, &pos);
      }
      break;
    case 2:
      for (num=0; num<count; num++)
      {
        ((unsigned short *)outptr)[num] = buff_to_uint16(tmp, &pos);
      }
      break;
    case 4:
      for (num=0; num<count; num++)
      {
        ((unsigned long *)outptr)[num] = buff_to_uint32(tmp, &pos);
      }
      break;
  }

  free(tmp);
  return count;
}


/** FIXME: this is not portable and the interface is ugly - rewrite. **/
int f_write(void *inptr, size_t size, size_t nmemb, FILE *stream)
{
  unsigned int num, pos, count;
  unsigned char tmp[8];

  switch (size)
  {
    default:
    case 1:
      count = 0;
      for (num=0; num<nmemb; num++)
      {
        pos = 0;
        uint8_to_buff(((unsigned char *)inptr)[num], tmp, &pos);
        if (fwrite(tmp, size, 1, stream)!=1)
          break;
	count ++;
      }
      break;
      break;
    case 2:
      count = 0;
      for (num=0; num<nmemb; num++)
      {
        pos = 0;
        uint16_to_buff(((unsigned short *)inptr)[num], tmp, &pos);
        if (fwrite(tmp, size, 1, stream)!=1)
          break;
	count ++;
      }
      break;
    case 4:
      count = 0;
      for (num=0; num<nmemb; num++)
      {
        pos = 0;
        uint32_to_buff(((unsigned long *)inptr)[num], tmp, &pos);
        if (fwrite(tmp, size, 1, stream)!=1)
          break;
	count ++;
      }
      break;
  }

  if (count!=nmemb)
    fprintf(stderr, "short write\n");

  return count;
}
