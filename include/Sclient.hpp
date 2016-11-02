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

#define CMD_CLASS_CAM		"cam"
#define CMD_CLASS_KALIVE	"kalive"

#define CAM_TYPE_ANT_YI		"ant-yi"
#define CMD_AUTH			"auth"
#define CMD_GET				"get"
#define CMD_EDIT			"edit"
#define CMD_REP				"rep"
#define CMD_CMD				"cmd"
#define CMD_REBOOT			"reboot"
#define CMD_START_RTMP		"start_RTMP"
#define CMD_STOP_RTMP		"stop_RTMP"
#define CMD_RES				"res"
#define CMD_REQ				"req"

#define INFO_CAM_ID				"camid"
#define INFO_CAM_NAME			"name"
#define INFO_CAM_IP				"camip"
#define INFO_CAM_PORT			"port"
#define INFO_CAM_TYPE			"type"

#define ERR_HAS_SETUP_ALREADY		-1
#define ERR_MISS_PARAMETER			-2
#define ERR_INVALID_COMMAND			-3
#define ERR_NOT_SETUP_YET			-4

#define ERR_COMMAND_NOT_ALLOWED		-11

#define ERR_WRONG_CAMERA_ID			-21
#define ERR_WRONG_CAMERA_USER		-22
#define ERR_WRONG_CAMERA_PASS		-23

#define RET_AUTHEN_SUCCESS		"0"
#define RET_WRONG_CAM_ID		"2"

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

	bool SendAuthenComand(std::string camType, std::string camid, std::string camname, std::string port);
	bool SendKeepAliveCommand();

private:
	std::string mRemainData;
	bool mCommandValid;
	enum State mState;
	enum DeviceClass mDeviceClass;
	ClientSock_p m_pClientSock;
};

typedef SClient  SClient_t;
typedef SClient* SClient_p;

#endif /* SCLIENT_HPP_ */
