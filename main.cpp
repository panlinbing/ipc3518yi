/*
 * main.cpp
 *
 *  Created on: Aug 8, 2016
 *      Author: hoang
 */

//#ifdef __cplusplus
//#if __cplusplus
//extern "C"{
//#endif
//#endif /* End of #ifdef __cplusplus */

#include "main.h"
#include "region.h"
#include "iq.h"
#include "rtmp/rtmp.h"
#include "json.h"
#include "vda.h"
#include "ClientSock.hpp"
#include "Sclient.hpp"
#include "md5.h"
#include "stunclient.hpp"
//#ifdef USE_VIETTEL_IDC
#include "aes.h"
#include "curl/curl.h"
#include <iomanip>
//#endif //USE_VIETTEL_IDC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <ifaddrs.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "sample_comm.h"
#include "zbar.h"
#include "opencv/highgui.h"

using namespace std;
using namespace zbar;
using namespace cv;

#define nalu_sent_len        1305
#define RTP_H264                    96
#define MAX_CHAN                 8
#define RTP_AUDIO              97
#define MAX_RTSP_CLIENT       4
#define RTSP_SERVER_PORT      554
#define RTSP_RECV_SIZE        1024
#define RTSP_MAX_VID          (640*1024)
#define RTSP_MAX_AUD          (15*1024)

#define AU_HEADER_SIZE    4
#define PARAM_STRING_MAX        100

typedef struct tagSAMPLE_AENC_S
{
    HI_BOOL bStart;
    pthread_t stAencPid;
    HI_S32  AeChn;
    HI_S32  AdChn;
    FILE    *pfd;
    HI_BOOL bSendAdChn;
} SAMPLE_AENC_S;

typedef struct tagSAMPLE_ADEC_S
{
    HI_BOOL bStart;
    HI_S32 AdChn;
    FILE *pfd;
    pthread_t stAdPid;
    HI_S32  AoDev;
    HI_S32  AoChn;
} SAMPLE_ADEC_S;

typedef struct
{
	int index;
	int socket;
	int status;
	char IP[20];
}AUDIO_CLIENT;

static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
static pthread_t gs_VencPid;
static SAMPLE_AENC_S gs_stSampleAenc;
//static pthread_t gs_AencPid;
static SAMPLE_ADEC_S gs_stStartScan;
static SAMPLE_ADEC_S gs_stScanSuccess;
static SAMPLE_ADEC_S gs_stNotificationFile;

static pthread_t threadSyncTime;
static pthread_t threadInitRTMP;
static pthread_t threadReceiveAudioData;
#define AUDIO_SERVER_PORT      2000
AUDIO_CLIENT g_AudioClients[MAX_RTSP_CLIENT];
#define AUDIO_RECV_SIZE        4000
#define MAX_AUDIO_CLIENT       4
static PAYLOAD_TYPE_E gs_AudioClientPayloadType = PT_LPCM;

#define AUTHEN_RECV_SIZE        4000
static pthread_t threadAuthenServer;
#define AUTHEN_SERVER_PORT      10000

//RTMP
static HI_BOOL send_RTMP_to_server = HI_FALSE;
static HI_BOOL reInitRTMP = HI_TRUE;
#ifdef ALWAYS_SEND_RTMP
int has_server_key = 1;
#else
int has_server_key = 0;
#endif

static pthread_t threadAuthenServerdetect;
static bool detect_is_start;

//detect motion
static VDA_MD_PARAM_S gs_stMdParam;
static VDA_CHN VdaChn = 0;
static DETECT_SENDING_STEP detect_sending_server = DETECT_IDLE;
static unsigned char detect_time_left = 60;
static pthread_t thread_detect_check_time_left;
#define VDA_MOTION_DETECT_THREADHOLD	4000

//HC
static pthread_t thread_read_HC_data;

//VTIDC
static std::string VTIDC_rtmp_url;

//cam info
static char camname[MAX_USER_PASS_LEN];
static char user[MAX_USER_PASS_LEN];
static char pass[MAX_USER_PASS_LEN];
static char camip[20];
static char port[10];
static char camid[CAMID_LEN + 5];
static bool has_setup;

typedef unsigned short u_int16_t;
typedef unsigned char u_int8_t;
typedef u_int16_t portNumBits;
typedef u_int32_t netAddressBits;
typedef long long _int64;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif
#define AUDIO_RATE    8000
#define PACKET_BUFFER_END            (unsigned int)0x00000000

typedef struct
{
	int startblock;
	int endblock;
	int BlockFileNum;
}IDXFILEHEAD_INFO;

typedef struct
{
	_int64 starttime;
	_int64 endtime;
	int startblock;
	int endblock;
	int stampnum;
}IDXFILEBLOCK_INFO;

typedef struct
{
	int blockindex;
	int pos;
	_int64 time;
}IDXSTAMP_INFO;

typedef struct
{
	char filename[150];
	int pos;
	_int64 time;
}FILESTAMP_INFO;

typedef struct
{
	char channelid[9];
	_int64 starttime;
	_int64 endtime;
	_int64 session;
	int		type;
	int		encodetype;
}FIND_INFO;

typedef enum
{
	RTP_UDP,
	RTP_TCP,
	RAW_UDP
}StreamingMode;


RTP_FIXED_HEADER  *rtp_hdr, *rtp_hdr_audio;
NALU_HEADER		  *nalu_hdr;
FU_INDICATOR	  *fu_ind;
FU_HEADER		  *fu_hdr;
AU_HEADER            *au_hdr;
RTSP_INTERLEAVED_FRAME	*rtsp_interleaved, *rtsp_interleaved_audio;

extern char g_rtp_playload[20];
extern int   g_audio_rate;

typedef enum
{
	RTSP_IDLE = 0,
	RTSP_CONNECTED = 1,
	RTSP_SENDING0 = 2,	//stream channel 0
	RTSP_SENDING1 = 3,  //stream channel 1
}RTSP_STATUS;

typedef struct
{
	int  nVidLen;
	int  nAudLen;
	int bIsIFrm;
	int bWaitIFrm;
	int bIsFree;
	char vidBuf[RTSP_MAX_VID];
	char audBuf[RTSP_MAX_AUD];
}RTSP_PACK;

typedef struct
{
	int index;
	int socket;
	int reqchn;
	int seqnum;
	int seqnum2;
	unsigned int tsvid;
	unsigned int tsaud;
	int status;
	int sessionid;
	int rtpport[2];
	int rtcpport;
	unsigned char rtpChannelID[2];
	unsigned char rtcpChannelID[2];
	StreamingMode streamMode;
	char IP[20];
	char urlPre[PARAM_STRING_MAX];
}RTSP_CLIENT;

//P2P
#ifdef USE_TEST_P2P
#define MAX_P2P_CLIENT	10
#define VIDEO_PORT_P2P  30000
int udpfd_video_p2p = -1;
typedef enum
{
	RTP_IDLE = 0,
	RTP_SENDING0 = 2,	//stream channel 0
	RTP_SENDING1 = 3,  //stream channel 1
}RTP_STATUS;
typedef struct
{
	unsigned int seqnum[2];
	unsigned int timestamp[2];
	int status;
	int rtpport[2];
	char ip[20];
}P2P_CLIENT;
P2P_CLIENT g_p2pClients[MAX_P2P_CLIENT];
unsigned char g_p2pNumClients = 0;
#endif //USE_TEST_P2P

typedef struct
{
	int  vidLen;
	int  audLen;
	int  nFrameID;
	char vidBuf[RTSP_MAX_VID];
	char audBuf[RTSP_MAX_AUD];
}FRAME_PACK;
typedef struct
{
  int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  unsigned max_size;            //! Nal Unit Buffer size
  int forbidden_bit;            //! should be always FALSE
  int nal_reference_idc;        //! NALU_PRIORITY_xxxx
  int nal_unit_type;            //! NALU_TYPE_xxxx
  char *buf;                    //! contains the first byte followed by the EBSP
  unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

typedef enum {
    START_SCAN = 0,
    SCAN_SUCCESS  = 1,
	WIFI_CONNECTED = 2,
	WIFI_FAILED = 3,
} AUDIO_FILE;

FRAME_PACK g_FrmPack[MAX_CHAN];
RTSP_PACK g_rtpPack[MAX_CHAN];
RTSP_CLIENT g_rtspClients[MAX_RTSP_CLIENT];

int g_nSendDataChn = -1;
pthread_mutex_t g_mutex;
pthread_cond_t  g_cond;
pthread_mutex_t g_sendmutex;

pthread_t g_SendDataThreadId = 0;
//HAL_CLIENT_HANDLE hMainStreamClient = NULL,hSubStreamClient = NULL,hAudioClient = NULL;
char g_rtp_playload[20];
int   g_audio_rate = 8000;
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_NTSC;//30fps
int g_nframerate;
//VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;//15fps
//int g_nframerate = 15;
int exitok = 0;
int udpfd_video, udpfd_audio;
#define VIDEO_PORT 20000
#define AUDIO_PORT 20002

#define SAMPLE_AUDIO_PTNUMPERFRM   160

static HI_U32 PTS_INC = 0;

// PT_G711A - PT_G711U - PT_ADPCMA - PT_G726 - PT_LPCM
static PAYLOAD_TYPE_E gs_enPayloadType = PT_LPCM;
static PAYLOAD_TYPE_E gs_enPayloadType_WifiScan = PT_G711U;

static HI_BOOL gs_bMicIn = HI_TRUE;

static HI_BOOL gs_bAiAnr = HI_FALSE;
static HI_BOOL gs_bAioReSample = HI_FALSE;
static HI_BOOL gs_bUserGetMode = HI_FALSE;
static AUDIO_RESAMPLE_ATTR_S *gs_pstAiReSmpAttr = NULL;
static AUDIO_RESAMPLE_ATTR_S *gs_pstAoReSmpAttr = NULL;

#define ENDIAN_SWAP16(data)  	((data >> 8) | ((data & 0x00ff) << 8))
		/* right shift 1 bytes */ /* left shift 1 byte */

#define SAMPLE_DBG(s32Ret)\
do{\
    printf("s32Ret=%#x,fuc:%s,line:%d\n", s32Ret, __FUNCTION__, __LINE__);\
}while(0)

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VENC_Usage(char *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0: 720p classic H264 encode.\n");
    printf("\t 1: wifi - login\n");
    printf("\t 2: 720p STREAM.\n");
    printf("\t 3: wifi connected.\n");
    printf("\t 4: wifi failed.\n");
    printf("\t 5: sync time.\n");

    printf("\t S: stream RTMP to server.\n");
    printf("\t 7: get cam id.\n");
    printf("\t H: test connect HC.\n");
    printf("\t V: test_Viettel_IDC.\n");

    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VENC_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

/******************************************************************************
* function :  H.264@720p@30fps+H.264@VGA@30fps+H.264@QVGA@30fps
******************************************************************************/
HI_S32 SAMPLE_VENC_720P_CLASSIC(HI_VOID)
{
    PAYLOAD_TYPE_E enPayLoad[3]= {PT_H264, PT_H264, PT_H264};
    PIC_SIZE_E enSize[3] = {PIC_HD720, PIC_VGA, PIC_QVGA};

    VB_CONF_S stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    VPSS_CHN_MODE_S stVpssChnMode;

    VENC_GRP VencGrp;
    VENC_CHN VencChn;
    SAMPLE_RC_E enRcMode= SAMPLE_RC_CBR;
    HI_S32 s32ChnNum = 2;

    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;

    /******************************************
     step  1: init sys variable
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    switch(SENSOR_TYPE)
    {
        case SONY_IMX122_DC_1080P_30FPS:
        case APTINA_MT9P006_DC_1080P_30FPS:
            enSize[0] = PIC_HD1080;
            break;

        default:
            break;
    }

    stVbConf.u32MaxPoolCnt = 128;

    /*video buffer*/
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize[0], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 8;

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize[1], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[1].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 2;

//    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
//                enSize[2], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
//    stVbConf.astCommPool[2].u32BlkSize = u32BlkSize;
//    stVbConf.astCommPool[2].u32BlkCnt = 2;

    /* hist buf*/
//    stVbConf.astCommPool[1].u32BlkSize = (196*4);
//    stVbConf.astCommPool[1].u32BlkCnt = 6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_VENC_720P_CLASSIC_0;
    }

    /******************************************
     step 3: start vi dev & chn to capture
    ******************************************/
    stViConfig.enViMode   = SENSOR_TYPE;
    stViConfig.enRotate   = ROTATE_NONE;
    stViConfig.enNorm     = VIDEO_ENCODING_MODE_AUTO;
    stViConfig.enViChnSet = VI_CHN_SET_NORMAL;
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto END_VENC_720P_CLASSIC_1;
    }

    /******************************************
     step 4: start vpss and vi bind vpss
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize[0], &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END_VENC_720P_CLASSIC_1;
    }

    VpssGrp = 0;
    stVpssGrpAttr.u32MaxW = stSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSize.u32Height;
    stVpssGrpAttr.bDrEn = HI_FALSE;
    stVpssGrpAttr.bDbEn = HI_FALSE;
    stVpssGrpAttr.bIeEn = HI_TRUE;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.bHistEn = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, &stVpssGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_VENC_720P_CLASSIC_2;
    }

    s32Ret = SAMPLE_COMM_VI_BindVpss(stViConfig.enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vi bind Vpss failed!\n");
        goto END_VENC_720P_CLASSIC_3;
    }

    VpssChn = 0;
    memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
    stVpssChnAttr.bFrameEn = HI_FALSE;
    stVpssChnAttr.bSpEn    = HI_FALSE;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, HI_NULL, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Enable vpss chn failed!\n");
        goto END_VENC_720P_CLASSIC_4;
    }

    VpssChn = 1;
    stVpssChnMode.enChnMode     = VPSS_CHN_MODE_USER;
    stVpssChnMode.bDouble       = HI_FALSE;
    stVpssChnMode.enPixelFormat = SAMPLE_PIXEL_FORMAT;
    stVpssChnMode.u32Width      = 640;
    stVpssChnMode.u32Height     = 360;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, &stVpssChnMode, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Enable vpss chn failed!\n");
        goto END_VENC_720P_CLASSIC_4;
    }

//    VpssChn = 3;
//    stVpssExtChnAttr.s32BindChn = 1;
//    stVpssExtChnAttr.s32SrcFrameRate = 30;
//    stVpssExtChnAttr.s32DstFrameRate = 30;
//    stVpssExtChnAttr.enPixelFormat   = SAMPLE_PIXEL_FORMAT;
//    stVpssExtChnAttr.u32Width        = 352;
//    stVpssExtChnAttr.u32Height       = 192;
//    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, HI_NULL, HI_NULL, &stVpssExtChnAttr);
//    if (HI_SUCCESS != s32Ret)
//    {
//        SAMPLE_PRT("Enable vpss chn failed!\n");
//        goto END_VENC_720P_CLASSIC_4;
//    }

    /******************************************
     step 5: start stream venc
    ******************************************/
    /*** HD720P **/
    VpssGrp = 0;
    VpssChn = 0;
    VencGrp = 0;
    VencChn = 0;
    s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[0],\
                                   gs_enNorm, enSize[0], enRcMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencGrp, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    /*** vga **/
    VpssChn = 1;
    VencGrp = 1;
    VencChn = 1;
    s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[1], \
                                    gs_enNorm, enSize[1], enRcMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    /*** vga **/
//    VpssChn = 3;
//    VencGrp = 2;
//    VencChn = 2;
//    s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[2], \
//                                    gs_enNorm, enSize[2], enRcMode);
//    if (HI_SUCCESS != s32Ret)
//    {
//        SAMPLE_PRT("Start Venc failed!\n");
//        goto END_VENC_720P_CLASSIC_5;
//    }
//
//    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VpssChn);
//    if (HI_SUCCESS != s32Ret)
//    {
//        SAMPLE_PRT("Start Venc failed!\n");
//        goto END_VENC_720P_CLASSIC_5;
//    }

    /******************************************
     step 6: stream venc process -- get stream, then save it to file.
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    printf("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     step 7: exit process
    ******************************************/
    SAMPLE_COMM_VENC_StopGetStream();

END_VENC_720P_CLASSIC_5:
    VpssGrp = 0;

    VpssChn = 0;
    VencGrp = 0;
    VencChn = 0;
    SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
    SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);

    VpssChn = 1;
    VencGrp = 1;
    VencChn = 1;
    SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
    SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);

//    VpssChn = 3;server_port=20000-20002
//    VencGrp = 2;
//    VencChn = 2;
//    SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
//    SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);

    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_4:	//vpss stop
    VpssGrp = 0;
//    VpssChn = 3;
//    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
    VpssChn = 0;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
    VpssChn = 1;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
END_VENC_720P_CLASSIC_3:    //vpss stop
    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_2:    //vpss stop
    SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
END_VENC_720P_CLASSIC_1:	//vi stop
    SAMPLE_COMM_VI_StopVi(&stViConfig);
END_VENC_720P_CLASSIC_0:	//system exit
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/*****************************************************************************
 * function : decode wifi info from image
 */
HI_S8 decode_wifi_info(HI_S32 name) {
	ImageScanner scanner;
	std::string data, ssid, type, pass;
	int index1, index2, index3;
	FILE *pFile;

	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
	// obtain image data
	char char_t[128]  = {0};
//	sprintf(char_t, "snap_%d.jpg", name);
	sprintf(char_t, "/tmp/snap_0.jpg");
	Mat img = imread(char_t, 0);

	int width = img.cols;
	int height = img.rows;
	uchar *raw = (uchar *)img.data;
	// wrap image data
	Image image(width, height, "Y800", raw, width * height);
	// scan the image for barcodes
	scanner.scan(image);
	// extract results

	int count = 0;
	for(Image::SymbolIterator symbol = image.symbol_begin();
			symbol != image.symbol_end();
			++symbol) {
		vector<Point> vp;
		// do something useful with results
		data = symbol->get_data();
		index1 = index2 = 0;

		printf("decoded %s symbol: "" %s ""\n", symbol->get_type_name().c_str(), data.c_str());

		//read wifi info
		index1 = data.find("ssid=");
		index2 = data.find("type=");
		if ((index1 == string::npos) || (index1 == string::npos) || (data[data.length()-1] != '|'))
			continue;

		ssid = data.substr(index1 + 5, data.find("|", index1) - index1 - 5);
		type = data.substr(index2 + 5, data.find("|", index2) - index2 - 5);

		if (type.compare("WPA-PSK") == 0) {
			index3 = data.find("pass=");
			pass = data.substr(index3 + 5, data.find("|", index3) - index3 - 5);
		}

		printf("ssid: %s\n", ssid.c_str());
		printf("type: %s\n", type.c_str());
		printf("pass: %s\n", pass.c_str());

		//write wifi info to qpa_supplicant.conf
		pFile = fopen("/etc/wpa_supplicant.conf", "wb");

		sprintf(char_t, "ctrl_interface=/var/run/wpa_supplicant\n");
		fwrite(char_t, 39, 1, pFile);

		sprintf(char_t, "ap_scan=1\n");
		fwrite(char_t, 10, 1, pFile);

		sprintf(char_t, "network={\n");
		fwrite(char_t, 10, 1, pFile);

		sprintf(char_t, "ssid=\"%s\"\n", ssid.c_str());
		fwrite(char_t, ssid.length() + 8, 1, pFile);

		sprintf(char_t, "scan_ssid=1\n");
		fwrite(char_t, 12, 1, pFile);

		if (type.compare("none") == 0) {
			sprintf(char_t, "key_mgmt=NONE\n");
			fwrite(char_t, 14, 1, pFile);
		}
		else if (type.compare("WPA-PSK") == 0) {
			sprintf(char_t, "proto=WPA RSN\n");
			fwrite(char_t, 14, 1, pFile);

			sprintf(char_t, "key_mgmt=WPA-PSK\n");
			fwrite(char_t, 17, 1, pFile);

			sprintf(char_t, "pairwise=CCMP TKIP\n");
			fwrite(char_t, 19, 1, pFile);

			sprintf(char_t, "group=CCMP TKIP\n");
			fwrite(char_t, 16, 1, pFile);

			sprintf(char_t, "psk=\"%s\"\n", pass.c_str());
			fwrite(char_t, pass.length() + 7, 1, pFile);
		}

		sprintf(char_t, "}\n");
		fwrite(char_t, 2, 1, pFile);

		fclose(pFile);
		count++;
	}
	printf("count = %d\n", count);
	// clean up
	image.set_data(NULL, 0);

	return count;
}

