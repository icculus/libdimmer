/*
 * Headers for libdimmer process communication routines.
 *
 *    Copyright (c) 1999 Lighting and Sound Technologies.
 *     Written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_PROCESS_COMM_H_
#define _INCLUDE_PROCESS_COMM_H_

#include "dimmer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO1FILENAME           ".dimmerfifo1"
#define FIFO2FILENAME           ".dimmerfifo2"

typedef unsigned char pcmsg_t;

#define PCMSG_NULL              0
#define PCMSG_ARE_YOU_ALIVE     1
#define PCMSG_I_AM_ALIVE        2
#define PCMSG_PLEASE_DIE        3
#define PCMSG_COMPLIANCE        4
#define PCMSG_NON_COMPLIANCE    5
#define PCMSG_QUERY_DEVICE      6
#define PCMSG_DEVICE_EXISTS     7
#define PCMSG_SET_CHANNEL       8
#define PCMSG_INIT_DEVICE       9
#define PCMSG_DEINIT_DEVICE     10
#define PCMSG_SET_DUPLEX        11

int createDeviceProcess(void);
void killDeviceProcess(void);
void devProcess_setChannel(int channel, unsigned char level);
int devProcess_queryExistence(int devID);
int devProcess_setDuplexMode(__boolean shouldSet);
int devProcess_queryDevice(struct DimmerDeviceInfo *info);
int devProcess_initDevice(int devID);
void devProcess_deinitDevice(void);

#ifdef __cplusplus
}
#endif

#endif /* !defined _INCLUDE_PROCESS_COMM_H_ */

/* end of process_communication.h ... */

