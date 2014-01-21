#include "config.h"

#ifndef WITH_SOUND
/* just give dummy functions. */
void snd_init(void) {}
void snd_fight(void) {}
void snd_attack(void) {}
void snd_gun(void) {}
void snd_para(void) {}
void snd_town(void) {}
void snd_march(void) {}
void snd_close(void) {}
#else

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
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif
#ifdef HAVE_LINUX_SOUNDCARD_H
# include <linux/soundcard.h>
#endif
#include "extern.h"
#include "options2.h"
#include "constant.h"

typedef unsigned char uchar;

static int dsp;
static int speed=22050;
static int stereo=1;
static int format=AFMT_S16_LE;
static pthread_mutex_t mtx_queue;
static pthread_t thread_snd;

static void snd_loop(void *args);
static void snd_playsamples(int length,short *samples);

struct snd_info {
  int length;
  short *samples;
};

#define CHANNELS 16
#define SND_EFFECT_FIGHT  0
#define SND_EFFECT_ATTACK 1
#define SND_EFFECT_GUN    2
#define SND_EFFECT_PARA   3
#define SND_EFFECT_TOWN   4
#define SND_EFFECT_MARCH  5
#define SND_EFFECT_CONSTRUCT 6
#define SND_EFFECT_CNT    7

struct snd_info snd_queue[CHANNELS];

/* preloaded samples */
struct snd_effect {
  int length;
  short *samples;  
  char *path;
};
struct snd_effect snd_effects[SND_EFFECT_CNT]={
  {-1,NULL,"cfx11.wav"},    /* fight */
  {-1,NULL,"plopp.wav"},    /* attack */
  {-1,NULL,"bang10.wav"},   /* gun */
  {-1,NULL,"plopp.wav"},    /* para */
  {-1,NULL,"gong10.wav"},   /* town */
  {-1,NULL,"foot3.wav"},    /* march */
  {-1,NULL,"cfx01.wav"},    /* march */
};

void snd_init(void) {
  int i,file,len,formats=0;
  short *tmp_data,*data;
  char str[256];

  if(!Config->enable_all[OPTION_SOUND]) return;
  dsp = open("/dev/dsp",O_WRONLY);
  if(dsp == -1) {
	printf("failed to open /dev/dsp... sounds disabled\n");
	Config->enable_all[OPTION_SOUND]=0;
	return;
  }
  /* setup soundcard */
  ioctl(dsp,SNDCTL_DSP_RESET,NULL);
  ioctl(dsp,SNDCTL_DSP_SPEED,&speed);
  ioctl(dsp,SNDCTL_DSP_STEREO,&stereo);
  ioctl(dsp,SNDCTL_DSP_GETFMTS,&formats);
  if(!(formats & format)) {
	printf("expected format (0x%x) not supported by soundcard\n",format);
	exit(-1);
  }
  ioctl(dsp,SNDCTL_DSP_SETFMT,&format);
  /* FIXME: check returned format to see if it is different */

  tmp_data=(short*)malloc(sizeof(short)*22050*2*10);
  for(i=0;i<SND_EFFECT_CNT;i++) {
	sprintf(str,"%s/%s",DEFAULT_SND_DIR,snd_effects[i].path);
	file=open(str,O_RDONLY);
	if(file == -1) {
	  printf("failed to open %s... sounds disabled\n",str);
	  Config->enable_all[OPTION_SOUND]=0;
	  free(tmp_data);
	  return;
	}
	len = read(file,tmp_data,sizeof(short)*22050*2*10) - 36;
	data=(short*)malloc(len);		
	memcpy(data,tmp_data+36/sizeof(short),len);
	memset(data,0,16);
	snd_effects[i].samples=data;
	snd_effects[i].length=len/sizeof(short);
	close(file);	
  }
  free(tmp_data);

  pthread_mutex_init(&mtx_queue,NULL);
  pthread_create(&thread_snd,NULL,(void*)&snd_loop,(void*)NULL);
}

static void snd_loop(void *args) {
  short *data;
  int len=2048; /* every fragment we play is this many samples */
  int i,channel;

  if(!Config->enable_all[OPTION_SOUND]) return;
  data = (short *) malloc(sizeof(short)*len);
  while(1) {
	memset((char*)data,0,sizeof(short)*len);
	pthread_mutex_lock(&mtx_queue);
	for(channel=0;channel<CHANNELS;channel++)
	  if(snd_queue[channel].length>0) {
		for(i=0;i<len && i<snd_queue[channel].length;i++)
		  data[i] += snd_queue[channel].samples[i]/2;
		snd_queue[channel].samples += len;
		snd_queue[channel].length -= len;
	  }
	pthread_mutex_unlock(&mtx_queue);
	write(dsp,data,len*sizeof(short));
  }
}

static void snd_playsamples(int length,short *samples) {
  int c;

  if(!Config->enable_all[OPTION_SOUND]) return;
  pthread_mutex_lock(&mtx_queue);
  for(c=0;c<CHANNELS;c++)
	if(snd_queue[c].length<=0) {
	  snd_queue[c].length=length;
	  snd_queue[c].samples=samples;
	  pthread_mutex_unlock(&mtx_queue);
	  return;
	}
  pthread_mutex_unlock(&mtx_queue);	
}

void snd_fight(void) {snd_playsamples(snd_effects[SND_EFFECT_FIGHT].length,snd_effects[SND_EFFECT_FIGHT].samples);}
void snd_attack(void) {snd_playsamples(snd_effects[SND_EFFECT_ATTACK].length,snd_effects[SND_EFFECT_ATTACK].samples);}
void snd_gun(void) {snd_playsamples(snd_effects[SND_EFFECT_GUN].length,snd_effects[SND_EFFECT_GUN].samples);}
void snd_para(void) {snd_playsamples(snd_effects[SND_EFFECT_PARA].length,snd_effects[SND_EFFECT_PARA].samples);}
void snd_town(void) {snd_playsamples(snd_effects[SND_EFFECT_TOWN].length,snd_effects[SND_EFFECT_TOWN].samples);}
void snd_march(void) {snd_playsamples(snd_effects[SND_EFFECT_MARCH].length,snd_effects[SND_EFFECT_MARCH].samples);}

void snd_close(void) {
  int i;

  if(!Config->enable_all[OPTION_SOUND]) return;

  close(dsp);
  for(i=0;i<SND_EFFECT_CNT;i++)
	free(snd_effects[i].samples);
}

#endif