static FILE *SAMPLE_AUDIO_Open_File(AUDIO_FILE file_name)
{
    FILE *pfd;
    HI_CHAR aszFileName[128];

    /* create file start_scan.g711*/
    if (file_name == START_SCAN)
    	sprintf(aszFileName, "start_scan.g711u");
    else if (file_name == SCAN_SUCCESS)
    	sprintf(aszFileName, "scan_success.g711u");
    else if (file_name == WIFI_CONNECTED)
    	sprintf(aszFileName, "wifi_connected.g711u");
    else if (file_name == WIFI_FAILED)
    	sprintf(aszFileName, "wifi_failed.g711u");
    pfd = fopen(aszFileName, "rb");
    if (NULL == pfd)
    {
        printf("%s: open file %s failed\n", __FUNCTION__, aszFileName);
        return NULL;
    }
    return pfd;
}

void *PlayNotificationFile(void *parg)
{
    HI_S32 s32Ret;
    AUDIO_STREAM_S stAudioStream;
    HI_U32 u32Len = 164;
    HI_U32 u32ReadLen;
    HI_S32 s32AdecChn;
    HI_U8 *pu8AudioStream = NULL;
    SAMPLE_ADEC_S *pstAdecCtl = (SAMPLE_ADEC_S *)parg;
    FILE *pfd = pstAdecCtl->pfd;
    s32AdecChn = pstAdecCtl->AdChn;

    pu8AudioStream = (HI_U8*)malloc(sizeof(HI_U8)*MAX_AUDIO_STREAM_LEN);
    if (NULL == pu8AudioStream)
    {
        printf("%s: malloc failed!\n", __FUNCTION__);
        return NULL;
    }

    //Output 1 at GPIO 4 - 1
    HI_U32 value, addr = 0x201803FC;
    HI_S32 rc;

    rc = HI_MPI_SYS_GetReg(addr, &value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_GetReg failed 0x%x\n", __FUNCTION__, addr);
    }
    value |= 0x00000002;
    rc = HI_MPI_SYS_SetReg(addr, value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_SetReg failed 0x%x\n", __FUNCTION__, addr);
    }

    while (HI_TRUE == pstAdecCtl->bStart)
    {
        /* read from file */
        stAudioStream.pStream = pu8AudioStream;
        u32ReadLen = fread(stAudioStream.pStream, 1, u32Len, pfd);

        if (u32ReadLen <= 0)
        {
            fseek(pfd, 0, SEEK_SET);/*read file again*/

            printf("%s: read file end!\n", __FUNCTION__);
            pstAdecCtl->bStart = HI_FALSE;
            break;
        }

        stAudioStream.u32Len = u32ReadLen;
        /* here only demo adec streaming sending mode, but pack sending mode is commended */
        s32Ret = HI_MPI_ADEC_SendStream(s32AdecChn, &stAudioStream, HI_TRUE);
        if (s32Ret)
        {
            printf("%s: HI_MPI_ADEC_SendStream(%d) failed with %#x!\n",\
                   __FUNCTION__, s32AdecChn, s32Ret);
            break;
        }
    }

    //Output 0 at GPIO 4 - 1
    rc = HI_MPI_SYS_GetReg(addr, &value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_GetReg failed 0x%x\n", __FUNCTION__, addr);
    }
    value &= 0xFFFFFFFD;
    rc = HI_MPI_SYS_SetReg(addr, value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_SetReg failed 0x%x\n", __FUNCTION__, addr);
    }

    free(pu8AudioStream);
    pu8AudioStream = NULL;
    fclose(pfd);
    pstAdecCtl->bStart = HI_FALSE;

    SAMPLE_COMM_AUDIO_StopAo(pstAdecCtl->AoDev, pstAdecCtl->AoChn, gs_bAioReSample);
    SAMPLE_COMM_AUDIO_StopAdec(s32AdecChn);
    SAMPLE_COMM_AUDIO_AoUnbindAdec(pstAdecCtl->AoDev, pstAdecCtl->AoChn, s32AdecChn);

    return NULL;
}

/******************************************************************************
* function :  wifi login
******************************************************************************/
HI_S32 WIFI_LOGIN(HI_VOID)
{
    PIC_SIZE_E enSize = PIC_HD720;

    VB_CONF_S stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig;

    VENC_GRP VencGrp;
    VENC_CHN VencChn;

    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;
    HI_S8 qr_ret = 0;

    AUDIO_DEV   AoDev = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    FILE        *pfd_start = NULL;
    FILE        *pfd_success = NULL;
    AIO_ATTR_S stAioAttr;

//    HI_S32 i;

    switch(SENSOR_TYPE)
    {
        case SONY_IMX122_DC_1080P_30FPS:
        case APTINA_MT9P006_DC_1080P_30FPS:
            enSize = PIC_HD1080;
            break;

        default:
            break;
    }

    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    stVbConf.u32MaxPoolCnt = 128;

    /* video buffer*/
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 10;

    /* hist buf*/
    stVbConf.astCommPool[1].u32BlkSize = (196*4);
    stVbConf.astCommPool[1].u32BlkCnt = 6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_VENC_SNAP_0;
    }

    /******************************************
     step 3: start vi dev & chn to capture
    ******************************************/
    stViConfig.enViMode   = SENSOR_TYPE;
    stViConfig.enRotate   = ROTATE_NONE;
    stViConfig.enNorm     = VIDEO_ENCODING_MODE_AUTO;
    stViConfig.enViChnSet = VI_CHN_SET_NORMAL;
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto END_VENC_SNAP_1;
    }

#ifdef CHECK_ISP_REGISTER
    //check if isp register has init
    check_isp_register();
#endif //CHECK_ISP_REGISTER

    /******************************************
     step 4: start vpss and vi bind vpss
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_BindVenc(stViConfig.enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vi bind Vpss failed!\n");
        goto END_VENC_SNAP_2;
    }

    /******************************************
     step 5: snap process
    ******************************************/
    VencGrp = 0;
    VencChn = 0;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END_VENC_SNAP_2;
    }

    //-----------------------------------------------
    /******************************************
     Play audio file start_scan to notify user use app on phone to display wifi infomation to cam
    ******************************************/
    /* init stAio. all of cases will use it */
    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_8000;
    stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag = 1;
    stAioAttr.u32FrmNum = 30;
    stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM;
    stAioAttr.u32ChnCnt = 2;
    stAioAttr.u32ClkSel = 1;

    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, gs_enPayloadType_WifiScan);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, AoChn, &stAioAttr, gs_pstAoReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    pfd_start = SAMPLE_AUDIO_Open_File(START_SCAN);
    if (!pfd_start)
    {
        SAMPLE_DBG(HI_FAILURE);
        return HI_FAILURE;
    }

	gs_stStartScan.AdChn = AdChn;
    gs_stStartScan.pfd = pfd_start;
    gs_stStartScan.bStart = HI_TRUE;
    gs_stStartScan.AoDev = AoDev;
    gs_stStartScan.AoChn = AoChn;

    pthread_create(&gs_stStartScan.stAdPid, 0, PlayNotificationFile, &gs_stStartScan);
    //-----------------------------------------------

    s32Ret = SAMPLE_COMM_VENC_SnapStart(VencGrp, VencChn, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start snap failed!\n");
        goto END_VENC_SNAP_3;
    }

//    printf("press 'q' to exit sample!\nperess ENTER to capture one picture to file\n");
//    i = 0;
//    while ((ch = getchar()) != 'q')
//    while (qr_ret == 0)
    while (qr_ret == 0)
    {
        s32Ret = SAMPLE_COMM_VENC_SnapProcess(VencGrp, VencChn);
        if (HI_SUCCESS != s32Ret)
        {
            printf("%s: sanp process failed!\n", __FUNCTION__);
            break;
        }
        printf("snap success!\n");

        qr_ret = decode_wifi_info(0);
//        i++;
    }

    //-----------------------------------------------
    /******************************************
     Play audio file scan_success to notify user that cam has complete scan
    ******************************************/
    //destroy start_scan thread first
    if (gs_stStartScan.bStart)
    {
    	gs_stStartScan.bStart = HI_FALSE;
    	pthread_join(gs_stStartScan.stAdPid, 0);
    }
    //start thread scan_success
    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, gs_enPayloadType_WifiScan);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, AoChn, &stAioAttr, gs_pstAoReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    pfd_success = SAMPLE_AUDIO_Open_File(SCAN_SUCCESS);
    if (!pfd_start)
    {
        SAMPLE_DBG(HI_FAILURE);
        return HI_FAILURE;
    }

	gs_stScanSuccess.AdChn = AdChn;
    gs_stScanSuccess.pfd = pfd_success;
    gs_stScanSuccess.bStart = HI_TRUE;
    gs_stScanSuccess.AoDev = AoDev;
    gs_stScanSuccess.AoChn = AoChn;

    pthread_create(&gs_stScanSuccess.stAdPid, 0, PlayNotificationFile, &gs_stScanSuccess);

    //wait for complete play success file
    if (gs_stScanSuccess.bStart) {
    	pthread_join(gs_stScanSuccess.stAdPid, 0);
    }
    //-----------------------------------------------

    /******************************************
     step 8: exit process
    ******************************************/
    printf("snap over!\n");

END_VENC_SNAP_3:
    s32Ret = SAMPLE_COMM_VENC_SnapStop(VencGrp, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Stop snap failed!\n");
        goto END_VENC_SNAP_3;
    }
END_VENC_SNAP_2:
    SAMPLE_COMM_VI_UnBindVenc(stViConfig.enViMode);
END_VENC_SNAP_1:	//vi stop
    SAMPLE_COMM_VI_StopVi(&stViConfig);
END_VENC_SNAP_0:	//system exit
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/******************************************************************************
* function :  play notification file
******************************************************************************/
HI_S32 play_notification_file(AUDIO_FILE file_name) {
    HI_S32 s32Ret = HI_SUCCESS;
    VB_CONF_S stVbConf;
    FILE        *pfd = NULL;
    AIO_ATTR_S stAioAttr;

    AUDIO_DEV   AoDev = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;

    /* init stAio. all of cases will use it */
    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_8000;
    stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag = 1;
    stAioAttr.u32FrmNum = 30;
    stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM;
    stAioAttr.u32ChnCnt = 2;
    stAioAttr.u32ClkSel = 1;

    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: system init failed with %d!\n", __FUNCTION__, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, gs_enPayloadType_WifiScan);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, AoChn, &stAioAttr, gs_pstAoReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    pfd = SAMPLE_AUDIO_Open_File(file_name);
    if (!pfd)
    {
        SAMPLE_DBG(HI_FAILURE);
        return HI_FAILURE;
    }

	gs_stNotificationFile.AdChn = AdChn;
    gs_stNotificationFile.pfd = pfd;
    gs_stNotificationFile.bStart = HI_TRUE;
    gs_stNotificationFile.AoDev = AoDev;
    gs_stNotificationFile.AoChn = AoChn;

    pthread_create(&gs_stNotificationFile.stAdPid, 0, PlayNotificationFile, &gs_stNotificationFile);

    //wait for complete play success file
    if (gs_stNotificationFile.bStart) {
    	pthread_join(gs_stNotificationFile.stAdPid, 0);
    }

    return HI_SUCCESS;
}

/******************************************************************************
* function :  720p stream
******************************************************************************/
static char const* dateHeader()
{
	static char buf[200];
#if !defined(_WIN32_WCE)
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
#endif

	return buf;
}

static char* GetLocalIP(int sock)
{
	struct ifreq ifreq;
	struct sockaddr_in *sin;
	char * LocalIP = (char*)malloc(20);
	strcpy(ifreq.ifr_name,"ra0");
	if (!(ioctl (sock, SIOCGIFADDR,&ifreq)))
    	{
		sin = (struct sockaddr_in *)&ifreq.ifr_addr;
		sin->sin_family = AF_INET;
       	strcpy(LocalIP,inet_ntoa(sin->sin_addr));
		//inet_ntop(AF_INET, &sin->sin_addr,LocalIP, 16);
    	}
	printf("--------------------------------------------%s\n",LocalIP);
	return LocalIP;
}

char* strDupSize(char const* str)
{
  if (str == NULL) return NULL;
  size_t len = strlen(str) + 1;
  char* copy = (char*)malloc(len);

  return copy;
}

int ParseRequestString(char const* reqStr,
		       unsigned reqStrSize,
		       char* resultCmdName,
		       unsigned resultCmdNameMaxSize,
		       char* resultURLPreSuffix,
		       unsigned resultURLPreSuffixMaxSize,
		       char* resultURLSuffix,
		       unsigned resultURLSuffixMaxSize,
		       char* resultCSeq,
		       unsigned resultCSeqMaxSize)
{
  // This parser is currently rather dumb; it should be made smarter #####

  // Read everything up to the first space as the command name:
  int parseSucceeded = FALSE;
  unsigned i;
  for (i = 0; i < resultCmdNameMaxSize-1 && i < reqStrSize; ++i) {
    char c = reqStr[i];
    if (c == ' ' || c == '\t') {
      parseSucceeded = TRUE;
      break;
    }

    resultCmdName[i] = c;
  }
  resultCmdName[i] = '\0';
  if (!parseSucceeded) return FALSE;

  // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
  unsigned j = i+1;
  while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j; // skip over any additional white space
  for (j = i+1; j < reqStrSize-8; ++j) {
    if ((reqStr[j] == 'r' || reqStr[j] == 'R')
	&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
	&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
	&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
	&& reqStr[j+4] == ':' && reqStr[j+5] == '/') {
      j += 6;
      if (reqStr[j] == '/') {
	// This is a "rtsp://" URL; skip over the host:port part that follows:
	++j;
	while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
      } else {
	// This is a "rtsp:/" URL; back up to the "/":
	--j;
      }
      i = j;
      break;
    }
  }

  // Look for the URL suffix (before the following "RTSP/"):
  parseSucceeded = FALSE;
  unsigned k;
  for (k = i+1; k < reqStrSize-5; ++k) {
    if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
	reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') {
      while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
      unsigned k1 = k;
      while (k1 > i && reqStr[k1] != '/' && reqStr[k1] != ' ') --k1;
      // the URL suffix comes from [k1+1,k]

      // Copy "resultURLSuffix":
      if (k - k1 + 1 > resultURLSuffixMaxSize) return FALSE; // there's no room
      unsigned n = 0, k2 = k1+1;
      while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
      resultURLSuffix[n] = '\0';

      // Also look for the URL 'pre-suffix' before this:
      unsigned k3 = --k1;
      while (k3 > i && reqStr[k3] != '/' && reqStr[k3] != ' ') --k3;
      // the URL pre-suffix comes from [k3+1,k1]

      // Copy "resultURLPreSuffix":
      if (k1 - k3 + 1 > resultURLPreSuffixMaxSize) return FALSE; // there's no room
      n = 0; k2 = k3+1;
      while (k2 <= k1) resultURLPreSuffix[n++] = reqStr[k2++];
      resultURLPreSuffix[n] = '\0';

      i = k + 7; // to go past " RTSP/"
      parseSucceeded = TRUE;
      break;
    }
  }
  if (!parseSucceeded) return FALSE;

  // Look for "CSeq:", skip whitespace,
  // then read everything up to the next \r or \n as 'CSeq':
  parseSucceeded = FALSE;
  for (j = i; j < reqStrSize-5; ++j) {
    if (reqStr[j] == 'C' && reqStr[j+1] == 'S' && reqStr[j+2] == 'e' &&
	reqStr[j+3] == 'q' && reqStr[j+4] == ':') {
      j += 5;
      unsigned n;
      while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
      for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
	char c = reqStr[j];
	if (c == '\r' || c == '\n') {
	  parseSucceeded = TRUE;
	  break;
	}

	resultCSeq[n] = c;
      }
      resultCSeq[n] = '\0';
      break;
    }
  }
  if (!parseSucceeded) return FALSE;

  return TRUE;
}

int OptionAnswer(char *cseq, int sock)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
			cseq,dateHeader(),"OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN");

		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s\n",buf);
		}
		return TRUE;
	}
	return FALSE;
}

int DescribeAnswer(char *cseq,int sock,char * urlSuffix,char* recvbuf)
{
	if (sock != 0)
	{
		char sdpMsg[1024];
		char buf[2048];
		memset(buf,0,2048);
		memset(sdpMsg,0,1024);
		char*localip;
		localip = GetLocalIP(sock);

		int re = send(sock, buf, 0, 0);

		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n",cseq);
		pTemp += sprintf(pTemp,"%s",dateHeader());
		pTemp += sprintf(pTemp,"Content-Type: application/sdp\r\n");

		//TODO
		char *pTemp2 = sdpMsg;
		pTemp2 += sprintf(pTemp2,"v=0\r\n");
		pTemp2 += sprintf(pTemp2,"o=StreamingServer 3331435948 1116907222000 IN IP4 %s\r\n",localip);
		pTemp2 += sprintf(pTemp2,"s=H.264\r\n");
//		pTemp2 += sprintf(pTemp2,"c=IN IP4 0.0.0.0\r\n");

		pTemp2 += sprintf(pTemp2,"i=%s\r\n", urlSuffix);

		pTemp2 += sprintf(pTemp2,"t=0 0\r\n");

		pTemp2 += sprintf(pTemp2,"a=DevVer:pusher2\r\n");
		pTemp2 += sprintf(pTemp2,"a=GroupName:IPCAM\r\n");
		pTemp2 += sprintf(pTemp2,"a=NickName:CIF\r\n");
		pTemp2 += sprintf(pTemp2,"a=CfgSection:PROG_CHN0\r\n");
		pTemp2 += sprintf(pTemp2,"a=tool:LIVE555 Streaming Media v2011.08.13\r\n");
		pTemp2 += sprintf(pTemp2,"a=type:broadcast\r\n");

		pTemp2 += sprintf(pTemp2,"a=control:*\r\n");
		pTemp2 += sprintf(pTemp2,"a=range:npt=0-\r\n");
		pTemp2 += sprintf(pTemp2,"a=x-qt-text-nam:H.264 Program Stream\r\n");
		pTemp2 += sprintf(pTemp2,"a=x-qt-text-inf:%s\r\n", urlSuffix);

		/*H264 TrackID=0 RTP_PT 96*/
		pTemp2 += sprintf(pTemp2,"m=video 0 RTP/AVP 96\r\n");
		pTemp2 += sprintf(pTemp2,"a=control:trackID=1\r\n");
		pTemp2 += sprintf(pTemp2,"c=IN IP4 0.0.0.0\r\n");
//		pTemp2 += sprintf(pTemp2,"b=AS:4000\r\n");

		pTemp2 += sprintf(pTemp2,"a=rtpmap:96 H264/90000\r\n");
//		pTemp2 += sprintf(pTemp2,"a=control:trackID=1\r\n");
//		pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1; sprop-parameter-sets=%s\r\n", "AAABBCCC");
//		pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1;profile-level-id=64001E;sprop-parameter-sets=Z2QAHqzoFglsBEAAAAZAAACWAQ==,aO48sA==\r\n");
		if (strcmp(urlSuffix, "ch0.h264") == 0) {
			pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1"
					";profile-level-id=64001F"
					";sprop-parameter-sets=Z2QAH6wrUCgC3IA=,aO48MA==\r\n");
//			pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1; sprop-parameter-sets=%s\r\n", "AAABBCCC");
			pTemp2 += sprintf(pTemp2,"a=framesize:96 1280-720\r\n");
			pTemp2 += sprintf(pTemp2,"a=cliprect:0,0,1280,720\r\n");
		}
		else {
			pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1"
					";profile-level-id=64001E"
					";sprop-parameter-sets=Z2QAHqwrUFAX/Kg=,aO48MA==\r\n");
//			pTemp2 += sprintf(pTemp2,"a=fmtp:96 packetization-mode=1; sprop-parameter-sets=%s\r\n", "AAABBCCC");
			pTemp2 += sprintf(pTemp2,"a=framesize:96 640-360\r\n");
			pTemp2 += sprintf(pTemp2,"a=cliprect:0,0,640,360\r\n");
		}

#if 0
		/*G726*/
		/*TODO */
		pTemp2 += sprintf(pTemp2,"m=audio 0 RTP/AVP 97\r\n");
//		pTemp2 += sprintf(pTemp2,"a=control:trackID=2\r\n");
		pTemp2 += sprintf(pTemp2,"c=IN IP4 0.0.0.0\r\n");
		if(strcmp(g_rtp_playload,"AAC")==0)
		{
			pTemp2 += sprintf(pTemp2,"a=rtpmap:97 MPEG4-GENERIC/%d/2\r\n",16000);
			pTemp2 += sprintf(pTemp2,"a=fmtp:97 streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1410\r\n");
		}
		else
		{
//			pTemp2 += sprintf(pTemp2,"a=rtpmap:97 PCMU/%d/1\r\n",8000);
			pTemp2 += sprintf(pTemp2,"a=rtpmap:97 s16le/%d/1\r\n",22050);
//			pTemp2 += sprintf(pTemp2,"a=fmtp:97 streamtype=5;profile-level-id=1;cpresent=0;mode=g711u-law\r\n");
			pTemp2 += sprintf(pTemp2,"a=fmtp:97 profile-level-id=1;streamtype=5;cpresent=0\r\n");
		}
		pTemp2 += sprintf(pTemp2,"a=control:trackID=2\r\n");


