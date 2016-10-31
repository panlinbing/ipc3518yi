#include "JsonCommand.hpp"

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
JsonCommand::JsonCommand() {
    m_strCmdClass = "";
    m_strCommand = "";
    m_bySrcFlag = DEFAULT_SRC_FLAG;
    m_byDesFlag = DEFAULT_DES_FLAG;
    m_boJsonAvailable = FALSE;
    m_jsonValue = 0;
    m_wIdentifyCommand = 0;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
JsonCommand::JsonCommand(
    std::string strCmdClass,
    std::string strCommand,
    std::string strJson,
    u8_t bySrcFlag,
    u8_t byDesFlag
) {
    m_strCmdClass = strCmdClass;
    m_strCommand = strCommand;
    m_bySrcFlag = bySrcFlag;
    m_byDesFlag = byDesFlag;
    m_wIdentifyCommand = 0;
    Json::Reader reader;
    m_boJsonAvailable = reader.parse(strJson, m_jsonValue, false);
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
JsonCommand::~JsonCommand() {
    //DEBUG1("del JsonCommand");
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetCmdClass(
    std::string strCmdClass
) {
    m_strCmdClass = strCmdClass;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
std::string
JsonCommand::GetCmdClass() {
    return m_strCmdClass;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetCommand(
    std::string strCmdClass
) {
    m_strCommand = strCmdClass;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
std::string
JsonCommand::GetCommand() {
    return m_strCommand;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetJsonObject(
    std::string strJson
) {
    Json::Reader reader;
    m_boJsonAvailable = reader.parse(strJson, m_jsonValue, false);
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetJsonObject(
    Json::Value& jsonValue
) {
    m_jsonValue = jsonValue;
    m_boJsonAvailable = TRUE;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
JsonCommand::IsJsonAvailable() {
    return m_boJsonAvailable;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
Json::Value&
JsonCommand::GetJsonOjbect() {
    return m_jsonValue;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
std::string
JsonCommand::GetJsonValue() {
    std::string strValue;
    strValue = m_jsonValue.toStyledString();
    return strValue;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetIdentifyCommand(
    u16_t wIdentifyCommand
) {
    m_wIdentifyCommand = wIdentifyCommand;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
u16_t
JsonCommand::GetIdentifyCommand() {
    return m_wIdentifyCommand;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetSrcFlag(
    u8_t bySrcFlag
) {
    m_bySrcFlag = bySrcFlag;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
u8_t
JsonCommand::GetSrcFlag() {
    return m_bySrcFlag;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
JsonCommand::SetDesFlag(
    u8_t byDesFlag
) {
    m_byDesFlag = byDesFlag;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
u8_t
JsonCommand::GetDesFlag() {
    return m_byDesFlag;
}
