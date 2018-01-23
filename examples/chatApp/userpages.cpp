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
AUTO_REGISTER_CLASS(MainPage,TOP_VISIBLE);
AUTO_REGISTER_CLASS(ChatWidget,INNER);
AUTO_REGISTER_CLASS(TimerWidget,INNER);


/* Put here the <head> section common to all pages.
 */
void WebContent::renderHeader( ostream& stream ) // this is a static function
{
  stream << "<title>Nettomation demo</title>";
}

/* This routine writes out HTML to be displayed. Caching is performed in the background, unless invalidate() is called.
 */
void ChatWidget::render( ostream& stream )
{
  SmartLock lockChat(_chat);

  for ( size_t i = 0; i < _chat.size(); i++ )
    stream << "<br>" << _chat[_chat.size()-i-1];
}

/* This is a user-defined routine
 */
void ChatWidget::addMessage(const std::string& message)
{
  SmartLock lockChat(_chat);

  _chat.push_back(message);
  invalidate();
}

/* If the class is registered, this loop will start automatically in separate thread.
 */
void TimerWidget::controlLoop()
{
  _timer = 0;

  while ( 1 )
    {
      sleep(1); // wait 1 s
      SmartLock lockTimer(_timer);
      _timer++;
      invalidate(); // this causes re-rendering in subsequent updates
    }
}

/* This routine writes out HTML to be displayed. Caching is performed in the background, unless invalidate() is called.
 */
void TimerWidget::render( ostream& stream )
{
  SmartLock lockTimer(_timer);

  stream << "Seconds elapsed since the last message: " << _timer;
}

/* This is a user-defined routine
 */
void TimerWidget::resetTimer()
{
  SmartLock lockTimer(_timer);

  _timer = 0;
  invalidate(); // trigger rendering next time
}

/* This routine writes out HTML to be displayed. Caching is performed in the background, unless invalidate() is called.
 */
void MainPage::render( ostream& stream )
{
  stream << "<hr>";
  stream << "Menu:";
  stream << " <button onclick=" << generateCallback("toggle_chat",1,1) << ">Toggle history</button>";
  stream << " <button onclick=" << generateCallback("logout",1,1) << ">Logout</button>";
  stream << "<hr>";

  stream << "<h2>Chat</h2>";

  // the following onchange event also clears the value
  stream << "<b>Say something: <input type=text onchange=\"(function(){" << generateCallback("add_message",1,1,false) << ";value=''})()\" /></b><br><br>";

  // this inserts the ChatWidget here
  // each widget can be inserted multiple times
  // multiple hierarchy levels are supported
  stream << renderClass(TimerWidget);
  stream << renderClass(ChatWidget);
}

/* This routine reacts to the buttons/input callback events
 * There exists a similar routine "drop" for drag-and-drop type of events
 */
void MainPage::callback(const string& callbackName, int index1, int index2, const string& value, int session)
{
  if ( callbackName == "toggle_chat" )
    {
      ChatWidget& chat = dispatcher()->findClass(ChatWidget);
      if ( chat.isHidden() )
	chat.show();
      else
	chat.hide();
    }
  else if ( callbackName == "add_message" )
    {
      dispatcher()->findClass(ChatWidget).addMessage(value);      
      dispatcher()->findClass(TimerWidget).resetTimer();
    }
  else if ( callbackName == "logout" )
    {
      dispatcher()->logout(session);
    }
}
