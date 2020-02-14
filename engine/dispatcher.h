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

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include "genericthread.h"
#include "abstractdispatcher.h"
#include "renderingrecord.h"
#include <vector>
#include <stdio.h>
#include <set>
#include <map>
#include <iostream>


using namespace std;

class WebContent;
class WebTop;

class Dispatcher : public GenericThread, public AbstractDispatcher {
 private:
  string         _password;
  set<int>       _authorizedSessions;
  long long int  _timeStamp;

  WebTop*          _webTop;
  RenderingRecord* _renderingTop;

  int            _serializer; // SmartLock to avoid interplay between dispatchCallback and renderIncremental
  long long int  _contextId;  // random number to make sure that the caching is invalidated after server restart


  map<string,WebContent*>  _map;

  virtual void run() { mainLoop(); }

 public:
  Dispatcher(char* password);
  ~Dispatcher();

  // This is the routine where synchronous user code (the user loop) will be added
  void mainLoop();

  // This is the routine where asynchronous user code (the web processing) will be added
  // return true if the fileName was processed, false if not
  bool processWeb( char* method,
		   char* fileName, 
		   const std::vector< std::pair< const char*, const char* > >& params,
		   const std::vector< std::pair< const char*, const char* > >& fields,
		   FILE* output,
		   int   session );

  // removes the session from the list of authorized sessions
  virtual void logout( int session );

  // This creates a javascript code for callbacks
  string generateCallback(const string& owner, const string& callbackName, int index1, int index2, bool withQuotes = true);
  string generateFileCallback(const string& owner, const string& callbackName, int index1, int index2, bool withQuotes = true);
  string generateDragSource(const string& owner, const string& sourceName, int sourceIndex1, int sourceIndex2, bool withQuotes = true);
  string generateDragDestination(const string& owner, const string& destinationName, int destinationIndex1, int destinationIndex2, bool withQuotes = true);

  // This creates a javascript code for embedding one WebContent instance into another
  string generateDivStart(long long int divId, bool hidden);
  string generateDivEnd();
  string generateUpdateContent(long long int divId, const std::string& innerHtml);
  string generateUpdateVisibility(long long int divId, bool hidden);

  // timestamps are used to invalidate objects, they ar compared against values on the client side
  virtual long long int getNewTimeStamp();
  virtual long long int getCurrentTimeStamp();

  // This is called when the event arrives
  void dispatchCallback(int session, const string& owner, const string& name, int index1, int index2, const string& value);
  void dispatchDrop(int session, const string& owner, const string& sourceName, int sourceIndex1, int sourceIndex2,
		    const string& destinationName, int destinationIndex1, int destinationIndex2);

  // This returns a javascript which updates the divs
  long long int renderIncremental(int session, long long int oldTimeStamp, string& buffer);

  // the following functions keep track of active elements to be updated
  void attach(WebContent* wc, const string& name);
  void detach(const string& name);

  // lookup enabling to access content through hierarchical names
  // report to error.log if not quiet
  // throws if not found (you can catch exceptions by try..catch clause)
  virtual WebContent* getContent(const string& name, bool quiet = false);

  void hideAllContent(); // loops over all content and calls hide
  void pushVisibility(); // this is stack-based, for implementing modal dialogs
  void restoreVisibility(); // this is stack-based, for implementing modal dialogs

  // 10 seconds before exit this method is called
  void exitHandler();
};

#endif // DISPATCHER_H_
