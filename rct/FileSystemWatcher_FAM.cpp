#include "rct/FileSystemWatcher.h"
#include "rct/EventLoop.h"
#include "rct/Log.h"
#include "rct-config.h"

#include "fam.h"

#include <iostream>

FileSystemWatcher::FileSystemWatcher()
{
  std::cout << __PRETTY_FUNCTION__ << "\n";
  auto ret = FAMOpen( &mFAMCon );
  assert( ! (ret < 0) );
  EventLoop::eventLoop()->
    registerTimer( std::bind(&FileSystemWatcher::checkFAMEvents,
			     this, std::placeholders::_1),
		   10 );
}

FileSystemWatcher::~FileSystemWatcher()
{FAMClose( &mFAMCon );}

void FileSystemWatcher::clear()
{
  std::cout << __PRETTY_FUNCTION__ << "\n";
  for (Map<Path, int>::const_iterator it = mWatchedByPath.begin();
       it != mWatchedByPath.end(); ++it) {
    FAMRequest freq;
    freq.reqnum = it->second;
    
    FAMCancelMonitor(&mFAMCon, &freq );
  }
  mWatchedByPath.clear();
  mWatchedById.clear();
}

bool FileSystemWatcher::watch(const Path &p)
{
  if (p.isEmpty())
    return false;
  Path path = p;

  std::cout << __PRETTY_FUNCTION__ << ": "
	    << path.fileName() << "\n";
  
  std::lock_guard<std::mutex> lock(mMutex);

  FAMRequest mFAMReq;
  auto type = path.type();
  switch (type) {
  case Path::File:
    FAMMonitorFile( &mFAMCon,
		    path.nullTerminated(),
		    &mFAMReq,
		    NULL );
    break;

  case Path::Directory:
    FAMMonitorDirectory( &mFAMCon,
			 path.nullTerminated(),
			 &mFAMReq,
			 NULL );
    break;

  default:
    return false;
  }

  auto reqid = FAMREQUEST_GETREQNUM( &mFAMReq );
  
  mWatchedByPath[path] = reqid;
  mWatchedById[reqid] = path;
  return true;
}

bool FileSystemWatcher::unwatch(const Path &path)
{
  std::cout << __PRETTY_FUNCTION__ << "\n";
  
  std::lock_guard<std::mutex> lock(mMutex);
  int wd;
  if (!mWatchedByPath.remove(path, &wd))
    return false;
  
  // TODO: hacky on private'ish struct!
  FAMRequest freq;
  freq.reqnum = wd;

  FAMCancelMonitor( &mFAMCon, &freq );

  return true;
}

bool FileSystemWatcher::isFAMEventPending()
{
  auto ret = FAMPending( &mFAMCon );
  assert( ! (ret < 0) );

  return ret != 0;
}

  
void FileSystemWatcher::checkFAMEvents(int something)
{
  //std::cout << __PRETTY_FUNCTION__ << "\n";
  
  Set<Path> modified, removed, added;
  std::lock_guard<std::mutex> lock(mMutex);
  
  while( isFAMEventPending() ) {
    
    FAMEvent fevent;
    FAMNextEvent( &mFAMCon, &fevent );

    Path reqpath = mWatchedById.value( fevent.fr.reqnum );
  
    bool isDir = reqpath.isDir();
    Path path = reqpath;
    char *filename = (char *)fevent.filename;

    if ( Path( filename ).isAbsolute() )
      path = filename;
    else if ( isDir )
      path.append( filename );
  
    switch( fevent.code ) {
    case FAMCreated:
      std::cout << __PRETTY_FUNCTION__ << " : FAMCreated : "
		<< path.nullTerminated() << "\n";
      added.insert( path );
    
    case FAMDeleted:
      std::cout << __PRETTY_FUNCTION__ << " : FAMDeleted : "
		<< path.nullTerminated() << "\n";
      added.remove( path );
      removed.insert( path );

    case FAMChanged:
      std::cout << __PRETTY_FUNCTION__ << " : FAMChanged : "
		<< path.nullTerminated() << "\n";
      modified.insert( path );
    
    default:
      break;
    }

  }
  
  struct {
    Signal<std::function<void(const Path&)> > &signal;
    const Set<Path> &paths;
  } signals[] = {
    { mModified, modified },
    { mRemoved, removed },
    { mAdded, added }
  };
  const unsigned count = sizeof(signals) / sizeof(signals[0]);
  for (unsigned i=0; i<count; ++i) {
    for (Set<Path>::const_iterator it = signals[i].paths.begin();
	 it != signals[i].paths.end(); ++it) {
      signals[i].signal(*it);
    }
  }

}

