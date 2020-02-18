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

 * NOTE: THIS FILE CONTAINS AN INTERACTIVE USER INTERFACE THAT DISPLAYS "Appropriate Legal Notices" ACCORDING TO AGPLv3, SECTION 0
 * YOUR USER INTERFACE MUST ALSO PROVIDE A CONVENIENT AND PROMINENTLY VISIBLE FEATURE TO DISPLAY THESE (AS REQUIRED BY AGPLv3, SECTION 5d)
 * Especially, you must ensure that (1) the copyright notice is displayed in full form
 * (including the donation link and the link to nettomation.com web)
 * (2) your site uses the Terms and conditions that contain protection of the authors as stated in legal.html
 * This is also for your own protection! By making the service available to your users you must also
 * make the code available to them, so you also want to be protected if they employ it for their new site.
 */

#include "dispatcher.h"
#include "smartlock.h"
#include "webcontent.h"
#include "webtop.h"
#include "encoder.h"
#include "sha256.h"
#include <unistd.h>
#include <string>
#include <string.h>
#include <vector>
#include <stdlib.h>
#include <sstream>

Dispatcher::Dispatcher(char* password)
{
  _timeStamp = 1;
  _webTop = NULL;
  _renderingTop = NULL;
  _password = password;
  _contextId = rand(); // random number to make sure that the caching is invalidated after server restart
}

void Dispatcher::mainLoop()
{
  _webTop = new WebTop(this); // cannot be created in the constructor, because it needs completed dispatcher
  _renderingTop = new RenderingRecord(_webTop, 1581700000); // unique number, the same for all contexts, to ensure non-collision
  _webTop->show();
  _webTop->start();

  // looping not needed, can exit afterwards
}

Dispatcher::~Dispatcher()
{
  if ( _webTop )
    {
      _webTop->cancel();
      delete _renderingTop;
      delete _webTop;
    }
}

string optionalQuote(const string& message, bool withQuotes)
{
  if ( withQuotes )
    return "\"" + message + "\"";
  else
    return message;
}

string Dispatcher::generateCallback(const string& owner, const string& callbackName, int index1, int index2, bool withQuotes)
{
  return optionalQuote("sendToDispatcher('" + owner + "','" + callbackName + "'," + to_string(index1) + "," + to_string(index2) + ",encodeURIComponent(value))",withQuotes);
}

string Dispatcher::generateFileCallback(const string& owner, const string& callbackName, int index1, int index2, bool withQuotes)
{
  return optionalQuote("sendFileToDispatcher('" + owner + "','" + callbackName + "'," + to_string(index1) + "," + to_string(index2) + ",files[0])",withQuotes);
}

string Dispatcher::generateDragSource(const string& owner, const string& sourceName, int sourceIndex1, int sourceIndex2, bool withQuotes)
{
  return optionalQuote("registerDragStart(event,'" + owner + "','" + sourceName + "'," + to_string(sourceIndex1) + "," + to_string(sourceIndex2) + ")",withQuotes);  
}

string Dispatcher::generateDragDestination(const string& owner, const string& destinationName, int destinationIndex1, int destinationIndex2, bool withQuotes)
{
  return optionalQuote("dispatchDropEvent(event,'" + owner + "','" + destinationName + "'," + to_string(destinationIndex1) + "," + to_string(destinationIndex2) + ")",withQuotes);  
}

string Dispatcher::generateDivStart(long long int divId, bool hidden)
{
  string style = "";
  if ( hidden )
      style = " style=\"display: none;\"";

  return "<div id=\"" + to_string(divId) + "\"" + style + ">";
}

string Dispatcher::generateDivEnd()
{
  return "</div>";
}

void Dispatcher::dispatchCallback(int session, const string& owner, const string& name, int index1, int index2, const string& value)
{
  //  SmartLock lock(_serializer); // avoid interplay between dispatchCallback and renderIncremental
  string decodedValue = decodeUriComponent(decodeUriComponent(value));
  //  printf("Callback: session=%d, owner=%s, name=%s, index1=%d, index2=%d, value=%s\n", 
  //	 session, owner.c_str(), name.c_str(), index1, index2, decodedValue.c_str());
  if ( _map.find(owner) != _map.end() )
    _map[owner]->callback(name,index1,index2,decodedValue,session);
}

