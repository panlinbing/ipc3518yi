/*
 * rtmp.h
 *
 *  Created on: Sep 30, 2016
 *      Author: hoang
 */

#ifndef RTMP_RTMP_H_
#define RTMP_RTMP_H_

#include <sys/types.h>
#include <string>
#include "srs_librtmp.h"

//My server
//#define RTMP_SERVER_URL_QUANGANH	"rtmp://192.168.1.30/live/myStream"
#define RTMP_SERVER_URL_QUANGANH	"rtmp://192.168.1.30/lia?doPublish=zZzZz12345/myStream"
//#define RTMP_SERVER_URL_LUMI		"rtmp://192.168.100.12:1935/live/myStream"
//#define RTMP_SERVER_URL_LUMI		"rtmp://lumi:1234@192.168.100.12:1935/live/myStream"
//#define RTMP_SERVER_URL_LUMI		"rtmp://192.168.100.12:1935/live?token=35ab9cc76d731b4f/myStream"
#define RTMP_SERVER_URL_LUMI		"rtmp://192.168.20.:1935/live?doPublish=12345/stream"
//#define RTMP_SERVER_URL_MAYAO		"rtmp://192.168.100.9:1935/live/myStream"
#define RTMP_SERVER_URL_MAYAO		"rtmp://192.168.100.9:1935/liva?doPublish=admin/myStream"
#define RTMP_SERVER_URL_LUMI_FORWARD	"rtmp://123.16.43.20:1935/live/myStream"
//rtmp anh Duc
#define RTMP_SERVER_URL_A_DUC		"rtmp://113.171.23.144:1935/live/camera"
//rtmp SCTV
#define RTMP_SERVER_URL_SCTV		"rtmp://112.197.2.156:1935/live/camera_1"
//#define RTMP_SERVER_URL_SCTV		"rtmp://admin:sctv123456@112.197.2.156:1935/live/camera_1"

/******************************************************************************
* funciton	: rtmp_init_client
* usage		: call to init rtmp client to send stream to rtmp server
******************************************************************************/
int rtmp_init_client(char* client);
int rtmp_init_client_full(std::string ip, std::string port, std::string application, std::string stream);

/******************************************************************************
* funciton	: rtmp_send_h264_raw_stream
* usage		: call to send h264 raw stream to rtmp server
* data		: pointer to data stream
* size		: len of data stream
* dts		: the dts of h.264 raw data
* pts		: the pts of h.264 raw data
* @remark, the tbn of dts/pts is 1/1000 for RTMP, that is, in ms.
******************************************************************************/
int rtmp_send_h264_raw_stream(char* data, int size, u_int32_t dts, u_int32_t pts);

/******************************************************************************
* funciton	: rtmp_send_audio_raw_stream
* usage		: call to send audio raw stream to rtmp server
* data		: pointer to data stream
* size		: len of data stream
* timestamp	: The timestamp of audio
******************************************************************************/
int rtmp_send_audio_raw_stream(char* data, int size, u_int32_t timestamp);

/******************************************************************************
* funciton	: rtmp_destroy_client
* usage		: call to destroy rtmp client
******************************************************************************/
int rtmp_destroy_client();

#endif /* RTMP_RTMP_H_ */
