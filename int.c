#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "int.h"


/******************************************************************************
  In order to make compatible binary files, the byte order will be
  be Most Significant Byte first.  Supported types are 1, 2, or 4 byte
  signed or unsigned integers.  Since C types may be larger or smaller, we
  use longs for everything (thus, we do not support systems with longs less
  than 32 bits).  Signed values are sent in 2's complement format.  1's
  complement and sign/value systems won't be able to handle the full range
  of values.  The conversion code tries to be correct on all systems by
  using addition/subtraction (avoiding overflows) instead of bit operations
  on signed values.
******************************************************************************/


unsigned long
buff_to_uint8(unsigned char const *buff, unsigned int *off)
{
  unsigned long val;

  val = buff[*off+0];
  *off += 1;

  return val;
}


unsigned long
buff_to_uint16(unsigned char const *buff, unsigned int *off)
{
  unsigned long val;

  val = (buff[*off+0]<<8) |
	 buff[*off+1];
  *off += 2;

  return val;
}


unsigned long
buff_to_uint32(unsigned char const *buff, unsigned int *off)
{
  unsigned long val;

  val = (buff[*off+0]<<24) |
	(buff[*off+1]<<16) |
	(buff[*off+2]<< 8) |
	 buff[*off+3];
  *off += 4;

  return val;
}

/*********************************/

long
buff_to_sint8(unsigned char const *buff, unsigned int *off)
{
  unsigned long tmp;
  long val;

  tmp = buff[*off+0];
  *off += 1;

  if (tmp>0x80UL)
  {
    tmp -= 0x81UL;
    val = ((long)tmp)-0x7f;
  }
  else if (tmp==0x80UL)
    val = -0x80; /* FIXME: should be -0x7f on one's complement... use INT_MIN?  but int can be any size! */
  else
    val = tmp;

  return val;
}


long
buff_to_sint16(unsigned char const *buff, unsigned int *off)
{
  unsigned long tmp;
  long val;

  tmp = (buff[*off+0]<<8) |
	 buff[*off+1];
  *off += 2;

  if (tmp>0x8000UL)
  {
    tmp -= 0x8001UL;
    val = ((long)tmp)-0x7fff;
  }
  else if (tmp==0x80UL)
    val = -0x8000; /* FIXME: should be -0x7f on one's complement... use INT_MIN?  but int can be any size! */
  else
    val = tmp;

  return val;
}


long
buff_to_sint32(unsigned char const *buff, unsigned int *off)
{
  unsigned long tmp;
  long val;

  tmp = (buff[*off+0]<<24) |
        (buff[*off+1]<<16) |
        (buff[*off+2]<< 8) |
         buff[*off+3];
  *off += 4;

  if (tmp>0x8000000UL)
  {
    tmp -= 0x80000001UL;
    val = ((long)tmp)-0x7fffffff;
  }
  else if (tmp==0x80000000UL)
    val = -0x80000000; /* FIXME: should be -0x7f on one's complement... use INT_MIN?  but int can be any size! */
  else
    val = tmp;

  return val;
}

/*********************************/

void
uint8_to_buff(unsigned long val, unsigned char *buff, unsigned int *off)
{
  buff[*off+0] = val&0xff;
  *off += 1;
}


void
uint16_to_buff(unsigned long val, unsigned char *buff, unsigned int *off)
{
  buff[*off+0] = (val>>8)&0xff;
  buff[*off+1] =  val    &0xff;
  *off += 2;
}


void
uint32_to_buff(unsigned long val, unsigned char *buff, unsigned int *off)
{
  buff[*off+0] = (val>>24)&0xff;
  buff[*off+1] = (val>>16)&0xff;
  buff[*off+2] = (val>> 8)&0xff;
  buff[*off+3] =  val     &0xff;
  *off += 4;
}

/*********************************/

void
sint8_to_buff(long val, unsigned char *buff, unsigned int *off)
{
  unsigned long tmp;

  if (val<0)
  {
    tmp = val+0x7f;
    tmp += 0x81UL;
  }
  else
    tmp = val;

  buff[*off+0] = tmp&0xff;
  *off += 1;
}


void
sint16_to_buff(long val, unsigned char *buff, unsigned int *off)
{
  unsigned long tmp;

  if (val<0)
  {
    tmp = val+0x7fff;
    tmp += 0x8001UL;
  }
  else
    tmp = val;

  buff[*off+0] = (tmp>>8)&0xff;
  buff[*off+1] =  tmp    &0xff;
  *off += 2;
}


void
sint32_to_buff(long val, unsigned char *buff, unsigned int *off)
{
  unsigned long tmp;

  if (val<0)
  {
    tmp = val+0x7fffffff;
    tmp += 0x80000001UL;
  }
  else
    tmp = val;

  buff[*off+0] = (tmp>>24)&0xff;
  buff[*off+1] = (tmp>>16)&0xff;
  buff[*off+2] = (tmp>> 8)&0xff;
  buff[*off+3] =  tmp     &0xff;
  *off += 4;
}

