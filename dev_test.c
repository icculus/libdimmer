/*
 * Support code for "test" device.
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


static void testdev_queryModName(char *buffer, int bufSize)
{
    strncpy(buffer, "testdev", bufSize);
    buffer[bufSize - 1] = '\0';  /* promises null termination. */
} /* testdev_queryModName */


static void testdev_updateDevice(unsigned char *levels)
{
    printf("testdev_updateDevice(%p)\n", levels);
} /* testdev_updateDevice */


static int testdev_queryExistence(void)
{
    int retVal = 0;
    struct stat statInfo;

    printf("testdev_queryExistence()\n");

    if (stat("/dev/testdev", &statInfo) != -1)
    {
        if (statInfo.st_mode & S_IFCHR)  /* character device? */
            retVal = 1;
    } /* if */

    return(retVal);
} /* testdev_queryExistence */


static int testdev_queryDevice(struct DimmerDeviceInfo *info)
{
    printf("testdev_queryDevice(%p)\n", info);
    memcpy(info, &devInfo, sizeof (struct DimmerDeviceInfo));
    return(0);
} /* testdev_queryDevice */


static int testdev_initialize(void)
{
    printf("testdev_initialize()\n");
    memset(&devInfo, '\0', sizeof (struct DimmerDeviceInfo));

    //open("/dev/testdev", O_RDWR);
    devInfo.numOutputs = 1;     /* !!! lose this later! */
    devInfo.numChannels = 512;
    devInfo.isDuplexed = 0;
    return(0);
} /* testdev_initialize */


static void testdev_deinitialize(void)
{
    printf("testdev_deinitialize()\n");
} /* testdev_deinitialize */


static int testdev_channelSet(int channel, int intensity)
{
    printf("testdev_channelSet(%d, %d)\n", channel, intensity);
    return(0);
} /* testdev_channelSet */


static int testdev_setDuplexMode(__boolean shouldSet)
{
    printf("testdev_setDuplexMode(%s)\n", shouldSet ? "__true" : "__false");
    return(-1);
} /* setDuplexMode */


    /*
     * This struct is down here so I don't need
     *  prototypes of all these functions...
     */
struct DimmerDeviceFunctions testdev_funcs =   {
                                                    testdev_queryModName,
                                                    testdev_queryExistence,
                                                    testdev_queryDevice,
                                                    testdev_initialize,
                                                    testdev_deinitialize,
                                                    testdev_channelSet,
                                                    testdev_setDuplexMode,
                                                    testdev_updateDevice
                                                };

/* end of dev_testdev.c ... */

