/*
 * Module to manage and communicate between processes.
 *
 *   Copyright (c) 1999 Lighting and Sound Technologies, inc.
 *    Written by Ryan C. Gordon.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include "boolean.h"
#include "process_communication.h"

#ifndef SEPARATE_BINARIES
void deviceForkEntry(void);
#endif


static pthread_t monitorThread;
static volatile __boolean threadLiveFlag = __false;
static pid_t childProcess = 0;
static int outPipe = -1;
static int inPipe = -1;


static inline __boolean createFifo(void)
{
    if (mkfifo(FIFO1FILENAME, S_IRUSR | S_IWUSR) == -1)
    {
        if (errno != EEXIST)
            return(__false);
    } /* if */

    if (mkfifo(FIFO2FILENAME, S_IRUSR | S_IWUSR) == -1)
    {
        if (errno != EEXIST)
            return(__false);
    } /* if */

    return(__true);
} /* createFifo */


int createDeviceProcess(void)
/*
 * Create a child process to handle device I/O.
 *
 *    params : void.
 *   returns : -1 on error, 0 on success.
 */
{
    int rc;
    pcmsg_t msg = PCMSG_NULL;

    signal(SIGPIPE, SIG_IGN);

    if (!createFifo())
        return(-1);

    rc = fork();

    if (rc == -1)
        return(-1);

    if (rc == 0)   /* child process? */
    {
#ifdef SEPARATE_BINARIES
        rc = execlp(DEVPROCESS_EXE_FILENAME, DEVPROCESS_EXE_FILENAME, NULL);
        if (rc == -1)
        {            /* talk to ourselves. */
            inPipe = open(FIFO1FILENAME, O_RDONLY);
            outPipe = open(FIFO2FILENAME, O_WRONLY);

            msg = PCMSG_NON_COMPLIANCE;
            read(inPipe, &msg, sizeof (pcmsg_t));
            write(outPipe, &msg, sizeof (pcmsg_t));
            exit(0);
        } /* if */
#else
        deviceForkEntry();
        exit(0);
#endif
    } /* if */
    else
    {
        outPipe = open(FIFO1FILENAME, O_RDONLY);
        inPipe = open(FIFO2FILENAME, O_WRONLY);

            /*
             * either of these failing usually
             *  signifies another instance of this
             *  code is running.
             */
        if ((outPipe == -1) || (inPipe == -1))
        {
            killDeviceProcess();
            return(-1);
        } /* if */

        msg = PCMSG_ARE_YOU_ALIVE;
        write(outPipe, &msg, sizeof (pcmsg_t));
        read(inPipe, &msg, sizeof (pcmsg_t));
        if (msg != PCMSG_I_AM_ALIVE)
        {
            close(outPipe);
            close(inPipe);
            return(-1);
        } /* if */

        read(inPipe, &childProcess, sizeof (pid_t));
    } /* else */

    return(0);
} /* createDeviceProcess */


static void *monitorThreadEntry(void *args)
{
    return(NULL);  /* !!! write me! */
} /* monitorThreadEntry */


static int spinMonitorThread(void)
/*
 * Spin the thread that keeps spawning device processes as they die.
 *
 *   params : void.
 *  returns : -1 on error, 0 on success. (errno) set on error.
 */
{
    int retVal = -1;
    pthread_attr_t attrs;

    if (!threadLiveFlag)
    {
        threadLiveFlag = __true;
        pthread_attr_init(&attrs);
        pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
        retVal = pthread_create(&monitorThread, &attrs,
                                monitorThreadEntry, NULL);
        pthread_attr_destroy(&attrs);
    } /* if */

    return(retVal);
} /* spinMonitorThread */


static void killMonitorThread(void)
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
        threadLiveFlag = __false;
        pthread_join(monitorThread, NULL);
    } /* if */
} /* killFadeThread */


void killDeviceProcess(void)
/*
 * One way or another, we're gonna slaughter that child process.
 *
 *     params : void.
 *    returns : void.
 */
{
    pcmsg_t msg;

    killMonitorThread();

    if ((outPipe != -1) && (inPipe != -1))
    {
        msg = PCMSG_PLEASE_DIE;
        write(outPipe, &msg, sizeof (pcmsg_t));
        read(inPipe, &msg, sizeof (pcmsg_t));
        if ((msg != PCMSG_COMPLIANCE) && (childProcess != 0))
            kill(childProcess, SIGTERM);
    } /* if */
    else
    {
        if (childProcess != 0)
            kill(childProcess, SIGTERM);
    } /* else */
} /* killDeviceProcess */



