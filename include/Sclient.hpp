/*
 * Sclient.hpp
 *
 *  Created on: Oct 31, 2016
 *      Author: hoang
 */

#ifndef SCLIENT_HPP_
#define SCLIENT_HPP_

#include "json.h"
#include <string.h>
#include <algorithm>
#include "typedefs.h"
#include "ClientSock.hpp"
#include "JsonCommand.hpp"

const std::string STA = std::string("$");
const std::string END = std::string("$end");
const std::string DE1 = std::string("{");
const std::string DE2 = std::string("=");

#define SPACE		(' ')
#define ENDLN		('\n')

#define CMD_CLASS_AUTH		"auth"
#define CMD_CLASS_DEV		"dev"
#define CMD_CLASS_KALIVE	"kalive"

#define CMD_REQ				"req"
#define CMD_RES				"res"
#define CMD_GET				"get"
#define CMD_SET				"set"
#define CMD_STT				"stt"

#define ROOT_MEMBER_MAC				"mac"
#define ROOT_MEMBER_TYPE			"type"
#define ROOT_MEMBER_TYPE_CAM		"cam"
#define ROOT_MEMBER_VAL				"val"

#define VAL_MEMBER_ACT				"act"
#define VAL_MEMBER_STATUS			"status"
#define VAL_MEMBER_CAM_ID			"camid"
#define VAL_MEMBER_CAM_NAME			"name"
#define VAL_MEMBER_CAM_IP			"camip"
#define VAL_MEMBER_CAM_PORT			"port"
#define VAL_MEMBER_CAM_MODEL		"model"
#define VAL_MEMBER_CAM_MODEL_YI		"ant-yi"
#define VAL_MEMBER_RET				"ret"

#define VAL_MEMBER_RTMP_SERVER		"server_addr"
#define VAL_MEMBER_RTMP_PORT		"server_port"
#define VAL_MEMBER_RTMP_APP			"application"
#define VAL_MEMBER_RTMP_STREAM		"stream"

#define ERR_HAS_SETUP_ALREADY		-1
#define ERR_MISS_PARAMETER			-2
#define ERR_INVALID_COMMAND			-3
#define ERR_NOT_SETUP_YET			-4

#define ERR_COMMAND_NOT_ALLOWED		-11

#define ERR_WRONG_CAMERA_ID			-21
#define ERR_WRONG_CAMERA_USER		-22
#define ERR_WRONG_CAMERA_PASS		-23
#define ERR_WRONG_DEVICE_TYPE		-24

#define RET_SUCCESS					"0"
#define RET_SET_NAME_FAILED			"1"
#define RET_SET_IP_FAILED			"2"
#define RET_SET_PORT_FAILED			"3"
#define RET_SET_START_RTMP_FAILED	"4"
#define RET_SET_STOP_RTMP_FAILED	"5"
#define RET_SET_REBOOT_FAILED		"6"
#define RET_WRONG_DEV_TYPE			"7"
#define RET_WRONG_CAM_ID			"8"

class SClient {
public:
	enum State {
		IDLE,
		WAIT_AUTHEN,
		WAIT_CMD,
	};

	enum DeviceClass {
		CLASS_CAM,
		OTHER,
	};

	SClient(ClientSock_p pClientSock, enum DeviceClass deviceClass = CLASS_CAM);
	SClient(ClientSock_p pClientSock, std::string data, enum DeviceClass deviceClass = CLASS_CAM);
	~SClient();

	std::string getRemainData();
	void setRemainData(std::string remainData);
	void addData(std::string data);

	bool isCommandValid();
	JsonCommand_p getJsonCommand();
	int sendJsonCommand(JsonCommand_p pJsonCommand);

	enum State getState();
	void setState(enum State state);

	bool Connect();
	bool Close();

	bool SendAuthenComand(std::string type, std::string camid);

private:
	std::string mRemainData;
	bool mCommandValid;
	enum State mState;
	enum DeviceClass mDeviceClass;
	ClientSock_p m_pClientSock;
	pthread_t mThreadKeepAlive;
	bool mThreadKeepAliveRun;

	bool SendKeepAliveCommand();
	static void * ThreadKeepAliveFunc(void *pData);
	bool StartThreadKeepAlive();
	bool StopThreadKeepAlive();
};

typedef SClient  SClient_t;
typedef SClient* SClient_p;

#endif /* SCLIENT_HPP_ */
