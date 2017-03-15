/*
 * region.cpp
 *
 *  Created on: Sep 16, 2016
 *      Author: hoang
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "loadbmp.h"
#include "region.h"
#include "config.h"

#include <unistd.h>

#define REGION_POS_VGA_X			356
#define REGION_POS_VGA_Y			8
#define REGION_SIZE_VGA_WIDTH		268
#define REGION_SIZE_VGA_HEIGHT		18
#define NUMBER_SIZE_VGA_WIDTH		14
#define NUMBER_SIZE_VGA_HEIGHT		18
#define SPEC_CHAR_SIZE_VGA_WIDTH	12

#define REGION_POS_720P_X			712
#define REGION_POS_72P0_Y			16
#define REGION_SIZE_720P_WIDTH		536
#define REGION_SIZE_720P_HEIGHT		36
#define NUMBER_SIZE_720P_WIDTH		28
#define NUMBER_SIZE_720P_HEIGHT		36
#define SPEC_CHAR_SIZE_720P_WIDTH	24

#define LOGO_POS_720P_X			16
#define LOGO_POS_720P_Y			16
#ifdef USE_VIETTEL_IDC_LOGO
#define LOGO_SIZE_720P_WIDTH	250
#define LOGO_SIZE_720P_HEIGHT	52
#else
#define LOGO_SIZE_720P_WIDTH	180
#define LOGO_SIZE_720P_HEIGHT	60
#endif //USE_VIETTEL_IDC_LOGO

#define LOGO_POS_VGA_X			8
#define LOGO_POS_VGA_Y			8
#ifdef USE_VIETTEL_IDC_LOGO
#define LOGO_SIZE_VGA_WIDTH		126
#define LOGO_SIZE_VGA_HEIGHT	26
#else
#define LOGO_SIZE_VGA_WIDTH		90
#define LOGO_SIZE_VGA_HEIGHT	30
#endif //USE_VIETTEL_IDC_LOGO

#define NUMBER_POS_YEAR	0
#define NUMBER_POS_MON	5
#define NUMBER_POS_DAY	8
#define NUMBER_POS_HOUR	12
#define NUMBER_POS_MIN	15
#define NUMBER_POS_SEC	18
#define SPEC_CHAR_POS_HYPHEN1	4
#define SPEC_CHAR_POS_HYPHEN2	7
#define SPEC_CHAR_POS_SPACE1	10
#define SPEC_CHAR_POS_SPACE2	11
#define SPEC_CHAR_POS_COLON1	14
#define SPEC_CHAR_POS_COLON2	17

/*
 * save characters bitmap to static BITMAP_S variable to use
 * stCharacters_vga[0-9] : 0 - 9
 * stCharacters_vga[10]  : '-'
 * stCharacters_vga[11]  : ' '
 * stCharacters_vga[12]  : ':'
 */
static BITMAP_S stCharacters_vga[13];
static BITMAP_S stCharacters_720p[13];

//data line to display
static LINE_BITMAP_S stLineBitmap_720p;
static LINE_BITMAP_S stLineBitmap_vga;

static tm stPre_tm;
static time_t stPre_time;
static REGION_S threadUpdateRegion;

