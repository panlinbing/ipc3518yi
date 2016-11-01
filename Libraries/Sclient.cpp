/*
 * Sclient.cpp
 *
 *  Created on: Oct 31, 2016
 *      Author: hoang
 */

#include "Sclient.hpp"


SClient::SClient(ClientSock_p pClientSock) {
	mRemainData = "";
	mCommandValid = FALSE;
	mState = IDLE;
	m_pClientSock = pClientSock;
}

SClient::SClient(ClientSock_p pClientSock, std::string data) {
	mRemainData = data;
	mCommandValid = TRUE;
	mState = IDLE;
	m_pClientSock = pClientSock;
}

SClient::~SClient() {

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
SClient::sendJsonCommand(JsonCommand_p pJsoncommand) {
	std::string strOutput = STA + pJsoncommand->GetCmdClass() +
			DE2 + pJsoncommand->GetCommand() +
			pJsoncommand->GetJsonValue() + END;

	strOutput.erase(std::remove(strOutput.begin(), strOutput.end(), ENDLN), strOutput.end());
	strOutput.erase(std::remove(strOutput.begin(), strOutput.end(), SPACE), strOutput.end());

	u32_t dwLength = strOutput.length();
	u8_p pByBuffer = (u8_p)malloc(strOutput.size() + 1);
	memcpy(pByBuffer, &strOutput[0], strOutput.size());

	m_pClientSock->PushBuffer(pByBuffer, dwLength);

	free(pByBuffer);
    if (pJsoncommand != NULL) {
        delete pJsoncommand;
        pJsoncommand = NULL;
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


