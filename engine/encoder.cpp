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

#include "encoder.h"
#include <string.h>

std::string decodeUriComponent(const std::string & src)
{
  std::string result;
  int from = 0;
  int len = src.length();
  while ( from < len )
    {
      if ( src[from] == 37 ) // percentage
	{
          from++;
	  if ( from == len )
	    break;
	  unsigned char c1 = src[from];
	  if ( (c1 >= '0') && (c1 <= '9') )
	    c1 -= '0';
	  else if ( (c1 >= 'a') && (c1 <= 'f') )
	    c1 = c1 - 'a' + 10;
	  else if ( (c1 >= 'A') && (c1 <= 'F') )
	    c1 = c1 - 'A' + 10;
	  else break;

          from++;
	  if ( from == len )
	    break;
	  unsigned char c2 = src[from];
	  if ( (c2 >= '0') && (c2 <= '9') )
	    c2 -= '0';
	  else if ( (c2 >= 'a') && (c2 <= 'f') )
	    c2 = c2 - 'a' + 10;
	  else if ( (c2 >= 'A') && (c2 <= 'F') )
	    c2 = c2 - 'A' + 10;
	  else break;
	  

	  unsigned char c = (c1 << 4 ) + c2;
	  result += c;
	}
      else if ( src[from] == '+' ) // special case
	{
	  result += ' ';
	}
      else
	{
	  result += src[from];
	}
      from++;
    }
  return result;
}

// For simplicity assume only alnum is safe.
// In RFC3986 -_.~ are also safe, but it does not harm to encode those.
inline bool isSafe(char c)
{
  if ( (c >= '0') && (c <= '9') )
    return true;
  else if ( (c >= 'a') && (c <= 'f') )
    return true;
  else if ( (c >= 'A') && (c <= 'F') )
    return true;
  else return false;
}

std::string encodeUriComponent(const std::string & src)
{
  std::string result;
  char* hexTab = "0123456789ABCDEF";
  int from = 0;
  int len = src.length();
  unsigned char c;
  while ( from < len )
    {
      c = src[from];
      if ( isSafe(c) )
	{
	  result += c;
	}
      else
	{
	  result += "%";
	  result += hexTab[c >> 4];
	  result += hexTab[c & 15];
	}
      from++;
    }
  return result;
}