/******************************************************************************
* funciton : load bmp from file
******************************************************************************/
HI_S32 SAMPLE_RGN_LoadBmp(const HI_CHAR *filename, BITMAP_S *pstBitmap)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if(GetBmpInfo(filename,&bmpFileHeader,&bmpInfo) < 0)
    {
        SAMPLE_PRT("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;

    pstBitmap->pData = malloc(2*(bmpInfo.bmiHeader.biWidth)*(bmpInfo.bmiHeader.biHeight));

    if(NULL == pstBitmap->pData)
    {
        SAMPLE_PRT("malloc osd memroy err!\n");
        return HI_FAILURE;
    }

    CreateSurfaceByBitMap(filename,&Surface,(HI_U8*)(pstBitmap->pData));

    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_1555;
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : update_region_digit_data
* usage		: call to update 1 digit of pLineBitmapChn
* pLineBitmapChn	: pointer to data LINE_BITMAP_S line to display
* pChar				: pointer to extract charater BITMAP_S to update to pLineBitmapChn
* start_pixel		: position in pixel base region line
* region_len		: width in pixel of region
* width				: size in pixel of the number
* height			: height in pixel of region
******************************************************************************/
HI_S32 update_region_digit_data(LINE_BITMAP_S *pLineBitmapChn, BITMAP_S *pChar, unsigned short start_pixel, unsigned short region_len, unsigned char width, unsigned char height) {
	int i;

	for (i = 0; i < pLineBitmapChn->height; i++) {
		memcpy((char *)pLineBitmapChn->bitmap.pData + start_pixel * 2 + i * region_len * 2,
				(char *)pChar->pData + i * width * 2, width * 2);
	}

	return HI_SUCCESS;
}

/******************************************************************************
* funciton : update_region_digit_data
* usage		: copy all special characters to pLineBitmapChn
* pCharacters		: pointer to static BITMAP_S array data of the channel
* pLineBitmapChn	: pointer to data LINE_BITMAP_S line to display
* enSize			: spec to chn
******************************************************************************/
HI_S32 save_spec_char_data(LINE_BITMAP_S *pLineBitmapChn, BITMAP_S *pCharacters, PIC_SIZE_E enSize) {
	update_region_digit_data(pLineBitmapChn, &pCharacters[10], pLineBitmapChn->pos[SPEC_CHAR_POS_HYPHEN1], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_HYPHEN1], pLineBitmapChn->height);
	update_region_digit_data(pLineBitmapChn, &pCharacters[10], pLineBitmapChn->pos[SPEC_CHAR_POS_HYPHEN2], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_HYPHEN2], pLineBitmapChn->height);

	update_region_digit_data(pLineBitmapChn, &pCharacters[11], pLineBitmapChn->pos[SPEC_CHAR_POS_SPACE1], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_SPACE1], pLineBitmapChn->height);
	update_region_digit_data(pLineBitmapChn, &pCharacters[11], pLineBitmapChn->pos[SPEC_CHAR_POS_SPACE2], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_SPACE2], pLineBitmapChn->height);

	update_region_digit_data(pLineBitmapChn, &pCharacters[12], pLineBitmapChn->pos[SPEC_CHAR_POS_COLON1], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_COLON1], pLineBitmapChn->height);
	update_region_digit_data(pLineBitmapChn, &pCharacters[12], pLineBitmapChn->pos[SPEC_CHAR_POS_COLON2], pLineBitmapChn->total_width, pLineBitmapChn->width[SPEC_CHAR_POS_COLON2], pLineBitmapChn->height);

	return HI_SUCCESS;
}

/******************************************************************************
* funciton : update_region_digit_data
* usage		: call to update year, mon, ...
* pLineBitmapChn	: pointer to data LINE_BITMAP_S line to display
* pCharacters		: pointer to static BITMAP_S array data of the channel
* number_pos		: position in digit of the value
* value				: new value
******************************************************************************/
HI_S32 update_region_number(LINE_BITMAP_S *pLineBitmapChn, BITMAP_S *pCharacters, unsigned char number_pos, int value) {
	unsigned short digit[4];

	if (number_pos == NUMBER_POS_YEAR) {
		int year = value + 1900;
		digit[0] = year / 1000;
		digit[1] = (year / 100) % 10;
		digit[2] = (year / 10) % 10;
		digit[3] = year % 10;
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[0], pLineBitmapChn->pos[number_pos], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos], pLineBitmapChn->height);
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[1], pLineBitmapChn->pos[number_pos + 1], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos + 1], pLineBitmapChn->height);
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[2], pLineBitmapChn->pos[number_pos + 2], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos + 2], pLineBitmapChn->height);
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[3], pLineBitmapChn->pos[number_pos + 3], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos + 3], pLineBitmapChn->height);
	}
	else {
		if (number_pos == NUMBER_POS_MON)
			value++;
		digit[0] = value / 10;
		digit[1] = value % 10;
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[0], pLineBitmapChn->pos[number_pos], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos], pLineBitmapChn->height);
		update_region_digit_data(pLineBitmapChn, pCharacters + digit[1], pLineBitmapChn->pos[number_pos + 1], pLineBitmapChn->total_width, pLineBitmapChn->width[number_pos + 1], pLineBitmapChn->height);
	}

	return HI_SUCCESS;
}

