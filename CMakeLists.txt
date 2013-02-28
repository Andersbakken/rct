cmake_minimum_required(VERSION 2.8)
project(rct)
include(rct.cmake)
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
add_library(rct STATIC ${RCT_SOURCES})
install(CODE "message(\"Installing rct...\")")
install(TARGETS rct DESTINATION lib COMPONENT rct EXPORT rct)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/rct/rct-config.h
        src/rct/Event.h
        src/rct/EventLoop.h
        src/rct/EventReceiver.h
        src/rct/FastDelegate.h
        src/rct/FileSystemWatcher.h
        src/rct/List.h
        src/rct/Log.h
        src/rct/Map.h
        src/rct/LocalClient.h
        src/rct/LocalServer.h
        src/rct/Memory.h
        src/rct/MemoryMonitor.h
        src/rct/Mutex.h
        src/rct/MutexLocker.h
        src/rct/Path.h
        src/rct/Point.h
        src/rct/Process.h
        src/rct/Rct.h
        src/rct/ReadLocker.h
        src/rct/ReadWriteLock.h
        src/rct/Rect.h
        src/rct/RegExp.h
        src/rct/SHA256.h
        src/rct/Semaphore.h
        src/rct/Serializer.h
        src/rct/Set.h
        src/rct/SharedMemory.h
        src/rct/SignalSlot.h
        src/rct/Size.h
        src/rct/StopWatch.h
        src/rct/String.h
        src/rct/Thread.h
        src/rct/ThreadLocal.h
        src/rct/ThreadPool.h
        src/rct/WaitCondition.h
        src/rct/WriteLocker.h
        DESTINATION include/rct)

install(EXPORT "rct" DESTINATION lib/cmake)
