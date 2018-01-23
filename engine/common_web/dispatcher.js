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

var sessionId = 0;
var eventCounter = 0;
var refreshCounter = 0;
var handleWarning = null;
var nrPendingTimeouts = 0;
var timestamp = 0;

function sendToDispatcher(owner,name,index1,index2,value)
{
    eventCounter = eventCounter + 1;
    var parameters = "?OWNER=" + owner + "&NAME=" + name + "&INDEX1=" + index1 + "&INDEX2=" + index2 + "&VALUE=" + escape(value);
    var topHead = document.getElementsByTagName('head').item(0);
    var auxScript = document.createElement('script');
    auxScript.src = "/dispatch_callback.js" + parameters
	+ "&COUNTER=" + eventCounter 
	+ "&SESSION=" + sessionId; // unique parameters to avoid caching;
    auxScript.id = "dispatch_callback_" + eventCounter;
    topHead.appendChild(auxScript);
}

function sendFileToDispatcher(owner,name,index1,index2,filename)
{
    var reader = new FileReader();
    reader.onload = function(progressEvent){
	sendToDispatcher(owner,name,index1,index2,this.result); // ! entire file - todo: limit size to max URL size
    };
    reader.readAsText(filename);
}

function connectionLost()
{
    nrPendingTimeouts = nrPendingTimeouts + 1;
    if ( nrPendingTimeouts > 1 )
	alert("Connection with server lost");
    updateFromDispatcher(); // try again after user clicks OK
}

function updateFromDispatcher()
{
    clearTimeout(handleWarning);

    var topHead = document.getElementsByTagName('head').item(0);
    if ( refreshCounter > 0 )
    {
	// deallocate memory of previous refresh
	var previousScript = document.getElementById("dispatch_refresh_" + refreshCounter);
	topHead.removeChild(previousScript);
    }
    refreshCounter = refreshCounter + 1;
    var newScript = document.createElement('script');
    newScript.id = "dispatch_refresh_" + refreshCounter;
    newScript.src = "/dispatch_refresh.js?counter=" + refreshCounter 
	+ "&TIMESTAMP=" + timestamp
	+ "&SESSION=" + sessionId; // unique parameters to avoid caching
    newScript.defer = "defer";

    handleWarning = setTimeout(connectionLost,5000);
    topHead.appendChild(newScript);
}

handleWarning = setTimeout(connectionLost,6000); // first timeout
setTimeout(updateFromDispatcher,500);            // first refresh

/*
  The code from dispatcher should contain:
  timestamp = <new timestamp>;
  setTimeout(updateFromDispatcher,100); // no need to delay next refresh more
*/

function allowDrop(ev) {
    ev.preventDefault();
}

function registerDragStart(event,owner,source_name,source_index1,source_index2)
{
  event.dataTransfer.setData("source_name",source_name);
  event.dataTransfer.setData("source_index1",source_index1);
  event.dataTransfer.setData("source_index2",source_index2);
}

function dispatchDropEvent(event,owner,destination_name,destination_index1,destination_index2)
{
  event.preventDefault();
  var source_name = event.dataTransfer.getData("source_name");
  var source_index1 = event.dataTransfer.getData("source_index1");
  var source_index2 = event.dataTransfer.getData("source_index2");

  eventCounter = eventCounter + 1;
  var parameters = "?OWNER=" + owner + "&SOURCE_NAME=" + source_name + "&SOURCE_INDEX1=" + source_index1 + "&SOURCE_INDEX2=" + source_index2
	+ "&DESTINATION_NAME=" + destination_name + "&DESTINATION_INDEX1=" + destination_index1 + "&DESTINATION_INDEX2=" + destination_index2;
  var topHead = document.getElementsByTagName('head').item(0);
  var auxScript = document.createElement('script');
  auxScript.src = "/dispatch_drop.js" + parameters
	+ "&COUNTER=" + eventCounter 
	+ "&SESSION=" + sessionId; // unique parameters to avoid caching;
  auxScript.id = "dispatch_drop_" + eventCounter;
  topHead.appendChild(auxScript);
}