void Dispatcher::dispatchDrop(int session, const string& owner, const string& sourceName, int sourceIndex1, int sourceIndex2,
			      const string& destinationName, int destinationIndex1, int destinationIndex2)
{
  //  SmartLock lock(_serializer); // avoid interplay between dispatchCallback and renderIncremental
  //  printf("Drop: session=%d, owner=%s, sourceName=%s, sourceIndex1=%d, sourceIndex2=%d, " \
  //	 "destinationName=%s, destinationIndex1=%d, destinationIndex2=%d\n", 
  //	 session, owner.c_str(), sourceName.c_str(), sourceIndex1, sourceIndex2,
  //	 destinationName.c_str(), destinationIndex1, destinationIndex2);
  if ( _map.find(owner) != _map.end() )
    _map[owner]->drop(sourceName, sourceIndex1, sourceIndex1, destinationName, destinationIndex1, destinationIndex2, session);
}

long long int Dispatcher::renderIncremental(int session, long long int oldTimeStamp, string& buffer)
{
  int i = 0;
  while (1) // long polling
    {
      i++;
      //  SmartLock lock(_serializer); // avoid interplay between dispatchCallback and renderIncremental
      long long int ts = getCurrentTimeStamp();
      //  printf("Render incremental in: session=%d, oldTimeStamp=%lld\n", session, oldTimeStamp);
      ostringstream stream;
      _webTop->incrementalUpdate(stream,*_renderingTop,oldTimeStamp);
      buffer = stream.str();
      //  printf("Render incremental out: %s\n", buffer.c_str());
      if ( !buffer.empty() )
	{
	  return ts;
	}
      if ( i > 100 )
	return ts;
      usleep(10000);
    }
  
}

void Dispatcher::logout( int session )
{
  SmartLock lock(_authorizedSessions);
  _authorizedSessions.erase(session);
}

