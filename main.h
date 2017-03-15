#ifndef MAIN_H
#define MAIN_H

#if !defined(WIN32)
	#define __PACKED__        __attribute__ ((__packed__))
#else
	#define __PACKED__ 
#endif

#include "config.h"

//server authen to send detect command
//#define SERVER_AUTHEN_IP	"112.197.10.210"
#define SERVER_AUTHEN_IP	"192.168.1.6"
//#define SERVER_AUTHEN_IP	"192.168.20.2"
#define SERVER_AUTHEN_PORT	11111
//#define SERVER_AUTHEN_PORT	6969

#define AUTHENTICATE_KEY_SCTV	"AODSIDSADN2983213bd"
#define CHECK_AUTHENTICATE_KEY	1

#define ERR_WRONG_AUTHENTICATE_KEY		-24

#define FILE_CAMNAME	"/etc/test/name"
#define FILE_USER	"/etc/test/user"
#define FILE_PASS	"/etc/test/pass"
#define FILE_IP		"/etc/test/ip"
#define FILE_PORT	"/etc/test/port"
#define MAX_USER_PASS_LEN	50
#define CAMID_LEN			20

typedef enum
{
	DETECT_IDLE = 0,
	DETECT_SEND_REQUEST = 1,
	DETECT_SEND_STREAM = 2,
}DETECT_SENDING_STEP;

typedef enum
{
	RTSP_VIDEO=0,
	RTSP_VIDEOSUB=1,
	RTSP_AUDIO=2,
	RTSP_YUV422=3,
	RTSP_RGB=4,
	RTSP_VIDEOPS=5,
	RTSP_VIDEOSUBPS=6
}enRTSP_MonBlockType;

struct _RTP_FIXED_HEADER
{
    /**//* byte 0 */
    unsigned char csrc_len:4;        /**//* expect 0 */
    unsigned char extension:1;        /**//* expect 1, see RTP_OP below */
    unsigned char padding:1;        /**//* expect 0 */
    unsigned char version:2;        /**//* expect 2 */
    /**//* byte 1 */
    unsigned char payload:7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned char marker:1;        /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short seq_no;            
    /**//* bytes 4-7 */
    unsigned  long timestamp;        
    /**//* bytes 8-11 */
    unsigned long ssrc;            /**//* stream number is used here. */
} __PACKED__;

typedef struct _RTP_FIXED_HEADER RTP_FIXED_HEADER;

struct _NALU_HEADER
{
    //byte 0
	unsigned char TYPE:5;
    	unsigned char NRI:2;
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _NALU_HEADER NALU_HEADER;

struct _FU_INDICATOR
{
    	//byte 0
    	unsigned char TYPE:5;
	unsigned char NRI:2; 
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _FU_INDICATOR FU_INDICATOR;

struct _FU_HEADER
{
   	 //byte 0
    	unsigned char TYPE:5;
	unsigned char R:1;
	unsigned char E:1;
	unsigned char S:1;    
} __PACKED__; /**//* 1 BYTES */
typedef struct _FU_HEADER FU_HEADER;

struct _AU_HEADER
{
    //byte 0, 1
    unsigned short au_len;
    //byte 2,3
    unsigned char au_index:3;
    unsigned  short frm_len:13;  
//    unsigned char au_index:3;
} __PACKED__; /**//* 1 BYTES */
typedef struct _AU_HEADER AU_HEADER;

struct _RTSP_INTERLEAVED_FRAME {
	//byte 0
	unsigned char magic;
	//byte 1
	unsigned char channel;
	//byte 2, 3
	unsigned short lenght;
} __PACKED__;
typedef struct _RTSP_INTERLEAVED_FRAME RTSP_INTERLEAVED_FRAME;

void InitRtspServer();
int AddFrameToRtspBuf(int nChanNum,enRTSP_MonBlockType eType, char * pData, unsigned int  nSize, unsigned int  nVidFrmNum,int bIFrm);

//sync time with NTP time server
int sync_time();

//region to display current
//int create_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt, PIC_SIZE_E *enSize);
//int destroy_time_region(VENC_GRP VencGrpStart, HI_S32 grpcnt);

void * connect_server_authen_send_detect_command(void* pParam);

#ifdef CHECK_ISP_REGISTER
int check_isp_register();
#endif //CHECK_ISP_REGISTER

#ifdef USE_CONNECT_HC
int connect_HC();
int disconnect_HC();
#endif //USE_CONNECT_HC

#ifdef USE_VIETTEL_IDC
int VTIDC_start_rtmp_stream();
int VTIDC_stop_rtmp_stream();
#endif //USE_VIETTEL_IDC

#endif
