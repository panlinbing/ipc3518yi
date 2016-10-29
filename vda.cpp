/*
 * vda.cpp
 *
 *  Created on: Oct 17, 2016
 *      Author: hoang
 */


#include "vda.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VDA_SIZE_WIDTH		320
#define VDA_SIZE_HEIGHT		240

#define VDA_MOTION_DETECT_THREADHOLD	2000

static VDA_MD_PARAM_S gs_stMdParam;
static VI_CHN ViExtChn = 1;
static VDA_CHN VdaChn = 0;


/******************************************************************************
* funciton : vda MD mode print -- Md OBJ
******************************************************************************/
HI_S32 SAMPLE_COMM_VDA_MdPrtObj(FILE *fp, VDA_DATA_S *pstVdaData)
{
    VDA_OBJ_S *pstVdaObj;
    HI_S32 i;

    fprintf(fp, "===== %s =====\n", __FUNCTION__);

    if (HI_TRUE != pstVdaData->unData.stMdData.bObjValid)
    {
        fprintf(fp, "bMbObjValid = FALSE.\n");
        return HI_SUCCESS;
    }

    fprintf(fp, "ObjNum=%d, IndexOfMaxObj=%d, SizeOfMaxObj=%d, SizeOfTotalObj=%d\n", \
                   pstVdaData->unData.stMdData.stObjData.u32ObjNum, \
		     pstVdaData->unData.stMdData.stObjData.u32IndexOfMaxObj, \
		     pstVdaData->unData.stMdData.stObjData.u32SizeOfMaxObj,\
		     pstVdaData->unData.stMdData.stObjData.u32SizeOfTotalObj);
    for (i=0; i<pstVdaData->unData.stMdData.stObjData.u32ObjNum; i++)
    {
        pstVdaObj = pstVdaData->unData.stMdData.stObjData.pstAddr + i;
        fprintf(fp, "[%d]\t left=%d, top=%d, right=%d, bottom=%d\n", i, \
			  pstVdaObj->u16Left, pstVdaObj->u16Top, \
			  pstVdaObj->u16Right, pstVdaObj->u16Bottom);
    }
    fflush(fp);
    return HI_SUCCESS;
}
/******************************************************************************
* funciton : vda MD mode print -- Alarm Pixel Count
******************************************************************************/
HI_S32 SAMPLE_COMM_VDA_MdPrtAp(FILE *fp, VDA_DATA_S *pstVdaData)
{
    fprintf(fp, "===== %s =====\n", __FUNCTION__);

    if (HI_TRUE != pstVdaData->unData.stMdData.bPelsNumValid)
    {
        fprintf(fp, "bMbObjValid = FALSE.\n");
        return HI_SUCCESS;
    }

    fprintf(fp, "AlarmPixelCount=%d\n", pstVdaData->unData.stMdData.u32AlarmPixCnt);
    fflush(fp);
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : vda MD mode print -- SAD
******************************************************************************/
HI_S32 SAMPLE_COMM_VDA_MdPrtSad(FILE *fp, VDA_DATA_S *pstVdaData)
{
    HI_S32 i, j;
    HI_VOID *pAddr;

    fprintf(fp, "===== %s =====\n", __FUNCTION__);
    if (HI_TRUE != pstVdaData->unData.stMdData.bMbSadValid)
    {
        fprintf(fp, "bMbSadValid = FALSE.\n");
        return HI_SUCCESS;
    }

    for(i=0; i<pstVdaData->u32MbHeight; i++)
    {
		pAddr = (HI_VOID *)((HI_U32)pstVdaData->unData.stMdData.stMbSadData.pAddr
		  			+ i * pstVdaData->unData.stMdData.stMbSadData.u32Stride);

		for(j=0; j<pstVdaData->u32MbWidth; j++)
		{
	        HI_U8  *pu8Addr;
	        HI_U16 *pu16Addr;

	        if(VDA_MB_SAD_8BIT == pstVdaData->unData.stMdData.stMbSadData.enMbSadBits)
	        {
	            pu8Addr = (HI_U8 *)pAddr + j;

	            fprintf(fp, "%-2x",*pu8Addr);

	        }
	        else
	        {
	            pu16Addr = (HI_U16 *)pAddr + j;

				fprintf(fp, "%-4x",*pu16Addr);
	        }
		}

        printf("\n");
    }

    fflush(fp);
    return HI_SUCCESS;
}

int check_motion(VDA_DATA_S *pstVdaData) {
    HI_S32 i, j;
    HI_VOID *pAddr;
    FILE *fp = stdout;
    unsigned int total = 0;

    if (HI_TRUE != pstVdaData->unData.stMdData.bMbSadValid)
    {
        fprintf(fp, "bMbSadValid = FALSE.\n");
        return HI_SUCCESS;
    }

    for(i=0; i<pstVdaData->u32MbHeight; i++)
    {
		pAddr = (HI_VOID *)((HI_U32)pstVdaData->unData.stMdData.stMbSadData.pAddr
		  			+ i * pstVdaData->unData.stMdData.stMbSadData.u32Stride);

		for(j=0; j<pstVdaData->u32MbWidth; j++)
		{
	        HI_U8  *pu8Addr;
            pu8Addr = (HI_U8 *)pAddr + j;
//            fprintf(fp, "%-2x",*pu8Addr);

            total += *pu8Addr;
		}
    }

    fprintf(fp, "===== %s ===== %u\n", __FUNCTION__, total);
    fflush(fp);

    return 0;
}

/******************************************************************************
* funciton : vda MD mode thread process
******************************************************************************/
int i = 0;
HI_VOID *SAMPLE_COMM_VDA_MdGetResult(HI_VOID *pdata)
{
    HI_S32 s32Ret;
    VDA_CHN VdaChn;
    VDA_DATA_S stVdaData;
    VDA_MD_PARAM_S *pgs_stMdParam;
    HI_S32 maxfd = 0;
    FILE *fp = stdout;
    HI_S32 VdaFd;
    fd_set read_fds;
    struct timeval TimeoutVal;

    pgs_stMdParam = (VDA_MD_PARAM_S *)pdata;

    VdaChn   = pgs_stMdParam->VdaChn;

    /* decide the stream file name, and open file to save stream */
    /* Set Venc Fd. */
    VdaFd = HI_MPI_VDA_GetFd(VdaChn);
    if (VdaFd < 0)
    {
        SAMPLE_PRT("HI_MPI_VDA_GetFd failed with %#x!\n",
               VdaFd);
        return NULL;
    }
    if (maxfd <= VdaFd)
    {
        maxfd = VdaFd;
    }


    system("clear");
    while (HI_TRUE == pgs_stMdParam->bThreadStart)
    {
        FD_ZERO(&read_fds);
        FD_SET(VdaFd, &read_fds);

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            break;
        }
        else
        {
            if (FD_ISSET(VdaFd, &read_fds))
            {
                /*******************************************************
                   step 2.3 : call mpi to get one-frame stream
                   *******************************************************/
                s32Ret = HI_MPI_VDA_GetData(VdaChn, &stVdaData, HI_TRUE);
                if(s32Ret != HI_SUCCESS)
                {
                    SAMPLE_PRT("HI_MPI_VDA_GetData failed with %#x!\n", s32Ret);
                    return NULL;
                }
                /*******************************************************
                   *step 2.4 : save frame to file
                   *******************************************************/
                system("clear");
                printf("\033[0;0H");/*move cursor*/
                fprintf(fp, "%d ", i++);
                check_motion(&stVdaData);

		        SAMPLE_COMM_VDA_MdPrtSad(fp, &stVdaData);
		        SAMPLE_COMM_VDA_MdPrtObj(fp, &stVdaData);
                SAMPLE_COMM_VDA_MdPrtAp(fp, &stVdaData);
                /*******************************************************
                   *step 2.5 : release stream
                   *******************************************************/
                s32Ret = HI_MPI_VDA_ReleaseData(VdaChn,&stVdaData);
                if(s32Ret != HI_SUCCESS)
	            {
	                SAMPLE_PRT("HI_MPI_VDA_ReleaseData failed with %#x!\n", s32Ret);
	                return NULL;
	            }
            }
        }
    }

    return HI_NULL;
}

