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

/*
 * These elements are function pointers to other modules...
 */

static int dimmerLibInitialized = 0;
static struct DimmerDeviceFunctions *devFunctions[TOTAL_DEVICES] = {&daddymax_funcs};
static struct DimmerSystemInfo sysInfo = {0, {0}, -1};


static int checkForDevices(void)
/*
 * Internal function called from dimmer_init(): Check to see which
 *  of supported devices exist. Fill them into (sysInfo).
 *
 *     params : void.
 *    returns : total found devices.
 */
{
    int retVal = 0;
    int devID;

    for (devID = 0; devID < TOTAL_DEVICES; devID++)
    {
        if (devFunctions[devID]->queryExistence())
        {
            sysInfo.devsAvailable[retVal] = devID;
            retVal++;
        } /* if */
    } /* for */

    sysInfo.devCount = retVal;

    return(retVal);
} /* checkForDevices */


static int attemptAutoInit(void)
/*
 * Attempt to initialize every existing device until one is successful.
 *  This is called exclusively from dimmer_init(). Parts of (sysInfo) are
 *  set here.
 *
 *   params : void.
 *  returns : 0 on successful init of any device, -1 on fatal errors and
 *             when no available device can initialize. (errno) set on errors.
 *    errno : anything dimmer_select_device() can set.
 */
{
    int i;
    int retVal = -1;

    for (i = 0; (i < sysInfo.devCount) && (retVal == -1); i++)
        retVal = dimmer_select_device(sysInfo.devsAvailable[i]);

    return(retVal);
} /* attemptAutoInit */


int dimmer_init(int autoInit)
/*
 * This function should be called before any other function in
 *  libdimmer. This sets up some basic stuff.
 *
 *     params : autoInit == if not zero, select the first device that
 *                          initializes successfully as the default device.
 *                          if zero, no devices initializations are attempted
 *                          by this function, and the default device is set to
 *                          (-1) (none of the above). This function returns an
 *                          error if this flag is set and either no devices
 *                          exist or no devices initialize. No error is
 *                          returned if this flag is not set and no devices
 *                          are usable.
 *    returns : -1 on error, 0 on success. (errno) is set on error.
 *      errno : EPERM (trying to call init more than once).
 */
{
    if (dimmerLibInitialized)
    {
        errno = EPERM;
        return(-1);
    } /* if */

    if (checkForDevices() == -1)
    {
        errno = ENODEV;
        return(-1);
    } /* if */

    if (autoInit)
    {
        if (attemptAutoInit() == -1)
        {
            errno = ENODEV;
            return(-1);
        } /* if */
    } /* if */

    dimmerLibInitialized = 1;
    return(0);
} /* dimmer_init */


int dimmer_device_available(int idNum)
/*
 * In the name of code reuse, this function will check if
 *  a given device is available for use. This could be
 *  accomplished on the application level by calling
 *  dimmer_query_system(), and then checking against
 *  devsAvailable and devCount. But we'll make your life easy. :)
 *
 *     params : idNum == device id (defined in dimmer.h) to check
 *                        for availability.
 *    returns : zero if not available, non-zero if exists.
 */
{
    int i;
    int retVal = 0;

    for (i = 0; (i < sysInfo.devCount) && (!retVal); i++)
    {
        if (sysInfo.devsAvailable[i] == idNum)
            retVal = 1;
    } /* for */

    return(retVal);
} /* dimmer_device_available */


int dimmer_select_device(int idNum)
/*
 * Use this function to switch to a device other than the default.
 *  You can find out the default through the dimmer_query_system()
 *  call (returnValue.currentDevID). This function call is
 *  unnecessary if you use the autoInit feature of dimmer_init() and
 *  don't care what specific protocol you use.
 *
 *      params : idNum == device ID (defined in dimmer.h) to select.
 *     returns : -1 on error, 0 on success. errno set.
 *       errno : ENODEV (bogus device ID number).
 *               anything else device initialization chooses to set.
 */
{
    int retVal = -1;

    if (!dimmer_device_available(idNum))
        errno = ENODEV;
    else
    {
        if (devFunctions[idNum]->initialize() != -1)
        {
            if (sysInfo.currentDevID != -1)
                devFunctions[sysInfo.currentDevID]->deinitialize();
            sysInfo.currentDevID = idNum;
            retVal = 0;  /* success. */
        } /* if */
    } /* else */

    return(retVal);
} /* dimmer_select_device */


