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

#ifndef SMARTLOCK_H_
#define SMARTLOCK_H_

#include <pthread.h>
#include <map>

class SmartLock {
private:
  static pthread_mutex_t* _globalMutex;
  static std::map< void*, pthread_mutex_t* > _mutexes;

  void* _data;

  void lockPointer(void* v);

  void unlockPointer(void* v);

public:
  template <typename T> 
  SmartLock(T& data) { _data = &data; lockPointer(_data); }
  ~SmartLock() { unlockPointer(_data); }
};

#endif
