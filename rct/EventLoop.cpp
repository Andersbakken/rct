#define HAVE_LIBEVENT2
#include "EventLoop.h"
#include "Timer.h"
#include "rct-config.h"
#include <algorithm>
#include <atomic>
#include <set>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#if defined(HAVE_EPOLL)
#  include <sys/epoll.h>
#elif defined(HAVE_KQUEUE)
#  include <sys/event.h>
#  include <sys/time.h>
#endif

#include <event2/event.h>
#include <event2/thread.h>

#ifdef HAVE_MACH_ABSOLUTE_TIME
#  include <mach/mach.h>
#  include <mach/mach_time.h>
#endif

EventLoop::WeakPtr EventLoop::mainLoop;
std::mutex EventLoop::mainMutex;
static std::once_flag mainOnce;
static pthread_key_t eventLoopKey;

// sadly GCC < 4.8 doesn't support thread_local
// fall back to pthread instead in order to support 4.7

// ### static leak, one EventLoop::WeakPtr for each thread
// ### that calls EventLoop::eventLoop()
static EventLoop::WeakPtr& localEventLoop()
{
    EventLoop::WeakPtr* ptr = static_cast<EventLoop::WeakPtr*>(pthread_getspecific(eventLoopKey));
    if (!ptr) {
        ptr = new EventLoop::WeakPtr;
        pthread_setspecific(eventLoopKey, ptr);
    }
    return *ptr;
}

void eventSIGINTcb(evutil_socket_t fd, short what, void *userdata)
{
  if ( what & EV_SIGNAL ) {
    std::cout << "Caught signal: " << fd << std::endl;
    std::terminate();
  }
}

typedef std::function<void(void)> EventCleanup;

struct EventLoop::EventCallbackData
{
  EventCallbackData()
    : id(0), evloop(nullptr), ev(nullptr), cb(), cleanup()
  {}

  EventCallbackData(int i, EventLoop *loop,
		    event *e = nullptr,
		    EventCallback ecallback = EventCallback(),
		    EventCleanup ecleanup = EventCleanup())
    : id(i), evloop(loop), ev(e), cb(ecallback), cleanup(ecleanup)
  {}
		    
  ~EventCallbackData()
  {
    if (cleanup) { std::cout << "Cleaning up!\n"; cleanup(); }
    if (ev) { event_free(ev); ev = nullptr; }
  }
  
  int id;
  EventLoop *evloop;
  event *ev;
  EventCallback cb;
  EventCleanup cleanup;
};

EventLoop::EventLoop()
    : nextId(1), flgs(0)
{
    std::call_once(mainOnce, [](){
            pthread_key_create(&eventLoopKey, 0);
        });
}

EventLoop::~EventLoop()
{
  cleanup();
  std::cout << __PRETTY_FUNCTION__ << "\n";
}

void EventLoop::init(unsigned f)
{
    std::lock_guard<std::mutex> locker(mutex);
    flgs = f;
    evthread_use_pthreads();
    eventBase = event_base_new();
    sigEvent = evsignal_new(eventBase, SIGINT, eventSIGINTcb, NULL);
    
    std::shared_ptr<EventLoop> that = shared_from_this();
    localEventLoop() = that;
    if (flgs & MainEventLoop)
        mainLoop = that;
}

void EventLoop::cleanup()
{
  std::unique_lock<std::mutex> locker(mutex);
  std::cout << __PRETTY_FUNCTION__ << "\n";

  localEventLoop().reset();

  if ( eventBase ) {
    event_del( sigEvent );
    event_free( sigEvent );

    eventCbMap.clear();
    
    event_base_free( eventBase );
    eventBase = nullptr;
  }
}

bool EventLoop::isRunning()
{
  std::lock_guard<std::mutex> locker(mutex);
  return !event_base_got_exit( eventBase );
}

EventLoop::SharedPtr EventLoop::eventLoop()
{
    EventLoop::SharedPtr loop = localEventLoop().lock();
    if (!loop) {
        std::lock_guard<std::mutex> locker(mainMutex);
        loop = mainLoop.lock();
    }
    return loop;
}


void EventLoop::error(const char* err)
{
    fprintf(stderr, "%s\n", err);
    abort();
}

