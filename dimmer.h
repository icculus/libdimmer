/*
 * Include file for libdimmer...
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_DIMMER_H_
#define _INCLUDE_DIMMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PROTID_DMX512   0

struct
{
             int devCount;
             int *devsAvailable;
             int currentDevID;
    unsigned int numChannels;
    unsigned int numCables;
    unsigned int maxStacks;
    unsigned int maxSubmasters;
} DimmerSystemInfo;

struct
{
    void (*queryExistence)(void);
    int (*initialize)(void);
    int (*deinitialize)(void);
    int (*channelSet)(int channel, int intensity);
} DeviceFunctions;

int dimmer_init(void);
int dimmer_select_protocol(unsigned int idNum);
int dimmer_query_system(struct DimmerSystemInfo *info);
int dimmer_channel_set(int channel, int intensity);
int dimmer_fade_channel(int channel, int intensity, double seconds);
int dimmer_blackout(void);
int dimmer_crossfade_channels(int ch1, int lev1, int ch2, int lev2, double s);

#define dimmer_channel_bump(chan)  dimmer_channel_set(channel, 100)


#if 0
int dimmer_create_stack(int stackid);
/*
 * This defines a cue stack, that is referenced by (stackid). If a stack
 *  already exists with this id, it is cleared and replaced.
 *  "Stack" is the term used on many boards, but it is more of a 
 *    queue, if you want to get technical.
 */

int dimmer_go_cue(int stackid, int cuenum);    // execute specified cue.

int dimmer_set_cue(int stackid, int cuenum !!!

int dimmer_set_scene(int sceneid, !!!);
int dimmer_set_submaster(!!!);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !defined _INCLUDE_DIMMER_H_ */

/* end of dimmer.h ... */

