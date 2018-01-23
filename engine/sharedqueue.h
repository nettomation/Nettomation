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

#ifndef SHAREDQUEUE_H_
#define SHAREDQUEUE_H_

#include <pthread.h>
#include <deque>

template <class T>
class SharedQueue {
 private:
  std::deque<T>    _queue;
  pthread_mutex_t  _accessMutex;
  pthread_mutex_t  _emptyMutex;
  pthread_mutex_t  _waitingMutex;
  size_t           _nrWaitingThreads;
 public:
  SharedQueue();
  virtual ~SharedQueue();
  bool push(const T& data);
  T pop();
};

template <class T>
SharedQueue<T>::SharedQueue()
{
  _nrWaitingThreads = 0;
  pthread_mutex_init(&_emptyMutex,NULL);
  pthread_mutex_init(&_accessMutex,NULL);
  pthread_mutex_init(&_waitingMutex,NULL);
  pthread_mutex_lock(&_emptyMutex);
}

template <class T>
bool SharedQueue<T>::push(const T& data)
{
  pthread_mutex_lock(&_accessMutex);
  bool emptyBefore = _queue.empty();

  pthread_mutex_lock(&_waitingMutex);
  bool waitingBefore = (_nrWaitingThreads != 0);
  pthread_mutex_unlock(&_waitingMutex);

  _queue.push_back(data);
  if (emptyBefore)
    pthread_mutex_unlock(&_emptyMutex);
  pthread_mutex_unlock(&_accessMutex);
  return waitingBefore;
}

template <class T>
T SharedQueue<T>::pop()
{
  pthread_mutex_lock(&_waitingMutex);
  _nrWaitingThreads++;
  pthread_mutex_unlock(&_waitingMutex);

  pthread_mutex_lock(&_emptyMutex);
  pthread_mutex_lock(&_accessMutex);
  T result = _queue.front();
  _queue.pop_front();

  pthread_mutex_lock(&_waitingMutex);
  _nrWaitingThreads--;
  pthread_mutex_unlock(&_waitingMutex);

  if (!_queue.empty())
    pthread_mutex_unlock(&_emptyMutex);
  pthread_mutex_unlock(&_accessMutex);
  return result;
}

template <class T>
SharedQueue<T>::~SharedQueue()
{
  pthread_mutex_destroy(&_emptyMutex);
  pthread_mutex_destroy(&_accessMutex);
  pthread_mutex_destroy(&_waitingMutex);
}

#endif // SHAREDQUEUE_H_
