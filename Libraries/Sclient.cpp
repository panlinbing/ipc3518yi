/*
 * Sclient.cpp
 *
 *  Created on: Oct 31, 2016
 *      Author: hoang
 */

#include "Sclient.hpp"

SClient::SClient() {
	mRemainData = "";
	mCommandValid = FALSE;
}

SClient::SClient(std::string data) {
	mRemainData = data;
	mCommandValid = TRUE;
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

bool SClient::isCommandExist() {
	return mCommandValid;
}

JsonCommand SClient::getJsonCommand() {
	size_t posEnd = mRemainData.find(END); // find 1st $end
	if (posEnd == std::string::npos) {
		mCommandValid = FALSE;
		return JsonCommand();
	}
	else {
		size_t posBegin = mRemainData.rfind(STA, posEnd);
        size_t posParen = mRemainData.find(DE1, posBegin); // find {
        size_t posEqual = mRemainData.find(DE2, posBegin); // find 1st =

		if ((std::string::npos == posBegin) || (std::string::npos == posParen) || (std::string::npos == posEqual)) {
			return JsonCommand();
		}

		std::string a = mRemainData.substr(posBegin + STA.length(), posEqual - posBegin - STA.length());
		std::string b = mRemainData.substr(posEqual + DE2.length(), posParen - posEqual - DE2.length());
		std::string c = mRemainData.substr(posParen, posEnd - posParen);

		return JsonCommand();
	}
}
