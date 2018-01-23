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

#ifndef USERPAGES_H_
#define USERPAGES_H_

#include "engine/webcontent.h"
#include <vector>

class ChatWidget : public WebContent{
 private:
  std::vector<std::string> _chat;

 public:
 ChatWidget(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  void addMessage(const std::string& message);

  virtual void render( ostream& stream );

  // in this example the ChatWidget does not react on events, so no callback method is needed 
  // no controlLoop method is needed either
  //  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);
  //  virtual void controlLoop();
};

class TimerWidget : public WebContent{
 private:
  int _timer;

 public:
 TimerWidget(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  void resetTimer();

  virtual void render( ostream& stream );
  virtual void controlLoop();

  // in this example the TimerWidget does not react on events, so no callback method is needed 
  //  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);
};

class MainPage : public WebContent{
 public:
 MainPage(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void render( ostream& stream );
  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);

  // in this example the MainPage does not need to control anything, so no controlLoop method is needed 
  //  virtual void controlLoop();
};

#endif
