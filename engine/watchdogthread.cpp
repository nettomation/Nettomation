/* Copyright (c) 2018 Jiri Ocenasek, http://nettomation.com
 *
 * This file is part of Nettomation.
 *
 * Nettomation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * For tutorials, donations and proprietary licensing options
 * please visit http://nettomation.com
 */

#include "watchdogthread.h"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

WatchdogThread::WatchdogThread(int time)
{
  _time = time;
  pthread_mutex_init(&_accessMutex,NULL);
}

void WatchdogThread::run()
{
  while(1)
    {
      pthread_mutex_lock(&_accessMutex);
      std::set< GenericThread* > canceledThreads;
      std::swap(canceledThreads,_previousThreads);
      std::swap(_previousThreads,_currentThreads);
      // perform cancellation after unlocking mutextes
      for ( std::set< GenericThread* >::iterator it = canceledThreads.begin(); it != canceledThreads.end(); ++it )
	{
	  timeprintf("Watchdog detected stalled thread\n");

	  GenericThread* thread = *it;

	  thread->cancel();

	  _threadOfAge.erase(_ageOfThread[thread]);
	  _ageOfThread.erase(thread);
	  
	  thread->start();
	}
      pthread_mutex_unlock(&_accessMutex);
      sleep( _time );
    }
}

void WatchdogThread::tic( GenericThread* thread )
{
  pthread_mutex_lock(&_accessMutex);
  _counter++;
  _currentThreads.insert(thread);
  _ageOfThread[thread] = _counter;
  _threadOfAge[_counter]  = thread;
  pthread_mutex_unlock(&_accessMutex);
}

void WatchdogThread::tac( GenericThread* thread )
{
  pthread_mutex_lock(&_accessMutex);
  _previousThreads.erase(thread);
  _currentThreads.erase(thread);
  _threadOfAge.erase(_ageOfThread[thread]);
  _ageOfThread.erase(thread);
  pthread_mutex_unlock(&_accessMutex);
}

WatchdogThread::~WatchdogThread()
{
  pthread_mutex_destroy(&_accessMutex);
}

void WatchdogThread::respawnSlowestThread()
{
  pthread_mutex_lock(&_accessMutex);
  if ( !_threadOfAge.empty() )
    {
      timeprintf("Watchdog is respawning the slowest thread\n");

      GenericThread* thread = _threadOfAge.begin()->second; // pick the slowest(oldest) thread

      thread->cancel();               // stop the thread

      _previousThreads.erase(thread);
      _currentThreads.erase(thread);  // do not watch the thread until the tic() comes
      _threadOfAge.erase(_ageOfThread[thread]);
      _ageOfThread.erase(thread);
      
      thread->start();                // restart the thread
    }
  pthread_mutex_unlock(&_accessMutex);
}
