/*
 * Basic API functions for libdimmer...
 *
 *   Copyright (c) 1999 Lighting and Sound Technologies.
 *    Written by Ryan C. Gordon.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include "boolean.h"
#include "dimmer.h"
#include "dev_daddymax.h"


struct
{
    __boolean fadeActive;                       /* are we fading this one?      */
    unsigned int channel;
    unsigned char destinationLevel;       /* What level should it end at? */
    struct timeval nextFadeTime;
    struct timeval fadeTimeIncrement;
    int levelChangeRate;
    struct ChannelFadeStatus *next;
} ChannelFadeStatus;


static __boolean dimmerLibInitialized = 0;

static struct DimmerSystemInfo sysInfo = {0, {0}, -1};
static struct DimmerDeviceInfo devInfo;

static volatile __boolean threadLiveFlag = 1;
static pthread_t fadeThread = -1;
static pthread_mutex_t fadeLock = -1;
static struct ChannelFadeStatus *fadeList = NULL;

static unsigned char *rawLevels = NULL;
static int *patchTable = NULL;

static unsigned char grandMasterLevel = 255;
static unsigned __boolean blackOutEnabled = 0;

static int deviceProcess = 0;

/* internal prototype... */
static void dimmer_deinit(void);



static inline __boolean isPastTime(struct timeval *t1, struct timeval *t2)
/*
 * Determine if (t1) is past the time recorded in (t2).
 *
 *     params : t1, t2 == times to compare.
 *    returns : __true if (t1) is greater than or EQUAL to (t2).
 *              __false otherwise.
 */
{
    if (t1.tv_sec > t2.tv_sec)
        return(__true);
    else if (t1.tv_sec < t2.tv_sec)
        return(__false);
    else /* equal */
        return((t1.tv_usec >= t2.tv_usec) ? __true : __false);
} /* isPastTime */


