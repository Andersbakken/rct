#include "rct/LocalServer.h"
#include "rct/EventLoop.h"
#include "rct/LocalClient.h"
#include "rct/Log.h"
#include "rct/Rct.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define LISTEN_BACKLOG 5

LocalServer::LocalServer()
    : mFd(-1)
{
}

LocalServer::~LocalServer()
{
    if (mFd != -1) {
        EventLoop::instance()->removeFileDescriptor(mFd);
        int ret;
        eintrwrap(ret, ::close(mFd));
    }
}

bool LocalServer::listen(const Path& path)
{
    if (path.exists()) {
        return false;
    }
    struct sockaddr_un address;

    mFd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (mFd < 0) {
        error("LocalServer::listen() Unable to create socket");
        return false;
    }

    memset(&address, 0, sizeof(struct sockaddr_un));

    if (static_cast<int>(sizeof(address.sun_path)) - 1 <= path.size()) {
        int ret;
        eintrwrap(ret, ::close(mFd));
        mFd = -1;
        error("LocalServer::listen() Path too long %s", path.constData());
        return false;
    }

    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path.nullTerminated(), path.size() + 1);

    if (bind(mFd, (struct sockaddr*)&address, sizeof(struct sockaddr_un)) != 0) {
        int ret;
        eintrwrap(ret, ::close(mFd));
        mFd = -1;
        error("LocalServer::listen() Unable to bind");
        return false;
    }

    if (::listen(mFd, LISTEN_BACKLOG) != 0) {
        int ret;
        eintrwrap(ret, ::close(mFd));
        error("LocalServer::listen() Unable to listen to socket");
        mFd = -1;
        return false;
    }

    EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read, listenCallback, this);
    return true;
}

void LocalServer::listenCallback(int, unsigned int, void* userData)
{
    LocalServer* server = reinterpret_cast<LocalServer*>(userData);

    int clientFd;
    eintrwrap(clientFd, ::accept(server->mFd, NULL, NULL));
    if (clientFd != -1) {
        server->mPendingClients.push_back(clientFd);
        server->mClientConnected();
    }
}

LocalClient* LocalServer::nextClient()
{
    if (mPendingClients.empty())
        return 0;
    const int clientFd = mPendingClients.front();
    mPendingClients.pop_front();
    return new LocalClient(clientFd);
}
