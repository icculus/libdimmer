/*
 * Code that runs in the device i/o process.
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

#include "boolean.h"
#include "process_communication.h"
#include "dev_daddymax.h"


/*
 * These elements are function pointers to other modules...
 */
static int inPipe = 0;
static int outPipe = 0;
static fd_set selectSet;
static struct timeval selectTimeout;
static __boolean selectDataUndefined = __true;
static struct ChannelData;
static unsigned char *levels = NULL;
static struct DimmerDeviceFunctions *curDevFuncs = NULL;
static struct DimmerDeviceFunctions *devFunctions[TOTAL_DEVICES] =
                                                        {
                                                            &daddymax_funcs
                                                        };

static void devProcessAtExit(void)
/*
 * Cleanup for process termination.
 *
 *    params : void.
 *   returns : void.
 */
{
    close(inPipe);
    close(outPipe);
} /* devProcessCleanup */


static void sigTermHandler(int signal)
/*
 * The device process receives a SIGTERM signal to request
 *  termination from the parent process. This signal handler
 *  just calls exit(), which guarantees our atexit() procedures
 *  run.
 *
 *     params : signal == should always be (SIGTERM).
 *    returns : void. (never returns)
 */
{
    signal(SIGTERM, SIG_IGN);
    exit(0);
} /* sigTermHandler */


static void processMessageHandler(pcmsg_t msg)
/*
 * Actual handling of process messages is done here.
 *
 *    params : msg == message to handle.
 *   returns : void. (potentially never returns.)
 */
{
    pcmsg_t retMsg = PCMSG_NON_COMPLIANCE;
    int dummy;
    __boolean dummyBool;
    struct DimmerDeviceInfo devInfo;

    switch (msg)
    {
        case PCMSG_SET_CHANNEL:
            read(inPipe, channelData, sizeof (struct ChannelData));
            levels[channelData.channelNum] = channelData.level;
            break;

        case PCMSG_ARE_YOU_ALIVE:
            retMsg = PCMSG_I_AM_ALIVE;
            write(outPipe, &retMsg, sizeof (pcmsg_t));
            write(outPipe, &pid, sizeof (pid_t));
            break;

        case PCMSG_PLEASE_DIE:
            retMsg = PCMSG_COMPLIANCE;
            write(outPipe, &retMsg, sizeof (pcmsg_t));
            exit(0);
            break;

        case PCMSG_QUERY_DEVICE:
            if (curDevFuncs != NULL)
            {
                if (curDevFuncs->queryDevice(&devInfo) != -1)
                    retMsg = PCMSG_COMPLIANCE;
            } /* if */

            write(outPipe, &retMsg, sizeof (pcmsg_t));
            if (retMsg = PCMSG_COMPLIANCE)
                write(outPipe, &devInfo, sizeof (struct DimmerDeviceInfo));

            break;

        case PCMSG_DEVICE_EXISTS;
            read(inPipe, &dummy, sizeof (int));
            if (dummy < TOTAL_DEVICES)
            {
                if (devFunctions[dummy]->queryExistence())
                    retMsg = PCMSG_COMPLIANCE;
            } /* if */
            write(outPipe, &retMsg, sizeof (pcmsg_t));
            break;

        case PCMSG_INIT_DEVICE:
            read(inPipe, &dummy, sizeof (int));
            if (devFunctions[dummy]->initialize() != -1)
            {
                if (curDevFuncs != NULL)
                    curDevFuncs->deinitialize();
                curDevFuncs = devFunctions[dummy];
                retMsg = PCMSG_COMPLIANCE;
            } /* if */
            write(outPipe, &retMsg, sizeof (pcmsg_t));
            break;

        case PCMSG_SET_DUPLEX:
            read(inPipe, &dummyBool, sizeof (__boolean));
            if (curDevFuncs != NULL)
            {
                if (curDevFuncs->setDuplexMode(dummyBool) != -1)
                {
                    curDevFuncs->queryDevice(&devInfo);
                    levels = realloc(levels,
                                devInfo.numChannels * sizeof (unsigned char));
                    if (levels != NULL)
                        retMsg = PCMSG_COMPLIANCE;
                } /* if */
            } /* if */
            write(outPipe, &retMsg, sizeof (pcmsg_t));
            break;

        default:    /* ignore else. */
            break;
    } /* switch */
} /* processMessageHandler */


static void checkMessages(void)
/*
 * Check for the existance of a new message, and if it's
 *  there, handle it.
 *
 *    params : void.
 *   returns : void.
 */
{
    int rc;
    pcmsg_t msg;

    if (selectDataUndefined)
    {
        selectTimeout.tv_sec = 0;
        selectTimeout.tv_usec = 0;

        FD_ZERO(&set);
        FD_SET(inPipe, &set);
        selectDataUndefined = __false;
    } /* if */

    rc = select(FD_SETSIZE, &set, NULL, NULL, &timeout);

    if (rc == -1)           /* error condition.         */
        selectDataUndefined = __true;

    else if (rc == 1)       /* inPipe has data to read. */
    {
        read(inPipe, &msg, sizeof (pcmsg_t));
        processMessageHandler(msg);
    } /* else if */
} /* checkMessages */


static __boolean initializeDeviceProcess(void)
/*
 * This routine attempts to initialize the device process.
 *  This involves preparing termination handlers and opening
 *  named pipes.
 *
 *    params : void.
 *   returns : (__true) if successful init, (__false) otherwise.
 */
{
    pcmsg_t msg;
    int pid = getpid();

    signal(SIGTERM, sigTermHandler);
    signal(SIGPIPE, SIG_IGN);
    atexit(devProcessAtExit);

    inPipe = open(FIFO1FILENAME, O_RDONLY);
    outPipe = open(FIFO2FILENAME, O_WRONLY);

        /*
         * either of these failing usually
         *  signifies another instance of this
         *  code is running.
         */
    if ((inPipe == -1) || (outPipe == -1))
        return(__false);

    read(inPipe, &msg, sizeof (pcmsg_t));
    if (msg != PCMSG_ARE_YOU_ALIVE)
        return(__false);

    processMessageHandler(msg);
} /* initializedDeviceProcess */


#ifdef SEPARATE_BINARIES
int main(void)
#else
void deviceForkEntry(void)
#endif
/*
 * This is called when the device process is fork()ed off.
 *  Initialization is done here, and the processing loop begins.
 *  This routine should never return, but call exit() if it must
 *  quit.
 *
 *    params : void.
 *   returns : void. (never returns.)
 */
{
    if (initializeDeviceProcess())
        deviceIOLoop();

#ifdef SEPARATE_BINARIES
    exit(0);  /* never return. */
#else
    return(0);
#endif
} /* deviceForkEntry */

/* end of device_process.c ... */

