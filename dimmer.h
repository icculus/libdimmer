/*
 * Include file for libdimmer.
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_DIMMER_H_
#define _INCLUDE_DIMMER_H_

#include "boolean.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DEVID_DADDYMAX   0
#define TOTAL_DEVICES    1


struct DimmerSystemInfo
{
    int devCount;
    int devsAvailable[TOTAL_DEVICES];
    int currentDevID;
};


struct DimmerDeviceInfo
{
    int numChannels;
    int numOutputs;
    int isDuplexed;
};


struct DimmerDeviceFunctions
{
    int (*queryExistence)(void);
    int (*queryDevice)(struct DimmerDeviceInfo *info);
    int (*initialize)(void);
    void (*deinitialize)(void);
    int (*channelSet)(int channel, int intensity);
    int (*setDuplexMode)(__boolean shouldSet);
};


int dimmer_init(int autoInit);
void dimmer_deinit(void);
int dimmer_device_available(int idNum);
int dimmer_select_device(int idNum);
int dimmer_query_system(struct DimmerSystemInfo *info);
int dimmer_query_device(struct DimmerDeviceInfo *info);
int dimmer_set_duplex_mode(int shouldSet);
int dimmer_channel_set(unsigned int channel, unsigned char intensity);
int dimmer_fade_channel(unsigned int chan, unsigned char level, double secs);
int dimmer_toggle_blackout(void);
int dimmer_channel_patch(int channel, int patchTo);
int dimmer_set_grand_master(int intensity);

#define dimmer_channel_bump(chan)  dimmer_channel_set(channel, 255)

#ifdef __cplusplus
}
#endif

#endif /* !defined _INCLUDE_DIMMER_H_ */

/* end of dimmer.h ... */

