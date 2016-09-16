#include "Semaphore.h"
#ifdef _WIN32
// todo: Implement on Windows
#else
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

#include "Rct.h"
#include "Path.h"

#ifndef RCT_PROJID
#define RCT_PROJID 3945
#endif

Semaphore::Semaphore(int key, CreateFlag flag, int value)
{
    const int flg = (flag == Create) ? (IPC_CREAT | IPC_EXCL) : 0;
    mSem = semget(key, value, flg);
    mOwner = ((flg & IPC_CREAT) == IPC_CREAT);
}

Semaphore::Semaphore(const Path& filename, CreateFlag flag, int value)
    : mSem(-1), mOwner(false)
{
    const key_t key = ftok(filename.nullTerminated(), RCT_PROJID);
    if (key == -1)
        return;
    const int flg = (flag == Create) ? (IPC_CREAT | IPC_EXCL) : 0;
    mSem = semget(key, value, flg);
    mOwner = ((flg & IPC_CREAT) == IPC_CREAT);
}

Semaphore::~Semaphore()
{
    if (mSem != -1 && mOwner)
        semctl(mSem, 0, IPC_RMID, 0);
}

void Semaphore::acquire(short num)
{
    sembuf buf = { 0, static_cast<short>(num * -1), SEM_UNDO };
    int ret;
    eintrwrap(ret, semop(mSem, &buf, 1));
    if (ret == -1 && errno == EIDRM)
        mSem = -1;
}

void Semaphore::release(short num)
{
    sembuf buf = { 0, num, SEM_UNDO };
    int ret;
    eintrwrap(ret, semop(mSem, &buf, 1));
    if (ret == -1 && errno == EIDRM)
        mSem = -1;
}

void Semaphore::op(short num)
{
    sembuf buf = { 0, num, SEM_UNDO };
    int ret;
    eintrwrap(ret, semop(mSem, &buf, 1));
    if (ret == -1 && errno == EIDRM)
        mSem = -1;
}
#endif