#endif
		pTemp += sprintf(pTemp,"Content-length: %d\r\n", strlen(sdpMsg));
		pTemp += sprintf(pTemp,"Content-Base: rtsp://%s/%s/\r\n\r\n",localip,urlSuffix);

		//printf("mem ready\n");
		strcat(pTemp, sdpMsg);
		free(localip);
		//printf("Describe ready sent\n");
		re = send(sock, buf, strlen(buf),0);
		if(re <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s\n",buf);
		}
	}

	return TRUE;
}

void ParseTransportHeader(char const* buf,
						  StreamingMode* streamingMode,
						 char**streamingModeString,
						 char**destinationAddressStr,
						 u_int8_t* destinationTTL,
						 portNumBits* clientRTPPortNum, // if UDP
						 portNumBits* clientRTCPPortNum, // if UDP
						 unsigned char* rtpChannelId, // if TCP
						 unsigned char* rtcpChannelId // if TCP
						 )
 {
	// Initialize the result parameters to default values:
	*streamingMode = RTP_UDP;
	*streamingModeString = NULL;
	*destinationAddressStr = NULL;
	*destinationTTL = 255;
	*clientRTPPortNum = 0;
	*clientRTCPPortNum = 1;
	*rtpChannelId = *rtcpChannelId = 0xFF;

	portNumBits p1, p2;
	unsigned ttl, rtpCid, rtcpCid;

	// First, find "Transport:"
	while (1) {
		if (*buf == '\0') return; // not found
		if (strncasecmp(buf, "Transport: ", 11) == 0) break;
		++buf;
	}

	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 11;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;]", field) == 1) {
		if (strcmp(field, "RTP/AVP/TCP") == 0) {
			*streamingMode = RTP_TCP;
			if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
				*rtpChannelId = (unsigned char)rtpCid;
				*rtcpChannelId = (unsigned char)rtcpCid;
			}
		} else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
			strcmp(field, "MP2T/H2221/UDP") == 0) {
			*streamingMode = RAW_UDP;
			//*streamingModeString = strDup(field);
		} else if (strncasecmp(field, "destination=", 12) == 0)
		{
			//delete[] destinationAddressStr;
			free(destinationAddressStr);
			//destinationAddressStr = strDup(field+12);
		} else if (sscanf(field, "ttl%u", &ttl) == 1) {
			destinationTTL = (u_int8_t *)ttl;
		} else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
			*clientRTPPortNum = p1;
			*clientRTCPPortNum = p2;
		} else if (sscanf(field, "client_port=%hu", &p1) == 1) {
			*clientRTPPortNum = p1;
			*clientRTCPPortNum = (*streamingMode == RAW_UDP) ? 0 : p1 + 1;
		} else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
			*rtpChannelId = (unsigned char)rtpCid;
			*rtcpChannelId = (unsigned char)rtcpCid;
		}

		fields += strlen(field);
		while (*fields == ';') ++fields; // skip over separating ';' chars
		if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
	}
	free(field);
}

int SetupAnswer(char *cseq,int sock,int SessionId,char * urlSuffix,char* recvbuf,int* rtpport, int* rtcpport,
		int trackID, StreamingMode* streamingMode, unsigned char *rtpChannelID, unsigned char *rtcpChannelID)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);

//		StreamingMode streamingMode;
		char* streamingModeString; // set when RAW_UDP streaming is specified
		char* clientsDestinationAddressStr;
		u_int8_t clientsDestinationTTL;
		portNumBits clientRTPPortNum, clientRTCPPortNum;
		unsigned char rtpChannelId, rtcpChannelId;
		ParseTransportHeader(recvbuf,streamingMode, &streamingModeString,
			&clientsDestinationAddressStr, &clientsDestinationTTL,
			&clientRTPPortNum, &clientRTCPPortNum,
			&rtpChannelId, &rtcpChannelId);

		//Port clientRTPPort(clientRTPPortNum);
		//Port clientRTCPPort(clientRTCPPortNum);
		*rtpport = clientRTPPortNum;
		*rtcpport = clientRTCPPortNum;

		//RTP/AVP/TCP mode
		*rtpChannelID = rtpChannelId;
		*rtcpChannelID = rtcpChannelId;

		char *pTemp = buf;
		char*localip;
		localip = GetLocalIP(sock);
		if (*streamingMode == RTP_UDP) {
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sTransport: RTP/AVP;unicast;destination=%s;client_port=%d-%d;server_port=%d-%d\r\nSession: %d\r\n\r\n",
			cseq,dateHeader(),localip,
			ntohs(htons(clientRTPPortNum)),
			ntohs(htons(clientRTCPPortNum)),
			trackID == 1 ? VIDEO_PORT : AUDIO_PORT,
			trackID == 1 ? VIDEO_PORT + 1 : AUDIO_PORT + 1,
			SessionId);
		}
		else {
			pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sTransport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d;ssrc=0000000a\r\nSession: %d\r\n\r\n",
				cseq,dateHeader(),"192.168.1.30","192.168.1.52",
				rtpChannelId,
				rtpChannelId + 1,
				SessionId);
		}

		free(localip);
		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
		}
		return TRUE;
	}
	return FALSE;
}

int PlayAnswer(char *cseq, int sock,int SessionId,char* urlPre,char* recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		char*localip;
		localip = GetLocalIP(sock);
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sRange: npt=0.000-\r\nSession: %d\r\nRTP-Info: url=rtsp://%s/%s;seq=0\r\n\r\n",
			cseq,dateHeader(),SessionId,localip,urlPre);

		free(localip);

		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
		}
		return TRUE;
	}
	return FALSE;
}

int PauseAnswer(char *cseq,int sock,char *recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n\r\n",
			cseq,dateHeader());

		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
		}
		return TRUE;
	}
	return FALSE;
}

int TeardownAnswer(char *cseq,int sock,int SessionId,char *recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %d\r\n\r\n",
			cseq,dateHeader(),SessionId);

		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
		}
		return TRUE;
	}
	return FALSE;
}

void * RtspClientMsg(void*pParam)
{
	pthread_detach(pthread_self());
	int nRes;
	char pRecvBuf[RTSP_RECV_SIZE];
	RTSP_CLIENT * pClient = (RTSP_CLIENT*)pParam;
	memset(pRecvBuf,0,sizeof(pRecvBuf));
	printf("RTSP:-----Create Client %s\n",pClient->IP);
	while(pClient->status != RTSP_IDLE)
	{
		nRes = recv(pClient->socket, pRecvBuf, RTSP_RECV_SIZE,0);
		//printf("-------------------%d\n",nRes);
		if(nRes < 1)
		{
			//usleep(1000);
			printf("RTSP:Recv Error--- %d\n",nRes);
			g_rtspClients[pClient->index].status = RTSP_IDLE;
			g_rtspClients[pClient->index].seqnum = 0;
			g_rtspClients[pClient->index].tsvid = 0;
			g_rtspClients[pClient->index].tsaud = 0;
			close(pClient->socket);
			break;
		}
		char cmdName[PARAM_STRING_MAX];
		char urlPreSuffix[PARAM_STRING_MAX];
		char urlSuffix[PARAM_STRING_MAX];
		char cseq[PARAM_STRING_MAX];

		ParseRequestString(pRecvBuf,nRes,cmdName,sizeof(cmdName),urlPreSuffix,sizeof(urlPreSuffix),
			urlSuffix,sizeof(urlSuffix),cseq,sizeof(cseq));

		char *p = pRecvBuf;

		printf("<<<<<%s\n",p);
		//printf("\--------------------------\n");
		//printf("%s %s\n",urlPreSuffix,urlSuffix);

		if(strstr(cmdName, "OPTIONS"))
		{
			OptionAnswer(cseq,pClient->socket);
		}
		else if(strstr(cmdName, "DESCRIBE"))
		{
			DescribeAnswer(cseq,pClient->socket,urlSuffix,p);
			//printf("-----------------------------DescribeAnswer %s %s\n",
			//	urlPreSuffix,urlSuffix);
		}
		else if(strstr(cmdName, "SETUP"))
		{
			StreamingMode streamingMode;
			unsigned char rtpChannelID, rtcpChannelID;
			int rtpport,rtcpport;
			int trackID=0;
			sscanf(urlSuffix, "trackID=%u", &trackID);
			SetupAnswer(cseq, pClient->socket, pClient->sessionid, urlPreSuffix, p, &rtpport, &rtcpport,
					trackID, &streamingMode, &rtpChannelID, &rtcpChannelID);

//			sscanf(urlSuffix, "trackID=%u", &trackID);
			//printf("----------------------------------------------TrackId %d\n",trackID);
			g_rtspClients[pClient->index].streamMode = streamingMode;
			if(trackID<0 || trackID>=2)trackID=0;

			g_rtspClients[pClient->index].rtpChannelID[trackID] = rtpChannelID;
			g_rtspClients[pClient->index].rtcpChannelID[trackID] = rtcpChannelID;

			g_rtspClients[pClient->index].rtpport[trackID] = rtpport;
			g_rtspClients[pClient->index].rtcpport= rtcpport;
			g_rtspClients[pClient->index].reqchn = atoi(urlPreSuffix);
			if(strlen(urlPreSuffix)<100)
				strcpy(g_rtspClients[pClient->index].urlPre,urlPreSuffix);
			//printf("-----------------------------SetupAnswer %s-%d-%d\n",
			//	urlPreSuffix,g_rtspClients[pClient->index].reqchn,rtpport);
			printf("++++++++++++++++++SetupAnswer: %s-%s -rtpport %d -rtpChannelID %d -- %d --- %d\n"
					"channel0 = %d, channel1 = %d\n",
							urlPreSuffix,
							urlSuffix,
							rtpport,
							rtpChannelID,
							trackID,
							pClient->sessionid,
							g_rtspClients[pClient->index].rtpChannelID[0],
							g_rtspClients[pClient->index].rtpChannelID[1]);
		}
		else if(strstr(cmdName, "PLAY"))
		{
			PlayAnswer(cseq,pClient->socket,pClient->sessionid,g_rtspClients[pClient->index].urlPre,p);

			//Choose channel to stream
			if (strstr(urlPreSuffix, "ch0.h264"))
				g_rtspClients[pClient->index].status = RTSP_SENDING0;
			else if (strstr(urlPreSuffix, "ch1.h264"))
				g_rtspClients[pClient->index].status = RTSP_SENDING1;


			printf("Start Play %d\n",pClient->index);
			//printf("-----------------------------PlayAnswer %d %d\n",pClient->index);
			//usleep(100);
		}
		else if(strstr(cmdName, "PAUSE"))
		{
			PauseAnswer(cseq,pClient->socket,p);
		}
		else if(strstr(cmdName, "TEARDOWN"))
		{
			TeardownAnswer(cseq,pClient->socket,pClient->sessionid,p);
			g_rtspClients[pClient->index].status = RTSP_IDLE;
			g_rtspClients[pClient->index].seqnum = 0;
			g_rtspClients[pClient->index].tsvid = 0;
			g_rtspClients[pClient->index].tsaud = 0;
			close(pClient->socket);
		}
		if(exitok){ exitok++;return NULL; }
	}
	printf("RTSP:-----Exit Client %s\n",pClient->IP);
	return NULL;
}

void * RtspServerListen(void*pParam)
{
	int s32Socket;
	struct sockaddr_in servaddr;
	int s32CSocket;
	int s32Rtn;
	int s32Socket_opt_value = 1;
	socklen_t nAddrLen;
	struct sockaddr_in addrAccept;

	memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(RTSP_SERVER_PORT);

	s32Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(s32Socket ,SOL_SOCKET,SO_REUSEADDR,&s32Socket_opt_value,sizeof(int)) == -1)
    {
        return (void *)(-1);
    }


	//test
	socklen_t optlen = sizeof(s32Socket_opt_value);

	s32Socket_opt_value = 1;
	setsockopt(s32Socket, IPPROTO_TCP, TCP_NODELAY, &s32Socket_opt_value, optlen);

	getsockopt(s32Socket, IPPROTO_TCP, TCP_NODELAY, &s32Socket_opt_value, &optlen);
	if (s32Socket_opt_value < 0)
		printf("TCP_NODELAY error \n");
	else
		printf("TCP_NODELAY = %d\n", s32Socket_opt_value);

	getsockopt(s32Socket, IPPROTO_TCP, TCP_CORK, &s32Socket_opt_value, &optlen);
	if (s32Socket_opt_value < 0)
		printf("TCP_CORK error \n");
	else
		printf("TCP_CORK = %d\n", s32Socket_opt_value);
	//end


    s32Rtn = bind(s32Socket, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    if(s32Rtn < 0)
    {
        return (void *)(-2);
    }

    s32Rtn = listen(s32Socket, 50);   /*50,×îŽóµÄÁ¬œÓÊý*/
    if(s32Rtn < 0)
    {

         return (void *)(-2);
    }


	nAddrLen = sizeof(struct sockaddr_in);
	int nSessionId = 1000;
    while ((s32CSocket = accept(s32Socket, (struct sockaddr*)&addrAccept, &nAddrLen)) >= 0)
    {
		printf("<<<<RTSP Client %s Connected...\n", inet_ntoa(addrAccept.sin_addr));

		int nMaxBuf = 10 * 1024;
		if(setsockopt(s32CSocket, SOL_SOCKET, SO_SNDBUF, (char*)&nMaxBuf, sizeof(nMaxBuf)) == -1)
			printf("RTSP:!!!!!! Enalarge socket sending buffer error !!!!!!\n");
		int i;
		int bAdd=FALSE;
		for(i=0;i<MAX_RTSP_CLIENT;i++)
		{
			if(g_rtspClients[i].status == RTSP_IDLE)
			{
				memset(&g_rtspClients[i],0,sizeof(RTSP_CLIENT));
				g_rtspClients[i].index = i;
				g_rtspClients[i].socket = s32CSocket;
				g_rtspClients[i].status = RTSP_CONNECTED ;//RTSP_SENDING;
				g_rtspClients[i].sessionid = nSessionId++;
				strcpy(g_rtspClients[i].IP,inet_ntoa(addrAccept.sin_addr));
				pthread_t threadIdlsn = 0;

				struct sched_param sched;
				sched.sched_priority = 1;
				//to return ACKecho
				pthread_create(&threadIdlsn, NULL, RtspClientMsg, &g_rtspClients[i]);
				pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);

				bAdd = TRUE;
				break;
			}
		}
		if(bAdd==FALSE)
		{
			memset(&g_rtspClients[0],0,sizeof(RTSP_CLIENT));
			g_rtspClients[0].index = 0;
			g_rtspClients[0].socket = s32CSocket;
			g_rtspClients[0].status = RTSP_CONNECTED ;//RTSP_SENDING;
			g_rtspClients[0].sessionid = nSessionId++;
			strcpy(g_rtspClients[0].IP,inet_ntoa(addrAccept.sin_addr));
			pthread_t threadIdlsn = 0;
			struct sched_param sched;
			sched.sched_priority = 1;
			//to return ACKecho
			pthread_create(&threadIdlsn, NULL, RtspClientMsg, &g_rtspClients[0]);
			pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);
			bAdd = TRUE;
		}
		if(exitok){ exitok++;return NULL; }
    }
	printf("----- INIT_RTSP_Listen() Exit !! \n");

	return NULL;
}

static HI_U8 adec_buf[800];
static unsigned short adec_index = 0;
#define AUDIO_CLIENT_PTNUMPERFRM	160
void send_audio_adec(char* buff, int len) {
    AUDIO_STREAM_S stAudioStream;
	unsigned int remain = len;
	unsigned int buf_pos = 0;
    HI_S32 s32Ret;
    HI_U32 u32Len = 2 * AUDIO_CLIENT_PTNUMPERFRM;

	stAudioStream.pStream = adec_buf;
//	printf("send_audio_adec %d bytes\n", remain);

	while (remain >= u32Len) {
		if (adec_index) {
//			printf("remain %d-\n", adec_index);
			memcpy(adec_buf + adec_index, buff + buf_pos, u32Len - adec_index);
			buf_pos += (u32Len - adec_index);
			remain -= (u32Len - adec_index);
			adec_index = 0;
		}
		else {
			memcpy(adec_buf, buff + buf_pos, u32Len);
			buf_pos += u32Len;
			remain -= u32Len;
		}

		//send to adec modul
        stAudioStream.u32Len = u32Len;
        /* here only demo adec streaming sending mode, but pack sending mode is commended */
        s32Ret = HI_MPI_ADEC_SendStream(0, &stAudioStream, HI_TRUE);
        if (s32Ret)
        {
            printf("%s: HI_MPI_ADEC_SendStream(%d) failed with %#x!\n",\
                   __FUNCTION__, 0, s32Ret);
            break;
        }
//		printf("%d-", remain);
	}
	if (remain > 0) {
		memcpy(adec_buf + adec_index, buff + buf_pos, remain);
		adec_index += remain;
//		printf("%d-", remain);
	}
//	printf("\n");
}

static char pAudioRecvBuf[AUDIO_RECV_SIZE];
void * AudioClientMsg(void*pParam)
{
	pthread_detach(pthread_self());
	int nRes;
//	int i;

//	FILE *pfd;
//	pfd = fopen("test-44.1k.pcm", "w+");

	AUDIO_CLIENT * pClient = (AUDIO_CLIENT*)pParam;
	memset(pAudioRecvBuf,0,sizeof(pAudioRecvBuf));
	printf("Audio:-----Create Client %s\n",pClient->IP);
	while(pClient->status != RTSP_IDLE)
	{
		nRes = recv(pClient->socket, pAudioRecvBuf, AUDIO_RECV_SIZE,0);
//		printf("--------%d\n",nRes);
		send_audio_adec(pAudioRecvBuf, nRes);

//		fwrite(pAudioRecvBuf, 1, nRes, pfd);

		//Handle client disconnected
		if (nRes == 0) {
			pClient->status = RTSP_IDLE;
			close(pClient->socket);
		}
	}
	printf("Audio:-----Exit Client %s\n",pClient->IP);
	return NULL;
}

//static HI_U8 adec_test[400];
void * AudioServerListen(void*pParam)
{
	int s32Socket;
	struct sockaddr_in servaddr;
	int s32CSocket;
	int s32Rtn;
	int s32Socket_opt_value = 1;
	socklen_t nAddrLen;
	struct sockaddr_in addrAccept;

	memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(AUDIO_SERVER_PORT);

	s32Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(s32Socket ,SOL_SOCKET,SO_REUSEADDR,&s32Socket_opt_value,sizeof(int)) == -1)
    {
        return (void *)(-1);
    }
    s32Rtn = bind(s32Socket, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    if(s32Rtn < 0)
    {
        return (void *)(-2);
    }

    s32Rtn = listen(s32Socket, 50);
    if(s32Rtn < 0)
    {
    	return (void *)(-2);
    }


	nAddrLen = sizeof(struct sockaddr_in);
    while ((s32CSocket = accept(s32Socket, (struct sockaddr*)&addrAccept, &nAddrLen)) >= 0)
    {
		printf("<<<<Audio Client %s Connected...\n", inet_ntoa(addrAccept.sin_addr));

		int nMaxBuf = 10 * 1024;
		if(setsockopt(s32CSocket, SOL_SOCKET, SO_SNDBUF, (char*)&nMaxBuf, sizeof(nMaxBuf)) == -1)
			printf("Audio:!!!!!! Enalarge socket sending buffer error !!!!!!\n");
		int i;
		int bAdd=FALSE;
		for(i=0;i<MAX_AUDIO_CLIENT;i++)
		{
			if(g_AudioClients[i].status == RTSP_IDLE)
			{
				memset(&g_AudioClients[i],0,sizeof(AUDIO_CLIENT));
				g_AudioClients[i].index = i;
				g_AudioClients[i].socket = s32CSocket;
				g_AudioClients[i].status = RTSP_CONNECTED ;//RTSP_SENDING;
				strcpy(g_AudioClients[i].IP,inet_ntoa(addrAccept.sin_addr));
				pthread_t threadIdlsn = 0;

				struct sched_param sched;
				sched.sched_priority = 1;
				//to return ACKecho
				pthread_create(&threadIdlsn, NULL, AudioClientMsg, &g_AudioClients[i]);
				pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);

				bAdd = TRUE;
				break;
			}
		}
		if(bAdd==FALSE)
		{
			memset(&g_AudioClients[0],0,sizeof(AUDIO_CLIENT));
			g_AudioClients[0].index = 0;
			g_AudioClients[0].socket = s32CSocket;
			g_AudioClients[0].status = RTSP_CONNECTED ;//RTSP_SENDING;
			strcpy(g_AudioClients[0].IP,inet_ntoa(addrAccept.sin_addr));
			pthread_t threadIdlsn = 0;
			struct sched_param sched;
			sched.sched_priority = 1;
			//to return ACKecho
			pthread_create(&threadIdlsn, NULL, AudioClientMsg, &g_AudioClients[0]);
			pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);
			bAdd = TRUE;
		}
		if(exitok){ exitok++;return NULL; }
    }
	printf("----- INIT_Audio_Listen() Exit !! \n");

	return NULL;
}

