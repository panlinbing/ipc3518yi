#ifndef JSONCOMMAND_HPP_
#define JSONCOMMAND_HPP_

#include "json.h"
#include <string.h>
#include "typedefs.h"

#define ID_NETWORK                  (0U)
#define ID_DATABASE                 (1U)
#define ID_ZWAVE                    (2U)
#define ID_ZIGBEE                   (3U)

#define NETWORK_FLAG                BV(ID_NETWORK)
#define DATABASE_FLAG               BV(ID_DATABASE)
#define ZWAVE_FLAG                  BV(ID_ZWAVE)
#define ZIGBEE_FLAG                 BV(ID_ZIGBEE)

#ifndef DEFAULT_SRC_FLAG
#define DEFAULT_SRC_FLAG            NETWORK_FLAG
#endif /* !DEFAULT_SRC_FLAG */

#ifndef DEFAULT_DES_FLAG
#define DEFAULT_DES_FLAG            (NETWORK_FLAG | DATABASE_FLAG | ZWAVE_FLAG | ZIGBEE_FLAG)
#endif /* !DEFAULT_DES_FLAG */

class JsonCommand {
private:
	std::string m_strCmdClass;
	std::string m_strCommand;

    u8_t m_bySrcFlag;
    u8_t m_byDesFlag;

    u16_t m_wIdentifyCommand;

    bool_t m_boJsonAvailable;
    Json::Value m_jsonValue;

public:
    JsonCommand();
    JsonCommand(std::string strCmdClass, std::string strCommand, std::string strJson = "",
                u8_t bySrcFlag = DEFAULT_SRC_FLAG, u8_t byDesFlag = DEFAULT_DES_FLAG);
    virtual ~JsonCommand();

    void_t SetCmdClass(std::string strCmdClass);
    std::string GetCmdClass();

    void_t SetCommand(std::string strCmdClass);
    std::string GetCommand();

    void_t SetJsonObject(std::string strJson);
    void_t SetJsonObject(Json::Value& jsonValue);
    bool_t IsJsonAvailable();
    Json::Value& GetJsonOjbect();
    std::string GetJsonValue();

    void_t SetIdentifyCommand(u16_t wIdentifyCommand);
    u16_t GetIdentifyCommand();

    void_t SetSrcFlag(u8_t bySrcFlag);
    u8_t   GetSrcFlag();

    void_t SetDesFlag(u8_t byDesFlag);
    u8_t   GetDesFlag();

};

typedef JsonCommand  JsonCommand_t;
typedef JsonCommand* JsonCommand_p;

#endif /* !JSONCOMMAND_HPP_ */
