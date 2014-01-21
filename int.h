#ifndef INCLUDED_INT_H
#define INCLUDED_INT_H

extern unsigned long buff_to_uint8(unsigned char const *buff, unsigned int *off);
extern unsigned long buff_to_uint16(unsigned char const *buff, unsigned int *off);
extern unsigned long buff_to_uint32(unsigned char const *buff, unsigned int *off);

extern long buff_to_sint8(unsigned char const *buff, unsigned int *off);
extern long buff_to_sint16(unsigned char const *buff, unsigned int *off);
extern long buff_to_sint32(unsigned char const *buff, unsigned int *off);

extern void uint8_to_buff(unsigned long val, unsigned char *buff, unsigned int *off);
extern void uint16_to_buff(unsigned long val, unsigned char *buff, unsigned int *off);
extern void uint32_to_buff(unsigned long val, unsigned char *buff, unsigned int *off);

extern void sint8_to_buff(long val, unsigned char *buff, unsigned int *off);
extern void sint16_to_buff(long val, unsigned char *buff, unsigned int *off);
extern void sint32_to_buff(long val, unsigned char *buff, unsigned int *off);

#endif /* INCLUDED_INT_H */
