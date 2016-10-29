/*
 * region.h
 *
 *  Created on: Sep 16, 2016
 *      Author: hoang
 */

#ifndef REGION_H_
#define REGION_H_

#include <stdio.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <sys/types.h>
#include "sample_comm.h"

#define NUMBER_OF_CHARACTERS	20

typedef enum luCHAR_TYPE
{
	CHAR_NUMBER = 0,
	CHAR_SPEC
}CHAR_TYPE;

typedef struct luLINE_BITMAP_S
{
	CHAR_TYPE type[NUMBER_OF_CHARACTERS];
	unsigned short pos[NUMBER_OF_CHARACTERS];	//pos in pixel
	unsigned short width[NUMBER_OF_CHARACTERS];	//width size in pixel
	unsigned short total_width;					//total region width in pixel
	unsigned short height;						//height size in pixel
	BITMAP_S bitmap;
} LINE_BITMAP_S;

typedef struct tagREGION_S
{
    HI_BOOL bStart;
    pthread_t stRegionPid;
} REGION_S;


//region to display current
int create_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt, PIC_SIZE_E *enSize);
int destroy_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt);

HI_S32 update_region();
int start_thread_update_region();
int stop_thread_update_region();

#endif /* REGION_H_ */
