/*
 * Support code for the DaddyMax DMX512 dongle.
 *
 *  Copyright (c) 1999 Lighting and Sound Technologies.
 *   Written by Ryan C. Gordon.
 */

struct DeviceFunctions daddymax_funcs = {
                                            daddymax_queryExistance,
                                            daddymax_initialize,
                                            daddymax_deinitialize,
                                            daddymax_channelSet
                                        };

void daddymax_queryExistence(void)
{
} /* daddymax_queryExistence */


int daddymax_initialize(void)
{
} /* daddymax_initialize */


int daddymax_deinitialize(void)
{
} /* daddymax_deinitialize */


int daddymax_channelSet(int channel, int intensity)
{
} /* daddymax_channelSet */


/* end of dev_daddymax.c ... */

