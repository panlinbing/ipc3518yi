#ifndef CLIENTSOCK_HPP_
#define CLIENTSOCK_HPP_

#include "typedefs.h"
#include "Locker.hpp"

#ifndef SOCKET_ERROR
#define SOCKET_ERROR                        (-1)
#endif /* SOCKET_ERROR */

#define SOCKET_COMPLETE                     (0)
#define SOCKET_INTERRUPTED                  (-2)

typedef struct sockaddr_in                  sockaddr_t;
typedef sockaddr_t*                         sockaddr_p;


class ClientSock {
private:
    int_t m_idwSockfd;
    int_t m_idwPort;
    u8_p m_pByBuffer;

    bool_t m_boIsConnected;
    bool_t m_boIsClosing;
//    bool_t m_boIsStarted;
//    bool_t m_boIsBlocked;

    sockaddr_p m_pSockAddr;

    Locker_p m_pClientSockLocker;

public:
//    ClientSock();
    ClientSock(const_char_p pChostname, int_t idwPort);
    ~ClientSock();
    
//    void_p ClientSockThreadProc(void_p pBuffer);

    bool_t Connect();
    bool_t Close();
    bool_t IsConnected();
//    bool_t IsBlocked();

    bool_t IsWritable(u32_t dwMsecTimeout);
    bool_t IsReadable(u32_t dwMsecTimeout);

//    bool_t Start();
//    bool_t Ping();

//    bool_t SetNonBlocking();
//    bool_t SetBlocking();

    int_t GetBuffer(u8_p *pBuffer);

    void_t PushData(u8_t byData);
    void_t PushBuffer(u8_p pByBuffer, u32_t dwLength);
};

typedef ClientSock  ClientSock_t;
typedef ClientSock* ClientSock_p;

#endif /* CLIENTSOCK_HPP_ */
