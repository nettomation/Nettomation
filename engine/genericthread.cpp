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

#include "genericthread.h"

void GenericThread::start()
{
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
  pthread_create(&_thread,&tattr,_launcher,(void*)this);
  pthread_attr_destroy(&tattr);
}

void* GenericThread::_launcher( void* genericThread )
{
  ((GenericThread*)genericThread)->run();
  return NULL;
}

void GenericThread::cancel()
{
  pthread_cancel(_thread);
  cleanup();
}