static int what2sockflags(int what)
{
  auto flags = ((  what & EV_READ ? EventLoop::SocketRead : 0)
		| (what & EV_WRITE ? EventLoop::SocketWrite : 0)
		| (what & EV_PERSIST ? 0 : EventLoop::SocketOneShot));
  return flags;
}

static int sockflags2what(int flags)
{
  auto what = (( flags & EventLoop::SocketRead ? EV_READ : 0 )
	       | (flags & EventLoop::SocketWrite ? EV_WRITE : 0)
	       | (flags & EventLoop::SocketOneShot ? 0 : EV_PERSIST ));
  return what; 
}

extern "C" void postCallback(evutil_socket_t fd,
			     short w,
			     void *data)
{
  EventLoop::EventCallbackData *cbdata = (EventLoop::EventCallbackData *)data;
  std::cout << __PRETTY_FUNCTION__ << " on fd = " << fd << "\n";

  if (!cbdata) {
    std::cout << "cbdata null!!!\n";
    return;
  }
  
  cbdata->cb(fd, what2sockflags(w) );

  std::lock_guard<std::mutex> locker(cbdata->evloop->mutex);
  cbdata->evloop->eventCbMap.erase( cbdata->id );
}

void EventLoop::post(Event* event)
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  std::lock_guard<std::mutex> locker(mutex);

  assert( eventBase != nullptr );

  auto func_wrapper =
    [event](int, int)
    { event->exec(); };

  auto cleanup =
    [event]()
    { delete event; };

  auto id = generateId();

  std::unique_ptr<EventCallbackData> data (
    new EventCallbackData( id, this, nullptr, func_wrapper, cleanup ));

  eventCbMap[ id ] = std::move( data );
  auto dataptr = eventCbMap[ id ].get();
  
  timeval tv = { 0, 0 };
  event_base_once( eventBase, -1, EV_TIMEOUT, postCallback,
		   dataptr, &tv );
}

void EventLoop::quit(int code)
{
  std::lock_guard<std::mutex> locker(mutex);

  std::cout << __PRETTY_FUNCTION__ << " : code : " << code << std::endl;

  if (eventBase && !event_base_got_exit( eventBase )) {
    timeval tv = { 0, 0 };
    event_base_loopexit( eventBase, &tv );
  }
}

void EventLoop::eventDispatch(evutil_socket_t fd, short what, void *arg)
{
  EventCallbackData *cbd = reinterpret_cast<EventCallbackData *>( arg );
  if (!cbd || !cbd->evloop )
    return;

  cbd->evloop->dispatch( cbd, fd, what );  
}

void EventLoop::dispatch(EventCallbackData *cbdata,
			 evutil_socket_t fd, short what)

{
  if ( !cbdata->cb )
    std::cout << "Warning! cb null?!\n";
  else
    cbdata->cb( fd, what2sockflags(what) );
}

// timeout (ms)
int EventLoop::registerTimer(TimerCallback&& func, int timeout, int flags)
{
  std::lock_guard<std::mutex> locker(mutex);

  timeval tv { 0, timeout * 1000l };

  short tflags = flags == Timer::SingleShot ? 0 : EV_PERSIST;

  auto id = generateId();
  auto func_wrapper =
    [func](int fd, int flags)
    {
      func( flags );
    };

  std::cout << "Register Timer Request! id = " << id
	    << "flags = " << flags << "\n";

  std::unique_ptr<EventCallbackData> data(
    new EventCallbackData( id, this,
			   nullptr,
			   func_wrapper ));
    
  event *etimer = event_new( eventBase, -1, tflags,
			     eventDispatch,
			     data.get() );

  data->ev = etimer;
  
  eventCbMap[ id ] = std::move( data );
    
  evtimer_add( etimer, &tv );

  return id;
}

void EventLoop::unregisterTimer(int id)
{
    std::cout << "UnRegister Timer! id = " << id << "\n";
    std::lock_guard<std::mutex> locker(mutex);

    assert( eventBase != nullptr );
    
    auto evcb_it = eventCbMap.find( id ); 

    if (evcb_it == std::end(eventCbMap)) {
      std::cout << "Event Not found in EventCB Map!\n";
    } else {
      eventCbMap.erase( evcb_it );
    }
}