int dimmer_query_system(struct DimmerSystemInfo *info)
/*
 * Gets details on the system in relation to dimmer control.
 *
 *   params : *info == ptr to structure to fill with information.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 *    errno : EPERM (dimmer_init() was never called).
 */
{
    int retVal = 0;

    if (dimmerLibInitialized)
        memcpy(info, &sysInfo, sizeof (struct DimmerSystemInfo));
    else
    {
        retVal = -1;
        errno = EPERM;
    } /* else */

    return(retVal);
} /* dimmer_query_system */


int dimmer_query_device(struct DimmerDeviceInfo *info)
/*
 * Get information regarding the currently selected dimming device.
 *
 *   params : *info == ptr to structure to fill with information.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 *    errno : ENODEV (no dimmer device is selected).
 *            Whatever device function wants to set.
 */
{
    int id = sysInfo.currentDevID;
    int retVal = -1;

    if (id == -1)
        errno = ENODEV;
    else
        retVal = devFunctions[id]->queryDevice(info);

    return(retVal);
} /* dimmer_query_device */


int dimmer_set_duplex_mode(int shouldSet)
/*
 * Some dimming hardware has multiple outputs. If possible, this
 *  function call instructs that hardware on how to treat the
 *  outputs. This call can set the hardware to attempt to duplex
 *  (use two separate outlets to double the number of accessible
 *  channels) the outputs, or send the same signal down both.
 *
 * See the documentation for a better explanation of duplexing.
 *
 *     params : shouldSet == 1 to attempt duplexing, 0 to disable duplexing.
 *    returns : -1 if unsuccessful, 0 on success. (errno) set on error.
 *      errno : ENODEV  (no dimmer device is selected).
 *              ENOSYS  (device can't duplex).
 */
{
    int id = sysInfo.currentDevID;
    int retVal = -1;

    if (id == -1)
        errno = ENODEV;
    else
    {
        retVal = devFunctions[id]->setDuplexMode(shouldSet);
        if (retVal == -1)
            errno = ENOSYS;
    } /* else */

    return(retVal);
} /* dimmer_set_duplex_mode */



int dimmer_channel_set(int channel, int intensity)
/*
 * Set a specific channel to a specific intensity.
 *
 *   params : channel == see above.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 *    errno : ENODEV (no dimmer device is selected).
 *            Whatever device function wants to set.
 */
{
    int id = sysInfo.currentDevID;
    int retVal = -1;

    if (id == -1)
        errno = ENODEV;
    else
        retVal = devFunctions[id]->channelSet(channel, intensity);

    return(retVal);
} /* dimmer_channel_set */


int dimmer_fade_channel(int channel, int intensity, double seconds)
{
    /* !!! write this! */
    return(-1);
} /* dimmer_fade_channel */


int dimmer_crossfade_channels(int ch1, int lev1, int ch2, int lev2, double s)
{
    /* !!! write this! */
    return(-1);
} /* dimmer_crossfade_channels */


int dimmer_toggle_blackout(void)
/*
 * First call to this bumps all channels to zero.
 * Next call restores (by bump) all channels to where they where before
 *  the blackout. Any other attempts to modify a channel's intensity will
 *  be noted, but the change will not be made during blackout.
 *
 *    params : void.
 *   returns : always returns (0);
 */
{
    /* !!! write this! */
    return(0);
} /* dimmer_blackout */


int dimmer_set_grand_master(int intensity)
/*
 * The grand master is a dimmer for all other dimmers. If you set
 *  channel X to 100%, and the grand master to 80%, then channel X
 *  will really only have an intensity of 80%. If channel X is set to
 *  50% and the GM is at 80%, then channel X will really only have an
 *  intensity of 40%. This affects every channel.
 *
 *    params : intensity == level (0 to 100) to set GM to.
 *   returns : always returns (0);
 */
{
    /* !!! write this! */
    return(0);
} /* dimmer_set_grand_master */

/* End of dimmer.c ... */