static char sendbuf_venc_sent[1500];
static float timestampRTP;
HI_S32 VENC_Sent(char *buffer, int buflen, int channel)
{
	int is=0;
	for(is=0;is<MAX_RTSP_CLIENT;is++)
	{
		if ((g_rtspClients[is].status != RTSP_SENDING0) && (g_rtspClients[is].status != RTSP_SENDING1))
		{
		    continue;
		}

		if (g_rtspClients[is].status != (channel + 2)) {
			continue;
		}

		char* nalu_payload;
		int nAvFrmLen = 0;

		nAvFrmLen = buflen;
		struct sockaddr_in server;
		server.sin_family=AF_INET;
		server.sin_port=htons(g_rtspClients[is].rtpport[1]);
		server.sin_addr.s_addr=inet_addr(g_rtspClients[is].IP);

		int	bytes=0;
		unsigned char rtp_offset = 0;

		if (g_rtspClients[is].streamMode == RTP_TCP) {
			rtsp_interleaved = (RTSP_INTERLEAVED_FRAME*)sendbuf_venc_sent;
			rtsp_interleaved->magic = 0x24;
			rtsp_interleaved->channel = g_rtspClients[is].rtpChannelID[1];
			rtp_offset = 4;
			sendbuf_venc_sent[4] = 0;
		}
		else {
			rtp_offset = 0;
			sendbuf_venc_sent[0] = 0;
		}

//		rtp_hdr =(RTP_FIXED_HEADER*)&sendbuf_venc_sent[0];
		rtp_hdr = (RTP_FIXED_HEADER*)&sendbuf_venc_sent[0 + rtp_offset];

		rtp_hdr->payload     = RTP_H264;
		rtp_hdr->version     = 2;
		rtp_hdr->marker    = 0;
		rtp_hdr->ssrc      = htonl(10);

		timestampRTP = (float)PTS_INC * (float)32000 / (float)49;
		g_rtspClients[is].tsvid = (unsigned int)timestampRTP;

		rtp_hdr->timestamp=htonl(g_rtspClients[is].tsvid);

		char NALU = buffer[4];

		if(nAvFrmLen<=nalu_sent_len)
		{
			rtp_hdr->marker=1;
			rtp_hdr->seq_no     = htons(g_rtspClients[is].seqnum++);
////			nalu_hdr =(NALU_HEADER*)&sendbuf_venc_sent[12];
//			nalu_hdr =(NALU_HEADER*)&sendbuf_venc_sent[12 + rtp_offset];
//			nalu_hdr->F=0;
//			nalu_hdr->NRI=  3;
//			nalu_hdr->TYPE=  1;

//			nalu_payload=&sendbuf_venc_sent[13];
			nalu_payload=&sendbuf_venc_sent[13 + rtp_offset];
			memcpy(nalu_payload,buffer+5,nAvFrmLen-5);

			//NAL header
			sendbuf_venc_sent[12 + rtp_offset] = NALU;

			bytes=nAvFrmLen + 13 - 5;
//			sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));

			if (g_rtspClients[is].streamMode == RTP_UDP) {
				sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));
			}
			else {
				rtsp_interleaved->lenght = ENDIAN_SWAP16(bytes);
				send(g_rtspClients[is].socket, sendbuf_venc_sent, bytes + rtp_offset, 0);
			}
		}
		else if(nAvFrmLen>nalu_sent_len)
		{
			int k=0,l=0;
			k=nAvFrmLen/nalu_sent_len;
			l=nAvFrmLen%nalu_sent_len;
			int t=0;

			while(t<=k)
			{
				rtp_hdr->seq_no = htons(g_rtspClients[is].seqnum++);
				if(t==0)
				{
					rtp_hdr->marker=0;
////					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12];
//					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12 + rtp_offset];
//					fu_ind->F= 0;
//					fu_ind->NRI= 3;
//					fu_ind->TYPE=28;
//
////					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13];
//					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13 + rtp_offset];
//					fu_hdr->E=0;
//					fu_hdr->R=0;
//					fu_hdr->S=1;
//					fu_hdr->TYPE=1;

//					nalu_payload=&sendbuf_venc_sent[14];
					nalu_payload=&sendbuf_venc_sent[14 + rtp_offset];
					memcpy(nalu_payload,buffer+5,nalu_sent_len-5);

					//FU IND and FU header
					sendbuf_venc_sent[12 + rtp_offset] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13 + rtp_offset] = (NALU & 0x1F) | 0x80;

					bytes=nalu_sent_len+14 - 5;
//					sendto( udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));

					if (g_rtspClients[is].streamMode == RTP_UDP) {
						sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));
					}
					else {
						rtsp_interleaved->lenght = ENDIAN_SWAP16(bytes);
						send(g_rtspClients[is].socket, sendbuf_venc_sent, bytes + rtp_offset, 0);
					}
					t++;
				}
				else if(k==t)
				{

					rtp_hdr->marker=1;
//					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12];
//					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12 + rtp_offset];
//					fu_ind->F= 0 ;
//					fu_ind->NRI= 3 ;
//					fu_ind->TYPE=28;
//
//					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13];
//					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13 + rtp_offset];
//					fu_hdr->R=0;
//					fu_hdr->S=0;
//					fu_hdr->TYPE= 1;
//					fu_hdr->E=1;
//					nalu_payload=&sendbuf_venc_sent[14];
					nalu_payload=&sendbuf_venc_sent[14 + rtp_offset];
					memcpy(nalu_payload,buffer+t*nalu_sent_len,l);

					//FU IND and FU header
					sendbuf_venc_sent[12 + rtp_offset] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13 + rtp_offset] = (NALU & 0x1F) | 0x40;

					bytes=l+14;
//					sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));

					if (g_rtspClients[is].streamMode == RTP_UDP) {
						sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));
					}
					else {
						rtsp_interleaved->lenght = ENDIAN_SWAP16(bytes);
						send(g_rtspClients[is].socket, sendbuf_venc_sent, bytes + rtp_offset, 0);
					}
					t++;
				}
				else if(t<k && t!=0)
				{
					rtp_hdr->marker=0;

//					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12];
//					fu_ind =(FU_INDICATOR*)&sendbuf_venc_sent[12 + rtp_offset];
//					fu_ind->F=0;
//					fu_ind->NRI=3;
//					fu_ind->TYPE=28;
//					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13];
//					fu_hdr =(FU_HEADER*)&sendbuf_venc_sent[13 + rtp_offset];
//					fu_hdr->R=0;
//					fu_hdr->S=0;
//					fu_hdr->E=0;
//					fu_hdr->TYPE=1;
//					nalu_payload=&sendbuf_venc_sent[14];
					nalu_payload=&sendbuf_venc_sent[14 + rtp_offset];
					memcpy(nalu_payload,buffer+t*nalu_sent_len,nalu_sent_len);

					//FU IND and FU header
					sendbuf_venc_sent[12 + rtp_offset] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13 + rtp_offset] = (NALU & 0x1F);

					bytes=nalu_sent_len+14;
//					sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));

					if (g_rtspClients[is].streamMode == RTP_UDP) {
						sendto(udpfd_video, sendbuf_venc_sent, bytes, 0, (struct sockaddr *)&server,sizeof(server));
					}
					else {
						rtsp_interleaved->lenght = ENDIAN_SWAP16(bytes);
						send(g_rtspClients[is].socket, sendbuf_venc_sent, bytes + rtp_offset, 0);
					}
					t++;
				}
			}
		}
	}


#ifdef USE_TEST_P2P
	for (int i = 0; i < MAX_P2P_CLIENT; i++) {
		if (g_p2pClients[i].status != RTP_SENDING0)
		{
		    continue;
		}
		if (g_p2pClients[i].status != (channel + 2)) {
			continue;
		}

		char NALU = buffer[4];
		char *nalu_payload;
		int send_len;
		struct sockaddr_in client;

		client.sin_family = AF_INET;
		client.sin_addr.s_addr = inet_addr(g_p2pClients[i].ip);
		client.sin_port = htons(g_p2pClients[i].rtpport[0]);

		timestampRTP = (float)PTS_INC * (float)32000 / (float)49;
		g_p2pClients[i].timestamp[0] = (unsigned int)timestampRTP;

		rtp_hdr = (RTP_FIXED_HEADER*)&sendbuf_venc_sent[0];
		rtp_hdr->payload = RTP_H264;
		rtp_hdr->version = 2;
		rtp_hdr->ssrc = htonl(10);
		rtp_hdr->timestamp = htonl(g_p2pClients[i].timestamp[0]);


		if (buflen <= nalu_sent_len) {
			rtp_hdr->marker = 1;
			rtp_hdr->seq_no     = htons(g_p2pClients[i].seqnum[0]++);

			//NAL header
			sendbuf_venc_sent[12] = NALU;

			nalu_payload = &sendbuf_venc_sent[13];
			memcpy(nalu_payload, buffer + 5, buflen - 5);

			send_len = buflen + 13 - 5;
			sendto(udpfd_video_p2p, sendbuf_venc_sent, send_len, 0, (struct sockaddr *)&client,sizeof(client));
		}
		else {
			int k = 0, l = 0, t = 0;
			k=buflen / nalu_sent_len;
			l=buflen % nalu_sent_len;

			while (t <= k) {
				rtp_hdr->seq_no = htons(g_p2pClients[i].seqnum[0]++);
				if (t == 0) {
					rtp_hdr->marker = 0;

					//FU IND and FU header
					sendbuf_venc_sent[12] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13] = (NALU & 0x1F) | 0x80;

					nalu_payload = &sendbuf_venc_sent[14];
					memcpy(nalu_payload, buffer + 5, nalu_sent_len - 5);

					send_len = nalu_sent_len + 14 - 5;
					sendto(udpfd_video_p2p, sendbuf_venc_sent, send_len, 0, (struct sockaddr *)&client,sizeof(client));
					t++;
				}
				else if (k == t) {
					rtp_hdr->marker = 1;

					//FU IND and FU header
					sendbuf_venc_sent[12] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13] = (NALU & 0x1F) | 0x40;

					nalu_payload = &sendbuf_venc_sent[14];
					memcpy(nalu_payload, buffer + t * nalu_sent_len, l);

					send_len = l + 14;
					sendto(udpfd_video_p2p, sendbuf_venc_sent, send_len, 0, (struct sockaddr *)&client,sizeof(client));
					t++;
				}
				else if ((t < k) && (t != 0)) {
					rtp_hdr->marker = 0;

					//FU IND and FU header
					sendbuf_venc_sent[12] = (NALU & 0x60) | 0x1C;
					sendbuf_venc_sent[13] = (NALU & 0x1F);

					nalu_payload = &sendbuf_venc_sent[14];
					memcpy(nalu_payload, buffer + t * nalu_sent_len, nalu_sent_len);

					send_len = nalu_sent_len + 14;
					sendto(udpfd_video_p2p, sendbuf_venc_sent, send_len, 0, (struct sockaddr *)&client,sizeof(client));
					t++;
				}
			}
		}
	}
#endif //USE_TEST_P2P


//send to server_test
	if ((send_RTMP_to_server) && (channel == 0) && (reInitRTMP == HI_FALSE)) {
		//send frame to rtmp server
		u_int32_t dts, pts;

//		dts = pts = PTS_INC * 20;
		timestampRTP = (float)PTS_INC * (float)3200 / (float)441;
		dts = pts = (u_int32_t)timestampRTP;
		if (rtmp_send_h264_raw_stream(buffer, buflen, dts, pts) == -2) {
			printf("%s: rtmp_send_h264_raw_stream send failed\n", __func__);
			reInitRTMP = HI_TRUE;
			sleep(1);
			rtmp_destroy_client();
		}
	}
	//end test server
//*/
	//------------------------------------------------------------
	return HI_SUCCESS;
}

static int post = 0;
static char sendbuf_venc_sentjin[320*1024];
HI_S32 SAMPLE_COMM_VENC_Sentjin(VENC_STREAM_S *pstStream, int channel)
{
    HI_S32 i, flag = 0;

    for(i=0;i<MAX_RTSP_CLIENT;i++)//have atleast a connect
    {
		if (((g_rtspClients[i].status == RTSP_SENDING0) && (channel == 0)) ||
				((g_rtspClients[i].status == RTSP_SENDING1) && (channel == 1)))
		{
			flag = 1;
			break;
		}
    }

    if ((send_RTMP_to_server) && (channel == 0)) flag = 1;

    if(flag)
    {
        //printf("a");
	    for (i = 0; i < pstStream->u32PackCount; i++)
	    {
			HI_S32 lens = 0;
//			char sendbuf[320*1024];
			lens = pstStream->pstPack[i].u32Len[0];
//			memcpy(&sendbuf_venc_sentjin[0],pstStream->pstPack[i].pu8Addr[0],lens);
			memcpy(&sendbuf_venc_sentjin[post],pstStream->pstPack[i].pu8Addr[0],lens);

			if (pstStream->pstPack[i].u32Len[1] > 0)
			{
//				memcpy(&sendbuf_venc_sentjin[lens],pstStream->pstPack[i].pu8Addr[1],lens+pstStream->pstPack[i].u32Len[1]);
				memcpy(&sendbuf_venc_sentjin[lens+post],pstStream->pstPack[i].pu8Addr[1],lens+pstStream->pstPack[i].u32Len[1]);
				lens = lens+pstStream->pstPack[i].u32Len[1];
			}

			//Join SPS, PPS, SEI and IDR in to 1 message
			//Because SPS, PPS, SEI always follow by IDR and SPS, PPS, SEI lenght is short
			//NRI     : bit 7-5: 3
			//NAL type: bit 4-0: 7-SPS, 8-PPS, 6-SEI (SEI don't go with NRI), 5-IDR, 1-P frame
			char NALU = sendbuf_venc_sentjin[post+4];
			if ((NALU != 0x61) && (NALU != 0x65)) {
				//Skip SEI type
				if (NALU != 0x06)
					post += lens;
			}
			else {
				lens += post;
				VENC_Sent(sendbuf_venc_sentjin, lens, channel);
				post = 0;;
			}
//			VENC_Sent(sendbuf_venc_sentjin, lens, channel);
			lens = 0;
	    }
    }
    return HI_SUCCESS;
}

HI_VOID* SAMPLE_COMM_VENC_GetVencStreamProcSent(HI_VOID *p)
{
//    pthread_detach(pthread_self());
    printf("RTSP:-----create send thread\n");
    //i=0,720p; i=1,VGA 640*480; i=2, 320*240;
    HI_S32 i=0;//exe ch NO.
    HI_S32 s32ChnTotal;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    VENC_CHN_STAT_S stStat;
    HI_S32 s32Ret;
    VENC_STREAM_S stStream;

    struct sockaddr_in si_me;

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }

    for (i = 0; i < s32ChnTotal; i++)
    {
        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n",
                   VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(VIDEO_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    udpfd_video = socket(AF_INET,SOCK_DGRAM,0);//UDP
    printf("udp video up\n");

    bind(udpfd_video, (struct sockaddr*)&si_me, sizeof(si_me));

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
//        HI_S32 is, flag[2] = { 0, 0 };

//	for(is=0;is<MAX_RTSP_CLIENT;is++)//have atleast a connect
//	{
//		if (g_rtspClients[is].status == RTSP_SENDING0) {
//		    flag[0] = 1;
//		    continue;
//		}
//		else if (g_rtspClients[is].status == RTSP_SENDING1) {
//		    flag[1] = 1;
//		    continue;
//		}
//
//	}


//	if(flag)
//	test changed
	if(1)
	{
		FD_ZERO(&read_fds);
		for (i = 0; i < s32ChnTotal; i++)
		{
		    FD_SET(VencFd[i], &read_fds);
		}

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
		    continue;
		}
		else
		{
		    for (i = 0; i < s32ChnTotal; i++)
		    {
		        if (FD_ISSET(VencFd[i], &read_fds))
		        {
		            /*******************************************************
		             step 2.1 : query how many packs in one-frame stream.
		            *******************************************************/
		            //printf("query how many packs \n");
		            memset(&stStream, 0, sizeof(stStream));
		            s32Ret = HI_MPI_VENC_Query(i, &stStat);
		            if (HI_SUCCESS != s32Ret)
		            {
		                SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
		                break;
		            }

		            /*******************************************************
		             step 2.2 : malloc corresponding number of pack nodes.
		            *******************************************************/
		            stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
		            if (NULL == stStream.pstPack)
		            {
		                SAMPLE_PRT("malloc stream pack failed!\n");
		                break;
		            }

		            /*******************************************************
		             step 2.3 : call mpi to get one-frame stream
		            *******************************************************/
		            if(exitok)
		            {
		                free(stStream.pstPack);
		                stStream.pstPack = NULL;
		                exitok++;return NULL;
		            }
		            stStream.u32PackCount = stStat.u32CurPacks;
		            s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
		            if (HI_SUCCESS != s32Ret)
		            {
		                free(stStream.pstPack);
		                stStream.pstPack = NULL;
		                SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
		                       s32Ret);
		                break;
		            }
//		            if (i == 0)
		            {
		            	SAMPLE_COMM_VENC_Sentjin(&stStream, i);
		            }

			    /*
			    	pthread_t threadsentId = 0;
			    	struct sched_param sentsched;
				sentsched.sched_priority = 20;
				//to listen visiting
				pthread_create(&threadsentId, NULL, SAMPLE_COMM_VENC_Sentjin,(HI_VOID*)&stStream);
				pthread_setschedparam(threadsentId,SCHED_RR,&sentsched);
				*/

		            //step 2.5 : release stream/
		            s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
		            if (HI_SUCCESS != s32Ret)
		            {
		                free(stStream.pstPack);
		                stStream.pstPack = NULL;
		                break;
		            }
		            //step 2.6 : free pack nodes/
		            free(stStream.pstPack);
		            stStream.pstPack = NULL;
		            if(exitok){ exitok++;return NULL; }

		        }
		    }
		}
        }
        usleep(100);
    }

    return NULL;
}

HI_S32 VENC_StopGetStream()
{
    if (HI_TRUE == gs_stPara.bThreadStart)
    {
        gs_stPara.bThreadStart = HI_FALSE;
        pthread_join(gs_VencPid, 0);
    }
    return HI_SUCCESS;
}

HI_S32 AENC_StopGetStream()
{
    if (HI_TRUE == gs_stSampleAenc.bStart)
    {
    	gs_stSampleAenc.bStart = HI_FALSE;
        pthread_join(gs_stSampleAenc.stAencPid, 0);
    }
    return HI_SUCCESS;
}

/******************************************************************************
* function : Open Aenc File
******************************************************************************/
static FILE * SAMPLE_AUDIO_OpenAencFile(AENC_CHN AeChn, PAYLOAD_TYPE_E enType)
{
    FILE *pfd;
    HI_CHAR aszFileName[128];

    /* create file for save stream*/
//    sprintf(aszFileName, "audio_chn%d.%s", AeChn, SAMPLE_AUDIO_Pt2Str(enType));
    sprintf(aszFileName, "test");
    pfd = fopen(aszFileName, "w+");
    if (NULL == pfd)
    {
        printf("%s: open file %s failed\n", __FUNCTION__, aszFileName);
        return NULL;
    }
//    printf("open stream file:\"%s\" for aenc ok\n", aszFileName);
    return pfd;
}

