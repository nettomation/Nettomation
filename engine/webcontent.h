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

#ifndef WEBCONTENT_H_
#define WEBCONTENT_H_

#include "abstractdispatcher.h"
#include "genericthread.h"
#include <vector>

class Dispatcher;
class RenderingRecord;

class WebContent : public GenericThread {
 protected:
  Dispatcher*       _dispatcher;
  string            _name;

  long long int     _timeStampVisibility;
  long long int     _timeStampContent;
  bool              _hidden;

  vector<RenderingRecord*>* _renderingRecords;

  virtual void run() { controlLoop(); }

  string renderContent(const string& name);

 public:
  WebContent( Dispatcher* dispatcher, const string& name);
  virtual ~WebContent();

  // this is the main GUI task to be reimplemented by every page, should not be called directly
  // some content can be purely dedicated to control, so no need to display - implement this method
  virtual void render( ostream& stream ) { }

  static void renderHeader( ostream& stream );

  // this calls render from inside, keeps track of the renderded children
  void renderWrapper( RenderingRecord& record );

  // this creates javascript code to update the content of invalid div's
  void incrementalUpdate( ostream& stream, RenderingRecord& record, long long int oldTimeStamp);

  // empty by default - not all web containers need to process events
  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session) { }

  // empty by default - not all web containers need to process events
  virtual void drop(const string& sourceName, int sourceIndex1, int sourceIndex2, 
		    const string& destinationName, int destinationIndex1, int destinationIndex2, 
		    int session) { }

  // empty by default - not all web containers need to control something
  virtual void controlLoop() { }

  string generateCallback(const string& callbackName, int index1, int index2, bool withQuotes = true);
  string generateFileCallback(const string& callbackName, int index1, int index2, bool withQuotes = true);
  string generateDragSource(const string& sourceName, int sourceIndex1, int sourceIndex2, bool withQuotes = true);
  string generateDragDestination(const string& destinationName, int destinationIndex1, int destinationIndex2, bool withQuotes = true);

  // This is for including the javascript into webpages
  // Scripts are not executed by innerHtml assignment, this implements a workaround
  // The script itself should not contain quotes
  string generateScriptStart();
  string generateScriptEnd();

  // functions to control visual content
  void show();
  void hide();
  void invalidate();
  bool isHidden() { return _hidden; }
  
  const string& getName() const { return _name; }

  AbstractDispatcher* dispatcher();
};

#endif
