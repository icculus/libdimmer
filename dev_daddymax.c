/*
 * Support code for the DaddyMax DMX512 dongle.
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "boolean.h"
#include "dimmer.h"

static struct DimmerDeviceInfo devInfo;


static void daddymax_queryModName(char *buffer, int bufSize)
{
    printf("daddymax_queryModName(%p, %d)\n", buffer, bufSize);
    strncpy(buffer, "daddymax_kernel", bufSize);
    buffer[bufSize - 1] = '\0';  /* promises null termination. */
} /* daddymax_queryModName */


static void daddymax_updateDevice(unsigned char *levels)
{
    printf("daddymax_updateDevice(%p)\n", levels);
} /* daddymac_updateDevice */


static int daddymax_queryExistence(void)
{
    int retVal = 0;
    struct stat statInfo;

    printf("daddymax_queryExistence()\n");

    if (stat("/dev/daddymax", &statInfo) != -1)
    {
        if (statInfo.st_mode & S_IFCHR)  /* character device? */
            retVal = 1;
    } /* if */

    return(retVal);
} /* daddymax_queryExistence */


static int daddymax_queryDevice(struct DimmerDeviceInfo *info)
{
    printf("daddymax_queryDevice(%p)\n", info);
    memcpy(info, &devInfo, sizeof (struct DimmerDeviceInfo));
    return(0);
} /* daddymax_queryDevice */


static int daddymax_initialize(void)
{
    printf("daddymax_initialize()\n");
    memset(&devInfo, '\0', sizeof (struct DimmerDeviceInfo));

    //open("/dev/daddymax", O_RDWR);
    devInfo.numOutputs = 1;     /* !!! lose this later! */
    devInfo.numChannels = 512;
    devInfo.isDuplexed = 0;
    return(0);
} /* daddymax_initialize */


static void daddymax_deinitialize(void)
{
    printf("daddymax_deinitialize()\n");
} /* daddymax_deinitialize */


static int daddymax_channelSet(int channel, int intensity)
{
    printf("daddymax_channelSet(%d, %d)\n", channel, intensity);
    return(0);
} /* daddymax_channelSet */


static int daddymax_setDuplexMode(__boolean shouldSet)
{
    printf("daddymax_setDuplexMode(%s)\n", shouldSet ? "__true" : "__false");
    return(-1);
} /* setDuplexMode */


    /*
     * This struct is down here so I don't need
     *  prototypes of all these functions...
     */
struct DimmerDeviceFunctions daddymax_funcs =   {
                                                    daddymax_queryModName,
                                                    daddymax_queryExistence,
                                                    daddymax_queryDevice,
                                                    daddymax_initialize,
                                                    daddymax_deinitialize,
                                                    daddymax_channelSet,
                                                    daddymax_setDuplexMode,
                                                    daddymax_updateDevice
                                                };

/* end of dev_daddymax.c ... */