HI_S32 update_region_Bitmap_data(LINE_BITMAP_S *pLineBitmapChn, BITMAP_S *pCharacters, struct tm *tm_now, PIC_SIZE_E enSize) {
	HI_S32 s32Ret;
	RGN_HANDLE RgnHandle;

	//year
	if (stPre_tm.tm_year != tm_now->tm_year) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_YEAR, tm_now->tm_year);
	}
	//month
	if (stPre_tm.tm_mon != tm_now->tm_mon) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_MON, tm_now->tm_mon);
	}
	//day
	if (stPre_tm.tm_mday != tm_now->tm_mday) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_DAY, tm_now->tm_mday);
	}
	//hour
	if (stPre_tm.tm_hour != tm_now->tm_hour) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_HOUR, tm_now->tm_hour);
	}
	//minute
	if (stPre_tm.tm_min != tm_now->tm_min) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_MIN, tm_now->tm_min);
	}
	//second
	if (stPre_tm.tm_sec != tm_now->tm_sec) {
		update_region_number(pLineBitmapChn, pCharacters, NUMBER_POS_SEC, tm_now->tm_sec);
	}

	if (enSize == PIC_HD720)
		RgnHandle = 0;
	else
		RgnHandle = 1;

    s32Ret = HI_MPI_RGN_SetBitMap(RgnHandle, &pLineBitmapChn->bitmap);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_SetBitMap failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

	return HI_SUCCESS;
}

HI_S32 update_region() {
	time_t time_now;
	struct tm *tm_now;

	time_now = time(NULL);
	tm_now = localtime(&time_now);

	if (stPre_time != time_now) {
		update_region_Bitmap_data(&stLineBitmap_720p, stCharacters_720p, tm_now, PIC_HD720);
		update_region_Bitmap_data(&stLineBitmap_vga, stCharacters_vga, tm_now, PIC_VGA);
	}

	return HI_SUCCESS;
}

HI_S32 save_characters_position(LINE_BITMAP_S *pLineBitmapChn, PIC_SIZE_E enSize) {
	int i;
	unsigned short pos = 0;
	unsigned int number_width, spec_char_width, number_height;

	number_width = enSize == PIC_HD720 ? NUMBER_SIZE_720P_WIDTH : NUMBER_SIZE_VGA_WIDTH;
	spec_char_width = enSize == PIC_HD720 ? SPEC_CHAR_SIZE_720P_WIDTH : SPEC_CHAR_SIZE_VGA_WIDTH;
	number_height = enSize == PIC_HD720 ? NUMBER_SIZE_720P_HEIGHT : NUMBER_SIZE_VGA_HEIGHT;

	for (i = 0; i < NUMBER_OF_CHARACTERS; i++) {
		if ((i == 4) || (i == 7) || (i == 10) || (i == 11) || (i == 14) || (i == 17)) {
			pLineBitmapChn->type[i] = CHAR_SPEC;
			pLineBitmapChn->width[i] = spec_char_width;
		}
		else {
			pLineBitmapChn->type[i] = CHAR_NUMBER;
			pLineBitmapChn->width[i] = number_width;
		}
	}

	for (i = 0; i < NUMBER_OF_CHARACTERS; i++) {
		pLineBitmapChn->pos[i] = pos;
		pLineBitmapChn->height = number_height;
		pos += pLineBitmapChn->width[i];
	}
	pLineBitmapChn->total_width = pos;

	return HI_SUCCESS;
}

