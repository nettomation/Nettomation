# Nettomation

Nettomation framework serves for rapid development of web-controlled aplications.
Using a basic knowledge of C++ and HTML the users can create their own applications in minutes.
The result is a small self-contained binary with integrated HTTP server.
All the communication and multithreading details are abstracted from the user, thus avoiding the lengthy learning curve.
It compiles on most linux platforms.

# Examples

Nettomation comes with several examples.
Each example contains application-specific code in `userpages.cpp`/`userpages.h`,
whereas the engine files are shared among applications.

## helloWorld

This application demonstrates how to create a single "Hello World" webpage
with counter whose value is incremented and invalidated each second.

usage:
 
```
git clone https://github.com/nettomation/nettomation
cd nettomation/examples/helloWorld
make
./run.sh
```

## chatApp

This application can be opened from multiple computers, each session can contribute to the chat. If you demo this at the Raspberry Jam, kids will love it!

usage:

```
git clone https://github.com/nettomation/nettomation
cd nettomation/examples/chatApp
make
./run.sh
```

## brewApp

This demo is included to show the most advanced features of Nettomation:
multiple control loops, multiple pages, click events, data events, drag-and-drop events,
interprocess communication, dynamic graphs, dynamic creation of task lists,
upload and download of files, locking and synchronization of objects, etc. 
It mimics the beer-brewing and beer-fermenting GUI on Raspberry PI,
but there is no real interaction with any hardware, so
it can compile and run on any linux platform.

usage:
```
git clone https://github.com/nettomation/nettomation
cd nettomation/examples/brewApp
make
sudo ./run.sh
```
General disclaimer: Changing the code to interact with real hardware is at your own risk!

# Creating new applications - the main concept

The key class in Nettomation is **WebContent**. 
User classes derived from **WebContent** need to be instantiated
using _AUTO_REGISTER_CLASS_ macro.
Each instance can run a separate _controlLoop()_ in its own thread, it can implement a _callback()_
method to react to GUI events and _render()_ method to display HTML.
Inside _render()_ it is possible to hierarchically insert one **WebContent**
into another **WebContent** by using _renderClass()_ function.
**WebContent** also provides methods _hide()_, _show()_ and _invalidate()_ to
control its display cache. In particular, _hide()_ and _show()_ can be used
to implement switching between multiple pages.
**WebContent** keeps a reference to **Dispatcher**
which is a singleton class responsible for routing the communication
and for providing access to other **WebContent** instances by using _findClass()_ (this operation performs also proper type-casting).

Note: _AUTO_REGISTER_CLASS_, _renderClass()_ and _findClass()_ assume that each class derived from **WebContent** needs to be instantiated only once. If you need several instances per class, you can use their equivalents _AUTO_REGISTER_CONTENT_, _renderContent()_ and _findContent()_ which use unique name of the instance.

The communication layer does not employ any client-side javascript framework,
so your selection of javascript toolsets and stylesheets is not limited.

# Starting applications

`./nettomation [--port <port_number>] [--ip <ip_address>] [--dir <web_directory>] [--password <password>]`

Nettomation itself does not require root privileges to be run, but binding to low port numbers or accessing I/O
pins may require "sudo".

The default port is 80, default IP is "0.0.0.0" which means "bind to all intefaces".
The files served by the web server are located in a directory specified by --dir parameter.
If password parameter is ommited, the authorization is not required, otherwise
the authorization takes precedence over any other content served by the web server.
The application can print debugging messages to std::cout/std::cerr, these
are stored in the output.log/error.log files in the web directory.

The included apps come with prepared script `run.sh` which kills the previous running instance, removes the log files and starts a new one on port _8800_ using the password _pwd_. If you need to start the app after reboot, you need to include it in the crontab using `crontab -e`. More details are beyond the scope of this readme.

# Using applications

Nettomation was tested to be compatible with mainstream desktop and mobile browsers.
For example, if the application was started on local computer using port 8800,
it can be opened using

`firefox localhost:8800`

One application can be viewed from multiple browser sessions simultaneously, 
the sessions can partially recover from lost connections, the control loops in the application are not
affected by browser hiccups.

# Contact

For tutorials, donations and proprietary licensing options
please visit http://nettomation.com

[![Donate with PayPal](https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=FZSWTJ78EBUXN)
