/*
 * Sclient.cpp
 *
 *  Created on: Oct 31, 2016
 *      Author: hoang
 */

#include <stdio.h>
#include "Sclient.hpp"


SClient::SClient(ClientSock_p pClientSock, enum DeviceClass deviceClass) {
	mRemainData = "";
	mCommandValid = FALSE;
	mState = IDLE;
	m_pClientSock = pClientSock;
	mDeviceClass = deviceClass;
	mThreadKeepAlive = NULL;
	mThreadKeepAliveRun = FALSE;
}

SClient::SClient(ClientSock_p pClientSock, std::string data, enum DeviceClass deviceClass) {
	mRemainData = data;
	mCommandValid = TRUE;
	mState = IDLE;
	m_pClientSock = pClientSock;
	mDeviceClass = deviceClass;
	mThreadKeepAlive = NULL;
	mThreadKeepAliveRun = FALSE;
}

SClient::~SClient() {
    if (m_pClientSock != NULL) {
        delete m_pClientSock;
        m_pClientSock = NULL;
    }
}

std::string
SClient::getRemainData() {
	return mRemainData;
}

void
SClient::setRemainData(std::string data) {
	mRemainData = data;
	mCommandValid = TRUE;
}

void
SClient::addData(std::string data) {
	mRemainData += data;
	mCommandValid = TRUE;
}

bool SClient::isCommandValid() {
	return mCommandValid;
}

JsonCommand_p SClient::getJsonCommand() {
	size_t posEnd = mRemainData.find(END); // find 1st $end
	if (posEnd == std::string::npos) {
		mCommandValid = FALSE;
		mRemainData.clear();
		return NULL;
	}
	else {
		size_t posBegin = mRemainData.rfind(STA, posEnd - 1);
        size_t posParen = mRemainData.substr(posBegin, posEnd - posBegin).find(DE1); // find {
        size_t posEqual = mRemainData.substr(posBegin, posEnd - posBegin).find(DE2); // find 1st =

		if ((std::string::npos == posBegin) || (std::string::npos == posParen) || (std::string::npos == posEqual)) {
			mCommandValid = FALSE;
			setRemainData(mRemainData.substr(posEnd + END.length()));
			return NULL;
		}

		std::string strCmdClass = mRemainData.substr(posBegin + STA.length(), posEqual - posBegin - STA.length());
		std::string strCommand = mRemainData.substr(posEqual + DE2.length(), posParen - posEqual - DE2.length());
		std::string strJsonValue = mRemainData.substr(posParen, posEnd - posParen);


		setRemainData(mRemainData.substr(posEnd + END.length()));
		return new JsonCommand(strCmdClass, strCommand, strJsonValue);
	}
}

int
SClient::sendJsonCommand(JsonCommand_p pJsonCommand) {
	std::string strOutput = STA + pJsonCommand->GetCmdClass() +
			DE2 + pJsonCommand->GetCommand() +
			pJsonCommand->GetJsonValue() + END;

	strOutput.erase(std::remove(strOutput.begin(), strOutput.end(), ENDLN), strOutput.end());
	strOutput.erase(std::remove(strOutput.begin(), strOutput.end(), SPACE), strOutput.end());

	printf("========== %s - %s\n", __FUNCTION__, strOutput.c_str());

	u32_t dwLength = strOutput.length();
	u8_p pByBuffer = (u8_p)malloc(strOutput.size() + 1);
	memcpy(pByBuffer, &strOutput[0], strOutput.size());

	m_pClientSock->PushBuffer(pByBuffer, dwLength);

	free(pByBuffer);
    if (pJsonCommand != NULL) {
        delete pJsonCommand;
        pJsonCommand = NULL;
    }
	return 0;
}

enum SClient::State
SClient::getState() {
	return mState;
}

void
SClient::setState(enum SClient::State state) {
	mState = state;
}

bool
SClient::Connect() {
	bool ret;
    if (m_pClientSock != NULL) {
        ret = m_pClientSock->Connect();
        if (ret == TRUE)
        	StartThreadKeepAlive();
        return ret;
    }
    return FALSE;
}

bool
SClient::Close() {
    if (m_pClientSock != NULL) {
    	StopThreadKeepAlive();
        return m_pClientSock->Close();
    }
    return FALSE;
}

bool
SClient::SendAuthenComand(std::string camType, std::string camid, std::string camname, std::string port) {
    Json::Value root;
    root["type"] = camType;
    root["camid"] = camid;
    root["name"] = camname;
    root["port"] = port;
    JsonCommand_p pJsonCommand = new JsonCommand();
    pJsonCommand->SetCmdClass("auth");
    pJsonCommand->SetCommand("req");
    pJsonCommand->SetJsonObject(root);

    sendJsonCommand(pJsonCommand);
    mState = WAIT_AUTHEN;
	return TRUE;
}

bool
SClient::SendKeepAliveCommand() {
	JsonCommand_p pJsonCommand = new JsonCommand(CMD_CLASS_KALIVE, CMD_REQ, "{}");
	sendJsonCommand(pJsonCommand);
	return TRUE;
}

void *
SClient::ThreadKeepAliveFunc(void *pData) {
	SClient_p pSClient = (SClient_p)pData;
	sleep(3);
	while (pSClient->mThreadKeepAliveRun) {
		if (pSClient->m_pClientSock->IsConnected()) {
			pSClient->SendKeepAliveCommand();
			sleep(3);
		}
		else {
			pSClient->mThreadKeepAliveRun = FALSE;
		}
	}
	pthread_exit(0);
	return NULL;
}

bool
SClient::StartThreadKeepAlive() {
	mThreadKeepAliveRun = TRUE;
	pthread_create(&mThreadKeepAlive, 0, ThreadKeepAliveFunc, this);
	return TRUE;
}

bool
SClient::StopThreadKeepAlive() {
	if (mThreadKeepAliveRun) {
		mThreadKeepAliveRun = FALSE;
		pthread_join(mThreadKeepAlive, 0);
	}
	return TRUE;
}

