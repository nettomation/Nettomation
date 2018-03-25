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
#include "sharedqueue.h"
#include "dispatcher.h"
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_IP   "0.0.0.0" // bind to all interfaces - can be changed from commandline
#define DEFAULT_PORT 80 // can be changed from commandline

#define MAXTHREADS 20 // watch out total memory
#define MAXTIME   300 // 5 min max runtime per web processing allowed

void ignore_sigpipe(void)
{
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, NULL);
}

int main(int argc, char **argv)
{
  // now analyze the commandline parameters and exit if necessary
  if ( argc == 2 )
    if ( strcmp(argv[1],"--help") == 0 )
      {
	printf("Usage: %s [--port <port_number>] [--ip <ip_address>] [--dir <web_directory>] [--password <password>]\n",argv[0]);
	return 0;
      }


  if (fork() != 0) // parent returns to the shell
    {
      return 0;
    }
  signal(SIGHUP, SIG_IGN); // stay alive if the terminal exists
  signal(SIGCLD, SIG_IGN); // ignore child death

  // ready to go

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if ( sock == -1 )
    {
      fprintf(stderr,"Error creating socket!\n");
      return 1;
    }

  int on = 1;
  if ( setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on)) != 0 ) // allow reusing the same port and address after crash
    {
      fprintf(stderr,"Error setting socket options!\n");
      return 2;
    }
  /*
  on = 1;
  if ( setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, (const char*) &on, sizeof(on)) != 0 ) // allow reusing the same port and address after crash
    {
      fprintf(stderr,"Error setting socket options!\n");
      return 2;
    }
  */

  char*          ip   = DEFAULT_IP;
  unsigned short port = DEFAULT_PORT;

  char* password = "";
  argc--;
  argv++;
  while ( argc >= 2 )
    {
      if ( strcmp(argv[0],"--port") == 0 )
	{
	  port = (unsigned short) atol(argv[1]);
	  if ( port == 0 )
	    {
	      fprintf(stderr,"Wrong port number: %s\n", argv[1]);
	      return 5;
	    }
	  argc = argc - 2;
	  argv = argv + 2;
	}
      else if ( strcmp(argv[0],"--ip") == 0 )
	{
	  ip = argv[1];
	  argc = argc - 2;
	  argv = argv + 2;
	}
      else if ( strcmp(argv[0],"--dir") == 0 )
	{
	  chdir(argv[1]);
	  argc = argc - 2;
	  argv = argv + 2;
	}
      else if ( strcmp(argv[0],"--password") == 0 )
	{
	  password = argv[1];
	  argc = argc - 2;
	  argv = argv + 2;
	}
      else
	break;
    }

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr(ip);
  sin.sin_port = htons(port);
  if ( int status = bind(sock, (struct sockaddr *) &sin, sizeof(sin) ) != 0 )
    {
      fprintf(stderr,"Error binding to socket! (code %d)\n", status); // if port and address in use
      return 3;
    }

  if ( listen(sock,50) != 0 ) // multiple sockets are accepted in parallel, so no need for long sequential queue
    {
      printf("Error listening to socket\n");
      return 4;
    }

  ignore_sigpipe();

  srand ( time(NULL) );

  freopen("output.log","w",stdout); // redirect stdout to file
  freopen("error.log","w",stderr);  // redirect stderr to file
  setvbuf(stdout, NULL, _IONBF, 0); // disable buffering of stdout

  time_t curtime;
  time(&curtime);

  printf("Started on %s",ctime(&curtime));
  printf("HTTP server listening on ip %s, port %d\n", ip, port);

  SharedQueue<int>* sharedQueue = new SharedQueue<int>();
  std::vector< WebThread* > servers;

  WatchdogThread* watchdog = new WatchdogThread(MAXTIME); // max delay allowed
  watchdog->start();

  Dispatcher* dispatcher = new Dispatcher(password);
  dispatcher->start();

  while (1)
    {
      int s = accept(sock, NULL, NULL);
      if ( s < 0 )
	break; // network failed

      bool someServerIsWaiting = sharedQueue->push(s);
      if ( ! someServerIsWaiting )
	{
	  if ( servers.size() < MAXTHREADS )
	    {
	      servers.push_back( new WebThread(sharedQueue,watchdog,dispatcher) );
	      servers.back()->start();
	      printf("Increasing the number of instantiated servers: %d\n", (int)servers.size());
	    }
	  else
	    {
	      printf("Max number of threads reached: %d. Enforcing thread termination.\n", (int)servers.size());
	      watchdog->respawnSlowestThread();
	    }
	}
    }    

  for ( unsigned int i = 0; i < servers.size(); i++ )
    sharedQueue->push(0); // terminate threads

  dispatcher->exitHandler();

  sleep(10);

  dispatcher->cancel();

  close(sock);
  printf("HTTP server finished.\n");
  return 0;
}