static char sendbuf_aenc_sent[400];
HI_S32 AENC_Sent(AUDIO_STREAM_S stream) {
//	char *buffer = stream.pStream;
	int buflen = stream.u32Len;
	int sendlen = buflen + 12;
	int is = 0;
	//printf("s");
	for(is = 0; is < MAX_RTSP_CLIENT; is++)	{
		if ((g_rtspClients[is].status != RTSP_SENDING0) && (g_rtspClients[is].status != RTSP_SENDING1))
		{
		    continue;
		}

		char* nalu_payload;
		unsigned char rtp_offset = 0;
//		char sendbuf_aenc_sent[sendlen];

		struct sockaddr_in server;
		server.sin_family = AF_INET;
		server.sin_port = htons(g_rtspClients[is].rtpport[0]);
		server.sin_addr.s_addr = inet_addr(g_rtspClients[is].IP);

		if (g_rtspClients[is].streamMode == RTP_TCP) {
			rtsp_interleaved_audio = (RTSP_INTERLEAVED_FRAME*)sendbuf_aenc_sent;
			rtsp_interleaved_audio->magic = 0x24;
			rtsp_interleaved_audio->channel = g_rtspClients[is].rtpChannelID[0];
			rtp_offset = 4;
			sendbuf_aenc_sent[4] = 0;
		}
		else {
			rtp_offset = 0;
			sendbuf_aenc_sent[0] = 0;
		}

//		PTS_INC++;

		//sendto(udpfd, buffer, nAvFrmLen, 0, (struct sockaddr *)&server,sizeof(server));
//		printf("send frame PTS = %d: %d bytes to %x:%x\n", PTS_INC, buflen, server.sin_addr.s_addr, server.sin_port);

//		rtp_hdr_audio =(RTP_FIXED_HEADER*)&sendbuf_aenc_sent[0];
		rtp_hdr_audio =(RTP_FIXED_HEADER*)&sendbuf_aenc_sent[0 + rtp_offset];

		rtp_hdr_audio->csrc_len = 0;
		rtp_hdr_audio->payload = RTP_AUDIO;
		rtp_hdr_audio->version = 2;
		rtp_hdr_audio->marker = 1;
		rtp_hdr_audio->ssrc = htonl(10);

//		if(nAvFrmLen<=nalu_sent_len)
		{
			rtp_hdr_audio->marker = 1;
			rtp_hdr_audio->seq_no = htons(g_rtspClients[is].seqnum2++);

//			au_hdr = (AU_HEADER*)&sendbuf[12];
			AU_HEADER au_hdr;
			au_hdr.au_len = 1;
			au_hdr.au_index = 0;
			au_hdr.frm_len = buflen;

//			nalu_payload = &sendbuf_aenc_sent[12];
			nalu_payload = &sendbuf_aenc_sent[12 + rtp_offset];
			memcpy(nalu_payload, stream.pStream, buflen);

//			g_rtspClients[is].tsaud = g_rtspClients[is].tsaud + timestamp_increse;

			g_rtspClients[is].tsaud = PTS_INC * 160;

			rtp_hdr_audio->timestamp = htonl(g_rtspClients[is].tsaud);
//			sendto(udpfd_audio, sendbuf_aenc_sent, sendlen, 0, (struct sockaddr *)&server, sizeof(server));

			if (g_rtspClients[is].streamMode == RTP_UDP) {
//				sendto(udpfd_audio, sendbuf_aenc_sent, sendlen, 0, (struct sockaddr *)&server,sizeof(server));
			}
			else {
				rtsp_interleaved_audio->lenght = ENDIAN_SWAP16(sendlen);
//				send(g_rtspClients[is].socket, sendbuf_aenc_sent, sendlen + rtp_offset, 0);
			}

//			g_rtspClients[is].tsaud = g_rtspClients[is].tsaud + timestamp_increse;
//			g_rtspClients[is].tsaud = PTS_INC * 160;
		}
	}

//send to server_test
	if (send_RTMP_to_server && (reInitRTMP == HI_FALSE)) {
		//send frame to rtmp server
		u_int32_t pts;

		memcpy(sendbuf_aenc_sent, stream.pStream , buflen );

//		pts = PTS_INC * 160;
		timestampRTP = (float)PTS_INC * (float)3200 / (float)441;
		pts = (u_int32_t)timestampRTP;
		if (rtmp_send_audio_raw_stream(sendbuf_aenc_sent, buflen , pts)) {
			printf("%s: rtmp_send_audio_raw_stream send failed\n", __func__);
			reInitRTMP = HI_TRUE;
			sleep(1);
			rtmp_destroy_client();
		}
	}

	return HI_SUCCESS;
}

void *SAMPLE_COMM_AUDIO_AencProcSent(void *parg) {
    HI_S32 s32Ret;
    HI_S32 AencFd;
    SAMPLE_AENC_S *pstAencCtl = (SAMPLE_AENC_S *)parg;
    AUDIO_STREAM_S stStream;
    fd_set read_fds;
    struct timeval TimeoutVal;

    struct sockaddr_in si_me;

    FD_ZERO(&read_fds);
    AencFd = HI_MPI_AENC_GetFd(pstAencCtl->AeChn);
    FD_SET(AencFd, &read_fds);

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(AUDIO_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    udpfd_audio = socket(AF_INET,SOCK_DGRAM,0);//UDP
    printf("udp audio up\n");

    bind(udpfd_audio, (struct sockaddr*)&si_me, sizeof(si_me));

    while (pstAencCtl->bStart)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(AencFd, &read_fds);

        s32Ret = select(AencFd+1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            break;
        }
        else if (0 == s32Ret)
        {
            printf("%s: get aenc stream select time out\n", __FUNCTION__);
            break;
        }

        if (FD_ISSET(AencFd, &read_fds))
        {
            /* get stream from aenc chn */
            s32Ret = HI_MPI_AENC_GetStream(pstAencCtl->AeChn, &stStream, HI_FALSE);
            if (HI_SUCCESS != s32Ret )
            {
                printf("%s: HI_MPI_AENC_GetStream(%d), failed with %#x!\n",\
                       __FUNCTION__, pstAencCtl->AeChn, s32Ret);
                pstAencCtl->bStart = HI_FALSE;
                return NULL;
            }

            /* send stream to decoder and play for testing */
//            if (HI_TRUE == pstAencCtl->bSendAdChn)
//            {
//                HI_MPI_ADEC_SendStream(pstAencCtl->AdChn, &stStream, HI_TRUE);
//            }

            /* save audio stream to file */
//            fwrite(stStream.pStream, 1, stStream.u32Len, pstAencCtl->pfd);
//            fwrite(stStream.pStream + 4, 1, stStream.u32Len - 4, pstAencCtl->pfd);

            /*Send stream to client */
            PTS_INC++;
            if (PTS_INC > 6576668)
                PTS_INC = 0;
            AENC_Sent(stStream);


            /* finally you must release the stream */
            HI_MPI_AENC_ReleaseStream(pstAencCtl->AeChn, &stStream);
        }
    }

//    fclose(pstAencCtl->pfd);
    pstAencCtl->bStart = HI_FALSE;
    return NULL;
}

HI_S32 init_audio_client(AIO_ATTR_S *pstAioAttr) {
    HI_S32 s32Ret = HI_SUCCESS;

    AUDIO_DEV   AoDev = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;

    s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, gs_AudioClientPayloadType);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, AoChn, pstAioAttr, gs_pstAoReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    //Output 1 at GPIO 4 - 1
    HI_U32 value, addr = 0x201803FC;
    HI_S32 rc;

    rc = HI_MPI_SYS_GetReg(addr, &value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_GetReg failed 0x%x\n", __FUNCTION__, addr);
    }
    value |= 0x00000002;
    rc = HI_MPI_SYS_SetReg(addr, value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_SetReg failed 0x%x\n", __FUNCTION__, addr);
    }

	//creat audio server to receive audio data
	pthread_create(&threadReceiveAudioData, 0, AudioServerListen, (void*)&AdChn);

	return HI_SUCCESS;
}

HI_S32 destroy_audio_client() {
    AUDIO_DEV   AoDev = 0;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_U32 value, addr = 0x201803FC;
    HI_S32 rc, i;

    for (i = 0; i < MAX_AUDIO_CLIENT; i++) {
    	if (g_AudioClients[i].status == RTSP_CONNECTED) {
    		g_AudioClients[i].status = RTSP_IDLE;
    		close(g_AudioClients[i].socket);
    	}
    }

    //Output 0 at GPIO 4 - 1
    rc = HI_MPI_SYS_GetReg(addr, &value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_GetReg failed 0x%x\n", __FUNCTION__, addr);
    }
    value &= 0xFFFFFFFD;
    rc = HI_MPI_SYS_SetReg(addr, value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_SetReg failed 0x%x\n", __FUNCTION__, addr);
    }

    SAMPLE_COMM_AUDIO_StopAo(AoDev, AoChn, gs_bAioReSample);
    SAMPLE_COMM_AUDIO_StopAdec(AdChn);
    SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);

    return HI_SUCCESS;
}


static char authen_key[200];
void *thread_init_RTMP(void *parg) {
	HI_S32 s32Ret;

	while (HI_TRUE) {
		if ((reInitRTMP == HI_TRUE) && has_server_key) {
#ifdef USE_VIETTEL_IDC
			std::string rtmp_url = *(std::string*)parg;
			s32Ret = rtmp_init_client_rtmp_url(rtmp_url);
#else
			s32Ret = rtmp_init_client_streamname(authen_key);
#endif //USE_VIETTEL_IDC
			if (s32Ret != 0) {
				//RTMP init failed - RTMP not sending
				reInitRTMP = HI_TRUE;
			}
			else {
				//RTMP is sending
				reInitRTMP = HI_FALSE;
			}
		}
		sleep(1);
	}

    return NULL;
}

int process_command_start_stream(Json::Value root) {
	std::string token = root["token"].asString();
	std::string user_read = root["user"].asString();
	std::string pass_read = root["pass"].asString();

	//check param
	if ((token == "") || (user_read == "") || (pass_read == ""))
		return -1;
	//check user name and password
	if ((user_read.compare(user) != 0) || (pass_read.compare(pass) != 0))
		return -2;

	strcpy(authen_key, token.c_str());
	has_server_key = 1;

	return 0;
}

int process_command_stop_stream(Json::Value root) {
	std::string user_read = root["user"].asString();
	std::string pass_read = root["pass"].asString();

	//check param
	if ((user_read == "") || (pass_read == ""))
		return -1;
	//check user name and password
	if ((user_read.compare(user) != 0) || (pass_read.compare(pass) != 0))
		return -2;

	has_server_key = 0;
	reInitRTMP = HI_TRUE;
	sleep(1);
	rtmp_destroy_client();

	return 0;
}

int process_command_check_stream(Json::Value root) {
	std::string user_read = root["user"].asString();
	std::string pass_read = root["pass"].asString();

	//check param
	if ((user_read == "") || (pass_read == ""))
		return -1;
	//check user name and password
	if ((user_read.compare(user) != 0) || (pass_read.compare(pass) != 0))
		return -2;

	if (has_server_key)
		return 1;
	else
		return 0;

	return 0;
}

int process_Authen_server_message(int socket, const char* data) {
	Json::Reader reader;
	Json::Value root;
	int ret = -1;

	if (reader.parse(data, root)) {
#ifdef CHECK_AUTHENTICATE_KEY
		std::string authenticate = root["authenticate"].asString();
		if (authenticate.compare(AUTHENTICATE_KEY_SCTV) != 0)
			return ERR_WRONG_AUTHENTICATE_KEY;
#endif //CHECK_AUTHENTICATE_KEY

		std::string cmd = root["command"].asString();
		if (cmd == "")
			return -1;

		if (cmd.compare("start") == 0) {
			ret = process_command_start_stream(root);
			Json::FastWriter fastWriter;
			Json::Value reply;
			std::string reply_str;
			reply["command"] = "start_reply";
			if (ret == 0)
				reply["status"] = "ok";
			else if (ret == -1)
				reply["status"] = "miss_parameter";
			else if (ret == -2)
				reply["status"] = "wrong_user/pass";
			reply_str = fastWriter.write(reply);
			printf("reply start command - %s", reply_str.c_str());

			//wait RTMP init done
			if (ret == 0) while (reInitRTMP == HI_TRUE) usleep(10000);
			send(socket, reply_str.c_str(), reply_str.length(), 0);
		}
		else if (cmd.compare("stop") == 0) {
			ret = process_command_stop_stream(root);
			Json::FastWriter fastWriter;
			Json::Value reply;
			std::string reply_str;
			reply["command"] = "stop_reply";
			if (ret == 0)
				reply["status"] = "ok";
			else if (ret == -1)
				reply["status"] = "miss_parameter";
			else if (ret == -2)
				reply["status"] = "wrong_user/pass";
			reply_str = fastWriter.write(reply);
			printf("reply stop comand - %s", reply_str.c_str());

			send(socket, reply_str.c_str(), reply_str.length(), 0);
		}
		else if (cmd.compare("check_stream") == 0) {
			ret = process_command_check_stream(root);
			Json::FastWriter fastWriter;
			Json::Value reply;
			std::string reply_str;
			reply["command"] = "check_stream_reply";
			if (ret == 0)
				reply["result"] = "0";
			else if (ret < 0)
				reply["result"] = "-1";
			else if (ret == 1) {
				reply["result"] = "1";
				reply["token"] = authen_key;
			}
			reply_str = fastWriter.write(reply);
			printf("reply check_stream comand - %s", reply_str.c_str());

			send(socket, reply_str.c_str(), reply_str.length(), 0);
		}
		else
			return -2;
	}
	return ret;
}

static char pAuthenRecvBuf[AUTHEN_RECV_SIZE];
void * AuthenClientMsg(void*pParam)
{
	pthread_detach(pthread_self());
	int nRes, ret;


	AUDIO_CLIENT * pClient = (AUDIO_CLIENT*)pParam;
	memset(pAuthenRecvBuf,0,sizeof(pAuthenRecvBuf));
	printf("Authen:-----Create Client %s\n",pClient->IP);
	while(pClient->status != RTSP_IDLE)
	{
		memset(pAuthenRecvBuf,0,sizeof(pAuthenRecvBuf));

		nRes = recv(pClient->socket, pAuthenRecvBuf, AUTHEN_RECV_SIZE,0);
		printf("--------%d - data = %s\n",nRes, pAuthenRecvBuf);

		//Handle client disconnected
		if (nRes == 0) {
			pClient->status = RTSP_IDLE;
			close(pClient->socket);
		}

		ret = process_Authen_server_message(pClient->socket, pAuthenRecvBuf);
		if (ret == ERR_WRONG_AUTHENTICATE_KEY) {
			printf("!!!!!!!!!! ERR_WRONG_AUTHENTICATE_KEY close connect %s\n\n", pClient->IP);
			pClient->status = RTSP_IDLE;
			close(pClient->socket);
		}
	}
	printf("Authen:-----Exit Client %s\n",pClient->IP);
	return NULL;
}

//static HI_U8 adec_test[400];
void * AuthenServerListen(void*pParam)
{
	int s32Socket;
	struct sockaddr_in servaddr;
	int s32CSocket;
	int s32Rtn;
	int s32Socket_opt_value = 1;
	socklen_t nAddrLen;
	struct sockaddr_in addrAccept;

	memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(AUTHEN_SERVER_PORT);

	s32Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(s32Socket ,SOL_SOCKET,SO_REUSEADDR,&s32Socket_opt_value,sizeof(int)) == -1)
    {
        return (void *)(-1);
    }
    s32Rtn = bind(s32Socket, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    if(s32Rtn < 0)
    {
        return (void *)(-2);
    }

    s32Rtn = listen(s32Socket, 50);   /*50,×îŽóµÄÁ¬œÓÊý*/
    if(s32Rtn < 0)
    {
    	return (void *)(-2);
    }

    printf("<<<<Authen server\n");
	nAddrLen = sizeof(struct sockaddr_in);
    while ((s32CSocket = accept(s32Socket, (struct sockaddr*)&addrAccept, &nAddrLen)) >= 0)
    {
		printf("<<<<Authen Client %s Connected...\n", inet_ntoa(addrAccept.sin_addr));

		int nMaxBuf = 10 * 1024;
		if(setsockopt(s32CSocket, SOL_SOCKET, SO_SNDBUF, (char*)&nMaxBuf, sizeof(nMaxBuf)) == -1)
			printf("Authen:!!!!!! Enalarge socket sending buffer error !!!!!!\n");
		int i;
		int bAdd=FALSE;
		for(i=0;i<MAX_AUDIO_CLIENT;i++)
		{
			if(g_AudioClients[i].status == RTSP_IDLE)
			{
				memset(&g_AudioClients[i],0,sizeof(AUDIO_CLIENT));
				g_AudioClients[i].index = i;
				g_AudioClients[i].socket = s32CSocket;
				g_AudioClients[i].status = RTSP_CONNECTED ;//RTSP_SENDING;
				strcpy(g_AudioClients[i].IP,inet_ntoa(addrAccept.sin_addr));
				pthread_t threadIdlsn = 0;

				struct sched_param sched;
				sched.sched_priority = 1;
				//to return ACKecho
				pthread_create(&threadIdlsn, NULL, AuthenClientMsg, &g_AudioClients[i]);
				pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);

				bAdd = TRUE;
				break;
			}
		}
		if(bAdd==FALSE)
		{
			memset(&g_AudioClients[0],0,sizeof(AUDIO_CLIENT));
			g_AudioClients[0].index = 0;
			g_AudioClients[0].socket = s32CSocket;
			g_AudioClients[0].status = RTSP_CONNECTED ;//RTSP_SENDING;
			strcpy(g_AudioClients[0].IP,inet_ntoa(addrAccept.sin_addr));
			pthread_t threadIdlsn = 0;
			struct sched_param sched;
			sched.sched_priority = 1;
			//to return ACKecho
			pthread_create(&threadIdlsn, NULL, AuthenClientMsg, &g_AudioClients[0]);
			pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);
			bAdd = TRUE;
		}
		if(exitok){ exitok++;return NULL; }
    }
    if(s32CSocket < 0)
    {
       // HI_OUT_Printf(0, "RTSP listening on port %d,accept err, %d\n", RTSP_SERVER_PORT, s32CSocket);
    }

	printf("----- INIT_Authen_Listen() Exit !! \n");

	return NULL;
}


void * detect_check_time_left_func(void *param) {
	sleep(1);
	while (detect_time_left) {
		detect_time_left--;
		sleep(1);
		if (detect_time_left == 0) {
			//notify server stop stream
			detect_is_start = FALSE;
			pthread_create(&threadAuthenServerdetect, 0, connect_server_authen_send_detect_command, (void*)&detect_is_start);

			has_server_key = 0;
			reInitRTMP = HI_TRUE;
			sleep(1);
			rtmp_destroy_client();

//			if (detect_sending_server == DETECT_SEND_STREAM)
				detect_sending_server = DETECT_IDLE;
				printf("detect_check_time_left_func end\n");
		}
	}

	return NULL;
}

int detect_process_command_start_stream(Json::Value root) {
	std::string token = root["token"].asString();

	//check param
	if (token == "")
		return -1;

	strcpy(authen_key, token.c_str());
	has_server_key = 1;

	//if start because detect motion, then stop after 60s
	if (detect_sending_server == DETECT_SEND_REQUEST) {
		detect_sending_server = DETECT_SEND_STREAM;
		pthread_create(&thread_detect_check_time_left, 0, detect_check_time_left_func, NULL);
	}

	return 0;
}

int detect_process_command_stop_stream(Json::Value root) {
	has_server_key = 0;
	reInitRTMP = HI_TRUE;
	sleep(1);
	rtmp_destroy_client();

	return 0;
}

int detect_process_Authen_server_message(int socket, const char* data) {
	Json::Reader reader;
	Json::Value root;
	int ret = -1;

	if (reader.parse(data, root)) {
#ifdef CHECK_AUTHENTICATE_KEY
		std::string authenticate = root["authenticate"].asString();
		if (authenticate.compare(AUTHENTICATE_KEY_SCTV) != 0)
			return ERR_WRONG_AUTHENTICATE_KEY;
#endif //CHECK_AUTHENTICATE_KEY

		std::string cmd = root["command"].asString();
		if (cmd == "")
			return -1;

		if (cmd.compare("detect_start") == 0) {
			ret = detect_process_command_start_stream(root);
			Json::FastWriter fastWriter;
			Json::Value reply;
			std::string reply_str;
			reply["command"] = "detect_start_reply";
			if (ret == 0)
				reply["status"] = "ok";
			else if (ret == -1)
				reply["status"] = "miss_parameter";
			reply["token"] = authen_key;
			reply_str = fastWriter.write(reply);
			printf("reply start command - %s", reply_str.c_str());

			//wait RTMP init done
			if (ret == 0) while (reInitRTMP == HI_TRUE) usleep(10000);
			send(socket, reply_str.c_str(), reply_str.length(), 0);
		}
		else if (cmd.compare("stop") == 0) {
			ret = detect_process_command_stop_stream(root);
			Json::FastWriter fastWriter;
			Json::Value reply;
			std::string reply_str;
			reply["command"] = "stop_reply";
			if (ret == 0)
				reply["status"] = "ok";
			else if (ret == -1)
				reply["status"] = "miss_parameter";
			reply_str = fastWriter.write(reply);
			printf("reply stop comand - %s", reply_str.c_str());

			send(socket, reply_str.c_str(), reply_str.length(), 0);
		}
		else
			return -2;
	}
	return ret;
}