void deinitProcessCommunication(void)
{
} /* deinitProcessCommunication */


int initProcessCommunication(void)
{
    return(spinMonitorThread());
} /* initProcessCommunication */


void devProcess_queryDevModName(int devID, char *buffer, int bufSize)
{
    int size;
    pcmsg_t msg = PCMSG_QUERY_DEVMODNAME;

    buffer[0] = '\0';   /* blank string. */

    write(outPipe, &msg, sizeof (msg));
    write(outPipe, &devID, sizeof (devID));

    read(inPipe, &msg, sizeof (msg));
    if (msg == PCMSG_COMPLIANCE)
    {
        read(inPipe, &size, sizeof (size));

        if (bufSize >= size)
            read(inPipe, buffer, size);
        else
        {
            read(inPipe, buffer, bufSize);
            #warning must handle overflows!
        } /* else */
    } /* if */
} /* devProcess_queryDevModName */


void devProcess_setChannel(int channel, unsigned char level)
{
    struct
    {
        pcmsg_t _msg;
        int _channel;
        unsigned char _level;
    } setChannelDetails = { PCMSG_SET_CHANNEL, channel, level };

    write(outPipe, &setChannelDetails, sizeof (setChannelDetails));
} /* devProcess_setChannel */


int devProcess_queryDeviceModules(void)
{
    int retVal;
    pcmsg_t msg = PCMSG_QUERY_DEVMODS;

    write(outPipe, &msg, sizeof (pcmsg_t));
    read(inPipe, &retVal, sizeof (int));

    return(retVal);
} /* devProcess_queryDeviceModules */


int devProcess_queryExistence(int devID)
{
    int retVal = -1;
    pcmsg_t msg = PCMSG_DEVICE_EXISTS;

    write(outPipe, &msg, sizeof (pcmsg_t));
    write(outPipe, &devID, sizeof (int));

    read(inPipe, &msg, sizeof (pcmsg_t));
    if (msg == PCMSG_COMPLIANCE)
        retVal = 0;

    return(retVal);
} /* devProcess_queryExistence */


int devProcess_setDuplexMode(__boolean shouldSet)
{
    int retVal = -1;
    pcmsg_t msg = PCMSG_SET_DUPLEX;

    write(outPipe, &msg, sizeof (pcmsg_t));
    write(outPipe, &shouldSet, sizeof (__boolean));

    read(inPipe, &msg, sizeof (pcmsg_t));
    if (msg == PCMSG_COMPLIANCE)
        retVal = 0;

    return(retVal);
} /* devProcess_queryExistence */


int devProcess_queryDevice(struct DimmerDeviceInfo *info)
{
    int retVal = -1;
    pcmsg_t msg = PCMSG_QUERY_DEVICE;

    write(outPipe, &msg, sizeof (pcmsg_t));

    read(inPipe, &msg, sizeof (pcmsg_t));
    if (msg == PCMSG_COMPLIANCE)
    {
        read(inPipe, info, sizeof (struct DimmerDeviceInfo));
        retVal = 0;
    } /* if */

    return(retVal);
} /* devProcess_queryDevice */


int devProcess_initDevice(int devID)
{
    int retVal = -1;
    pcmsg_t msg = PCMSG_INIT_DEVICE;

    write(outPipe, &msg, sizeof (pcmsg_t));
    write(outPipe, &devID, sizeof (int));

    read(inPipe, &msg, sizeof (pcmsg_t));
    if (msg == PCMSG_COMPLIANCE)
        retVal = 0;

    return(retVal);
} /* devProcess_initDevice */


int devProcess_deinitDevice(void)
{
    int retVal = 0;
    pcmsg_t msg = PCMSG_DEINIT_DEVICE;
    write(outPipe, &msg, sizeof (pcmsg_t));
    read(inPipe, &msg, sizeof (pcmsg_t));

    if (msg != PCMSG_COMPLIANCE)
        retVal = -1;

    return(retVal);
} /* devProcess_deinitDevice */


/* end of process_communication.c ... */

