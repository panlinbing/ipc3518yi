#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <netdb.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "debug.hpp"
#include "ClientSock.hpp"

#ifndef DEBUG_CLIENTSOCK
#define debug1_clientsock(x)
#define debug2_clientsock(x,args ...)
#else /* DEBUG_CLIENTSOCK */
#define debug1_clientsock(x)                DEBUG1(x)
#define debug2_clientsock(x)                DEBUG2(x,args ...)
#endif /* DEBUG_CLIENTSOCK */

#ifndef INVALID_SOCKET
#define INVALID_SOCKET                      SOCKET_ERROR
#endif /* INVALID_SOCKET */

#define CONNECTION_TIMEOUT_SEC              (10)

#define BUFFER_SOCKET_SIZE                  (1024)

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
ClientSock::ClientSock(
    const_char_p pChAddress,
    int_t idwPort
) {
    int idwResult = SOCKET_ERROR;

    struct sockaddr_in* pAddress = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
    struct addrinfo* pResult = NULL;
    struct addrinfo hints = { 0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL };

    if ((idwResult = getaddrinfo(pChAddress, NULL, &hints, &pResult)) == 0) {
        struct addrinfo* res = pResult;
        while (res != NULL) {
            if (res->ai_family == AF_INET) {
                pResult = res;
                break;
            }
            res = res->ai_next;
        }

        if (pResult == NULL) {
            idwResult = SOCKET_ERROR;
        } else {
            if (pResult->ai_family == AF_INET) {
                pAddress->sin_port = htons(idwPort);
                pAddress->sin_family = AF_INET;
                pAddress->sin_addr = ((struct sockaddr_in*)(pResult->ai_addr))->sin_addr;
            } else {
                idwResult = SOCKET_ERROR;
            }
            freeaddrinfo(pResult);
        }
    }

    m_idwSockfd = 0;
    m_idwPort = idwPort;

    m_pByBuffer = new u8_t[BUFFER_SOCKET_SIZE];
    memset(m_pByBuffer, '\0', BUFFER_SOCKET_SIZE);

    m_boIsConnected = FALSE;
    m_boIsClosing = FALSE;

    m_pSockAddr = pAddress;
    m_pClientSockLocker = new Locker();
}

