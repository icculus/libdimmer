/*
 * Basic API functions for libdimmer...
 *
 *   Copyright (c) 1999 Lighting and Sound Technologies.
 *    Written by Ryan C. Gordon.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "dimmer.h"
#include "dev_daddymax.h"

#define DEVID_DADDYMAX   0
#define TOTAL_DEVICES    1


/*
 * These elements are function pointers to other modules...
 */

static int dimmerLibInitialized = 0;
static DimmerDeviceFunctions *devFunctions[TOTAL_DEVICES] = {daddymax_funcs};
static struct DimmerSystemInfo sysInfo;


static int checkForDevices(void)
/*
 * Internal function called from dimmer_init(): Check to see which
 *  of supported devices exist. Fill them into (sysInfo).
 *
 *     params : void.
 *    returns : total found devices on success, -1 on fatal error. (errno) set.
 */
{
    int retVal = 0;
    int devID;

    sysInfo.devsAvailable = calloc(TOTAL_DEVICES, sizeof (unsigned int));

    if (sysInfo.devsAvailable == NULL)
        retVal = -1;    /* bomb on malloc problems. errno already set. */

    else
    {
        for (devID = 0; devID < TOTAL_DEVICES; devID++)
        {
            if (devFunctions[devID]->queryExistence())
            {
                sysInfo.devsAvailable[retVal] = devID;
                retVal++;
            } /* if */
        } /* for */
        sysInfo.devCount = retVal;
    } /* else */

    return(retVal);
} /* checkForDevices */


static int attemptAutoInit(void)
/*
 * Attempt to initialize every existing device until one is successful.
 * This is called exclusively from dimmer_init(). Parts of (sysInfo) are set here.
 *
 *   params : void.
 *  returns : 0 on successful init of any device, -1 on fatal errors and
 *             when no available device can initialize. (errno) set on errors.
 */
{
    int i;
    int retVal = -1;

    for (i = 0; i < sysInfo.devCount; i++)
    {
    } /* for */

    return(retVal);
} /* attemptAutoInit */


int dimmer_init(int autoInit)
/*
 * This function should be called before any other function in
 *  libdimmer. This sets up some basic stuff.
 *
 *     params : autoInit == if not zero, select the first device that
 *                          initializes successfully as the default device.
 *                          if zero, no devices initializations are attempted by
 *                          this function, and the default device is set to (-1)
 *                          (none of the above). This function returns an error
 *                          if this flag is set and either no devices exist or
 *                          no devices initialize. No error is returned if this
 *                          flag is not set and no devices are usable.
 *    returns : -1 on error, 0 on success. (errno) is set on error.
 */
{
    if (dimmerLibInitialized)
    {
        errno = EPERM;
        return(-1);
    } /* if */

    memset(sysInfo, '\0', sizeof (struct DimmerSystemInfo));

    if (checkForDevices() == -1)
        return(-1);

    if (autoInit)
        attemptAutoInit();
    else
        currentDevID = -1;

    dimmerLibInitialized = 1;
} /* dimmer_init */


int dimmer_select_protocol(unsigned int idNum)
/*
 * Use this function to switch to a protocol other than the default.
 *  You can find out the default through the dimmer_query_system()
 *  call (returnValue.currentProtID)
 *
 */
{
} /* dimmer_select_protocol */

int dimmer_query_system(struct DimmerSystemInfo *info);
int dimmer_channel_set(int channel, int intensity);
int dimmer_fade_channel(int channel, int intensity, double seconds);
int dimmer_blackout(void);
int dimmer_crossfade_channels(int ch1, int lev1, int ch2, int lev2, double s);




/* End of dimmer.c ... */