int send_detect_command(int socket) {
	Json::FastWriter fastWriter;
	Json::Value root;
	std::string str;

	root["command"] = "detect";
	root["camid"] = camid;

	str = fastWriter.write(root);
	printf("send_detect_command - %s", str.c_str());
	send(socket, str.c_str(), str.length(), 0);

	return 0;
}

int send_detect_end_command(int socket) {
	Json::FastWriter fastWriter;
	Json::Value root;
	std::string str;

	root["command"] = "detect_end";
	root["token"] = authen_key;

	str = fastWriter.write(root);
	printf("send_detect_end_command - %s", str.c_str());
	send(socket, str.c_str(), str.length(), 0);

	return 0;
}

int sockfd;
static char pAuthenServerRecvBuf[AUTHEN_RECV_SIZE];
void * connect_server_authen_send_detect_command(void* pParam) {
	struct sockaddr_in serveraddr;
	struct hostent *hostp;
	int nRes;

	unsigned long nonblock = 1;
	fd_set wfds;
	int ret;
	bool is_start = *(bool*)pParam;

	// get a socket descriptor
	int id = 0;
	if ((id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Connect Server: Cannot create socket\n");
//		return NULL;
		goto ERR_CONNECT_FAIL;
	}

	memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVER_AUTHEN_PORT);
	if ((serveraddr.sin_addr.s_addr = inet_addr(SERVER_AUTHEN_IP)) == (unsigned long) INADDR_NONE) {
		hostp = gethostbyname(SERVER_AUTHEN_IP);
		if (hostp == (struct hostent *) NULL) {
			printf("Connect Server: ---This is a client program---\n");
		}
		memcpy(&serveraddr.sin_addr, hostp->h_addr,	sizeof(serveraddr.sin_addr));
	}

	// create non-block connection and use select() to timeout if connect error
	ioctl(id, FIONBIO, &nonblock);
	FD_ZERO(&wfds);
	FD_SET(id, &wfds);
	struct timeval timeout;
	timeout.tv_sec = 7;
	timeout.tv_usec = 0;
	ret = connect(id, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if (ret == SOCKET_ERROR) {
        if (errno != EINPROGRESS) {
            close(id);
//            return NULL;
            goto ERR_CONNECT_FAIL;
        }
    }

	ret = select(id+1, NULL, &wfds, NULL, &timeout);
	if (ret < 0) {
		printf("Connect Server: Error Select\n");
//		return NULL;
		goto ERR_CONNECT_FAIL;
	}
	else if (ret == 0) {
		sockfd = -1;
		printf("Connect Server: Select Timeout\n");
//		return NULL;
		goto ERR_CONNECT_FAIL;
	}
	else {
		socklen_t lon = sizeof(int);
		int valopt;
		if (getsockopt(id, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon)<0) {
			printf("Connect Server: Error in getsockopt()\n");
//			return NULL;
			goto ERR_CONNECT_FAIL;
		}
		if (valopt) {
			printf("Connect Server: Error in connection\n");
//			return NULL;
			goto ERR_CONNECT_FAIL;
		}
		sockfd = id;

		// set connection back to block
		nonblock = 0;
		ioctl(id, FIONBIO, &nonblock);
	}

	if (is_start)
		send_detect_command(sockfd);
	else
		send_detect_end_command(sockfd);

	while(1)
	{
		memset(pAuthenServerRecvBuf,0,sizeof(pAuthenServerRecvBuf));

		nRes = recv(sockfd, pAuthenServerRecvBuf, AUDIO_RECV_SIZE,0);
		printf("--------%d - data = %s\n",nRes, pAuthenServerRecvBuf);
//		printf("data = %s", pAuthenRecvBuf);

		//Handle client disconnected
		if (nRes == 0) {
			close(sockfd);
			break;
		}

		if (detect_process_Authen_server_message(sockfd, pAuthenServerRecvBuf) != 0) {
			detect_sending_server = DETECT_IDLE;
		}
		close(sockfd);
		break;
	}

	return NULL;
ERR_CONNECT_FAIL:
	detect_sending_server = DETECT_IDLE;
	return NULL;
}

int check_motion_main(VDA_DATA_S *pstVdaData) {
    HI_S32 i, j;
    HI_VOID *pAddr;
    FILE *fp = stdout;
    unsigned int total = 0;

    detect_is_start = TRUE;

    if (HI_TRUE != pstVdaData->unData.stMdData.bMbSadValid)
    {
        fprintf(fp, "bMbSadValid = FALSE.\n");
        return HI_SUCCESS;
    }

    for(i=0; i<pstVdaData->u32MbHeight; i++) {
		pAddr = (HI_VOID *)((HI_U32)pstVdaData->unData.stMdData.stMbSadData.pAddr
		  			+ i * pstVdaData->unData.stMdData.stMbSadData.u32Stride);
		for(j=0; j<pstVdaData->u32MbWidth; j++)
		{
	        HI_U8  *pu8Addr;
            pu8Addr = (HI_U8 *)pAddr + j;
            total += *pu8Addr;
		}
    }
//    fprintf(fp, "===== %s ===== %u\n", __FUNCTION__, total);
    if (total > VDA_MOTION_DETECT_THREADHOLD) {
//    	fprintf(fp, "===== %s ===== detect motion\n", __FUNCTION__);

    	if (!has_server_key) {
			detect_time_left = 60;
			if (detect_sending_server == DETECT_IDLE) {
				fprintf(fp, "===== %s ===== connect server  - total = %d\n", __FUNCTION__, total);
				//connect server authen and send detect command
				pthread_create(&threadAuthenServerdetect, 0, connect_server_authen_send_detect_command, (void*)&detect_is_start);
				detect_sending_server = DETECT_SEND_REQUEST;
			}
    	}
    	else {
			fprintf(fp, "===== %s ===== camera is streaming to server => not send command detect%d\n", __FUNCTION__, total);
    	}
    }
    fflush(fp);

    return 0;
}

HI_VOID *VDA_MdGetResult(HI_VOID *pdata)
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
        SAMPLE_PRT("HI_MPI_VDA_GetFd failed with %#x!\n", VdaFd);
        return NULL;
    }
    if (maxfd <= VdaFd)
        maxfd = VdaFd;

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
                check_motion_main(&stVdaData);
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

int start_thread_check_motion_main() {
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

    pthread_create(&gs_stMdParam.stVdaPid, 0, VDA_MdGetResult, (HI_VOID *)&gs_stMdParam);

	return 0;
}

int stop_thread_check_motion_main() {
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


int read_register(HI_U32 addr, HI_U32 *value) {
    HI_S32 rc;

    rc = HI_MPI_SYS_GetReg(addr, value);
    if (rc) {
    	printf("%s: HI_MPI_SYS_GetReg failed 0x%x\n", __FUNCTION__, addr);
    	return rc;
    }
    printf("%s: register 0x%x - 0x%x\n", __FUNCTION__, addr, *value);
	return rc;
}

//int write_register(HI_U32 addr, HI_U32 value) {
//    HI_S32 rc;
//
//    rc = HI_MPI_SYS_SetReg(addr, value);
//    if (rc) {
//    	printf("%s: HI_MPI_SYS_SetReg failed 0x%x\n", __FUNCTION__, addr);
//    	return rc;
//    }
//    printf("%s: set register 0x%x to 0x%x\n", __FUNCTION__, addr, value);
//	return rc;
//}

//int read_read() {
//	HI_U32 value;
//	read_register(0x205a0000, &value);
//	read_register(0x205a0004, &value);
//	read_register(0x205a0008, &value);
//	read_register(0x205a000c, &value);
//	read_register(0x205a0010, &value);
//	read_register(0x205a0014, &value);
//	read_register(0x205a0018, &value);
//	read_register(0x205a001c, &value);
//	read_register(0x205a0030, &value);
//	return 0;
//}

#ifdef CHECK_ISP_REGISTER
int check_isp_register() {
	HI_U32 addr, value;
	addr = 0x205a0010;
	read_register(addr, &value);
	if (value != 0x500) {
		printf("ISP not inited\nReboot\n\n");
		system("rm /home/isp_ready");
		system("reboot");
		system("reboot");
	}
	return 0;
}
#endif //CHECK_ISP_REGISTER

HI_S32 create_stream(HI_VOID)
{
    PAYLOAD_TYPE_E enPayLoad[2]= {PT_H264, PT_H264};
    PIC_SIZE_E enSize[2] = {PIC_HD720, PIC_VGA};

    VB_CONF_S stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    VPSS_CHN_MODE_S stVpssChnMode;

    VENC_GRP VencGrp;
    VENC_CHN VencChn;
//    SAMPLE_RC_E enRcMode= SAMPLE_RC_CBR;
    SAMPLE_RC_E enRcMode= SAMPLE_RC_VBR;
    HI_S32 s32ChnNum = 2;

    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;

//    char ch;
//    HI_S32 s32Ret= HI_SUCCESS;
    HI_S32 i;
    AIO_ATTR_S stAioAttr;

    AUDIO_DEV   AiDev = 0;
    AI_CHN      AiChn;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn;

    /******************************************
     step  1: init sys variable
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    switch(SENSOR_TYPE)
    {
        case SONY_IMX122_DC_1080P_30FPS:
        case APTINA_MT9P006_DC_1080P_30FPS:
            enSize[0] = PIC_HD1080;
            break;

        default:
            break;
    }

    stVbConf.u32MaxPoolCnt = 128;

    /*video buffer*/
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize[0], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 8;

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
                enSize[1], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[1].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 2;

    /* hist buf*/
    stVbConf.astCommPool[1].u32BlkSize = (196*4);
    stVbConf.astCommPool[1].u32BlkCnt = 6;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto END_VENC_720P_CLASSIC_0;
    }

    /******************************************
     step 3: start vi dev & chn to capture
    ******************************************/
    stViConfig.enViMode   = SENSOR_TYPE;
    stViConfig.enRotate   = ROTATE_NONE;
    stViConfig.enNorm     = VIDEO_ENCODING_MODE_AUTO;
    stViConfig.enViChnSet = VI_CHN_SET_NORMAL;
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto END_VENC_720P_CLASSIC_1;
    }

#ifdef CHECK_ISP_REGISTER
    //check if isp register has init
    check_isp_register();
#endif //CHECK_ISP_REGISTER

    /******************************************
     step 4: start vpss and vi bind vpss
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize[0], &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END_VENC_720P_CLASSIC_1;
    }

    VpssGrp = 0;
    stVpssGrpAttr.u32MaxW = stSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSize.u32Height;
    stVpssGrpAttr.bDrEn = HI_FALSE;
    stVpssGrpAttr.bDbEn = HI_FALSE;
    stVpssGrpAttr.bIeEn = HI_TRUE;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.bHistEn = HI_TRUE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, &stVpssGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_VENC_720P_CLASSIC_2;
    }

    s32Ret = SAMPLE_COMM_VI_BindVpss(stViConfig.enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vi bind Vpss failed!\n");
        goto END_VENC_720P_CLASSIC_3;
    }

    VpssChn = 0;
    memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
    stVpssChnAttr.bFrameEn = HI_FALSE;
    stVpssChnAttr.bSpEn    = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, HI_NULL, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Enable vpss chn failed!\n");
        goto END_VENC_720P_CLASSIC_4;
    }

    VpssChn = 1;
    stVpssChnMode.enChnMode     = VPSS_CHN_MODE_USER;
    stVpssChnMode.bDouble       = HI_FALSE;
    stVpssChnMode.enPixelFormat = SAMPLE_PIXEL_FORMAT;
    stVpssChnMode.u32Width      = 640;
    stVpssChnMode.u32Height     = 360;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, &stVpssChnMode, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Enable vpss chn failed!\n");
        goto END_VENC_720P_CLASSIC_4;
    }

    /******************************************
     step 5: start stream venc
    ******************************************/
    /*** HD720P **/
    VpssGrp = 0;
    VpssChn = 0;
    VencGrp = 0;
    VencChn = 0;
    s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[0],\
                                   gs_enNorm, enSize[0], enRcMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencGrp, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    /*** vga **/
    VpssChn = 1;
    VencGrp = 1;
    VencChn = 1;
    s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[1], \
                                    gs_enNorm, enSize[1], enRcMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    /******************************************
     step 6: stream venc process -- get stream, then save it to file.
    ******************************************/
//    s32Ret = SAMPLE_COMM_VENC_StartGetStream(s32ChnNum);

    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32ChnNum;

    struct sched_param schedvenc;
    schedvenc.sched_priority = 10;

    s32Ret = pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProcSent, (HI_VOID*)&gs_stPara);
    pthread_setschedparam(gs_VencPid, SCHED_RR, &schedvenc);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto END_VENC_720P_CLASSIC_5;
    }

    /******************************************
     section 2: create audio stream
    ******************************************/
    /* init stAio. all of cases will use it */
//    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_8000;
    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_22050;
    stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag = 1;
    stAioAttr.u32FrmNum = 30;
    stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM;
    stAioAttr.u32ChnCnt = 2;
    stAioAttr.u32ClkSel = 1;

    /********************************************
      step 1: config audio codec
    ********************************************/
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
//        goto END_VENC_720P_CLASSIC_5;
    }


    /********************************************
      step 2: start Ai
    ********************************************/
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32AencChnCnt = 1;//s32AiChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, gs_bAiAnr, gs_pstAiReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
//        goto END_VENC_720P_CLASSIC_5;
    }

    /********************************************
      step 3: start Aenc
    ********************************************/
    s32Ret = SAMPLE_COMM_AUDIO_StartAenc(s32AencChnCnt, gs_enPayloadType);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
//        goto END_VENC_720P_CLASSIC_5;
    }

    /********************************************
      step 4: Aenc bind Ai Chn
    ********************************************/
    for (i=0; i<s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = 0;

        if (HI_TRUE == gs_bUserGetMode)
        {
            s32Ret = SAMPLE_COMM_AUDIO_CreatTrdAiAenc(AiDev, AiChn, AeChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_DBG(s32Ret);
//                goto END_VENC_720P_CLASSIC_5;
            }
        }
        else
        {
            s32Ret = SAMPLE_COMM_AUDIO_AencBindAi(AiDev, AiChn, AeChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_DBG(s32Ret);
//                goto END_VENC_720P_CLASSIC_5;
            }
        }
        printf("Ai(%d,%d) bind to AencChn:%d ok!\n",AiDev , AiChn, AeChn);
    }

    /********************************************
      step 5: start stream aenc
    ********************************************/
//    pfd = SAMPLE_AUDIO_OpenAencFile(AeChn, gs_enPayloadType);
//    if (!pfd)
//    {
//        SAMPLE_DBG(HI_FAILURE);
////        goto END_VENC_720P_CLASSIC_5;
//    }

    gs_stSampleAenc.AeChn = AeChn;
    gs_stSampleAenc.AdChn = AdChn;
    gs_stSampleAenc.bSendAdChn = HI_FALSE;
//    gs_stSampleAenc.pfd = pfd;
    gs_stSampleAenc.bStart = HI_TRUE;

    struct sched_param schedaenc;
    schedaenc.sched_priority = 10;

    s32Ret = pthread_create(&gs_stSampleAenc.stAencPid, 0, SAMPLE_COMM_AUDIO_AencProcSent, (HI_VOID*)&gs_stSampleAenc);
    pthread_setschedparam(gs_stSampleAenc.stAencPid, SCHED_RR, &schedaenc);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
//        goto END_VENC_720P_CLASSIC_5;
    }

    /******************************************
     section 3: create region area to display time
    ******************************************/
#ifdef USE_TIME_REGION
    s32Ret = create_time_region(0, s32ChnNum, enSize);
    if (s32Ret)
    	SAMPLE_PRT("create_time_region failed!\n");
    else
    	start_thread_update_region();
#endif //USE_TIME_REGION

    /******************************************
     section 4: create audio client and send audio to audio out
    ******************************************/
#ifdef USE_AUDIO_SERVER
    s32Ret = init_audio_client(&stAioAttr);
    if (s32Ret)
    	SAMPLE_PRT("init_audio_client failed!\n");
#endif //USE_AUDIO_SERVER

    /******************************************
     section 5: start VDA modul
    ******************************************/
#ifdef USE_MOTION_DETECT
    s32Ret = init_vda_modul(VdaChn);
    if (s32Ret)
    	SAMPLE_PRT("init_vda_modul failed!\n");
    else
//    	start_thread_check_motion();
    	start_thread_check_motion_main();
#endif //USE_MOTION_DETECT

#ifdef RELEASE_NOT_STOP
    while (HI_TRUE) {
    	sleep(1);
    }
#endif //RELEASE_NOT_STOP

    printf("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     step 7: exit process
    ******************************************/
    //VDA
#ifdef USE_MOTION_DETECT
//    stop_thread_check_motion();
    stop_thread_check_motion_main();
    destroy_vda_modul();
#endif //USE_MOTION_DETECT
    //audio
#ifdef USE_AUDIO_SERVER
    destroy_audio_client();
#endif //USE_AUDIO_SERVER
    //region
#ifdef USE_TIME_REGION
    stop_thread_update_region();
    destroy_time_region(0, s32ChnNum);
#endif //USE_TIME_REGION

    VENC_StopGetStream();
    AENC_StopGetStream();

    for (i=0; i<s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        if (HI_TRUE == gs_bUserGetMode)
        {
            SAMPLE_COMM_AUDIO_DestoryTrdAi(AiDev, AiChn);
        }
        else
        {
            SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, AiChn, AeChn);
        }
    }

    SAMPLE_COMM_AUDIO_StopAenc(s32AencChnCnt);
    SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, gs_bAiAnr, gs_bAioReSample);

END_VENC_720P_CLASSIC_5:
    VpssGrp = 0;

    VpssChn = 0;
    VencGrp = 0;
    VencChn = 0;
    SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
    SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);

    VpssChn = 1;
    VencGrp = 1;
    VencChn = 1;
    SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
    SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);

    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_4:	//vpss stop
    VpssGrp = 0;
    VpssChn = 0;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
    VpssChn = 1;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
END_VENC_720P_CLASSIC_3:    //vpss stop
    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_2:    //vpss stop
    SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
END_VENC_720P_CLASSIC_1:	//vi stop
    SAMPLE_COMM_VI_StopVi(&stViConfig);
END_VENC_720P_CLASSIC_0:	//system exit
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

void InitRtspServer()
{
//	int i;
	HI_S32 s32Ret;
	pthread_t threadId;

#ifdef USE_RTSP
	pthread_create(&threadId, 0, RtspServerListen, NULL);
//	pthread_setschedparam(threadId,SCHED_RR,&thdsched);
	printf("RTSP:-----Init Rtsp server\n");
#endif //USE_RTSP

#ifdef USE_AUTHEN_REQUEST_STREAM
	//creat socket server to receive request stream from authen server
	pthread_create(&threadAuthenServer, 0, AuthenServerListen, NULL);
#endif //USE_AUTHEN_REQUEST_STREAM

#ifdef USE_RTMP_INIT_THREAD
	//init rtmp client
	if (send_RTMP_to_server) {
		pthread_create(&threadInitRTMP, 0, thread_init_RTMP, &VTIDC_rtmp_url);
	}
#endif //USE_RTMP_INIT_THREAD

#ifdef USE_CONNECT_HC
	connect_HC();
#endif //USE_CONNECT_HC

//#ifdef USE_VIETTEL_IDC
//	s32Ret = VTIDC_start_rtmp_stream();
//#endif //USE_VIETTEL_IDC

	//init VENC and AENC to get video and audio stream
	s32Ret = create_stream();

//#ifdef USE_VIETTEL_IDC
//	VTIDC_stop_rtmp_stream();
//#endif //USE_VIETTEL_IDC

#ifdef USE_CONNECT_HC
	disconnect_HC();
#endif //USE_CONNECT_HC

#ifdef USE_RTMP_INIT_THREAD
	if (send_RTMP_to_server) {
		rtmp_destroy_client();
	}
#endif //USE_RTMP_INIT_THREAD

	//exit
	if (HI_SUCCESS == s32Ret)
		printf("program exit normally!\n");
	else
	    printf("program exit abnormally!\n");
	exitok++;
}

time_t t_TimeLaseMsg;

void * thread_sync_time(HI_VOID *p) {
	time_t t;
	struct tm *now;
	int pre_hour = -1;

	//Sync time after boot around 60s to avoid peripheral sync time
	sleep(60);

	while (HI_TRUE) {
		t = time(NULL);
		now = localtime(&t);

		if (now->tm_hour != pre_hour) {
			sync_time();
			pre_hour = now->tm_hour;
			t_TimeLaseMsg = time(NULL);
			printf("---------- update t_TimeLaseMsg: t_TimeLaseMsg = %d\n", t_TimeLaseMsg);
		}

		//run image quality auto
		image_quality_auto();

		sleep(30);
	}

	return NULL;
}

