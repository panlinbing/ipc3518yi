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
#include "typedefs.h"
#include "JsonCommand.hpp"

const std::string STA = std::string("$");
const std::string END = std::string("$end");
const std::string DE1 = std::string("{");
const std::string DE2 = std::string("=");

class SClient {
private:
	std::string mRemainData;
	bool mCommandValid;

public:
	SClient();
	SClient(std::string data);
	~SClient();

	std::string getRemainData();
	void setRemainData(std::string remainData);
	void addData(std::string data);

	bool isCommandExist();
	JsonCommand getJsonCommand();
};

#endif /* SCLIENT_HPP_ */
