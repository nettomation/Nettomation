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

#include "userpages.h"
#include "engine/autoregister.h"
#include "engine/smartlock.h"
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <sstream>

/*
 * This example shows how the control loop and the rendering works,
 * how to react on button events, how to render one content into another
 * how to use dispatcher class for getting access to named instances,
 * and how to toggle visibility
 */

/* Every type of web content needs to be registered.
 * parameter 1 : class name
 * parameter 2 : type of rendering: TOP_VISIBLE, TOP_INVISIBLE, INNER
 */
AUTO_REGISTER_CLASS(HelloPage,TOP_VISIBLE);
AUTO_REGISTER_CLASS(CounterWidget,INNER);


/* Put here the <head> section common to all pages.
 */
void WebContent::renderHeader( ostream& stream ) // this is a static function
{
  stream << "<title>Nettomation demo</title>";
}

/* If the class is registered, this loop will start automatically in separate thread.
 */
void CounterWidget::controlLoop()
{
  _counter = 0;

  while ( 1 )
    {
      sleep(1); // wait 1 s
      SmartLock lockCounter(_counter);
      _counter++;
      invalidate(); // this causes re-rendering in subsequent updates
    }
}

/* This routine writes out HTML to be displayed. Caching is performed in the background, unless invalidate() is called.
 */
void CounterWidget::render( ostream& stream )
{
  SmartLock lockCounter(_counter);

  stream << "Seconds elapsed since the app started: " << _counter;
}

/* This is a user-defined routine
 */
void CounterWidget::resetCounter()
{
  SmartLock lockCounter(_counter);

  _counter = 0;
  invalidate(); // trigger rendering next time
}

/* This routine writes out HTML to be displayed. Caching is performed in the background, unless invalidate() is called.
 */
void HelloPage::render( ostream& stream )
{
  stream << "<hr>";
  stream << "Menu:";
  stream << " <button onclick=" << generateCallback("toggle_counter",1,1) << ">Toggle counter</button>";
  stream << " <button onclick=" << generateCallback("reset_counter",1,1) << ">Reset counter</button>";
  stream << " <button onclick=" << generateCallback("logout",1,1) << ">Logout</button>";
  stream << "<hr>";

  stream << "<h2>Hello World!</h2>";

  // this inserts the CounterWidget here
  // each widget can be inserted multiple times
  // multiple hierarchy levels are supported
  stream << renderClass(CounterWidget);
}

/* This routine reacts to the buttons/input callback events
 * There exists a similar routine "drop" for drag-and-drop type of events
 */
void HelloPage::callback(const string& callbackName, int index1, int index2, const string& value, int session)
{
  if ( callbackName == "toggle_counter" )
    {
      CounterWidget& counter = dispatcher()->findClass(CounterWidget);
      if ( counter.isHidden() )
	counter.show();
      else
	counter.hide();
    }
  else if ( callbackName == "reset_counter" )
    {
      // the findClass(TYPE) operator also performs typecasting from the WebContent class to derived TYPE
      // it throws an exception if not found
      dispatcher()->findClass(CounterWidget).resetCounter();
    }
  else if ( callbackName == "logout" )
    {
      dispatcher()->logout(session);
    }
}
