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

#include "webtop.h"
#include "autoregister.h"
#include <sstream>

vector<WebContentCreator*>   contentCreators;


WebTop::WebTop(Dispatcher* dispatcher)
 :
  WebContent(dispatcher,"DUMMY")
{
  for ( size_t i = 0; i < contentCreators.size(); i++ )
    {
      WebContent* ptr = contentCreators[i]->createContent(dispatcher);

      if ( contentCreators[i]->isVisible() )
	ptr->show();
      else
	ptr->hide();

      if ( contentCreators[i]->isTop() )
	_topContent.push_back(ptr);
      else
	_innerContent.push_back(ptr);	
    }
}

void WebTop::render( ostream& stream )
{
  if ( _topContent.empty() )
    {
      stream << "No user content defined! Use AUTO_REGISTER_CLASS.";
    }
  else
    {
      for ( size_t i = 0; i < _topContent.size(); i++ )
	stream << renderContent(_topContent[i]->getName());
    }
}

void WebTop::controlLoop()
{
  for ( size_t i = 0; i < _topContent.size(); i++ )
    _topContent[i]->start();
  for ( size_t i = 0; i < _innerContent.size(); i++ )
    _innerContent[i]->start();
}

WebTop::~WebTop()
{
  for ( size_t i = 0; i < _topContent.size(); i++ )
    _topContent[i]->cancel();
  for ( size_t i = 0; i < _innerContent.size(); i++ )
    _innerContent[i]->cancel();
  for ( size_t i = 0; i < _topContent.size(); i++ )
    delete _topContent[i];
  for ( size_t i = 0; i < _innerContent.size(); i++ )
    delete _innerContent[i];
}
