#include <stddef.h>
#include "debug.hpp"
#include "Locker.hpp"

#ifndef DEBUG_LOCKER
#define debug_locker(x)
#else /* DEBUG_LOCKER */
#define debug_locker(x)             DEBUG(x)
#endif /* DEBUG_LOCKER */

#define LOCK_SUCCESS                (0)
#define LOCK_ERROR                  (-1)

Locker::Locker() {
    m_isLocked = FALSE;
    m_pLocker = new locker_t();
    if (pthread_mutex_init(m_pLocker, NULL) != LOCK_SUCCESS) {
        debug_locker("init fail");
    }
}

Locker::~Locker() {
    debug_locker("delete");
    if (m_pLocker != NULL) {
        pthread_mutex_destroy(m_pLocker);
        delete(m_pLocker);
        m_pLocker = NULL;
    }
}

bool_t
Locker::Lock() {
    int idwResult = 0;
    if (m_pLocker != NULL) {
        if ((idwResult = pthread_mutex_lock(m_pLocker)) == LOCK_SUCCESS) {
            debug_locker("lock success");
            m_isLocked = TRUE;
            return TRUE;
        }
    }
    debug_locker("lock fail");
    return FALSE;
}

bool_t
Locker::UnLock() {
    int idwResult = 0;
    if (m_pLocker != NULL) {
        if((idwResult = pthread_mutex_unlock(m_pLocker)) == LOCK_SUCCESS) {
            debug_locker("unlock success");
            m_isLocked = FALSE;
            return TRUE;
        }
    }
    debug_locker("unlock fail");
    return FALSE;
}

bool_t
Locker::TryLock() {
    int idwResult = 0;
    if (m_pLocker != NULL) {
        if ((idwResult = pthread_mutex_trylock(m_pLocker)) == LOCK_SUCCESS) {
            debug_locker("trylock success");
            m_isLocked = TRUE;
            return TRUE;
        }
    }
    return FALSE;
}

bool_t
Locker::IsLocked() {
    return m_isLocked;
}


locker_p
Locker::GetLocker() {
    return m_pLocker;
}