int start_thread_check_motion() {
	HI_S32 s32Ret;
    /* step 3: vda chn start recv picture */
    s32Ret = HI_MPI_VDA_StartRecvPic(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err!\n");
        return s32Ret;
    }

    /* step 4: create thread to get result */
    gs_stMdParam.bThreadStart = HI_TRUE;
    gs_stMdParam.VdaChn   = VdaChn;

    pthread_create(&gs_stMdParam.stVdaPid, 0, SAMPLE_COMM_VDA_MdGetResult, (HI_VOID *)&gs_stMdParam);

	return 0;
}

int stop_thread_check_motion() {
	HI_S32 s32Ret;
    /* join thread */
    if (HI_TRUE == gs_stMdParam.bThreadStart)
    {
    	gs_stMdParam.bThreadStart = HI_FALSE;
        pthread_join(gs_stMdParam.stVdaPid, 0);
    }

    /* vda stop recv picture */
    s32Ret = HI_MPI_VDA_StopRecvPic(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err(0x%x)!!!!\n",s32Ret);
    }

	return 0;
}

int init_vda_modul(VDA_CHN stVdaChn) {
	HI_S32 s32Ret;
    VI_EXT_CHN_ATTR_S stViExtChnAttr;

    VDA_CHN_ATTR_S stVdaChnAttr;
    MPP_CHN_S stSrcChn, stDestChn;

    /******************************************
     section 1: start vi ext chn to capture
    ******************************************/
    stViExtChnAttr.s32BindChn           = 0;
    stViExtChnAttr.stDestSize.u32Width  = 320;
    stViExtChnAttr.stDestSize.u32Height = 240;
    stViExtChnAttr.s32SrcFrameRate      = 30;
    stViExtChnAttr.s32FrameRate         = 30;
    stViExtChnAttr.enPixFormat          = SAMPLE_PIXEL_FORMAT;

    s32Ret = HI_MPI_VI_SetExtChnAttr(ViExtChn, &stViExtChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set vi  extchn failed!\n");
        return -1;
    }
    s32Ret = HI_MPI_VI_EnableChn(ViExtChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set vi  extchn failed!\n");
        return -1;
    }

    VdaChn = stVdaChn;

    sleep(2);
    /******************************************
     ssection  2: VDA process
    ******************************************/
    /* step 1 create vda channel */
    stVdaChnAttr.enWorkMode = VDA_WORK_MODE_MD;
    stVdaChnAttr.u32Width   = VDA_SIZE_WIDTH;
    stVdaChnAttr.u32Height  = VDA_SIZE_HEIGHT;

    stVdaChnAttr.unAttr.stMdAttr.enVdaAlg      = VDA_ALG_REF;
    stVdaChnAttr.unAttr.stMdAttr.enMbSize      = VDA_MB_16PIXEL;
    stVdaChnAttr.unAttr.stMdAttr.enMbSadBits   = VDA_MB_SAD_8BIT;
    stVdaChnAttr.unAttr.stMdAttr.enRefMode     = VDA_REF_MODE_DYNAMIC;
    stVdaChnAttr.unAttr.stMdAttr.u32MdBufNum   = 8;
    stVdaChnAttr.unAttr.stMdAttr.u32VdaIntvl   = 4;
    stVdaChnAttr.unAttr.stMdAttr.u32BgUpSrcWgt = 128;
    stVdaChnAttr.unAttr.stMdAttr.u32SadTh      = 240;
    stVdaChnAttr.unAttr.stMdAttr.u32ObjNumMax  = 128;

    s32Ret = HI_MPI_VDA_CreateChn(VdaChn, &stVdaChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err!\n");
        return s32Ret;
    }

    /* step 2: vda chn bind vi chn */
    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = ViExtChn;
    stSrcChn.s32DevId = 0;

    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = VdaChn;
    stDestChn.s32DevId = 0;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err!\n");
        return s32Ret;
    }

	return 0;
}

int destroy_vda_modul() {
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;

    stSrcChn.enModId = HI_ID_VIU;
    stSrcChn.s32ChnId = ViExtChn;
    stSrcChn.s32DevId = 0;
    stDestChn.enModId = HI_ID_VDA;
    stDestChn.s32ChnId = VdaChn;
    stDestChn.s32DevId = 0;

    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err(0x%x)!!!!\n", s32Ret);
    }

    /* destroy vda chn */
    s32Ret = HI_MPI_VDA_DestroyChn(VdaChn);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("err(0x%x)!!!!\n", s32Ret);
    }

    HI_MPI_VI_DisableChn(ViExtChn);

	return 0;
}