ClientSock::~ClientSock() {
    if (m_pSockAddr != NULL) {
        delete(m_pSockAddr);
    }

    if (m_pByBuffer != NULL) {
        delete[] m_pByBuffer;
        m_pByBuffer = NULL;
    }

    if (m_pSockAddr != NULL) {
        delete m_pSockAddr;
        m_pSockAddr = NULL;
    }

    if (m_pClientSockLocker != NULL) {
        delete m_pClientSockLocker;
        m_pClientSockLocker = NULL;
    }
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
ClientSock::Connect() {
    int idwSockfd = SOCKET_ERROR;
    unsigned long nonblock = 1;
    /* Set socket fd */
    if ((idwSockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        debug1_clientsock("sock fail"); /* Debug */
        m_boIsConnected = FALSE;
        return FALSE;
    }

    m_boIsClosing = FALSE;
    m_idwSockfd = idwSockfd;

    if (m_pSockAddr != NULL) {
        int_t idwResult;
    	// create non-block connection and use select() to timeout if connect error
    	ioctl(m_idwSockfd, FIONBIO, &nonblock);
        idwResult = connect(m_idwSockfd, (struct sockaddr*) m_pSockAddr, sizeof(*m_pSockAddr));

        {
            fd_set rset, wset;
            struct timeval tval;
            int_t idwError = 0;

            if (idwResult == SOCKET_ERROR) {
//                debug1_clientsock("waiting for connect"); /* Debug */
                DEBUG1("wait connect");
                if (errno != EINPROGRESS) {
//                    debug1_clientsock("connect fail");
                    DEBUG1("connect fail");
                    close(m_idwSockfd);
                    m_boIsConnected = FALSE;
                    return FALSE;
                }
            }

            FD_ZERO(&wset);
//            FD_SET(m_idwSockfd, &rset);
            FD_SET(m_idwSockfd, &wset);
            tval.tv_sec = CONNECTION_TIMEOUT_SEC;
            tval.tv_usec = 0;
            /* Waiting for the socket to be ready for either reading and writing */
            if ((idwResult = select(m_idwSockfd + 1, NULL, &wset, NULL, &tval)) == 0) {
//                debug1_clientsock("timeout"); /* timeout */
                DEBUG1("timeout");
                m_boIsConnected = FALSE;
                close(m_idwSockfd);
                idwError = ETIMEDOUT;
                return FALSE;
            }

//            if (FD_ISSET(m_idwSockfd, &rset) || FD_ISSET(m_idwSockfd, &wset)) {
                socklen_t dwLen = sizeof(idwError);
                if (getsockopt(m_idwSockfd, SOL_SOCKET, SO_ERROR, &idwError, &dwLen) < 0) {
                    /* Solaris pending error */
                	DEBUG1("Error in getsockopt");
                    close(m_idwSockfd);
                    m_boIsConnected = FALSE;
                    return FALSE;
                }
//                else {
//                    DEBUG1("connected");
//                    m_boIsConnected = TRUE;
//                }
//            } else {
//                close(m_idwSockfd);
//                m_boIsConnected = FALSE;
//                return FALSE;
//            }

            m_boIsConnected = TRUE;
            if (idwError > 0) {
            	DEBUG1("Error in connection");
                errno = idwError;
                close(m_idwSockfd);
                m_boIsConnected = FALSE;
                return FALSE;
            }
            DEBUG1("connected");
    		// set connection back to block
    		nonblock = 0;
    		ioctl(m_idwSockfd, FIONBIO, &nonblock);
        }
    }
    return TRUE;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
ClientSock::Close() {
    int_t idwResult;

    debug2_clientsock("close sockfd %d", m_idwSockfd);

    m_pClientSockLocker->Lock();
    if ((idwResult = shutdown(m_idwSockfd, SHUT_RDWR)) == SOCKET_ERROR) {
        m_pClientSockLocker->UnLock();
        debug1_clientsock("shutdown fail");
        return FALSE;
    }
    m_pClientSockLocker->UnLock();

    m_pClientSockLocker->Lock();
    if ((idwResult = close(m_idwSockfd)) == SOCKET_ERROR) {
        m_pClientSockLocker->UnLock();
        debug1_clientsock("close fail");
        return FALSE;
    }
    m_pClientSockLocker->UnLock();

    debug2_clientsock("after close sockfd %d", m_idwSockfd);

    m_pClientSockLocker->Lock();
    m_boIsConnected = FALSE;
    m_boIsClosing = TRUE;
    m_pClientSockLocker->UnLock();

    return TRUE;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
ClientSock::IsConnected() {
    return m_boIsConnected;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
ClientSock::IsWritable(
    u32_t dwMsecTimeout
) {
    fd_set Writefds;
    struct timeval timeout;
    bool_t boRetVal = FALSE;
    int_t idwResult;

    FD_ZERO(&Writefds);
    FD_SET(m_idwSockfd, &Writefds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while (dwMsecTimeout > 1000) {
        dwMsecTimeout -= 1000;
        timeout.tv_sec++;
    }
    timeout.tv_usec = dwMsecTimeout * 1000;

    idwResult = select(m_idwSockfd + 1, NULL, &Writefds, NULL, &timeout);

    if (idwResult == 0) {
        // debug_clientsock("timeout"); /* timeout */
    } else if (idwResult == -1) {
        debug1_clientsock("error"); /* error */
    } else {
        if (FD_ISSET(m_idwSockfd, &Writefds)) {
            boRetVal = TRUE;
        }
    }
    return boRetVal;
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
bool_t
ClientSock::IsReadable(
    u32_t dwMsecTimeout
) {
    fd_set Readfds;
    struct timeval timeout;
    bool_t boRetVal = FALSE;
    int_t idwResult;

    FD_ZERO(&Readfds);
    FD_SET(m_idwSockfd, &Readfds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while (dwMsecTimeout > 1000) {
        dwMsecTimeout -= 1000;
        timeout.tv_sec++;
    }
    timeout.tv_usec = dwMsecTimeout * 1000;

    idwResult = select(m_idwSockfd + 1, &Readfds, NULL, NULL, &timeout);

    if (idwResult == 0) {
        //debug_clientsock("timeout"); /* timeout */
    } else if (idwResult == -1) {
        //debug_clientsock("error"); /* error */
    } else {
        if (FD_ISSET(m_idwSockfd, &Readfds)) {
            boRetVal = TRUE;
        }
    }
    return boRetVal;
}

int_t
ClientSock::GetBuffer(u8_p *pBuffer) {
	int_t iLength = 0;
    if (IsConnected()) {
        //DEBUG1("recv Data");
        iLength = recv(m_idwSockfd, m_pByBuffer, BUFFER_SOCKET_SIZE, 0);
        *pBuffer = m_pByBuffer;
        //DEBUG2("len: %d", iLength);
//        memset(m_pByBuffer, '\0', BUFFER_SOCKET_SIZE);
        //handle close socket
        if (iLength == 0) {
        	Close();
        }
    }
    return iLength;
}

void
ClientSock::ResetBuffer() {
	memset(m_pByBuffer, '\0', BUFFER_SOCKET_SIZE);
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
ClientSock::PushData(
    u8_t byData
) {
	if (m_boIsConnected && IsWritable(0)) {
		m_pClientSockLocker->Lock();
		send(m_idwSockfd, &byData, 1, 0);
		m_pClientSockLocker->UnLock();
	}
}

/**
 * @func
 * @brief  None
 * @param  None
 * @retval None
 */
void_t
ClientSock::PushBuffer(
    u8_p pByBuffer,
    u32_t dwLength
) {
	if (m_boIsConnected && IsWritable(0)) {
		m_pClientSockLocker->Lock();
		send(m_idwSockfd, pByBuffer, dwLength, 0);
		m_pClientSockLocker->UnLock();
	}
}

