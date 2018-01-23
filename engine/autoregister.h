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

#ifndef AUTOREGISTER_H_
#define AUTOREGISTER_H_

#include "webcontent.h"
#include <string>

enum PositionType { TOP_VISIBLE = 0,
		    TOP_INVISIBLE = 1,
		    INNER = 2 };

class Dispatcher;

class WebContentCreator
{
 protected:
  string         _name;
  PositionType   _position;
 public:
  WebContentCreator(const string& name, PositionType position)
    { _name = name; _position = position; }
  bool isTop()     { return _position != INNER; }
  bool isVisible() { return _position != TOP_INVISIBLE; }
  virtual WebContent* createContent(Dispatcher* dispatcher) = 0;
};


extern vector<WebContentCreator*>   contentCreators;


template<class T>
class DerivedWebContentCreator: public WebContentCreator
{
 public:
 DerivedWebContentCreator(const string& name, PositionType position)
   : WebContentCreator(name,position)
    { 
    }

  virtual WebContent* createContent(Dispatcher* dispatcher)
  {
    T* ptr;
    ptr = new T(dispatcher,_name);
    return ptr;
  } 
};

template<class T>
class AutoContentCreator
{
 public:
  AutoContentCreator(const string& name, PositionType position)
    {
      // AutoContentCreator can be local, so the autoregistration macro can be anywhere
      //  but the allocated DerivedWebContentCreator will always persist
      contentCreators.push_back(new DerivedWebContentCreator<T>(name,position));
    }
};

#define CONCAT_(x,y) x##y
#define CONCAT(x,y) CONCAT_(x,y)

#define AUTO_REGISTER_CONTENT(class,name,position)			\
  AutoContentCreator<class> CONCAT( CONCAT(creator_ , class), __COUNTER__ ) (name, position);

// AUTO_REGISTER_CLASS is for singletons where the content holds the name of its class
#define AUTO_REGISTER_CLASS(class,position) AUTO_REGISTER_CONTENT( class, #class, position )

#endif
