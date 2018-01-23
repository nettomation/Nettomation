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

#include "webthread.h"
#include "dispatcher.h"
#include "smartlock.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <stdio.h>

void urlDecode( char* param )
{
  int from = 0;
  int to = 0;
  while ( param[from] != 0 )
    {
      if ( param[from] == 37 ) // percentage
	{
          from++;
	  if ( param[from] == 0 )
	    break;
	  unsigned char c1 = param[from];
	  if ( (c1 >= '0') && (c1 <= '9') )
	    c1 -= '0';
	  else if ( (c1 >= 'a') && (c1 <= 'f') )
	    c1 = c1 - 'a' + 10;
	  else if ( (c1 >= 'A') && (c1 <= 'F') )
	    c1 = c1 - 'A' + 10;
	  else break;

          from++;
	  if ( param[from] == 0 )
	    break;
	  unsigned char c2 = param[from];
	  if ( (c2 >= '0') && (c2 <= '9') )
	    c2 -= '0';
	  else if ( (c2 >= 'a') && (c2 <= 'f') )
	    c2 = c2 - 'a' + 10;
	  else if ( (c2 >= 'A') && (c2 <= 'F') )
	    c2 = c2 - 'A' + 10;
	  else break;
	  

	  char c = (c1 << 4 ) + c2;
	  param[to] = c;
	}
      else if ( param[from] == '+' ) // special case
	{
	  param[to] = ' ';
	}
      else
	{
	  param[to] = param[from];
	}
      from++;
      to++;
    }
  param[to] = 0; // finalize the string
}

WebThread::WebThread(SharedQueue<int>* sharedQueue, WatchdogThread* watchdog, Dispatcher* dispatcher)
{
  _file = NULL;
  _sharedQueue = sharedQueue;
  _watchdog = watchdog;
  _dispatcher = dispatcher;
}

