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


struct DimmerSystemInfo
{
    int devCount;
    int *devsAvailable;
    int activeDevID;
};


struct DimmerDeviceInfo
{
    int numChannels;
    int numOutputs;
    int isDuplexed;
};


struct DimmerDeviceFunctions
{
    void (*queryModuleName)(char *buffer, int bufSize);
    int (*queryExistence)(void);
    int (*queryDevice)(struct DimmerDeviceInfo *info);
    int (*initialize)(void);
    void (*deinitialize)(void);
    int (*channelSet)(int channel, int intensity);
    int (*setDuplexMode)(__boolean shouldSet);
    void (*updateDevice)(unsigned char *levels);
};


void dimmer_deinit(void);
int dimmer_init(int autoInit);
int dimmer_device_available(char *devName, int *devID);
int dimmer_select_device(char *devName);
int dimmer_query_system(struct DimmerSystemInfo *info);
int dimmer_query_device(struct DimmerDeviceInfo *info);
int dimmer_set_duplex_mode(int shouldSet);
int dimmer_channel_set(unsigned int channel, unsigned char intensity);
int dimmer_fade_channel(unsigned int chan, unsigned char level, double secs);
int dimmer_toggle_blackout(void);
int dimmer_channel_patch(int channel, int patchTo);
int dimmer_set_grand_master(int intensity);

#define dimmer_channel_bump(chan)     dimmer_channel_set(channel, 255)
#define dimmer_channel_blackout(chan) dimmer_channel_set(channel, 0)

#ifdef __cplusplus
}
#endif

#endif /* !defined _INCLUDE_DIMMER_H_ */

/* end of dimmer.h ... */