void EventLoop::registerSocket(int fd, int mode,
			       std::function<void(int, int)>&& func)
{
  std::lock_guard<std::mutex> locker(mutex);
  std::cout << "Register Socket Request! fd = " << fd
	    << " mode = " << mode << "\n";

  auto id = fdToId(fd);
  
  assert( eventBase != nullptr );
  assert( eventCbMap.find( id ) == std::end( eventCbMap ) );
  
  auto what = sockflags2what( mode );

  auto ret = evutil_make_socket_nonblocking( fd );

  std::unique_ptr<EventCallbackData> data(
    new EventCallbackData( id, this,
			   nullptr,
			   func ));
  
  event *e = event_new( eventBase, fd, what,
			eventDispatch,
			data.get() );

  data->ev = e;

  if ( mode & SocketOneShot ) {
    data->cleanup = [&, id]() { eventCbMap.erase( id ); };
  }
  
  eventCbMap[ id ] = std::move( data );
    
  event_add( e, nullptr );
}

void EventLoop::updateSocket(int fd, int mode)
{
    std::unique_lock<std::mutex> locker(mutex);
    std::cout << __PRETTY_FUNCTION__ << " fd = " << fd << " new mode = " << mode << "\n";
    
    auto id = fdToId( fd );

    assert( eventBase != nullptr );
    assert( eventCbMap.find( id ) != std::end( eventCbMap ) );

    auto what = (( mode & SocketRead ? EV_READ : 0 )
    		 | (mode & SocketWrite ? EV_WRITE : 0)
    		 | (mode & SocketOneShot ? 0 : EV_PERSIST ));

    auto data = std::move( eventCbMap[ id ] );

    locker.unlock();
    
    unregisterSocket( fd );

    locker.lock();
    
    event *ev = event_new( eventBase, fd, what,
			   eventDispatch,
			   data.get() );

    eventCbMap[ id ] = std::move( data );
    eventCbMap[ id ]->id = id;
    eventCbMap[ id ]->ev = ev;
    
    event_add( ev, nullptr );
}

void EventLoop::unregisterSocket(int fd)
{
  std::cout << __PRETTY_FUNCTION__ << " : fd = " << fd << "\n";
  std::lock_guard<std::mutex> locker(mutex);

  auto id = fdToId( fd );

  assert( eventBase != nullptr );
  assert( eventCbMap.find( id ) != std::end( eventCbMap ) );

  eventCbMap.erase( id );
}

int EventLoop::exec(int timeoutTime)
{
  bool exit = false;
  int ret;
  
  while (!exit) {

    if ( !eventBase )
      return Success;
    
    else if ( event_base_got_exit( eventBase ) ) {
      exit = true;
      continue;
    }
    
    else if (timeoutTime != -1) {
      // register a timer that will quit the event loop
      //registerTimer(std::bind(&EventLoop::quit, this, Timeout), timeoutTime, Timer::SingleShot);
      // std::cout << "Setting Timeout for EventLoop => " << timeoutTime << "\n"; 
      // timeval tv = { 0, timeoutTime * 1000l };
      //event_base_loopexit( eventBase, &tv );
      //exit = true;
    } 
  
    //ret = event_base_dispatch( eventBase );
    //ret = event_base_loop( eventBase, EVLOOP_NONBLOCK );
    ret = event_base_loop( eventBase, EVLOOP_NO_EXIT_ON_EMPTY );
    if ( ret == -1 )
      std::cout << "ERROR: EventLoop dispatch ret = " << ret << "\n";

    std::cout << "event_base_loop returned! ret = " << ret << "\n";
  }

  std::cout << "Exiting EventLoop!\n";
  std::cout << "Events, Active: "
	    << event_base_get_num_events( eventBase, EVENT_BASE_COUNT_ACTIVE )
	    << " Virtual: "
	    << event_base_get_num_events( eventBase, EVENT_BASE_COUNT_VIRTUAL )
	    << " Added: "
	    << event_base_get_num_events( eventBase, EVENT_BASE_COUNT_ADDED )
	    << "\n";

  //cleanup();
  
  return ret == 1 ? Success : Timeout ;
}
