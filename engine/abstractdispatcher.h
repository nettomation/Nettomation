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

#ifndef ABSTRACTDISPATCHER_H_
#define ABSTRACTDISPATCHER_H_

#include <string>

using namespace std;

class WebContent;

class AbstractDispatcher {
 public:
  // removes the session from the list of authorized sessions
  virtual void logout( int session ) = 0;

  // timestamps are used to invalidate objects, they ar compared against values on the client side
  virtual long long int getNewTimeStamp() = 0;
  virtual long long int getCurrentTimeStamp() = 0;

  // lookup enabling to access content through hierarchical names
  // report to error.log if not quiet
  // throws if not found (you can catch exceptions by try..catch clause)
  virtual WebContent* getContent(const string& name, bool quiet = false) = 0;

  // lookup enabling to access content through hierarchical names
  // casts to given class
  // throws if not found
  template <class DerivedWebContent>
    DerivedWebContent& get(const string& name, bool quiet = false)
    {
      DerivedWebContent* ptr = dynamic_cast<DerivedWebContent*> (getContent(name,quiet));
      if ( !ptr )
	throw;
      return *ptr;      
    }
};

#define findClass(CLASS) get<CLASS>(#CLASS)
#define findClassQuiet(CLASS) get<CLASS>(#CLASS,true)

#define renderClass(CLASS) renderContent(#CLASS)

#endif // ABSTRACTDISPATCHER_H_
