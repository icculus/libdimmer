/*
 * Support code for "test" device.
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include "boolean.h"
#include "dimmer.h"


static struct DimmerDeviceInfo devInfo;
static int cons = -1;
static unsigned char *channelColor;
static int lines = 25;
static int columns = 80;


static void testdev_queryModName(char *buffer, int bufSize)
{
    strncpy(buffer, "testdev", bufSize);
    buffer[bufSize - 1] = '\0';  /* promises null termination. */
} /* testdev_queryModName */


static void testdev_updateDevice(unsigned char *levels)
{
    int max = devInfo.numChannels;
    int i;
    int j;
    unsigned char buffer[columns * 2];
    int starCount;

    buffer[0] = '[';
    buffer[1] = 7;
    buffer[(columns * 2) - 2] = ']';
    buffer[(columns * 2) - 1] = 7;

    for (i = 0; i < max; i++)
    {
        starCount = ((int) (((double) levels[i] / 255.0) * columns)) * 2;
        starCount += 2; /* add two since '[' is first char... */

        for (j = 2; j < (columns - 1) * 2; j += 2)
        {
            buffer[j] = ((j < starCount) ? '*' : ' ');
            buffer[j + 1] = channelColor[i];
        } /* for */

        lseek(cons, (i * (columns * 2)) + 4, SEEK_SET);
        write(cons, buffer, columns * 2);
    } /* for */

} /* testdev_updateDevice */


static int testdev_queryExistence(void)
{
    return(1);  /* always exists. */
} /* testdev_queryExistence */


static int testdev_queryDevice(struct DimmerDeviceInfo *info)
{
    printf("dev_test::query.\n");
    memcpy(info, &devInfo, sizeof (struct DimmerDeviceInfo));
    return(0);
} /* testdev_queryDevice */


static int setupConsole(char *tty)
{
    char consDev[strlen(tty) + 20];
    unsigned char br[4];
    unsigned char *buf;
    int retVal = -1;
    int i;
    int rc;

    if ( (strncmp("/dev/tty", tty, 8) == 0) && (isdigit(tty[8])) )
    {
        strcpy(consDev, "/dev/vcsa");
        strcat(consDev, tty + 8);       /* "/dev/tty" is 8 chars ... */

        rc = open(consDev, O_RDWR);
        if (rc != -1)
        {
            if (read(rc, &br, 4) != 4)
                close(rc);
            else
            {
                lines = br[0];
                columns = br[1];
                buf = malloc((lines * columns) * 2);
                if (buf == NULL)
                    close(rc);
                else
                {
                    memset(buf, '\0', (lines * columns) * 2);
                    for (i = 1; i < (lines * columns) * 2; i += 2)
                    {
                        buf[i] = ' ';
                        buf[i + 1] = 7;
                    } /* for */
                    lseek(rc, 3, SEEK_SET);
                    write(rc, buf, (lines * columns) * 2);
                    free(buf);
                    br[2] = 0;           /* br[2] is cursor X. */
                    br[3] = lines - 5;   /* br[3] is cursor Y */
                    lseek(rc, 0, SEEK_SET);
                    write(rc, br, 4);
                    retVal = rc;
                } /* else */
            } /* else */
        } /* if */
    } /* if */

    return(retVal);
} /* setupConsole */


static int testdev_initialize(void)
{
    int retVal = -1;
    char *tty = ttyname(STDOUT_FILENO);

    if (tty != NULL)
    {
        cons = setupConsole(tty);
        if (cons != -1)
        {
            memset(&devInfo, '\0', sizeof (struct DimmerDeviceInfo));
            devInfo.numChannels = (int) ((lines - 5) / 3);
            devInfo.numOutputs = 3;
            devInfo.isDuplexed = 0;

            channelColor = malloc(devInfo.numChannels);
            if (channelColor == NULL)
            {
                close(cons);
                cons = -1;
            } /* if */
            else
                retVal = 0;
        } /* if */
    } /* if */

    return(retVal);
} /* testdev_initialize */


static void testdev_deinitialize(void)
{
    close(cons);
    free(channelColor);
    channelColor = NULL;
} /* testdev_deinitialize */


static int testdev_channelSet(int channel, int intensity)
{
    int retVal = -1;

    if (channel < devInfo.numChannels)
    {
        channelColor[channel] = 7;   // !!! finish this.
        retVal = 0;
    } /* if */

    return(retVal);
} /* testdev_channelSet */


static int testdev_setDuplexMode(__boolean shouldSet)
{
    devInfo.numChannels = (int) ((lines - 5) / 3);

    if (shouldSet == __false)
        devInfo.isDuplexed = 0;
    else
    {
        devInfo.isDuplexed = 1;
        devInfo.numChannels *= 3;
    } /* else */

    return(0);
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

