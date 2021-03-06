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
#include <time.h>
#include "boolean.h"
#include "dimmer.h"

//define sched_yield() sleep(0)


    /* dimmer device modules... */
extern struct DimmerDeviceFunctions daddymax_funcs;
extern struct DimmerDeviceFunctions testdev_funcs;



struct ChannelFadeStatus
{
    __boolean fadeActive;               /* are we fading this one?          */
    unsigned int channel;               /* channel number to fade.          */
    unsigned char destinationLevel;     /* What level should it end at?     */
    struct timeval nextFadeTime;        /* time after epoch for next fade.  */
    struct timeval fadeTimeIncrement;   /* Amount of time between fades.    */
    int levelChangeRate;                /* Amount to fade with each update. */
    struct ChannelFadeStatus *next;     /* Next struct in linked list.      */
};


static __boolean dimmerLibInitialized = __false;

static struct DimmerSystemInfo sysInfo = {0, NULL, -1};
static struct DimmerDeviceInfo devInfo;

static volatile __boolean threadLiveFlag = __false;
static pthread_t fadeThread;
static pthread_t deviceThread;
static pthread_mutex_t fadeLock;
static struct ChannelFadeStatus *fadeList = NULL;

static unsigned char *rawLevels = NULL;
static unsigned char *cookedLevels = NULL;
static int *patchTable = NULL;

static unsigned char grandMasterLevel = 255;
static __boolean blackOutEnabled = __false;
static __boolean duplexEnabled = __false;


    /*
     * These elements are function pointers to other modules...
     */
static struct DimmerDeviceFunctions *activeModFuncs = NULL;
static struct DimmerDeviceFunctions *devFunctions[] = {
                                                          &daddymax_funcs,
                                                          &testdev_funcs
                                                      };

#define TOTAL_DEVICES  \
            (sizeof (devFunctions) / sizeof (struct DimmerDeviceFunctions *))


static void *deviceThreadEntry(void *args)
/*
 * Check for new "events" endlessly.
 *
 *    params : args == always (NULL).
 *   returns : Always (NULL). (terminates thread.)
 */
{
    while (threadLiveFlag)      /* endless loop. */
    {
        if ((activeModFuncs != NULL) && (cookedLevels != NULL))
            activeModFuncs->updateDevice(cookedLevels);
        sched_yield();
    } /* while */

    return(NULL);
} /* deviceThreadEntry */


static inline __boolean isPastTime(struct timeval *t1, struct timeval *t2)
/*
 * Determine if (t1) is past the time recorded in (t2).
 *
 *     params : t1, t2 == times to compare.
 *    returns : __true if (t1) is greater than or EQUAL to (t2).
 *              __false otherwise.
 */
{
        /* check the seconds first, and if equals, check microseconds... */
    if (t1->tv_sec > t2->tv_sec)
        return(__true);
    else if (t1->tv_sec < t2->tv_sec)
        return(__false);
    else /* equal */
        return((t1->tv_usec >= t2->tv_usec) ? __true : __false);
} /* isPastTime */


static void addTimevalStructs(struct timeval *toThis, struct timeval *addThis)
{
        // !!! how efficient is this?
    toThis->tv_sec += addThis->tv_sec;
    toThis->tv_usec += addThis->tv_usec;
    toThis->tv_sec += (long) (toThis->tv_usec / 1000000L);
    toThis->tv_usec = (long) (toThis->tv_usec % 1000000L);
} /* addTimevalStructs */


static inline void updateChannelFade(struct ChannelFadeStatus *list)
/*
 * Update a ChannelFadeStatus structure. Make actual changes to dimmers.
 *
 *    params : list == struct to update.
 *   returns : void.
 */
{
    int change = rawLevels[list->channel] + list->levelChangeRate;

    addTimevalStructs(&list->nextFadeTime, &list->fadeTimeIncrement);

        /* Make sure we don't go past destination level... */
    if (list->levelChangeRate < 0)            /* lights are dimming? */
    {
        if (change < list->destinationLevel)
            change = list->destinationLevel;
    } /* if */
    else                                      /* lights are brightening? */
    {
        if (change > list->destinationLevel)
            change = list->destinationLevel;
    } /* else */

    if (change == list->destinationLevel)        /* done with this one? */
        list->fadeActive = __false;

    dimmer_channel_set(list->channel, (unsigned char) change); /* do update. */
} /* updateChannelFade */