void * thread_update_region(HI_VOID *parg) {
	REGION_S *pstRegionCtl = (REGION_S *)parg;

	//Sync time after boot around 60s to avoid peripheral sync time
//	sleep(60);

	while (pstRegionCtl->bStart) {
		update_region();
		usleep(100000);
	}

	return NULL;
}

int start_thread_update_region() {
	stPre_tm.tm_year = stPre_tm.tm_mon = stPre_tm.tm_mday = -1;
	stPre_tm.tm_hour = stPre_tm.tm_min = stPre_tm.tm_sec = -1;

	threadUpdateRegion.bStart = HI_TRUE;
	pthread_create(&threadUpdateRegion.stRegionPid, 0, thread_update_region, &threadUpdateRegion);

	return 0;
}

int stop_thread_update_region() {
	if (threadUpdateRegion.bStart) {
		threadUpdateRegion.bStart = HI_FALSE;
		pthread_join(threadUpdateRegion.stRegionPid, 0);
	}
	return 0;
}

int create_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt, PIC_SIZE_E *enSize) {
	HI_S32 i;
	RGN_HANDLE RgnHandle[grpcnt];
	RGN_ATTR_S stRgnAttr[grpcnt];
    HI_S32 s32Ret = HI_FAILURE;
    MPP_CHN_S stChn;
    RGN_CHN_ATTR_S stChnAttr;

    BITMAP_S stBitmap;
	RGN_HANDLE RgnHandleLogo[grpcnt];
	RGN_ATTR_S stRgnAttrLogo[grpcnt];

    HI_CHAR cFileName[20];

    /****************************************
     step 1: create overlay regions
    ****************************************/
    for (i = 0; i < grpcnt; i++)
    {
        stRgnAttr[i].enType = OVERLAY_RGN;
        stRgnAttr[i].unAttr.stOverlay.enPixelFmt = PIXEL_FORMAT_RGB_1555;
        switch (enSize[i]) {
        	case PIC_HD720:
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Width  = REGION_SIZE_720P_WIDTH;
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Height = REGION_SIZE_720P_HEIGHT;
        		break;
        	case PIC_VGA:
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Width  = REGION_SIZE_VGA_WIDTH;
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Height = REGION_SIZE_VGA_HEIGHT;
        		break;
        	default:
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Width  = 0;
        		stRgnAttr[i].unAttr.stOverlay.stSize.u32Height = 0;
        		break;
        }

        RgnHandle[i] = i;
        s32Ret = HI_MPI_RGN_Create(RgnHandle[i], (stRgnAttr + i));
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_Create (%d) failed with %#x!\n", \
                   RgnHandle[i], s32Ret);
            return HI_FAILURE;
        }
        SAMPLE_PRT("the handle:%d,creat success!\n",RgnHandle[i]);
    }

    /*********************************************
     step 2: display overlay regions to venc groups
    *********************************************/
    for (i = 0; i < grpcnt; i++) {
        stChn.enModId = HI_ID_GROUP;
        stChn.s32DevId = VencGrpStart + i;
        stChn.s32ChnId = 0;

        memset(&stChnAttr,0,sizeof(stChnAttr));
        stChnAttr.bShow = HI_TRUE;
        stChnAttr.enType = OVERLAY_RGN;
        switch (enSize[i]) {
        	case PIC_HD720:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = REGION_POS_720P_X;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = REGION_POS_72P0_Y;
                break;
        	case PIC_VGA:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = REGION_POS_VGA_X;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = REGION_POS_VGA_Y;
                break;
        	default:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
                break;
        }
        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayChn.u32Layer = 0;

        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = HI_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp  = 0;

        s32Ret = HI_MPI_RGN_AttachToChn(RgnHandle[i], &stChn, &stChnAttr);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_AttachToChn (%d) failed with %#x!\n",\
                   RgnHandle[i], s32Ret);
            return HI_FAILURE;
        }
    }

    /*********************************************
     step 3: load stCharacters_vga bitmap (0 -> 9, '-', ' ', ':'
    *********************************************/
    for (i = 0; i < 10; i++) {
    	sprintf(cFileName, "RGN_SD_%d.bmp", i);
    	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_vga + i);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
	sprintf(cFileName, "RGN_SD_hyphen.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_vga + 10);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
	sprintf(cFileName, "RGN_SD_space.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_vga + 11);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
	sprintf(cFileName, "RGN_SD_colon.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_vga + 12);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*********************************************
     step 4: load stLineBitmap_720p bitmap (0 -> 9, '-', ' ', ':'
    *********************************************/
    for (i = 0; i < 10; i++) {
    	sprintf(cFileName, "RGN_HD_%d.bmp", i);
    	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_720p + i);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
	sprintf(cFileName, "RGN_HD_hyphen.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_720p + 10);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
	sprintf(cFileName, "RGN_HD_space.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_720p + 11);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
	sprintf(cFileName, "RGN_HD_colon.bmp");
	s32Ret = SAMPLE_RGN_LoadBmp(cFileName, stCharacters_720p + 12);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*********************************************
     step 5: create region to display current time with format
     "YYYY-MM-DD  hh:mm:ss"
     - 14 number
     - 2 hyphen
     - 2 space
     - 2 colon
     region size SD: w x h : 268 x 18
    *********************************************/
	//720p channel
    stLineBitmap_720p.bitmap.u32Width = REGION_SIZE_720P_WIDTH;
    stLineBitmap_720p.bitmap.u32Height = REGION_SIZE_720P_HEIGHT;
    stLineBitmap_720p.bitmap.pData = malloc(2*(REGION_SIZE_720P_WIDTH)*(REGION_SIZE_720P_HEIGHT));
    if(NULL == stLineBitmap_720p.bitmap.pData)
    {
        SAMPLE_PRT("malloc osd memroy err!\n");
        s32Ret = HI_FAILURE;
        goto ERR_MALLOC;
    }
    else
    	printf("malloc osd memroy 720p success = %d bytes!\n", 2*(REGION_SIZE_720P_WIDTH)*(REGION_SIZE_720P_HEIGHT));
    stLineBitmap_720p.bitmap.enPixelFormat = PIXEL_FORMAT_RGB_1555;


	//vga channel
	stLineBitmap_vga.bitmap.u32Width = REGION_SIZE_VGA_WIDTH;
	stLineBitmap_vga.bitmap.u32Height = REGION_SIZE_VGA_HEIGHT;
	stLineBitmap_vga.bitmap.pData = malloc(2*(REGION_SIZE_VGA_WIDTH)*(REGION_SIZE_VGA_HEIGHT));
    if(NULL == stLineBitmap_vga.bitmap.pData)
    {
        SAMPLE_PRT("malloc osd memroy err!\n");
        s32Ret = HI_FAILURE;
        goto ERR_MALLOC;
    }
    else
    	printf("malloc osd memroy VGA success = %d bytes!\n", 2*(REGION_SIZE_VGA_WIDTH)*(REGION_SIZE_VGA_HEIGHT));
	stLineBitmap_vga.bitmap.enPixelFormat = PIXEL_FORMAT_RGB_1555;

	//Save characters position
	save_characters_position(&stLineBitmap_vga, PIC_VGA);
	save_characters_position(&stLineBitmap_720p, PIC_HD720);

	//Copy special character to stLineBitmap_vga and stLineBitmap_720p
	save_spec_char_data(&stLineBitmap_vga, stCharacters_vga, PIC_VGA);
	save_spec_char_data(&stLineBitmap_720p, stCharacters_720p, PIC_HD720);

	update_region();

    /*********************************************
     step 6: logo
    *********************************************/
    for (i = 0; i < grpcnt; i++)
    {
    	stRgnAttrLogo[i].enType = OVERLAY_RGN;
    	stRgnAttrLogo[i].unAttr.stOverlay.enPixelFmt = PIXEL_FORMAT_RGB_1555;
        switch (enSize[i]) {
        	case PIC_HD720:
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Width  = LOGO_SIZE_720P_WIDTH;
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Height = LOGO_SIZE_720P_HEIGHT;
        		break;
        	case PIC_VGA:
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Width  = LOGO_SIZE_VGA_WIDTH;
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Height = LOGO_SIZE_VGA_HEIGHT;
        		break;
        	default:
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Width  = 0;
        		stRgnAttrLogo[i].unAttr.stOverlay.stSize.u32Height = 0;
        		break;
        }

        RgnHandleLogo[i] = i + 2;
        s32Ret = HI_MPI_RGN_Create(RgnHandleLogo[i], (stRgnAttrLogo + i));
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_Create (%d) failed with %#x!\n", \
            		RgnHandleLogo[i], s32Ret);
            return HI_FAILURE;
        }
        SAMPLE_PRT("the handle:%d,creat success!\n",RgnHandleLogo[i]);
    }

    for (i = 0; i < grpcnt; i++) {
        stChn.enModId = HI_ID_GROUP;
        stChn.s32DevId = VencGrpStart + i;
        stChn.s32ChnId = 0;

        memset(&stChnAttr,0,sizeof(stChnAttr));
        stChnAttr.bShow = HI_TRUE;
        stChnAttr.enType = OVERLAY_RGN;
        switch (enSize[i]) {
        	case PIC_HD720:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = LOGO_POS_720P_X;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = LOGO_POS_720P_Y;
                break;
        	case PIC_VGA:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = LOGO_POS_VGA_X;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = LOGO_POS_VGA_Y;
                break;
        	default:
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
                break;
        }
        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayChn.u32Layer = 0;

        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = HI_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp  = 0;

        s32Ret = HI_MPI_RGN_AttachToChn(RgnHandleLogo[i], &stChn, &stChnAttr);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_AttachToChn (%d) failed with %#x!\n",\
                   RgnHandle[i], s32Ret);
            return HI_FAILURE;
        }

        if (enSize[i] == PIC_HD720) {
#ifdef USE_VIETTEL_IDC_LOGO
        	s32Ret = SAMPLE_RGN_LoadBmp("logo_250x52.bmp", &stBitmap);
#else
        	s32Ret = SAMPLE_RGN_LoadBmp("logo_180x60.bmp", &stBitmap);
#endif //USE_VIETTEL_IDC_LOGO
        }
        else {
#ifdef USE_VIETTEL_IDC_LOGO
        	s32Ret = SAMPLE_RGN_LoadBmp("logo_126x26.bmp", &stBitmap);
#else
        	s32Ret = SAMPLE_RGN_LoadBmp("logo_90x30.bmp", &stBitmap);
#endif //USE_VIETTEL_IDC_LOGO
        }
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("load bmp failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_RGN_SetBitMap(RgnHandleLogo[i], &stBitmap);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_RGN_SetBitMap failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        if (NULL != stBitmap.pData)
        {
            free(stBitmap.pData);
            stBitmap.pData = NULL;
        }
    }

ERR_MALLOC:
    return s32Ret;
}

int destroy_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt) {
	HI_S32 i;
	HI_S32 s32Ret = HI_FAILURE;
	RGN_HANDLE RgnHandle[grpcnt];
	MPP_CHN_S stChn;

	//free memory for region
	for (i = 0; i < 13; i++) {
		free(stCharacters_720p[i].pData);
		free(stCharacters_vga[i].pData);
	}

	free(stLineBitmap_720p.bitmap.pData);
	free(stLineBitmap_vga.bitmap.pData);

	for (i = 0; i < grpcnt; i++) {
		RgnHandle[i] = i;
        stChn.enModId = HI_ID_GROUP;
        stChn.s32DevId = VencGrpStart + i;
        stChn.s32ChnId = 0;

        s32Ret = HI_MPI_RGN_DetachFrmChn(RgnHandle[i], &stChn);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_DetachFrmChn (%d) failed with %#x!\n",\
                   RgnHandle[i], s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_RGN_Destroy(RgnHandle[i]);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_RGN_Destroy [%d] failed with %#x\n",\
                    RgnHandle[i], s32Ret);
        }
	}

	return HI_SUCCESS;
}
