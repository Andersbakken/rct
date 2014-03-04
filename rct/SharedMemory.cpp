#include "SharedMemory.h"
#include "Log.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <assert.h>
#include <errno.h>

#define PROJID 3946

SharedMemory::SharedMemory(key_t key, unsigned int size, CreateFlag flag)
    : mShm(-1), mOwner(false), mAddr(0), mKey(-1), mSize(0)
{
    init(key, size, flag);
}

SharedMemory::SharedMemory(const Path& filename, unsigned int size, CreateFlag flag)
    : mShm(-1), mOwner(false), mAddr(0), mKey(-1), mSize(0)
{
    init(ftok(filename.nullTerminated(), PROJID), size, flag);
}

bool SharedMemory::init(key_t key, unsigned int size, CreateFlag flag)
{
    if (key == -1)
        return false;

    mShm = shmget(key, size, (flag == Create) ? (IPC_CREAT | IPC_EXCL) : 0);
    if (mShm == -1)
        return false;

    if (flag == Create) {
        shmid_ds ds;
        memset(&ds, 0, sizeof(ds));
        ds.shm_perm.uid = getuid();
        ds.shm_perm.mode = 0600 | SHM_DEST;
        const int ret = shmctl(mShm, IPC_SET, &ds);
        if (ret == -1) {
            error() << strerror(errno) << errno;
            if (flag == Create)
                shmctl(mShm, IPC_RMID, 0);
            mShm = -1;
            return false;
        }
    }
    mKey = key;
    mOwner = (flag == Create);
    mSize = size;

    return true;
}

SharedMemory::~SharedMemory()
{
    cleanup();
}

void* SharedMemory::attach(AttachFlag flag, void* address)
{
    if (mAddr)
        return mAddr;

    int flg = address ? SHM_RND : 0;
    if (!(flag & Write))
        flg |= SHM_RDONLY;
    mAddr = shmat(mShm, address, flg);
    if (mAddr == reinterpret_cast<void*>(-1)) {
        error() << strerror(errno) << errno;
        mAddr = 0;
    }
    return mAddr;
}

void SharedMemory::detach()
{
    if (!mAddr)
        return;

    shmdt(mAddr);
    mAddr = 0;
}

void SharedMemory::cleanup()
{
    detach();
    if (mShm != -1 && mOwner)
        shmctl(mShm, IPC_RMID, 0);
}
