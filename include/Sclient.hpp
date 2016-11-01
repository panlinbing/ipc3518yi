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

class SClient {
public:
	enum State {
		IDLE,
		WAIT_AUTHEN,
	};

	SClient(ClientSock_p pClientSock);
	SClient(ClientSock_p pClientSock, std::string data);
	~SClient();

	std::string getRemainData();
	void setRemainData(std::string remainData);
	void addData(std::string data);

	bool isCommandValid();
	JsonCommand_p getJsonCommand();
	int sendJsonCommand(JsonCommand_p pJsoncommand);

	enum State getState();
	void setState(enum State state);

private:
	std::string mRemainData;
	bool mCommandValid;
	enum State mState;
	ClientSock_p m_pClientSock;
};

typedef SClient  SClient_t;
typedef SClient* SClient_p;

#endif /* SCLIENT_HPP_ */
