cmake_minimum_required(VERSION 2.8)
project(rct)
include(rct.cmake)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
add_library(rct STATIC ${RCT_SOURCES})
install(CODE "message(\"Installing rct...\")")
install(TARGETS rct DESTINATION lib COMPONENT rct EXPORT rct)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h
        rct/Connection.h
        rct/Config.h
        rct/Event.h
        rct/EventLoop.h
        rct/EventReceiver.h
        rct/FastDelegate.h
        rct/FileSystemWatcher.h
        rct/List.h
        rct/Log.h
        rct/Map.h
        rct/SocketClient.h
        rct/SocketServer.h
        rct/Memory.h
        rct/MemoryMonitor.h
        rct/Message.h
        rct/Messages.h
        rct/Mutex.h
        rct/MutexLocker.h
        rct/Path.h
        rct/Point.h
        rct/Process.h
        rct/Rct.h
        rct/ReadLocker.h
        rct/ReadWriteLock.h
        rct/Rect.h
        rct/RegExp.h
        rct/ResponseMessage.h
        rct/SHA256.h
        rct/Semaphore.h
        rct/Serializer.h
        rct/Set.h
        rct/SharedMemory.h
        rct/SignalSlot.h
        rct/Size.h
        rct/StopWatch.h
        rct/String.h
        rct/Thread.h
        rct/ThreadLocal.h
        rct/ThreadPool.h
        rct/Value.h
        rct/WaitCondition.h
        rct/WriteLocker.h
        DESTINATION include/rct)

install(EXPORT "rct" DESTINATION lib/cmake)