static inline void updateChannelFade(struct ChannelFadeStatus *list)
/*
 * Update a ChannelFadeStructure. Make actual changes to dimmers.
 *
 *    params : list == struct to update.
 *   returns : void.
 */
{
    change = rawLevels[list->channel] + list->levelChangeRate;
    list->nextFadeTime += list->fadeTimeIncrement;

        /* Make sure we don't go past destination level... */
    if ((list->levelChangeRate < 0)
    {
        if (change < list->destinationLevel)
            change = list->destinationLevel;
    } /* if */
    else
    {
        if (change > list->destinationLevel)
            change = list->destinationLevel;
    } /* else */

    if (change = list->destinationLevel)        /* done with this one? */
        list->activeFlag = __false;

    dimmer_set_channel(list->channel, change);   /* do actual update. */
} /* updateChannelFade */


static inline __boolean runFadeList(struct ChannelFadeStatus *list)
/*
 * Run through the entire pending list of fades once.
 *
 *    params : list == list to run through.
 *   returns : 0 if there were no fades. 1 if there was at least one.
 */
{
    __boolean retVal = __false;
    int change;
    struct timeval currentTime;

    for (list = fadeList; list != NULL; list = list->next)
    {
        if (list->fadeActive)
        {
            gettimeofday(&currentTime, NULL);
            if (isPastTime(currentTime, nextFadeTime))
            {
                updateChannelFade(list);
                retVal = __true;
            } /* if */
        } /* if */
    } /* for */

    return(retVal);
} /* runFadeList */


static void *fadeThreadEntry(void *args)
/*
 * Entry point for fadeThread.
 *
 *    params : args == always (NULL).
 *   returns : Always (NULL). (terminates thread.)
 */
{
    __boolean atLeastOneFade = __false;
    struct ChannelFadeStatus *list;

    while (threadLiveFlag)  /* Thread lives until dimmer_deinit()... */
    {
        if (pthread_mutex_lock(&fadeLock) == 0)
        {
            atLeastOneFade = runFadeList(list);
            pthread_mutex_unlock(&fadeLock);
        } /* if */

        if (!atLeastOneFade)      /* Only hog CPU while fades are active. */
            sched_yield();
    } /* while */

    return(NULL);
} /* fadeThreadEntry */


static int spinThreads(void)
/*
 * Spin all the threads this library needs.
 *
 *   params : void.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 */
{
    int retVal = -1;
    pthread_attr_t attrs;

    if (!threadLiveFlag)
    {
        threadLiveFlag = 1;
        pthread_attr_init(&attrs);
        pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
        retVal = pthread_create(&fadeThread, &attrs, comThreadEntry, NULL);
        pthread_attr_destroy(&attrs);
    } /* if */

    return(retVal);
} /* spinThreads */


static void killThreads(void)
/*
 * Use this function to terminate the library's internal threads.
 *  Blocks until threads see flag and terminate.
 *
 *    params : void.
 *   returns : void.
 */
{
    if (threadLiveFlag)
    {
        threadLiveFlag = 0;

        pthread_join(fadeThread);
        fadeThread = -1;
    } /* if */
} /* killThreads */


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
        if (devProcess_queryExistence(devID))
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
 *              EAGAIN (threads wouldn't spin.)
 */
{
    if (dimmerLibInitialized)
    {
        errno = EPERM;
        return(-1);
    } /* if */

    if (createDeviceProcess() == -1)
        return(-1);

    pthread_mutex_init(&fadeLock, NULL);
    
    atexit(dimmer_deinit);

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

    dimmerLibInitialized = __true;
    return(0);
} /* dimmer_init */


void dimmer_deinit(void)
/*
 * This is called atexit() to clean up, but for flexibility, we
 *  set everything back to its default state.
 *
 *    params : void.
 *   returns : void.
 */
{
    if (dimmerLibInitialized)
    {
        killThreads();

        pthread_mutex_destroy(&fadeLock);
        fadeLock = -1;

        if (curDevFuncs != NULL)
        {
            curDevFuncs->deinitialize();
            curDevFuncs = NULL;
        } /* if */

        if (rawLevels != NULL)
        {
            free(rawLevels);
            rawLevels = NULL;
        } /* if */

        grandMasterLevel = 255;
        blackOutEnabled = __false;
        memset(sysInfo, '\0', sizeof (struct DimmerSystemInfo));
        sysInfo.currentDevID = -1;
        dimmerLibInitialized = __false;
        destroyDeviceProcess();
    } /* if */
} /* dimmer_deinit */


static int resize_channel_buffers(void)
/*
 * Make channel buffers match the amount of channels supported
 *  by the dimmer device. This is needed after a change in duplexing
 *  or after selecting a dimming device.
 *
 *     params : void.
 *    returns : -1 on error, 0 on success. (errno) set on error.
 *      errno : EAGAIN (thread problems.)
 *              ENOMEM (couldn't allocate new buffers.)
 *              ENODEV (No device selected.)
 */
{
    killThreads();  /* threads can't be checking a buffer while we resize. */

    rawLevels = realloc(rawLevels, sizeof (unsigned char) *
                           devInfo.numChannels);
    patchTable = realloc(patchTable, sizeof (int) * devInfo.numChannels);

    if ((rawLevels == NULL) || (patchTable == NULL))
        return(-1);

    memset(rawLevels, '\0', sizeof (unsigned char) * devInfo.numChannels);

    for (i = 0; i < devInfo.numChannels; i++)
        patchTable[i] = i;

    if (spinThreads() == -1)  /* restart buffer scanners... */
        return(-1);

    return(0);
} /* resize_channel_buffers */


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
        if (devProcess_initDevice(idNum) != -1)
        {
            sysInfo.currentDevID = idNum;
            query_device_info(&devInfo);
            resize_channel_buffers();
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
    return(devProcess_queryDevice(info));
} /* dimmer_query_device */


int dimmer_set_duplex_mode(int shouldSet)
/*
 * Some dimming hardware has multiple outputs. If possible, this
 *  function call instructs that hardware on how to treat the
 *  outputs. This call can set the hardware to attempt to duplex
 *  (use two separate outlets to double the number of accessible
 *  channels) the outputs, or send the same signal down both.
 *
 * See the (potentially nonexistant) documentation for a better
 *  explanation of duplexing.
 *
 *     params : shouldSet == 1 to attempt duplexing, 0 to disable duplexing.
 *    returns : -1 if unsuccessful, 0 on success. (errno) set on error.
 *      errno : ENODEV  (no dimmer device is selected).
 *              ENOSYS  (device can't duplex).
 */
{
    int retVal = -1;

    retVal = devProcess_setDuplexMode((shouldSet == 0) ? __false : __true);

    if (retVal != -1)
        retVal = resize_channel_buffers();

    return(retVal);
} /* dimmer_set_duplex_mode */



int dimmer_channel_set(unsigned int channel, unsigned char intensity)
/*
 * Set a specific channel to a specific intensity.
 *
 *   params : channel == see above.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 *    errno : ENODEV (no dimmer device is selected).
 *            Whatever device function wants to set.
 */
{
    int retVal = -1;
    int cookedLevel;
    int patched = patchTable[channel];

    rawLevels[patched] = intensity;
    if (blackOutEnabled)   /* cooked level should already be zero. */
        retVal = 0;
    else
    {
        ;// !!! grandmaster! cookedLevel = rawLevels ;

        devProcess_setChannel(patched, cookedLevel);
    } /* else */

    return(retVal);
} /* dimmer_channel_set */


int dimmer_fade_channel(unsigned int channel,
                        unsigned char intensity,
                        double seconds)
/*
 * Fade a channel. Even though the fade may take many seconds,
 *  this call does not block. The fade is passed off to another
 *  thread to be processed.
 *
 *      params : channel = channel # to fade.
 *               intensity = 0-255 level to fade light to.
 *               seconds = number of seconds (or fractions thereof) that
 *                         it should take for light to reach (intensity).
 *      returns : -1 on error, 0 on success. (errno) set on error.
 *        errno : ENOMEM (Not enough memory for malloc).
 */
{
    struct ChannelFadeStatus *fadePtr;
    struct ChannelFadeStatus *lastPtr = NULL;
    __boolean newStruct = __false;

    for (fadePtr = fadeList;
        (fadePtr != NULL) && (fadePtr->channel < channel);
        fadePtr = fadePtr->next)
    {
        lastPtr = fadePtr;
    } /* for */

    if ((fadePtr = NULL) || (fadePtr->channel != channel))
    {
        newStruct = __true;
        fadePtr = calloc(1, sizeof (struct ChannelFadeStatus));
        if (fadePtr == NULL)
        {
            errno = ENOMEM;
            return(-1);
        } /* if */
        fadePtr->next = lastPtr->next;
        fadePtr->channel = channel;
    } /* if */

    if (pthread_mutex_lock(&fadeLock) != 0)
    {
        if (newStruct)
            free(fadePtr);
        errno = EAGAIN;
        return(-1);
    } /* if */

    if (lastPtr != NULL)
        lastPtr->next = fadePtr;

    #warning dimmer_fade_channel() need to set rates!
    fadePtr->fadeActive = __true;
    fadePtr->destinationLevel = intensity;
    fadePtr->nextFadeTime.tv_sec = 0;
    fadePtr->nextFadeTime.tv_usec = 0;
    fadePtr->fadeTimeIncrement.tv_sec
    fadePtr->fadeTimeIncrement.tv_usec
    fadePtr->levelChangeRate = 0;

    pthread_mutex_unlock(&fadeLock);

    return(0);
} /* dimmer_fade_channel */


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
    int i;

    blackoutEnabled = ((blackoutEnabled) ? __false : __true);

    if (blackoutEnabled)
    {
        blackoutEnabled = __true;
        for (i = 0; i < devInfo.numChannels; i++)
            devProcess_setChannel(i, 0);  /* bypass dimmer_set_channel() */
    } /* if */
    else
    {
        blackoutEnabled = __false;
        for (i = 0; i < devInfo.numChannels; i++)
            dimmer_set_channel(i, rawLevels[patchTable[i]]);
    } /* else */

    return(0);
} /* dimmer_blackout */


int dimmer_set_channel_patch(int channel, int patchTo)
/*
 * Define a channel patch. Any time access is attempted on
 *  (channel), it'll actually use (patchTo) instead.
 *
 *    params : channel == channel to patch.
 *             patchTo == what (channel) will now access.
 *   returns : -1 on error, 0 on success.
 */
{
    int retVal = -1;

    if ((!dimmerLibInitialized) ||
        (patchTo < 0) || (patchTo >= devInfo.numChannels) ||
        (channel < 0) || (patchTo >= devInfo.numChannels))
    {
        errno = EINVAL;
    } /* if */
    else
    {
        if (pthread_mutex_lock(&fadeLock) != -1)
        {
            patchTable[channel] = patchTo;
            retVal = 0;
            pthread_mutex_unlock(&fadeLock);
        } /* if */
    } /* else */
    return(retVal);
} /* dimmer_set_channel_patch */


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