void WebThread::parseStream(int handle)
{
  int   state = 0;         // finite-state automaton
  char* method = _buffer;   // method always starts at the beginning, also in Apache
  char* fileName = NULL;   // first part of the URL
  char* lastName = NULL;  // helper variable
  char* lastValue = NULL;  // helper variable
  std::vector< std::pair< const char*, const char* > > params;
  std::vector< std::pair< const char*, const char* > > fields;

  //  printf("Parsing header of stream no %d\n",handle);
  int i = 0;
  int length = 0;
  bool endReached = false;
  bool syntaxError = false;
  while (length < BUFFSIZE)
    {
      int ret = read(handle,_buffer+length,BUFFSIZE-length);
      if( ret < 1 )
	{
	  //	  printf("Failed to read browser request");
	  break;
	}
      length += ret;

      for ( ; i < length; i++ )
	{
	  switch ( state ) 
	    {
	    case 0:                   // read-in method
	      if ( _buffer[i] == ' ' )
		{
		  _buffer[i] = 0;      // zero terminate the method
		  state = 1;
		}
	      else if ( ( _buffer[i] == '\n' ) || ( _buffer[i] == '\r' ) )
		{
		  syntaxError = true;
		}
	      break;

	    case 1:                       // skip whitespace after the method
	      if ( ( _buffer[i] == '\n' ) || ( _buffer[i] == '\r' ) )
		{
		  syntaxError = true;
		}
	      else if ( _buffer[i] != ' ' )
		{
		  fileName = _buffer+i;  // start of the fileName
		  state = 2;
		}
	      break;

	    case 2:                      // read in the fileName, finish by 
	      if ( _buffer[i] == '?' )
		{
		  _buffer[i] = 0;         // terminate fileName
		  lastName = _buffer+i+1;
		  state = 3;
		}
	      else if ( _buffer[i] == ' ' ) // HTTP version follows
		{
		  _buffer[i] = 0;         // terminate fileName
		  state = 5;
		}
	      else if ( _buffer[i] == '\n' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  endReached = true;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  endReached = true;
		}
	      else if ( _buffer[i] == '.' ) // this is a security feature - we want to avoid "../" in the path
                {
		  state = 11;
                }
	      break;

	    case 3:                     // process param name
	      if ( _buffer[i] == '=' )
		{
		  _buffer[i] = 0;        // terminate param
		  lastValue = _buffer+i+1;
		  state = 4;
		}
	      else if ( _buffer[i] == '\n' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;        // terminate param
		  params.push_back( std::make_pair( lastName, (const char*)NULL) );		
		  endReached = true;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate param
		  params.push_back( std::make_pair( lastName, (const char*)NULL) );		
		  endReached = true;
		}
	      break;

	    case 4:                     // process value
	      if ( _buffer[i] == '&' )
		{
		  _buffer[i] = 0;       // terminate value
		  params.push_back( std::make_pair( lastName, lastValue ) );
		  lastName = _buffer+i+1;
		  state = 3;
		}
	      else if ( _buffer[i] == ' ' ) // HTTP version follows
		{
		  _buffer[i] = 0;       // terminate value
		  params.push_back( std::make_pair( lastName, lastValue ) );		
		  state = 5;
		}
	      else if ( _buffer[i] == '\n' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  params.push_back( std::make_pair( lastName, lastValue ) );
		  endReached = true;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  params.push_back( std::make_pair( lastName, lastValue ) );
		  endReached = true;
		}
	      break;

	    case 5:                     // read in HTTP version
	      if ( _buffer[i] == '\n' ) // end of the line - simple HTTP request ends
		{
		  state = 6;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - simple HTTP request ends
		{
		  state = 7;
		}
	      break;

	    case 6:                     // skip single '\r'
	      if ( _buffer[i] != '\r' )
		--i; // push back the character
	      lastName = _buffer+i+1;
	      state = 8;
	      break;

	    case 7:                     // skip single '\n'
	      if ( _buffer[i] != '\n' )
		--i; // push back the character
	      lastName = _buffer+i+1;
	      state = 8;
	      break;

	    case 8:                     // read first char of field name
	      if ( _buffer[i] == '\n' )
		{
		  endReached = true;
		}
	      else if ( _buffer[i] == '\r' )
		{
		  endReached = true;
		}
	      else
		state = 9;
	      break;

	    case 9:                     // read rest of field name until ":"
	      if ( _buffer[i] == '\n' ) // end of the line - single field name ends
		{
		  _buffer[i] = 0; // terminate field name
		  fields.push_back(std::make_pair(lastName,(const char*)NULL));
		  state = 6;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - single field name ends
		{
		  _buffer[i] = 0; // terminate field name
		  fields.push_back(std::make_pair(lastName,(const char*)NULL));
		  state = 7;
		}
	      else if ( _buffer[i] == ':' ) // end of the line - single field name ends
		{
		  _buffer[i] = 0; // terminate field name
		  lastValue = _buffer+i+1;
		  state = 10;
		}
	      break;
	      
	    case 10:                    // read in field value until CRLF
	      if ( _buffer[i] == '\n' ) // end of the line - field:value ends
		{
		  _buffer[i] = 0; // terminate field value
		  fields.push_back(std::make_pair(lastName,lastValue));
		  state = 6;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - field:value ends
		{
		  _buffer[i] = 0; // terminate field name
		  fields.push_back(std::make_pair(lastName,lastValue));
		  state = 7;
		}
	      break;

	      // the following 2 states are called from state 2 when "." is found 
	    case 11:                      // the fileName contains "."
	    case 12:                      // the fileName contains ".."
	      if ( _buffer[i] == '?' )
		{
		  _buffer[i] = 0;         // terminate fileName
		  lastName = _buffer+i+1;
		  state = 3;
		}
	      else if ( _buffer[i] == ' ' ) // HTTP version follows
		{
		  _buffer[i] = 0;         // terminate fileName
		  state = 5;
		}
	      else if ( _buffer[i] == '\n' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  endReached = true;
		}
	      else if ( _buffer[i] == '\r' ) // end of the line - simple HTTP request ends
		{
		  _buffer[i] = 0;       // terminate value
		  endReached = true;
		}
	      else if ( _buffer[i] == '.' ) // this is a security feature - we want to avoid "../" in the path
                {
		  state = 12;
                }
	      else if ( _buffer[i] == '/' ) // this is a security feature - we want to avoid "../" in the path
                {
		  if ( state == 12 )
		    syntaxError = true;
		  else
		    state = 2;
                }
	      else
		{
		  state = 2;
		}
	      break;
	    }

	  if ( endReached || syntaxError )
	    break;
	}

      if ( endReached || syntaxError )
	break;
    }

  //  std::vector< std::pair< const char*, const char* > > postParams;
  // read POST body
  if ( endReached && (strcmp(method,"POST") == 0) )
    {
      int responseSize = 0;
      for( int j = 0; j < fields.size(); j++ )
	if (strcmp(fields[j].first,"Content-Length") == 0)
	  {
	    responseSize = atol(fields[j].second);
	    break;
	  }
      
      state = 0;
      while (length < BUFFSIZE)
	{
	  for ( ; i < length; i++ )
	    {
	      if ( responseSize == 0 )
		break;

	      switch ( state ) 
		{
		case 0:
		  if ( _buffer[i] == '\n' )
		    {
		    }
		  else if ( _buffer[i] == '\r' )
		    {
		    }
		  else
		    {
		      lastName = _buffer + i;
		      responseSize--;
		      state = 1;
		    }
		  break;

		case 1:
		  responseSize--;
		  if ( _buffer[i] == '=' )
		    {
		      _buffer[i] = 0;        // terminate param
		      lastValue = _buffer+i+1;
		      state = 2;
		    }
		  break;

		case 2:
		  responseSize--;
		  if ( _buffer[i] == '&' )
		    {
		      _buffer[i] = 0;        // terminate param
		      state = 1;
		      urlDecode(lastName);
		      urlDecode(lastValue);
		      params.push_back( std::make_pair( lastName, lastValue ) );
		      lastName = _buffer+i+1;
		    }
		  break;
		}
	    }

	  if ( responseSize == 0 )
	    {
	      if ( state == 2 )
		{
		  _buffer[i] = 0;
		  urlDecode(lastName);
		  urlDecode(lastValue);
		  params.push_back( std::make_pair( lastName, lastValue ) );
		}
	      break;
	    }

	  int ret = read(handle,_buffer+length,BUFFSIZE-length);
	  if( ret < 1 )
	    {
	      //	  printf("Failed to read browser request");
	      break;
	    }
	  length += ret;
	}
      /*
      for( int i = 0; i < params.size(); i++ )
	{
	  printf("  postParam#%d: %s = %s\n",i,postParams[i].first,postParams[i].second);
	}
      */
    }

  FILE* output = fdopen(handle,"w");

  if ( (length == BUFFSIZE) || syntaxError || (fileName == NULL) )
    {
      //      printf("Syntax error\n");
      fprintf(output, 
	      "HTTP/1.0 400 Bad Request\r\n" \
	      "Content-Type: text/html\r\n" \
	      "\r\n" \
	      "<html>" \
	      "<head><title>400 Bad Request</title></head>" \
	      "<body><h1>Bad Request</h1><p>Your browser sent a request that this server could not understand.</p></body>" \
	      "</html>");
    }
  else
    {
      int session = -1; // read the sessionId
      for ( size_t i = 0; i < fields.size(); i++ )
	if ( strcmp(fields[i].first,"Cookie") == 0 )
	  {
	    const char* pos = strstr(fields[i].second,"session=");
	    if (pos)
	      {
		session = atol(pos + 8);
		break;
	      }
	  }

      if ( session == -1 ) // session Id not found yet
	{
	  // save the random session and refresh the page immediately
	  fprintf(output,
		  "HTTP/1.0 200 OK\r\n"		\
		  "Set-Cookie: session=%d\r\n"	\
		  "Content-Type: text/html\r\n" \
		  "\r\n"			\
		  "<html>"			\
		  "<head>"				   \
		  "  <meta http-equiv=\"refresh\" content=\"0\" />"	\
		  "</head><body></body>"						\
		  "</html>",rand());
	}
      else if ( _dispatcher->processWeb(method, fileName, params, fields, output, session ) )
	{
	  // served by user code
	}
      else if ( _file = fopen(fileName+1,"rb") ) // serve static content if the file exists on disk, ignore the first slash in the name
	{
	  char* mimeType = NULL;
	  char* extension = NULL;
	  char* ptr = fileName;
	  while ( *ptr )
	    {
	      if ( (*ptr) == '.' )
		extension = ptr;
	      ptr++;
	    }
	  if ( extension )
	    {
	      if  ( strcmp(extension,".bmp") == 0 )
		mimeType = "image/bmp";
	      else if  ( strcmp(extension,".css") == 0 )
		mimeType = "text/css";
	      else if  ( strcmp(extension,".gif") == 0 )
		mimeType = "image/gif";
	      else if ( strcmp(extension,".jpg") == 0 )
		mimeType = "image/jpeg";
	      else if ( strcmp(extension,".jpeg") == 0 )
		mimeType = "image/jpeg";
	      else if ( strcmp(extension,".js") == 0 )
		mimeType = "application/javascript";
	      else if ( strcmp(extension,".png") == 0 )
		mimeType = "image/png";
	      else if ( strcmp(extension,".ico") == 0 )
		mimeType = "image/x-icon";
	      else if ( strcmp(extension,".zip") == 0 )
		mimeType = "application/zip";
	      else if ( strcmp(extension,".gz") == 0 )
		mimeType = "application/x-zip";
	      else if ( strcmp(extension,".tar") == 0 )
		mimeType = "application/x-tar";
	      else if ( strcmp(extension,".tgz") == 0 )
		mimeType = "application/x-tar";
	      else if ( strcmp(extension,".htm") == 0 )
		mimeType = "text/html";
	      else if ( strcmp(extension,".html") == 0 )
		mimeType = "text/html";
	      else if ( strcmp(extension,".txt") == 0 )
		mimeType = "text/plain";
	    }

	  if ( mimeType )
	    {
	      fprintf(output,
		      "HTTP/1.0 200 OK\r\n" \
		      "Content-Type: %s\r\n" \
		      "\r\n",mimeType);

	      //      printf("Static file served: %s\n", fileName);
	      while (size_t size = fread(_fbuf, 1, FBSIZE, _file)) {
		fflush(output);
		fwrite(_fbuf, 1, size, output);
	      }
	    }
	  else
	    {
	      //      printf("File type not allowed/supported: %s\n", fileName);
	      fprintf(output,
		      "HTTP/1.0 403 Forbidden\r\n"  \
		      "Content-Type: text/html\r\n" \
		      "\r\n"   \
		      "<html>" \
		      "<head><title>403 Forbidden</title></head>" \
		      "<body><h1>Forbidden</h1><p>File type not allowed/supported: %s </p></body>" \
		      "</html>",fileName);
	    }
	}
      else // fileName not found, send the error response with some debugging info
	{
	  fprintf(output,
		  "HTTP/1.0 404 Not Found\r\n" \
		  "Content-Type: text/html\r\n" \
		  "\r\n"   \
		  "<html>" \
		  "<head><title>404 Not Found</title></head>" \
		  "<body><h1>File or directory not found.</h1>" \
		  " <h2>Please check the details below:</h2>");

	  fprintf(output,"<h3>method</h3> %s<br>", method);
	  fprintf(output,"<h3>fileName</h3> %s<br>", fileName);
	  for ( int i = 0; i < params.size(); i++ )
	    {
	      fprintf(output,"<h3>param #%d</h3> %s = %s<br>",i,params[i].first,params[i].second);
	    }
	  for ( int i = 0; i < fields.size(); i++ )
	    {
	      fprintf(output,"<h3>field #%d</h3> %s : %s<br>",i,fields[i].first,fields[i].second);
	    }

	  fprintf(output,"<h3>session</h3>%d<br>",session);

	  fprintf(output,
		  "</body>"		       \
		  "</html>");
	}
    }

  fflush(output);

  //  printf("Finished stream no %d\n",handle);

  fclose(output);
}

void WebThread::run()
{
  //  printf("WebThread started\n");
  _watchdog->atomicCloseFileIfOpen(_file); // close file when stalled thread was forced to terminate during previous file reading
  while( int handle = _sharedQueue->pop() )
    {
      _watchdog->tic(this,handle);
      parseStream(handle);
      _watchdog->tac(this);
      _watchdog->atomicCloseFileIfOpen(_file); // if file was read, close the file and set the pointer to null
      //      close(handle); // already closed by fclose(output) inside parseStream
    }
  //  printf("WebThread terminated\n");
}