// This is the routine where asynchronous user code (the web processing) will be added
// return true if the fileName was processed, false if you want the main engine to process it
bool Dispatcher::processWeb( char* method,
			     char* fileName, 
			     const std::vector< std::pair< const char*, const char* > >& params,
			     const std::vector< std::pair< const char*, const char* > >& fields,
			     FILE* output,
			     int   session )
{
  // no requests are authorized at this point (even not the downloads)

  if ( _password[0]!= 0 ) // if password not empty, display authorization dialog
    {
      SmartLock lock(_authorizedSessions);
      while ( _authorizedSessions.find(session) == _authorizedSessions.end() )
	{
	  if ( strcmp(fileName,"/sha256.js") == 0 ) // this file is not protected - it is needed for password hashing
	    {
	      return false; // read the file afterwards
	    }
	  else if ( strcmp(fileName,"/legal.html") == 0 ) // the legal file is not protected
	    {
	      return false; // read the file afterwards
	    }

	  const char* pwd = "";
	  for ( int i = 0; i < params.size(); i++ )
	    {
	      if ( strcmp( params[i].first, "password" ) == 0 )
		{
		  pwd = params[i].second;
		  break;
		}
	    }
	  if ( decodeUriComponent(sha256(to_string(session)+_password)) == pwd ) // password is salted with session number and hashed
	    {
	      _authorizedSessions.insert(session);
	      break; // jump after the authorization block
	    }
	  else if ( strcmp(fileName,"/dispatch_refresh.js") == 0 ) // this is a refresh from expired session
	    {
	      fprintf(output,
		      "HTTP/1.0 200 OK\r\n" \
		      "Content-Type: application/javascript\r\n" \
		      "\r\n" \
		      "window.location.replace(\"/\")");

	      return true;
	    }
	  else
	    {
	      fprintf(output,
		      "HTTP/1.0 200 OK\r\n" \
		      "Content-Type: text/html\r\n" \
		      "\r\n");

	      // send the main page here
	      fprintf(output,
		      "<html>" \
		      "<head>" \
		      "  <title>Login page</title>" \
		      "  <script src=\"sha256.js\"></script>"	\
		      "</head>" \
		      "<body>" );

	      // THE LEGAL CONDITIONS ARE PART OF THE "Appropriate Legal Notice" ACCORDING TO AGPLv3, SECTION 0
	      // YOUR USER INTERFACE MUST ALSO PROVIDE A CONVENIENT AND PROMINENTLY VISIBLE FEATURE TO DISPLAY THESE LEGAL CONDITIONS 
	      // (AS REQUIRED BY AGPLv3, SECTION 5d)
	      fprintf(output, // pwd_input has empty name attribute, so it is excluded from the submit
		      "<form method=\"post\" action=\"\" " \
		      "onsubmit=\"document.getElementById('pwd').value = sha256(encodeURIComponent('%d'+document.getElementById('pwd').value));\">"\
		      "Password:" \
		      "  <input type=\"password\" id=\"pwd\" name=\"password\" />" \
		      "  <button type=\"submit\">Login & accept conditions</button>" \
		      " &emsp; <font size='1'><a target='_blank' href='legal.html'>Terms and conditions.</a></font>" \
		      "</form>", session);

	      // THE FOLLOWING HTML STRING (INCLUDING THE DONATION LINK) IS THE COPYRIGHT PART OF THE "Appropriate Legal Notice" ACCORDING TO AGPLv3, SECTION 0
	      // YOUR USER INTERFACE MUST ALSO PROVIDE A CONVENIENT AND PROMINENTLY VISIBLE FEATURE TO DISPLAY IT (AS REQUIRED BY AGPLv3, SECTION 5d)
	      fprintf(output,
		      "<hr><div align='right'><font size='2'>Powered by <a target='_blank' href='http://nettomation.com'>Nettomation</a> engine, &copy; 2018 J. Ocenasek." \
		      " <a target='_blank' href='https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FZSWTJ78EBUXN' style='color:#ffcc00'>Donate</a></font></div>" );
	      
	      // send the end of page here
	      fprintf(output,
		      "</body>"		       \
		      "</html>");

	      return true; // fileName was processed - login form is shown
	    }
	}
    }

  // from now on the requests are authorized
  if ( strcmp( fileName, "/" ) == 0 )
    {
      long long int newTimeStamp = -1;//getCurrentTimeStamp();

      // send the response header
      // the content will be up to date, so the timestamp should be also updated
      fprintf(output,
	      "HTTP/1.0 200 OK\r\n" \
	      "Content-Type: text/html\r\n" \
	      "\r\n");

      // send the main page here
      fprintf(output,
	      "<!doctype html>" \
	      "<html>" \
	      "<head>" \
	      "  <script src=\"dispatcher.js\"></script>");

      ostringstream stream;
      WebContent::renderHeader(stream);
      fprintf(output,"%s",stream.str().c_str());

      fprintf(output,
	      "  <meta http-equiv='cache-control' content='no-cache' />" \
	      "  <meta http-equiv='cache-control' content='no-store' />" \
	      "</head>"				\
	      "<body>" );

      // THE LEGAL CONDITIONS ARE PART OF THE "Appropriate Legal Notice" ACCORDING TO AGPLv3, SECTION 0
      // YOUR USER INTERFACE MUST ALSO PROVIDE A CONVENIENT AND PROMINENTLY VISIBLE FEATURE TO DISPLAY THESE LEGAL CONDITIONS 
      // (AS REQUIRED BY AGPLv3, SECTION 5d)
      if ( _password[0] == 0 ) // if password not empty, then the link to terms and conditions was already in the login
	fprintf(output,
		"<div align='right'><font size='1'><a target='_blank' href='legal.html'>Terms and conditions.</a></font></div>");

      //      _webTop->renderWrapper(*_renderingTop);
      fprintf(output,"%s",generateDivStart(_renderingTop->uniqueId(),false).c_str());
      fprintf(output,"%s",generateDivEnd().c_str());
 
      // THE FOLLOWING HTML STRING (INCLUDING THE DONATION LINK) IS THE COPYRIGHT PART OF THE "Appropriate Legal Notice" ACCORDING TO AGPLv3, SECTION 0
      // YOUR USER INTERFACE MUST ALSO PROVIDE A CONVENIENT AND PROMINENTLY VISIBLE FEATURE TO DISPLAY IT (AS REQUIRED BY AGPLv3, SECTION 5d)
      fprintf(output,
	      "<hr><div align='right'><font size='2'>Powered by <a target='_blank' href='http://nettomation.com'>Nettomation</a> engine, &copy; 2018 J. Ocenasek." \
	      " <a target='_blank' href='https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FZSWTJ78EBUXN' style='color:#ffcc00'>Donate</a></font></div>" );

      // send the end of page here
      fprintf(output,
	      "</body>"		       \
	      "</html>");

      return true; // fileName was processed
    }
  else if ( strcmp(fileName,"/dispatch_callback.js") == 0 )
    {
      const char* owner = "";
      const char* name = "";
      int         index1;
      int         index2;
      const char* value = "";
      const char* counter = "";
      long long int oldContextId = -1;
      for ( int i = 0; i < params.size(); i++ )
	{
	  if ( strcmp( params[i].first, "OWNER" ) == 0 )
	    owner = params[i].second;
	  else if ( strcmp( params[i].first, "NAME" ) == 0 )
	    name = params[i].second;
	  else if ( strcmp( params[i].first, "INDEX1" ) == 0 )
	    index1 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "INDEX2" ) == 0 )
	    index2 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "VALUE" ) == 0 )
	    value = params[i].second;
	  else if ( strcmp( params[i].first, "COUNTER" ) == 0 )
	    counter = params[i].second;
	  else if ( strcmp( params[i].first, "CONTEXT" ) == 0 )
	    oldContextId = atoll(params[i].second);
	}

      if ( oldContextId == _contextId ) // otherwise the callback comes from different server instance
	dispatchCallback(session, owner, name, index1, index2, value);

      fprintf(output,
	      "HTTP/1.0 200 OK\r\n" \
	      "Content-Type: application/javascript\r\n" \
	      "\r\n" \
	      " var topHead = document.getElementsByTagName('head').item(0); " \
	      " var previousScript = document.getElementById('dispatch_callback_' + '%s'); " \
	      " topHead.removeChild(previousScript);",
	      counter);
      // return almost empty javascript, contextId is needed to make sure that the requests with the same content are not cached over multiple sessions

      return true; // event was processed
    }
  else if ( strcmp(fileName,"/dispatch_drop.js") == 0 )
    {
      const char* owner = "";
      const char* sourceName = "";
      int         sourceIndex1;
      int         sourceIndex2;
      const char* destinationName = "";
      int         destinationIndex1;
      int         destinationIndex2;
      const char* counter = "";
      long long int oldContextId = -1;
      for ( int i = 0; i < params.size(); i++ )
	{
	  if ( strcmp( params[i].first, "OWNER" ) == 0 )
	    owner = params[i].second;
	  else if ( strcmp( params[i].first, "SOURCE_NAME" ) == 0 )
	    sourceName = params[i].second;
	  else if ( strcmp( params[i].first, "SOURCE_INDEX1" ) == 0 )
	    sourceIndex1 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "SOURCE_INDEX2" ) == 0 )
	    sourceIndex2 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "DESTINATION_NAME" ) == 0 )
	    destinationName = params[i].second;
	  else if ( strcmp( params[i].first, "DESTINATION_INDEX1" ) == 0 )
	    destinationIndex1 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "DESTINATION_INDEX2" ) == 0 )
	    destinationIndex2 = atoi(params[i].second);
	  else if ( strcmp( params[i].first, "COUNTER" ) == 0 )
	    counter = params[i].second;
	  else if ( strcmp( params[i].first, "CONTEXT" ) == 0 )
	    oldContextId = atoll(params[i].second);
	}
      
      if ( oldContextId == _contextId ) // otherwise the callback comes from different server instance
	dispatchDrop(session, owner, sourceName, sourceIndex1, sourceIndex1, destinationName, destinationIndex1, destinationIndex2);

      fprintf(output,
	      "HTTP/1.0 200 OK\r\n" \
	      "Content-Type: application/javascript\r\n" \
	      "\r\n" \
	      " var topHead = document.getElementsByTagName('head').item(0); " \
	      " var previousScript = document.getElementById('dispatch_drop_' + '%s'); " \
	      " topHead.removeChild(previousScript);",
	      counter);
      // return almost empty javascript, contextId is needed to make sure that the requests with the same content are not cached over multiple sessions

      return true; // event was processed
    }
  else if ( strcmp(fileName,"/dispatch_refresh.js") == 0 )
    {
      long long int oldTimeStamp = -1; // read the timeStamp
      for ( int i = 0; i < params.size(); i++ )
	{
	  if ( strcmp( params[i].first, "TIMESTAMP" ) == 0 )
	    {
	      oldTimeStamp = atoll(params[i].second);
	      break;
	    }
	}
      long long int oldContextId = -1;
      for ( int i = 0; i < params.size(); i++ )
	{
	  if ( strcmp( params[i].first, "CONTEXT" ) == 0 )
	    {
	      oldContextId = atoll(params[i].second);
	      break;
	    }
	}
      if ( oldContextId != _contextId )
	{
	  oldTimeStamp = -1; // invalidate stamping if the context does not belong to current server
	}
      
      string buffer;
      long long int newTimeStamp;
      newTimeStamp = renderIncremental(session,oldTimeStamp,buffer);

      fprintf(output,
	      "HTTP/1.0 200 OK\r\n" \
	      "Content-Type: application/javascript\r\n" \
	      "\r\n nrPendingTimeouts = 0; if ((%lld > timestamp) || (contextId != %lld)) { %s ; contextId = %lld; timestamp=%lld; }; clearTimeout(handleWarning); setTimeout(updateFromDispatcher,100);",
	      // avoid timestap moving backwards from out-of-order data
	      // first javascript part is hardcoded
	      newTimeStamp,
	      _contextId,
	      buffer.c_str(),
	      _contextId,
	      newTimeStamp ); // return a javascript which updates the divs

      return true; // event was processed
    }
  else
    return false; // fileName was not processed, let the main web engine serve the real file
}