HI_S32 STREAM_720p(HI_VOID)
{
	pthread_create(&threadSyncTime, 0, thread_sync_time, NULL);

	InitRtspServer();

	return HI_SUCCESS;
}

int get_cam_id() {
	FILE* file = fopen("/home/mmap_tmpfs/mmap.info", "rb");

	if (!file)
		return -1;
	if (fseek(file, 0x2e08, SEEK_SET))
		return -1;
	fread(camid, 1, 20, file);
	printf("cam id is: \"%s\"\n", camid);

	return 0;
}

int get_cam_local_ip(char *data, std::string name) {
	struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if ((ifa->ifa_addr->sa_family == AF_INET) && (name.compare(ifa->ifa_name) == 0)) {
            // is a valid IP4 Address and name is "name"
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, data, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, data);
            break;
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return 0;
}

int read_info(const char *filename, char *data, int len){
	FILE *pfile;
	int ret;

	pfile = fopen(filename, "rb");
	if (!pfile)
		return -1;

	memset(data, 0, len);
	ret = fread(data, 1, len, pfile);
	if (data[ret - 1] == '\n')
		data[ret - 1] = '\0';
	printf("%s is: \"%s\" - %d\n", filename, data, ret);

	fclose(pfile);
	return 0;
}

int write_info(const char *filename, const char *data, int len) {
	FILE *pfile;
	int ret;

	pfile = fopen(filename, "w");
	if (!pfile)
		return -1;

	ret = fwrite(data, 1, len, pfile);
	printf("%s set to: \"%s\" - %d\n", filename, data, ret);

	fclose(pfile);
	return 0;
}

int get_camera_info() {
	FILE *pfile;
	int ret;

	get_cam_id();

	pfile = fopen("/etc/test/has_setup", "r");
	if (pfile == NULL) {
		printf("not setup\n");
		//not setup yet
		has_setup = FALSE;
		ret = read_info(FILE_IP, camip, 20);
	}
	else {
		printf("has setup\n");
		has_setup = TRUE;
		fclose(pfile);

		//read camera info
		ret = read_info(FILE_USER, user, MAX_USER_PASS_LEN);
		ret = read_info(FILE_PASS, pass, MAX_USER_PASS_LEN);
		ret = read_info(FILE_CAMNAME, camname, MAX_USER_PASS_LEN);
		ret = read_info(FILE_PORT, port, 10);
	}
	ret = get_cam_local_ip(camip, "ra0");
	return 0;
}

/******************************************************************************
* group: connect HC - manager request
******************************************************************************/
#ifdef USE_CONNECT_HC
//static ClientSock_p pClientSocket       = NULL;
#define HC_ADDRESS			"192.168.1.8"
#define HC_ADDRESS_POSTFIX	"30"
#define HC_PORT				1235
static ClientSock_p	pClientSocket	= NULL;
static SClient_p	pSClient		= NULL;

int HC_check_dev_type_and_camid(Json::Value& root) {
	std::string read_dev_type = root[ROOT_MEMBER_TYPE].asString();
	if (read_dev_type.compare(ROOT_MEMBER_TYPE_CAM) != 0)
		return ERR_WRONG_DEVICE_TYPE;
	std::string read_camid = root[ROOT_MEMBER_MAC].asString();
	if (read_camid.compare(camid) != 0)
		return ERR_WRONG_CAMERA_ID;
	return 0;
}

int HC_rep_infos(Json::Value& val, std::string ret) {
	val[VAL_MEMBER_RET] = ret;
	Json::Value rep;
    rep[ROOT_MEMBER_TYPE] = ROOT_MEMBER_TYPE_CAM;
    rep[ROOT_MEMBER_MAC] = camid;
    rep[ROOT_MEMBER_VAL] = val;

	JsonCommand_p pJsonCommand = new JsonCommand();
	pJsonCommand->SetCmdClass(CMD_CLASS_DEV);
	pJsonCommand->SetCommand(CMD_STT);
	pJsonCommand->SetJsonObject(rep);
	return pSClient->sendJsonCommand(pJsonCommand);
}

int HC_reply_command_fail_wrong_info(Json::Value& root, int ret) {
    Json::Value val;
    std::string act_str = root[ROOT_MEMBER_VAL][VAL_MEMBER_ACT].asString();
    if (act_str == "") act_str = "0";
    val[VAL_MEMBER_STATUS] = act_str;
    if (ret == ERR_WRONG_CAMERA_ID) {
    	return HC_rep_infos(val, RET_WRONG_CAM_ID);
    }
    else {
    	return HC_rep_infos(val, RET_WRONG_DEV_TYPE);
    }
}

int HC_rep_cmd(std::string status, std::string ret) {
	Json::Value val;
	val[VAL_MEMBER_STATUS] = status;

	return HC_rep_infos(val, ret);
}

int HC_command_get(Json::Value& root) {
	Json::Value val;
	val[VAL_MEMBER_CAM_ID] = camid;
	val[VAL_MEMBER_CAM_NAME] = user;
	val[VAL_MEMBER_CAM_IP] = camip;
	val[VAL_MEMBER_CAM_PORT] = port;
	val[VAL_MEMBER_CAM_MODEL] = VAL_MEMBER_CAM_MODEL_YI;
	val[VAL_MEMBER_STATUS] = "0";
	return HC_rep_infos(val, RET_SUCCESS);
}

int HC_command_reboot(Json::Value& val, std::string act_str) {
	HC_rep_cmd(act_str, RET_SUCCESS);
	return 0;
}

int HC_command_start_RTMP(Json::Value& val, std::string act_str) {
	int ret = 0;
	std::string ip = val[VAL_MEMBER_RTMP_SERVER].asString();
	std::string port = val[VAL_MEMBER_RTMP_PORT].asString();
	std::string application = val[VAL_MEMBER_RTMP_APP].asString();
	std::string stream =val[VAL_MEMBER_RTMP_STREAM].asString();

	if ((ip == "") || (port == "") || (application == "") || (stream == "")) {
		HC_rep_cmd(act_str, RET_SET_START_RTMP_FAILED);
		return 4;
	}

	if ((reInitRTMP == HI_TRUE)) {
		ret = rtmp_init_client_full(ip, port, application, stream);
		if (ret != 0) {
			//RTMP init failed - RTMP not sending
			reInitRTMP = HI_TRUE;
			HC_rep_cmd(act_str, RET_SET_START_RTMP_FAILED);
		}
		else {
			//RTMP is sending
			reInitRTMP = HI_FALSE;
			HC_rep_cmd(act_str, RET_SUCCESS);
		}
	}
	return ret;
}

int HC_command_stop_RTMP(Json::Value& val, std::string act_str) {
	reInitRTMP = HI_TRUE;
	sleep(1);
	rtmp_destroy_client();
	HC_rep_cmd(act_str, RET_SUCCESS);
	return 0;
}

int HC_command_set(Json::Value& root) {
	int ret = 0;
	Json::Value val = root[ROOT_MEMBER_VAL];
	std::string act_str = val["act"].asString();
	int act = strtol(act_str.c_str(), NULL, 10);
	std::string val_value;

	switch (act) {
		case 1:
			//set cam name
			val_value = val[VAL_MEMBER_CAM_NAME].asString();
			if (val_value != "") {
				ret = write_info(FILE_USER, val_value.c_str(), val_value.length());
				if (!ret) HC_rep_cmd(act_str, RET_SUCCESS);
			}
			if ((val_value == "") || ret)
				HC_rep_cmd(act_str, RET_SET_NAME_FAILED);
			break;
		case 2:
			//set cam ip
			val_value = val[VAL_MEMBER_CAM_IP].asString();
			if (val_value != "") {
				ret = write_info(FILE_IP, val_value.c_str(), val_value.length());
				if (!ret) HC_rep_cmd(act_str, RET_SUCCESS);
			}
			if ((val_value == "") || ret)
				HC_rep_cmd(act_str, RET_SET_IP_FAILED);
			break;
		case 3:
			//set cam port
			val_value = val[VAL_MEMBER_CAM_PORT].asString();
			if (val_value != "") {
				ret = write_info(FILE_PORT, val_value.c_str(), val_value.length());
				if (!ret) HC_rep_cmd(act_str, RET_SUCCESS);
			}
			if ((val_value == "") || ret)
				HC_rep_cmd(act_str, RET_SET_PORT_FAILED);
			break;
		case 4:
			//start rtmp
			ret = HC_command_start_RTMP(val, act_str);
			break;
		case 5:
			//stop rtmp
			ret = HC_command_stop_RTMP(val, act_str);
			break;
		case 6:
			//reboot
			ret = HC_command_reboot(val, act_str);
//			system("reboot");
//			system("reboot");
			exit(0);
			break;
	}

	return ret;
}

int process_HC_command(JsonCommand_p pJsoncommand) {
	if (!pJsoncommand->IsJsonAvailable())
		return -1;

	int ret = 0;
	std::string strCmdClass =  pJsoncommand->GetCmdClass();
	std::string strCmd = pJsoncommand->GetCommand();
	Json::Value root = pJsoncommand->GetJsonOjbect();

	switch (pSClient->getState()) {
		case SClient::IDLE:
			break;
		case SClient::WAIT_AUTHEN:
			if (strCmdClass.compare(CMD_CLASS_AUTH) || strCmd.compare(CMD_RES)) {
				return ERR_COMMAND_NOT_ALLOWED;
			}
			else {
				std::string ret = root["ret"].asString();
				if (ret.compare(RET_SUCCESS) == 0) {
					pSClient->setState(SClient::WAIT_CMD);
				}
			}
			break;
		case SClient::WAIT_CMD:
			if (strCmdClass.compare(CMD_CLASS_DEV))
				return ERR_COMMAND_NOT_ALLOWED;
			ret = HC_check_dev_type_and_camid(root);
			if (ret) {
				HC_reply_command_fail_wrong_info(root, ret);
				return ret;
			}

			if (strCmd.compare(CMD_GET) == 0)
				ret = HC_command_get(root);
			else if (strCmd.compare(CMD_SET) == 0)
				ret = HC_command_set(root);
			else
				return ERR_INVALID_COMMAND;
			break;
		default:
			break;
	}

	return ret;
}

int process_HC_Json_command(string data) {
	int ret = 0;
	pSClient->addData(data);

	while (pSClient->isCommandValid()) {
		printf("find Json command: remain data :\'%s\'\n", pSClient->getRemainData().c_str());
		JsonCommand_p pJsoncommand = pSClient->getJsonCommand();
		if (pJsoncommand != NULL) {
			printf("command class: \'%s\'\n", pJsoncommand->GetCmdClass().c_str());
			printf("command: \'%s\'\n", pJsoncommand->GetCommand().c_str());
			printf("Json value: \'%s\'\n", pJsoncommand->GetJsonValue().c_str());
			ret = process_HC_command(pJsoncommand);
			delete pJsoncommand;
			pJsoncommand = NULL;
		}
	}
	return ret;
}

void * thread_read_HC_data_func(HI_VOID *p) {
	int len, ret;
	unsigned char *pBuffer = NULL;

	while (TRUE) {
		len = pClientSocket->GetBuffer(&pBuffer);
		if (len > 0) {
			std::string data = (char*)pBuffer;
			printf("read HC data: %s - %d\n", data.c_str(), len);
			ret = process_HC_Json_command(data);
			pClientSocket->ResetBuffer();
		}
		else {
			printf("read HC data: null\n");
			break;
		}
	}
	pthread_exit(NULL);
	return NULL;
}

int connect_HC() {
	std::string hc_ip = camip;
	hc_ip = hc_ip.substr(0, hc_ip.rfind(".") + 1) + HC_ADDRESS_POSTFIX;
	printf("hc_ip is: %s\n", hc_ip.c_str());

	pClientSocket = new ClientSock(hc_ip.c_str(), HC_PORT);
	pSClient = new SClient(pClientSocket);

	if (!pSClient->Connect(TRUE)) {
		printf("connect HC failed\n");
		return -1;
	}

	pSClient->SendAuthenComand(ROOT_MEMBER_TYPE_CAM, camid);
	pthread_create(&thread_read_HC_data, 0, thread_read_HC_data_func, NULL);

	return 0;
}

int disconnect_HC() {
    if (pClientSocket->IsConnected()) {
		if (!pSClient->Close()) {
			printf("disconnect HC failed\n");
			return -1;
		}
    }
	if (pSClient != NULL) {
		delete pSClient;
		pSClient = NULL;
	}
	return 0;
}
#endif //USE_CONNECT_HC


// prints string as hex
void phex(uint8_t* str)
{
    unsigned char i;
    for(i = 0; i < 16; ++i)
        printf("%.2x", str[i]);
    printf("\n");
}

/******************************************************************************
* group: AES 128
******************************************************************************/
const std::string AES128_KEY_VIETTEL_IDC	= "cloudcameraVTIDC";
const std::string AES128_KEY_LUMI			= "lumihi3518camera";
const unsigned char AES_BLOCK_NUM_BYTE		= 16;
unsigned char plain_data[300];
unsigned char cipher_data[300];
unsigned char aes128_key[20];

int encrypt_string(std::string input_str, std::string encrypt_key) {
	int i, num_block, str_len;
	unsigned char pad_byte;

	//remove \n if exist in input_str
	if (input_str[input_str.length() - 1] == '\n') {
		input_str = input_str.substr(0, input_str.length() - 1);
	}
	str_len = input_str.length();
	//calculate pad byte
	//if len % 16 = 0 -> pad 16 byte 0x10
	pad_byte = 16 - (str_len % 16);
	memset(plain_data, pad_byte, 300);

	strcpy((char *)plain_data, input_str.c_str());
	strcpy((char *)aes128_key, encrypt_key.c_str());

	plain_data[input_str.length()] = pad_byte;
	num_block = input_str.length() / AES_BLOCK_NUM_BYTE + 1;

	//debug - print plain text
//    printf("plain text: - %d - %d - pad - 0x%2x\n", input_str.length(), num_block, pad_byte);
//    for(i = 0; i < num_block; ++i)
//    {
//        phex(plain_data + i * (uint8_t) AES_BLOCK_NUM_BYTE);
//    }
//    printf("\n");

    //debug - print cipher text
//    printf("ciphertext:\n");
    for(i = 0; i < num_block; ++i)
    {
        AES128_ECB_encrypt(plain_data + (i*16), aes128_key, cipher_data+(i*16));
//        phex(cipher_data + (i*16));
    }
    printf("\n");
	return num_block * 16;
}

std::string decrypt_string(int num_block, std::string encrypt_key) {
	int i;
	unsigned char pad_byte = 0;

	strcpy((char *)aes128_key, encrypt_key.c_str());

//	printf("plain text: - %d\n", num_block);
    for(i = 0; i < num_block; ++i)
    {
    	AES128_ECB_decrypt(cipher_data + (i*16), aes128_key, plain_data+(i*16));
//        phex(plain_data + (i*16));
    }

    //remove pad byte
	if (plain_data[num_block * 16 - 1] != '}')
		pad_byte = plain_data[num_block * 16 - 1];
    for (i = 0; i < pad_byte; i++)
    	plain_data[num_block * 16 - i - 1] = '\0';

    std::string json_link(reinterpret_cast<const char*>(plain_data));
	return json_link;
}

/******************************************************************************
* group: Viettel IDC - HTTP POST - RTMP
******************************************************************************/
#ifdef USE_VIETTEL_IDC
#define VTIDC_AUTHEN_SERVER	"http://125.212.227.174/cloud-camera/services/camera"
#define VTIDC_AUTHEN_SERVER_PUSH "http://125.212.227.174/cloud-camera/services/camera/push"
#define VTIDC_AUTHEN_SERVER_PULL "http://125.212.227.174/cloud-camera/services/camera/pull"
#define CLOUD_ID				"cloudidtest01"
#define SECURE_CODE				"123456"
#define DEVICE_SERIAL			"23172463"

//TRUE - rtmp; FALSE - rtsp
std::string VTIDC_create_message(bool rtmp) {
	int num_chars;
	//Create encrypted data -> cipher_data
	Json::FastWriter fastWriter;
	Json::Value root;
	std::string input_str;
	root["cloudId"] = CLOUD_ID;
	root["secureCode"] = SECURE_CODE;
	root["deviceSerial"] = DEVICE_SERIAL;
	if (!rtmp) {
		root["mainStream"] = "rtsp://123.16.3.190:554/ch0.h264";
		root["subStream"] = "rtsp://123.16.3.190:554/ch1.h264";
	}

	input_str = fastWriter.write(root);
	printf("input_str: %d - %s\n", input_str.length(), input_str.c_str());

	num_chars = encrypt_string(input_str, AES128_KEY_VIETTEL_IDC);

	//convert cipher_data to string. Ex: 0x2a -> "2a"
	std::stringstream ss;
	for(int i=0; i<num_chars; ++i)
	    ss << std::hex << std::setfill('0') << std::setw(2) << (int)cipher_data[i];
	std::string mystr = ss.str();

	//create Json message - {"data":"aes128msg"}
	root.clear();
	root["data"] = mystr;
	input_str = fastWriter.write(root);
	return fastWriter.write(root);
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string VTIDC_send_http_post(const char* server_url, std::string send_str) {
	CURL *curl;
	CURLcode res;
	std::string readString;

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();
	if(curl) {
		/* send all data to this function  */
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	    /* we pass our readString to the callback function */
	    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readString);

		/* First set the URL that is about to receive our POST. This URL can
		 just as well be a https:// URL if that is what should receive the
		 data. */
		curl_easy_setopt(curl, CURLOPT_URL, server_url);

		//char v_ct[64] = "Content-Type: application/json";
		struct curl_slist *curlHeader = NULL;
		curlHeader = curl_slist_append(curlHeader, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeader);

		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send_str.c_str());

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);

		/* free the custom headers */
		curl_slist_free_all(curlHeader);
	}
	curl_global_cleanup();
	return readString;
}

std::string VTIDC_parse_recv_message(std::string recv_str) {
	Json::Reader reader;
	Json::Value root;
	std::string rtmp_url;

	if (reader.parse(recv_str, root)) {
		int status = root["status"].asInt();
		std::string message = root["message"].asString();

		if ((status == 0) && (message.compare("success") == 0)) {
			std::string data = root["data"].asString();
			if (data != "") {
				//convert recv string to cipher data. Ex: "2a" -> 0x2a
				int len = data.length();
				int i;
				for (i = 0; i < len; i += 2) {
					std::string byte = data.substr(i, 2);
					cipher_data[i / 2] = strtol(byte.c_str(), NULL, 16);
			    }

			    int num_block = (len / 2 + 15) / AES_BLOCK_NUM_BYTE;

			    printf("ciphertext: %d - %d \n", len, num_block);
			    for(i = 0; i < num_block; ++i)
			    {
			        phex(cipher_data + (i*16));
			    }
			    printf("\n");

			    std::string json_link = decrypt_string(num_block, AES128_KEY_VIETTEL_IDC);
			    Json::Value root_link;

			    if (reader.parse(json_link, root_link)) {
			    	rtmp_url = root_link["streamingServer"].asString();
			    }
			}
		}
	}
	return rtmp_url;
}

std::string VTIDC_get_rtmp_link() {
	//create message to send http post - rtmp
	std::string send_str = VTIDC_create_message(TRUE);
//	std::string send_str = VTIDC_create_message(FALSE);
	printf("send_str: %d - %s\n", send_str.length(), send_str.c_str());

	//send http post message - push server
	std::string recv_str = VTIDC_send_http_post(VTIDC_AUTHEN_SERVER_PUSH, send_str);
//	std::string recv_str = VTIDC_send_http_post(VTIDC_AUTHEN_SERVER_PULL, send_str);
	printf("recv_str: %d - %s\n", recv_str.length(), recv_str.c_str());

	//parse recv message
	std::string rtmp_url = VTIDC_parse_recv_message(recv_str);
	printf("rtmp_url: %d - %s\n", rtmp_url.length(), rtmp_url.c_str());

	return rtmp_url;
}

int VTIDC_start_rtmp_stream() {
	int retry_count = 3;
	int ret = -1;

	while ((retry_count > 0) && (ret != 0)) {
		//http post - authen server to get rtmp url
		VTIDC_rtmp_url = VTIDC_get_rtmp_link();

		//send rtmp stream to rtmp url
		if ((reInitRTMP == HI_TRUE) && (VTIDC_rtmp_url != "")) {
			ret = rtmp_init_client_rtmp_url(VTIDC_rtmp_url);
			if (ret != 0) {
				//RTMP init failed - RTMP not sending
				reInitRTMP = HI_TRUE;
			}
			else {
				//RTMP is sending
				reInitRTMP = HI_FALSE;
			}
		}
		sleep(1);
	}

	if (VTIDC_rtmp_url != "") {
		has_server_key = 1;
	}

	return ret;
}