static inline __boolean runFadeList(void)
/*
 * Run through the entire pending list of fades once.
 *
 *    params : list == list to run through.
 *   returns : 0 if there were no fades. 1 if there was at least one.
 */
{
    __boolean retVal = __false;
    struct timeval currentTime;
    struct ChannelFadeStatus *list;

    for (list = fadeList; list != NULL; list = list->next)
    {
        if (list->fadeActive == __true)
        {
            gettimeofday(&currentTime, NULL);
            if (isPastTime(&currentTime, &list->nextFadeTime))
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

    while (threadLiveFlag == __true)  /* live until dimmer_deinit()... */
    {
        if (pthread_mutex_lock(&fadeLock) == 0)
        {
            atLeastOneFade = runFadeList();
            pthread_mutex_unlock(&fadeLock);
        } /* if */

//        if (atLeastOneFade == __false)  /* Only hog CPU when active fades. */
        sched_yield();
    } /* while */

    return(NULL);
} /* fadeThreadEntry */


static int spinJoinableThread(pthread_t *thread, void *(*entry)(void *))
/*
 * Use this to spin separate, joinable threads.
 *
 *   params : thread == where to store thread handle.
 *            entry  == entry point for thread.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 */
{
    int retVal = -1;
    pthread_attr_t attrs;

    pthread_attr_init(&attrs);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
    retVal = pthread_create(thread, &attrs, entry, NULL);
    pthread_attr_destroy(&attrs);

    return(retVal);
} /* spinJoinableThread */


static int spinThreads(void)
{
    int retVal = -1;

    pthread_mutex_init(&fadeLock, NULL);

    if (threadLiveFlag == __false)
    {
        threadLiveFlag = __true;

        if (spinJoinableThread(&fadeThread, fadeThreadEntry) != -1)
        {
            if (spinJoinableThread(&deviceThread, deviceThreadEntry) != -1)
                retVal = 0;

            else    /* no device thread? Kill off the first thread, too. */
            {
                threadLiveFlag = __false;
                pthread_join(fadeThread, NULL);
            } /* else */
        } /* if */
    } /* if */

    return(retVal);
} /* spinThreads */


static void killThreads(void)
{
    if (threadLiveFlag == __true)
    {
        threadLiveFlag = __false;
        pthread_join(fadeThread, NULL);
        pthread_join(deviceThread, NULL);
        pthread_mutex_destroy(&fadeLock);
    } /* if */
} /* killThreads */


static int checkForDevices(void)
/*
 * Internal function called from dimmer_init(): Check to see which
 *  of supported devices exist. Fill them into (sysInfo).
 *
 *     params : void.
 *    returns : total found devices. (-1) on error.
 */
{
    int retVal = 0;
    int devID;
    int max = TOTAL_DEVICES;

    sysInfo.devsAvailable = malloc(max * sizeof (int));

    if (sysInfo.devsAvailable == NULL)
        return(-1);

    for (devID = 0; devID < max; devID++)
    {
        if (devFunctions[devID]->queryExistence() != 0)
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
    char buffer[50];

    for (i = 0; (i < sysInfo.devCount) && (retVal == -1); i++)
    {
        devFunctions[sysInfo.devsAvailable[i]]->queryModuleName(buffer,
                                                            sizeof (buffer));
        retVal = dimmer_select_device(buffer);
    } /* for */

    return(retVal);
} /* attemptAutoInit */


static inline void deinitDevice(void)
{
    if (activeModFuncs != NULL)
    {
        activeModFuncs->deinitialize();
        activeModFuncs = NULL;
    } /* if */
} /* deinitDevice */


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

    if (checkForDevices() <= 0)
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

    if (spinThreads() == -1)
    {
        deinitDevice();
        return(-1);
    } /* if */

    atexit(dimmer_deinit);

    dimmerLibInitialized = __true;
    return(0);
} /* dimmer_init */


void dimmer_deinit(void)
/*
 * This is called atexit() to clean up, but for flexibility, we
 *  set everything back to its default state.
 *
 *    params : void.
 *   returns : Always (0).
 */
{
    struct ChannelFadeStatus *list;

    if (dimmerLibInitialized)
    {
        killThreads();
        deinitDevice();

        if (rawLevels != NULL)
            free(rawLevels);

        if (cookedLevels != NULL)
            free(cookedLevels);

        if (patchTable != NULL)
            free(patchTable);

        if (sysInfo.devsAvailable != NULL)
            free(sysInfo.devsAvailable);

        (void *) patchTable = (void *) rawLevels = (void *) cookedLevels = NULL;

        grandMasterLevel = 255;
        blackOutEnabled = __false;

        memset(&sysInfo, '\0', sizeof (struct DimmerSystemInfo));
        sysInfo.activeDevID = -1;
        activeModFuncs = NULL;
        duplexEnabled = __false;

        list = fadeList;
        while (list != NULL)
        {
            list = fadeList->next;
            free(fadeList);
            fadeList = list;
        } /* for */

            /* fadeList is returned to NULL after the above loop... */

        dimmerLibInitialized = __false;
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
    int i;
    int chan = devInfo.numChannels;
    __boolean threadsRunning = threadLiveFlag;

    if (threadsRunning)
        killThreads(); /* threads can't be checking buffers while we resize. */

    cookedLevels = realloc(cookedLevels, sizeof (unsigned char) * chan);
    rawLevels = realloc(rawLevels, sizeof (unsigned char) * chan);
    patchTable = realloc(patchTable, sizeof (int) * chan);

        // !!! these should not overwrite globals prematurely.
    if ((rawLevels == NULL) || (patchTable == NULL) || cookedLevels == NULL)
        return(-1);

    memset(rawLevels, '\0', sizeof (unsigned char) * chan);
    memset(cookedLevels, '\0', sizeof (unsigned char) * chan);

    for (i = 0; i < chan; i++)
        patchTable[i] = i;

    if (threadsRunning)
    {
        if (spinThreads() == -1)  /* restart buffer scanners... */
            return(-1);
    } /* if */

    return(0);
} /* resize_channel_buffers */


int dimmer_device_available(char *devName, int *devID)
/*
 * In the name of code reuse, this function will check if
 *  a given device is available for use. This could be
 *  accomplished on the application level by calling
 *  dimmer_query_system(), and then checking against
 *  devsAvailable and devCount. But we'll make your life easy. :)
 *
 *     params : devName == name of device module to check for availability.
 *              devID == filled in with ID number of device module.
 *    returns : zero if not available, non-zero if exists.
 */
{
    int i;
    int curDevID;
    int retVal = 0;
    char buffer[100];

    for (i = 0; (i < sysInfo.devCount) && (!retVal); i++)
    {
        curDevID = sysInfo.devsAvailable[i];
        devFunctions[curDevID]->queryModuleName(buffer, sizeof (buffer));
        if (strcmp(buffer, devName) == 0)
        {
            *devID = curDevID;
            retVal = -1;
        } /* if */
    } /* for */

    return(retVal);
} /* dimmer_device_available */


int dimmer_select_device(char *devName)
/*
 * Use this function to switch to a device other than the default.
 *  You can find out the default through the dimmer_query_system()
 *  call (returnValue.currentDevID). This function call is
 *  unnecessary if you use the autoInit feature of dimmer_init() and
 *  don't care what specific protocol you use.
 *
 *      params : devName == device module name to select.
 *     returns : -1 on error, 0 on success. errno set.
 *       errno : ENODEV (bogus device ID number).
 *               anything else device initialization chooses to set.
 */
{
    int retVal = -1;
    int devModID;

    if (!dimmer_device_available(devName, &devModID))
        errno = ENODEV;
    else
    {
        if (devFunctions[devModID]->initialize() != -1)
        {
            if (activeModFuncs != NULL)
                activeModFuncs->deinitialize();
            activeModFuncs = devFunctions[devModID];
            sysInfo.activeDevID = devModID;
            dimmer_query_device(&devInfo);
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
    int retVal = -1;

    if (activeModFuncs == NULL)
        errno = ENODEV;
    else
        retVal = activeModFuncs->queryDevice(info);

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
    __boolean wantDuplex = (shouldSet == 0) ? __false : __true;

    if (activeModFuncs == NULL)
        errno = ENODEV;
    else
    {
        if (duplexEnabled != wantDuplex)
        {
            retVal = activeModFuncs->setDuplexMode(duplexEnabled);

            if (retVal != -1)
            {
                duplexEnabled = wantDuplex;
                retVal = resize_channel_buffers();
            } /* if */
        } /* if */
    } /* else */

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
        // !!! grandmaster/etc...!
        cookedLevel = rawLevels[patched];
        cookedLevels[patched] = cookedLevel;
        retVal = 0;
    } /* else */

    return(retVal);
} /* dimmer_channel_set */



static inline void initChannelFadeStatus(struct ChannelFadeStatus *fadePtr,
                                         unsigned int channel,
                                         unsigned char intensity,
                                         double seconds)
/*
 * This is called by dimmer_fade_channel() to initialize a ChannelFadeStatus
 *  structure before it is initially passed on to the fading thread for
 *  use. All parameters are guaranteed to be valid before this call, so
 *  no sanity checks should be necessary here.
 *
 *     params : fadePtr   == struct to initialize.
 *              channel   == channel we're fading.
 *              intensity == raw level to fade to.
 *              seconds   == time to fade over.
 *    returns : void.
 */
{
    double timeBetweenFades;
    long secsBetweenFades;
    long microsecsBetweenFades;
    int totalChange = intensity - rawLevels[patchTable[channel]];

    if (totalChange == 0)
        fadePtr->fadeActive = __false;
    else
    {
        fadePtr->fadeActive = __true;

        timeBetweenFades = seconds / ((double) abs(totalChange));
        secsBetweenFades = (long) timeBetweenFades;   /* lose fractions. */
        microsecsBetweenFades =
           (long) (1000000.0 * (timeBetweenFades - (double) secsBetweenFades));

        fadePtr->fadeTimeIncrement.tv_sec = secsBetweenFades;
        fadePtr->fadeTimeIncrement.tv_usec = microsecsBetweenFades;

            /*
             * Set up the first fade time here. This will be handled
             *  from now on by the fade thread.
             */
        gettimeofday(&fadePtr->nextFadeTime, NULL);
        addTimevalStructs(&fadePtr->nextFadeTime, &fadePtr->fadeTimeIncrement);

            /* fade up or fade down? */
        fadePtr->levelChangeRate = ((totalChange > 0) ? 1 : -1);

            /* the easy stuff. :) */
        fadePtr->destinationLevel = intensity;
        fadePtr->channel = channel;
    } /* else */
} /* initChannelFadeStatus */


int dimmer_channel_fade(unsigned int channel,
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
 *                EINVAL (bad agruments.)
 */
{
    struct ChannelFadeStatus *fadePtr;
    struct ChannelFadeStatus *lastPtr = NULL;
    __boolean newStruct = __false;

        /* sanity checks... */
    if ((channel >= devInfo.numChannels) || (seconds < 0.0))
    {
        errno = EINVAL;
        return(-1);
    } /* if */

    channel = patchTable[channel];

    for (fadePtr = fadeList;
        (fadePtr != NULL) && (fadePtr->channel < channel);
        fadePtr = fadePtr->next)
    {
        lastPtr = fadePtr;
    } /* for */

        /* this channel hasn't faded yet? Allocate a new list item... */
    if ((fadePtr == NULL) || (fadePtr->channel != channel))
    {
        newStruct = __true;
        fadePtr = calloc(1, sizeof (struct ChannelFadeStatus));
        if (fadePtr == NULL)
        {
            errno = ENOMEM;
            return(-1);
        } /* if */
    } /* if */

        /* grabbing the ThreadLock halts the fade thread... */
    if (pthread_mutex_lock(&fadeLock) != 0)
    {
        if (newStruct == __true)
            free(fadePtr);
        errno = EAGAIN;
        return(-1);
    } /* if */

        /* set up the structure... */
    initChannelFadeStatus(fadePtr, channel, intensity, seconds);

        /* plug the structure into the list, if need be... */
    if (newStruct == __true)
    {
        if (lastPtr == NULL)        /* start of new list? */
        {
            fadePtr->next = NULL;
            fadeList = fadePtr;
        } /* if */
        else                        /* insert item into place. */
        {
            fadePtr->next = lastPtr->next;
            lastPtr->next = fadePtr;
        } /* else */
    } /* else */

        /* we're golden; let the fade thread go again... */
    pthread_mutex_unlock(&fadeLock);

    return(0);
} /* dimmer_channel_fade */


int dimmer_toggle_blackout(int shouldToggleOn)
/*
 * First call to this bumps all channels to zero.
 * Next call restores (by bump) all channels to where they where before
 *  the blackout. Any other attempts to modify a channel's intensity will
 *  be noted, but the change will not be made during blackout.
 *
 *    params : shouldToggleOn == nonZero to start blackout, zero to stop.
 *   returns : always returns (0);
 */
{
    int i;

        /* make sure we aren't requesting the current state... */
    if ( ((blackOutEnabled == __true)  && (shouldToggleOn == 0)) ||
         ((blackOutEnabled == __false) && (shouldToggleOn != 0)) )
    {
            /* swap state. */
        blackOutEnabled = ((blackOutEnabled) ? __false : __true);

        if (blackOutEnabled)
        {
            blackOutEnabled = __true;
            for (i = 0; i < devInfo.numChannels; i++)
                cookedLevels[i] = 0;  /* bypass dimmer_channel_set() */
        } /* if */
        else
        {
            blackOutEnabled = __false;
            for (i = 0; i < devInfo.numChannels; i++)
                dimmer_channel_set(i, rawLevels[patchTable[i]]);
        } /* else */
    } /* if */

    return(0);
} /* dimmer_toggle_blackout */


int dimmer_channel_patch(int channel, int patchTo)
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
} /* dimmer_channel_patch */


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

