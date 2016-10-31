#ifndef LOCKER_HPP_
#define LOCKER_HPP_

#include <pthread.h>
#include "typedefs.h"

typedef pthread_mutex_t locker_t;
typedef locker_t* locker_p;

class Locker {
protected:
    bool_t m_isLocked;
    locker_p m_pLocker;
public:
    Locker();
    ~Locker();

    bool_t Lock();
    bool_t UnLock();
    bool_t TryLock();
    bool_t IsLocked();
    locker_p GetLocker();
    void_t DelLocker();
};

typedef Locker  Locker_t;
typedef Locker* Locker_p;

#endif /* !LOCKER_HPP_ */