void Dispatcher::exitHandler()
{
}

long long int Dispatcher::getNewTimeStamp()
{
  SmartLock lock(_timeStamp); 
  _timeStamp++;
  return _timeStamp;
}

long long int Dispatcher::getCurrentTimeStamp()
{
  SmartLock lock(_timeStamp); 
  return _timeStamp;
}

void Dispatcher::attach(WebContent* wc, const string& name)
{
  _map[name] = wc;
}

void Dispatcher::detach(const string& name)
{
  _map.erase(name);
}

WebContent* Dispatcher::getContent(const string& name, bool quiet) // throws if not found, can report to error.log if not quiet
{
  if ( _map.find(name) == _map.end() )
    {
      if ( !quiet )
	std::cerr << "Problem in getContent/getClass: '" + name + "' not registered!";
      throw;
    }
  return _map[name];
  
}

void Dispatcher::hideAllContent()
{
  for ( map<string,WebContent*>::iterator it = _map.begin(); it != _map.end(); ++it )
    {
      it->second->hide();
    }
}

string Dispatcher::generateUpdateContent(long long int divId, const std::string& innerHtml)
{
  //  printf("Update content: id=%lld, HTML=%s\n",divId,innerHtml.c_str());
  string encoded = encodeUriComponent(innerHtml);
  //  printf("Update content out: %s\n",encoded.c_str());
  return "document.getElementById(\"" + to_string(divId) + "\").innerHTML = decodeURIComponent('" + encoded + "');";
}

string Dispatcher::generateUpdateVisibility(long long int divId, bool hidden)
{
  //  printf("Update visibility: id=%lld, hidden=%d\n",divId,hidden);
  if ( hidden )
    {
      return "document.getElementById(\"" + to_string(divId) + "\").style.display = \"none\";";
    }
  else
    {
      return "document.getElementById(\"" + to_string(divId) + "\").style.display=\"\";";
    }
}
