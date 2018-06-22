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

#include "webcontent.h"
#include "dispatcher.h"
#include "smartlock.h"
#include "renderingrecord.h"
#include <sstream>

WebContent::WebContent( Dispatcher* dispatcher, const string& name)
:
  _dispatcher(dispatcher),
  _name(name)
{
  _dispatcher->attach(this,_name);
  _hidden = true;
  _timeStampVisibility = _dispatcher->getNewTimeStamp();
  _timeStampContent = _timeStampVisibility;
}

WebContent::~WebContent()
{
  _dispatcher->detach(_name);
}

void WebContent::show()
{
  SmartLock lock(_hidden); 
  if ( !_hidden )
    return;
  _hidden = false;
  _timeStampVisibility = _dispatcher->getNewTimeStamp();
}
  
void WebContent::hide()
{
  SmartLock lock(_hidden); 
  if ( _hidden )
    return;
  _hidden = true;
  _timeStampVisibility = _dispatcher->getNewTimeStamp();
}

void WebContent::invalidate()
{
  SmartLock lock(_timeStampContent); 
  _timeStampContent = _dispatcher->getNewTimeStamp();
}

string WebContent::generateCallback(const string& callbackName, int index1, int index2, bool withQuotes)
{
  return _dispatcher->generateCallback(_name,callbackName,index1,index2,withQuotes);
}

string WebContent::generateFileCallback(const string& callbackName, int index1, int index2, bool withQuotes)
{
  return _dispatcher->generateFileCallback(_name,callbackName,index1,index2,withQuotes);
}

string WebContent::generateDragSource(const string& sourceName, int sourceIndex1, int sourceIndex2, bool withQuotes)
{
  return _dispatcher->generateDragSource(_name,sourceName,sourceIndex1,sourceIndex2,withQuotes);
}

string WebContent::generateDragDestination(const string& destinationName, int destinationIndex1, int destinationIndex2, bool withQuotes)
{
  return _dispatcher->generateDragDestination(_name,destinationName,destinationIndex1,destinationIndex2,withQuotes);
}

string WebContent::generateScriptStart()
{
  // <script(s)> are not executed by innerHtml assignment, img-onload is a workaround
  return "<img src='data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7' style='display: none;' onload=\"";
}

string WebContent::generateScriptEnd()
{
  return "\">";
}

string WebContent::renderContent(const string& name)
{
  try {
    WebContent* child = _dispatcher->getContent(name);
    // for debugging show problems on the page instead of re-throwing
    if ( child == this )
      return "Problem: '" + name + "' cannot render itself!";
    if ( child == NULL )
      return "Problem: '" + name + "' is not registered!";      
    RenderingRecord* record = new RenderingRecord(child);
    _renderingRecords->push_back(record);
    child->renderWrapper(*record);
    return _dispatcher->generateDivStart(record->uniqueId(),child->isHidden())
      + record->_cache + _dispatcher->generateDivEnd();
  }
  catch ( ... ) {
    // for debugging show problems on the page instead of re-throwing
    return "Problem: cannot render '" + name + "' - not registered!";
  }
}

void WebContent::renderWrapper(RenderingRecord& record)
{
  // the following approach assumes that the rendering is serialized, which is sensible to avoid double work
  vector<RenderingRecord*>* tmp = _renderingRecords; // push context pointer to stack
  _renderingRecords = &record._nestedRecords; // set context pointer to record
  long long int  ts = _dispatcher->getCurrentTimeStamp(); // just in case the timestamp increases during the rendering, we prefer to snapshot it before
  ostringstream stream;
  render(stream);
  record._cache = stream.str();
  record._timestamp = ts;
  _renderingRecords = tmp; // restore context pointer
}

void WebContent::incrementalUpdate(ostream& stream, RenderingRecord& record, long long int oldTimeStamp)
{
  // the update of cache depends on the timestamp of the cache, not on the timestamp of the client
  SmartLock lock(record); // avoid double work by serializing - do not reenter the renderWrapper
  if ( record._timestamp < _timeStampContent ) // record needs update
    {
      record.clearRecords(); // deallocate old tree
      renderWrapper(record); // implicitly sets also record._timestamp
    }

  if ( _timeStampVisibility > oldTimeStamp )
    {
      stream << _dispatcher->generateUpdateVisibility(record.uniqueId(),isHidden());
    }

  if ( record._timestamp > oldTimeStamp ) // the GUI needs update
    {
      stream << _dispatcher->generateUpdateContent(record.uniqueId(),record._cache);
      oldTimeStamp = record._timestamp; // this is now the new time of reference
    }

  for ( size_t i = 0; i < record._nestedRecords.size(); i++ )
    record._nestedRecords[i]->owner()->incrementalUpdate(stream,*record._nestedRecords[i],oldTimeStamp);
}

AbstractDispatcher* WebContent::dispatcher()
{
  return _dispatcher;
}
