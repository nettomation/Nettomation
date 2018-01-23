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

#include "smartlock.h"

pthread_mutex_t* SmartLock::_globalMutex = NULL;
std::map< void*, pthread_mutex_t* > SmartLock::_mutexes;

void SmartLock::lockPointer(void* v)
{ 
  if ( _globalMutex == NULL )
    {
      pthread_mutex_t* newMutex = new pthread_mutex_t();
      pthread_mutex_init(newMutex,NULL);
      _globalMutex = newMutex;
    }
  pthread_mutex_t* m;
  // global lock
  pthread_mutex_lock(_globalMutex);
  if ( _mutexes.find(v) == _mutexes.end() )
    {
      m = new pthread_mutex_t();
      pthread_mutex_init(m,NULL);
      _mutexes[v] = m;
    }
  else
    m = _mutexes[v];
  pthread_mutex_unlock(_globalMutex);
  // global unlock
  pthread_mutex_lock(m);
}

void SmartLock::unlockPointer(void* v)
{
  pthread_mutex_t* m;
  // global lock
  pthread_mutex_lock(_globalMutex);
  m = _mutexes[v];
  // global unlock
  pthread_mutex_unlock(_globalMutex);
  pthread_mutex_unlock(m);
}
