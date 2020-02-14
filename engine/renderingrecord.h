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

#ifndef RENDERINGRECORD_H_
#define RENDERINGRECORD_H_

#include <vector>
#include <string>

using namespace std;

class WebContent;

class RenderingRecord {
 private:
  WebContent*               _owner;
  long long int             _uniqueId;
 public:
  vector<RenderingRecord*>  _nestedRecords;
  string                    _cache;
  long long int             _timestamp;

  RenderingRecord(WebContent* owner, long long int forceId = -1);
  ~RenderingRecord();
  void clearRecords();
  long long int uniqueId() const { return _uniqueId; }
  WebContent* owner() const { return _owner; }
};

#endif