int VTIDC_stop_rtmp_stream() {
	reInitRTMP = HI_TRUE;
	sleep(1);
	rtmp_destroy_client();
	return 0;
}

#endif //USE_VIETTEL_IDC


#ifdef CHECK_ENCRYPT_CAMID
#define KEY_FILE	"/etc/key"

int read_key(char *data, int len){
	FILE *pfile;
	int ret;

	pfile = fopen(KEY_FILE, "rb");
	if (!pfile)
		return -1;

	ret = fread(data, 1, len, pfile);
	if (data[ret - 1] == '\n')
		data[ret - 1] = '\0';
	printf("%s is: \"%s\" - %d\n", KEY_FILE, data, ret);

	fclose(pfile);
	return 0;
}

int check_encrypt_camid() {
	int i, ret, len;
	char key[100];

	memset(key, 0, 100);
	read_key(key, 100);
	std::string str_key = key;
	printf("str_key - %d - %s\n", str_key.length(), str_key.c_str());

	//convert recv string to cipher data. Ex: "2a" -> 0x2a
	len = str_key.length();
	for (i = 0; i < len; i += 2) {
		std::string byte = str_key.substr(i, 2);
		cipher_data[i / 2] = strtol(byte.c_str(), NULL, 16);
    }

    int num_block = (len / 2 + 15) / AES_BLOCK_NUM_BYTE;

    std::string str_decrypt = decrypt_string(num_block, AES128_KEY_LUMI);
    printf("str_decrypt - %d - %s\n", str_decrypt.length(), str_decrypt.c_str());

    if (str_decrypt.compare(camid)) {
    	printf("!!!!!!!!!! Check key fail\n");
    	ret = 1;
    }
    else {
    	printf("========== Check key success\n");
    	ret = 0;
    }
	return ret;
}
#endif //CHECK_ENCRYPT_CAMID

#ifdef USE_TEST_P2P
ClientSock_p	p_P2PClientSocket	= NULL;
SClient_p	p_P2PSClient		= NULL;
static pthread_t thread_read_P2P_server_data;
static pthread_t thread_update_NAT_info;

static char s_mapping_type[200];
static char s_filtering_type[200];
static char s_local_address[50];
static char s_pubic_address[50];

#define BUFLEN 512  //Max length of buffer
struct sockaddr_in si_all;
socklen_t slen_all = sizeof(si_all);
pthread_t pthread_all;
ssize_t recv_len;
bool p2p_receive = false;

std::string p2p_server_ip;
int p2p_server_port;

void* threadRecvMsg_all(void* arg) {
    char buf_all[BUFLEN];
    printf("\n**********%s start\n", __FUNCTION__);

    while (p2p_receive) {
        memset(buf_all,'\0', BUFLEN);
        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(udpfd_video_p2p, buf_all, BUFLEN, 0, (struct sockaddr *) &si_all, &slen_all)) == -1) {
            printf("recvfrom() all");
        }
        printf("\n==========---------- udp from %s:%d - len = %d- %s\n", inet_ntoa(si_all.sin_addr), ntohs(si_all.sin_port), recv_len, buf_all);
    }
    printf("\n**********%s exit\n", __FUNCTION__);
    pthread_exit(NULL);
    return NULL;
}

int P2P_stream_to(Json::Value& root) {
	int i, index;
	std::string app_public, app_public_ip, app_public_port;
	std::string app_local, app_local_ip, app_local_port, app_local_prefix;
	std::string cam_local_ip = s_local_address;
	std::string cam_public_ip = s_pubic_address;
	std::string cam_local_prefix;
	bool send_to_app_public = TRUE;

	app_public = root["mapped_address"].asString();
	index = app_public.find(':', 0);
	app_public_ip = app_public.substr(0, index);
	app_public_port = app_public.substr(index + 1, app_public.length() - index - 1);

	app_local = root["local_address"].asString();
    index = app_local.find(':', 0);
    app_local_ip = app_local.substr(0, index);
    app_local_port = app_local.substr(index + 1, app_local.length() - index - 1);

    index = app_local_ip.rfind('.');
    app_local_prefix = app_local_ip.substr(0, index);

    index = cam_public_ip.rfind(':');
    cam_public_ip = cam_public_ip.substr(0, index);

    index = cam_local_ip.rfind('.');
    cam_local_prefix = cam_local_ip.substr(0, index);

    if ((app_public_ip.compare(cam_public_ip) == 0)
            && (app_local_prefix.compare(cam_local_prefix) == 0)) {
        send_to_app_public = FALSE;
    }

	for (i = 0; i < MAX_P2P_CLIENT; i++) {
		if (g_p2pClients[i].status == RTP_IDLE) {
		    if (send_to_app_public){
                strcpy(g_p2pClients[i].ip, app_public_ip.c_str());
                g_p2pClients[i].rtpport[0] = atoi(app_public_port.c_str());
		    }
		    else {
                strcpy(g_p2pClients[i].ip, app_local_ip.c_str());
                g_p2pClients[i].rtpport[0] = atoi(app_local_port.c_str());
		    }

			g_p2pClients[i].seqnum[0] = 0;
			g_p2pClients[i].status = RTP_SENDING0;
			g_p2pNumClients++;
			printf("========== %s: g_p2pNumClients - %d\n", __FUNCTION__, g_p2pNumClients);
			printf("ip - %s\n", g_p2pClients[i].ip);
			printf("rtpport - %d\n", g_p2pClients[i].rtpport[0]);
			break;
		}
	}
	if (i == MAX_P2P_CLIENT) {
		printf("%s: out of client", __FUNCTION__);
		return -1;
	}

	//Send respond to server
    Json::Value respond;
    respond["result_code"] = 0;
    respond["description"] = "Success";

    JsonCommand_p pJsonCommand = new JsonCommand();
    pJsonCommand->SetCmdClass("app");
    pJsonCommand->SetCommand("ACK");
    pJsonCommand->SetJsonObject(respond);
    return p_P2PSClient->sendJsonCommand(pJsonCommand);

	return 0;
}

int P2P_wait_request_stream_from(Json::Value& root) {
	return 0;
}

int P2P_stop_stream_to(Json::Value& root) {
	int i, index;
    std::string app_public, app_public_ip, app_public_port;
    std::string app_local, app_local_ip, app_local_port, app_local_prefix;
    std::string cam_local_ip = s_local_address;
    std::string cam_public_ip = s_pubic_address;
    std::string cam_local_prefix;
    bool app_same_public_ip_local_prefix = FALSE;

    app_public = root["mapped_address"].asString();
    index = app_public.find(':', 0);
    app_public_ip = app_public.substr(0, index);
    app_public_port = app_public.substr(index + 1, app_public.length() - index - 1);

    app_local = root["local_address"].asString();
    index = app_local.find(':', 0);
    app_local_ip = app_local.substr(0, index);
    app_local_port = app_local.substr(index + 1, app_local.length() - index - 1);

    index = app_local_ip.rfind('.');
    app_local_prefix = app_local_ip.substr(0, index);

    index = cam_public_ip.rfind(':');
    cam_public_ip = cam_public_ip.substr(0, index);

    index = cam_local_ip.rfind('.');
    cam_local_prefix = cam_local_ip.substr(0, index);

    if ((app_public_ip.compare(cam_public_ip) == 0)
            && (app_local_prefix.compare(cam_local_prefix) == 0)) {
        app_same_public_ip_local_prefix = TRUE;
    }

	for (i = 0; i < MAX_P2P_CLIENT; i++) {
		if (g_p2pClients[i].status == RTP_SENDING0) {
			if (!app_same_public_ip_local_prefix && (app_public_ip.compare(g_p2pClients[i].ip) == 0) && (atoi(app_public_port.c_str()) == g_p2pClients[i].rtpport[0])) {
				g_p2pClients[i].status = RTP_IDLE;
				g_p2pNumClients--;
				printf("========== %s: g_p2pNumClients - %d - stop send to %s:%d\n", __FUNCTION__, g_p2pNumClients, g_p2pClients[i].ip, g_p2pClients[i].rtpport[0]);
			}
			else if (app_same_public_ip_local_prefix && (app_local_ip.compare(g_p2pClients[i].ip) == 0) && (atoi(app_local_port.c_str()) == g_p2pClients[i].rtpport[0])) {
                g_p2pClients[i].status = RTP_IDLE;
                g_p2pNumClients--;
                printf("========== %s: g_p2pNumClients - %d - stop send to %s:%d\n", __FUNCTION__, g_p2pNumClients, g_p2pClients[i].ip, g_p2pClients[i].rtpport[0]);
			}
		}
	}
	return 0;
}

int P2P_process_server_command(JsonCommand_p pJsoncommand) {
	if (!pJsoncommand->IsJsonAvailable())
		return -1;

	std::string strCmdClass =  pJsoncommand->GetCmdClass();
	std::string strCmd = pJsoncommand->GetCommand();
	Json::Value root = pJsoncommand->GetJsonOjbect();

	if (strCmdClass.compare("cam") && strCmdClass.compare("app")) {
		printf("not command class cam");
		return -1;
	}
	if (strCmd.compare("SEND_STREAM") == 0) {
		return P2P_stream_to(root);
	} else if (strCmd.compare("wait") == 0) {
		return P2P_wait_request_stream_from(root);
	} else if (strCmd.compare("STOP_STREAM") == 0) {
		return P2P_stop_stream_to(root);
	}
	return 0;
}

int P2P_process_server_Json_command(string data) {
	int ret = 0;
	p_P2PSClient->addData(data);

	while (p_P2PSClient->isCommandValid()) {
		printf("find Json command: remain data :\'%s\'\n", p_P2PSClient->getRemainData().c_str());
		JsonCommand_p pJsoncommand = p_P2PSClient->getJsonCommand();
		if (pJsoncommand != NULL) {
			printf("command class: \'%s\'\n", pJsoncommand->GetCmdClass().c_str());
			printf("command: \'%s\'\n", pJsoncommand->GetCommand().c_str());
			printf("Json value: \'%s\'\n", pJsoncommand->GetJsonValue().c_str());
			ret = P2P_process_server_command(pJsoncommand);
			delete pJsoncommand;
			pJsoncommand = NULL;
		}
	}
	return ret;
}

int P2P_send_cam_info() {
    Json::Value root;
    root["type"] = "camera";
//    root["id"] = camid;

    read_info("/etc/local_address", s_local_address, 50);
    read_info("/etc/mapped_address", s_pubic_address, 50);
    read_info("/etc/mapping_behavior", s_mapping_type, 200);
    read_info("/etc/filtering_behavior", s_filtering_type, 200);

    root["local_address"] = s_local_address;
    root["mapped_address"] = s_pubic_address;
    root["mapping_behavior"] = s_mapping_type;
    root["filtering_behavior"] = s_filtering_type;

    JsonCommand_p pJsonCommand = new JsonCommand();
    pJsonCommand->SetCmdClass("app");
    pJsonCommand->SetCommand("NAT_INFO");
    pJsonCommand->SetJsonObject(root);
    return p_P2PSClient->sendJsonCommand(pJsonCommand);
}

int P2P_register_camera() {
    Json::Value root;
    root["id"] = camid;
//    root["name"] = "camera2";
    root["name"] = camname;
    root["username"] = "user1";
//    root["password"] = "123456";
    std::string pass("123456");
    root["password"] = md5sum(pass.c_str(), pass.length());

    JsonCommand_p pJsonCommand = new JsonCommand();
    pJsonCommand->SetCmdClass("app");
    pJsonCommand->SetCommand("REGISTER_CAMERA");
    pJsonCommand->SetJsonObject(root);
    return p_P2PSClient->sendJsonCommand(pJsonCommand);
}

void P2P_reconnect_server() {
    if (p_P2PSClient != NULL) {
        printf("********** reconnect server **********\n");
        p_P2PSClient->Close();
        sleep(10);
    	if (!p_P2PSClient->Connect(FALSE)) {
    		printf("connect P2P failed\n");
    	}
        P2P_register_camera();
        sleep(1);
        P2P_send_cam_info();
    }
}

void * thread_read_P2P_server_data_func(void *p) {
	int len, ret;
	unsigned char *pBuffer = NULL;
    P2P_register_camera();
    sleep(1);
    P2P_send_cam_info();

	while (TRUE) {
		len = p_P2PClientSocket->GetBuffer(&pBuffer);
		if (len > 0) {
			std::string data = (char*)pBuffer;
			printf("read P2P server data: %s - %d\n", data.c_str(), len);
			ret = P2P_process_server_Json_command(data);
			p_P2PClientSocket->ResetBuffer();
			t_TimeLaseMsg = time(NULL);
			printf("---------- update t_TimeLaseMsg: t_TimeLaseMsg = %d\n", t_TimeLaseMsg);
		}
//		else {
//			if (p_P2PClientSocket->m_boIsConnected)
//				printf("read P2P server data: null\n");
//			break;
//			P2P_reconnect_server();
//		}
	}
	pthread_exit(NULL);
	return NULL;
}

void P2P_recreate_udp_socket() {
    struct sockaddr_in si_me;
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(VIDEO_PORT_P2P);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    udpfd_video_p2p = socket(AF_INET, SOCK_DGRAM, 0);//UDP
    printf("udp video p2p up\n");

    bind(udpfd_video_p2p, (struct sockaddr*)&si_me, sizeof(si_me));
    p2p_receive = true;
}

void P2P_close_udp_socket() {
    p2p_receive = false;
    close(udpfd_video_p2p);
    udpfd_video_p2p = -1;
}

void P2P_update_NAT_info(bool full) {
    char *ptr_array[6];
    ptr_array[1] = (char*)malloc(30 * sizeof(char));
    ptr_array[2] = (char*)malloc(30 * sizeof(char));
    ptr_array[3] = (char*)malloc(30 * sizeof(char));
    if (full) {
        ptr_array[4] = (char*)malloc(30 * sizeof(char));
        ptr_array[5] = (char*)malloc(30 * sizeof(char));
    }
    strcpy(ptr_array[1], "125.212.227.131");
    strcpy(ptr_array[2], "--localport");
    strcpy(ptr_array[3], "30000");
    if (full) {
        strcpy(ptr_array[4], "--mode");
        strcpy(ptr_array[5], "full");
    }

    stunclient(full ? 6 : 4, ptr_array);

    free(ptr_array[1]);
    free(ptr_array[2]);
    free(ptr_array[3]);
    if (full) {
        free(ptr_array[4]);
        free(ptr_array[5]);
    }
}

bool P2P_check_last_time_msg() {
    time_t t_TimeCurrent = time(NULL);
    if (t_TimeCurrent - t_TimeLaseMsg > 300) {
        printf("========== time out server (t_TimeCurrent = %d, t_TimeLaseMsg = %d)=> reconnect\n", t_TimeCurrent, t_TimeLaseMsg);
        t_TimeLaseMsg = t_TimeCurrent;
        return FALSE;
    }
    return TRUE;
}

void * thread_update_NAT_info_func(void *p) {
    while (TRUE) {
        sleep(30);
        if (!P2P_check_last_time_msg())
            P2P_reconnect_server();
        if (g_p2pNumClients == 0) {
            P2P_close_udp_socket();
            P2P_update_NAT_info(FALSE);
            P2P_recreate_udp_socket();
        }
        P2P_send_cam_info();
    }
    pthread_exit(NULL);
    return NULL;
}

int P2P_connect_server() {
//	std::string p2p_server_ip = std::string(argv[3]);
//	int p2p_server_port = atoi(argv[4]);

	p_P2PClientSocket = new ClientSock(p2p_server_ip.c_str(), p2p_server_port);
	p_P2PSClient = new SClient(p_P2PClientSocket);

	if (!p_P2PSClient->Connect(FALSE)) {
		printf("connect P2P failed\n");
//		return -1;
	}

    pthread_create(&thread_read_P2P_server_data, 0, thread_read_P2P_server_data_func, NULL);
    pthread_create(&thread_update_NAT_info, 0, thread_update_NAT_info_func, NULL);
	return 0;
}

void P2P_parse_clients(int argc, char *argv[]) {
	int i;

	if (argc < 3)
		return;

	t_TimeLaseMsg = time(NULL);
	//get NAT info from stun server
	P2P_update_NAT_info(TRUE);

    P2P_recreate_udp_socket();

    //Read incoming udp package for test
//    pthread_create(&pthread_all, NULL, &threadRecvMsg_all, NULL);
//    pthread_detach(pthread_all);
    //End

	g_p2pNumClients = atoi(argv[2]);
	//default send p2p stream to no enpoint
	for (i = 0; i < MAX_P2P_CLIENT;i++) {
		g_p2pClients[i].status = RTP_IDLE;
	}
	//connnect p2p server
	if (g_p2pNumClients == 0) {
	    p2p_server_ip = std::string(argv[3]);
	    p2p_server_port = atoi(argv[4]);
		P2P_connect_server();
		return;
	}

	if (g_p2pNumClients > MAX_P2P_CLIENT) g_p2pNumClients = MAX_P2P_CLIENT;
	for (i = 0; i < g_p2pNumClients; i++) {
		strcpy(g_p2pClients[i].ip, argv[3 + i * 2]);
		g_p2pClients[i].rtpport[0] = atoi(argv[4 + i * 2]);
		g_p2pClients[i].seqnum[0] = 0;
		g_p2pClients[i].status = RTP_SENDING0;
		printf("========== g_p2pNumClients - %d\n", i);
		printf("ip - %s\n", g_p2pClients[i].ip);
		printf("rtpport - %d\n", g_p2pClients[i].rtpport[0]);
	}
}
#endif //USE_TEST_P2P

/******************************************************************************
* function    : main()
* Description : main program
******************************************************************************/
int main(int argc, char *argv[])
{
    HI_S32 s32Ret;
    if ( (argc < 2) || (1 != strlen(argv[1])))
    {
        SAMPLE_VENC_Usage(argv[0]);
        return HI_FAILURE;
    }

    signal(SIGINT, SAMPLE_VENC_HandleSig);
    signal(SIGTERM, SAMPLE_VENC_HandleSig);
    
    get_camera_info();

#ifdef CHECK_ENCRYPT_CAMID
    s32Ret = check_encrypt_camid();
//    if (s32Ret)
//    	return -1;
#endif //CHECK_ENCRYPT_CAMID

#ifdef USE_TEST_P2P
    P2P_parse_clients(argc, argv);
#endif //USE_TEST_P2P

    switch (*argv[1])
    {
    	case '0':/* H.264@720p@30fps+H.264@VGA@30fps+H.264@QVGA@30fps */
    		s32Ret = SAMPLE_VENC_720P_CLASSIC();
    		break;
        case '1':/* 1*wifi login */
            s32Ret = WIFI_LOGIN();
            break;
        case '2':
        	s32Ret = STREAM_720p();
        	break;
        case '3':
        	s32Ret = play_notification_file(WIFI_CONNECTED);
        	break;
        case '4':
        	s32Ret = play_notification_file(WIFI_FAILED);
        	break;
        case '5':
        	s32Ret = sync_time();
        	break;
        case 'S':
        	send_RTMP_to_server = HI_TRUE;
        	s32Ret = STREAM_720p();
        	break;
        case '7':
        	s32Ret = get_cam_id();
        	break;
        case 'H':
#ifdef USE_CONNECT_HC
        	s32Ret = connect_HC();
            printf("please press twice ENTER to exit this sample\n");
            getchar();
            getchar();
        	s32Ret = disconnect_HC();
#endif //USE_CONNECT_HC
        	break;
        case 'V':
#ifdef USE_VIETTEL_IDC
        	send_RTMP_to_server = HI_TRUE;
        	VTIDC_rtmp_url = "rtmp://125.212.200.243/cloud-camera/testSbd";
//        	VTIDC_rtmp_url = "rtmp://khiembt:khiembt@43.239.148.86:1935/live/cam-lumi.stream";
//        	VTIDC_rtmp_url = "rtmp://lumi:lumi@192.168.1.30:1935/live/cam-lumi.stream";
//        	VTIDC_rtmp_url = "rtmp://rad.haipv.com:1935/live/cam-lumi.stream";
        	has_server_key = HI_TRUE;
        	s32Ret = STREAM_720p();
//        	s32Ret = VTIDC_start_rtmp_stream();
//            printf("please press twice ENTER to exit this sample\n");
//            getchar();
//            getchar();
//            VTIDC_stop_rtmp_stream();
#endif //USE_VIETTEL_IDC
        	break;
        default:
            printf("the index is invaild!\n");
            SAMPLE_VENC_Usage(argv[0]);
            return HI_FAILURE;
    }

//    if (HI_SUCCESS == s32Ret)
//        printf("program exit normally!\n");
//    else
//        printf("program exit abnormally!\n");
//    exit(s32Ret);
}

//#ifdef __cplusplus
//#if __cplusplus
//}
//#endif
//#endif /* End of #ifdef __cplusplus */
