utility client example application
==================================

chapter 1: initial setup & basics
---------------------------------

setting up the basic types, opening and closing connections, sending and receiving messages.

### step 1

a basic program loop that prompts the user for a command and then processes it. in this tutorial we will modify this program to perform tasks and retrieve data from a remote server over a websocket connection.

#### build
`clang++ step1.cpp`

#### code so far

*note* a code snapshot for each step is present next to this tutorial file in the git repository.

```cpp
#include <iostream>
#include <string>

int main() {
    bool done = false;
    std::string input;

    while (!done) {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout
                << "\ncommand list:\n"
                << "help: display this help text\n"
                << "quit: exit the program\n"
                << std::endl;
        } else {
            std::cout << "unrecognized command" << std::endl;
        }
    }

    return 0;
}
```

### step 2

_add websocket++ includes and set up an endpoint type._

websocket++ includes two major object types. the endpoint and the connection. the
endpoint creates and launches new connections and maintains default settings for
those connections. endpoints also manage any shared network resources.

the connection stores information specific to each websocket session.

> **note:** once a connection is launched, there is no link between the endpoint and the connection. all default settings are copied into the new connection by the endpoint. changing default settings on an endpoint will only affect future connections.
connections do not maintain a link back to their associated endpoint. endpoints do not maintain a list of outstanding connections. if your application needs to iterate over all connections it will need to maintain a list of them itself.

websocket++ endpoints are built by combining an endpoint role with an endpoint config. there are two different types of endpoint roles, one each for the client and server roles in a websocket session. this is a client tutorial so we will use the client role `websocketpp::client` which is provided by the `<websocketpp/client.hpp>` header.

> ###### terminology: endpoint config
> websocket++ endpoints have a group of settings that may be configured at compile time via the `config` template parameter. a config is a struct that contains types and static constants that are used to produce an endpoint with specific properties. depending on which config is being used the endpoint will have different methods available and may have additional third party dependencies.

the endpoint role takes a template parameter called `config` that is used to configure the behavior of endpoint at compile time. for this example we are going to use a default config provided by the library called `asio_client`, provided by `<websocketpp/config/asio_no_tls_client.hpp>`. this is a client config that uses boost::asio to provide network transport and does not support tls based security. later on we will discuss how to introduce tls based security into a websocket++ application, more about the other stock configs, and how to build your own custom configs.

combine a config with an endpoint role to produce a fully configured endpoint. this type will be used frequently so i would recommend a typedef here.

`typedef websocketpp::client<websocketpp::config::asio_client> client`

#### build
adding websocket++ has added a few dependencies to our program that must be addressed in the build system. firstly, the websocket++ and boost library headers must be in the include search path of your build system. how exactly this is done depends on where you have the websocket++ headers installed and what build system you are using.

in addition to the new headers, boost::asio depends on the `boost_system` shared library. this will need to be added (either as a static or dynamic) to the linker. refer to your build environment documentation for instructions on linking to shared libraries.

`clang++ step2.cpp -lboost_system`

#### code so far
```cpp
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> client;

int main() {
    bool done = false;
    std::string input;

    while (!done) {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout
                << "\ncommand list:\n"
                << "help: display this help text\n"
                << "quit: exit the program\n"
                << std::endl;
        } else {
            std::cout << "unrecognized command" << std::endl;
        }
    }

    return 0;
}

```

### step 3

_create endpoint wrapper object that handles initialization and setting up the background thread._

in order to process user input while network processing occurs in the background we are going to use a separate thread for the websocket++ processing loop. this leaves the main thread free to process foreground user input. in order to enable simple raii style resource management for our thread and endpoint we will use a wrapper object that configures them both in its constructor.

> ###### terminology: websocketpp::lib namespace
> websocket++ is designed to be used with a c++11 standard library. as this is not universally available in popular build systems the boost libraries may be used as polyfills for the c++11 standard library in c++98 build environments. the `websocketpp::lib` namespace is used by the library and its associated examples to abstract away the distinctions between the two. `websocketpp::lib::shared_ptr` will evaluate to `std::shared_ptr` in a c++11 environment and `boost::shared_ptr` otherwise.
>
> this tutorial uses the `websocketpp::lib` wrappers because it doesn't know what the build environment of the reader is. for your applications, unless you are interested in similar portability, are free to use the boost or std versions of these types directly.
>
>[todo: link to more information about websocketpp::lib namespace and c++11 setup]

within the `websocket_endpoint` constructor several things happen:

first, we set the endpoint logging behavior to silent by clearing all of the access and error logging channels. [todo: link to more information about logging]
```cpp
m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
```

next, we initialize the transport system underlying the endpoint and set it to perpetual mode. in perpetual mode the endpoint's processing loop will not exit automatically when it has no connections. this is important because we want this endpoint to remain active while our application is running and process requests for new websocket connections on demand as we need them. both of these methods are specific to the asio transport. they will not be  necessary or present in endpoints that use a non-asio config.
```cpp
m_endpoint.init_asio();
m_endpoint.start_perpetual();
```

finally, we launch a thread to run the `run` method of our client endpoint. while the endpoint is running it will process connection tasks (read and deliver incoming messages, frame and send outgoing messages, etc). because it is running in perpetual mode, when there are no connections active it will wait for a new connection.
```cpp
m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
```

#### build

now that our client endpoint template is actually instantiated a few more linker dependencies will show up. in particular, websocket clients require a cryptographically secure random number generator. websocket++ is able to use either `boost_random` or the c++11 standard library <random> for this purpose. because this example also uses threads, if we do not have c++11 std::thread available we will need to include `boost_thread`.

##### clang (c++98 & boost)
`clang++ step3.cpp -lboost_system -lboost_random -lboost_thread`

##### clang (c++11)
`clang++ -std=c++0x -stdlib=libc++ step3.cpp -lboost_system -d_websocketpp_cpp11_stl_`

##### g++ (c++98 & boost)
`g++ step3.cpp -lboost_system -lboost_random -lboost_thread`

##### g++ v4.6+ (c++11)
`g++ -std=c++0x step3.cpp -lboost_system -d_websocketpp_cpp11_stl_`

#### code so far

```cpp
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <iostream>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> client;

class websocket_endpoint {
public:
    websocket_endpoint () {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
    }
private:
    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
};

int main() {
    bool done = false;
    std::string input;
    websocket_endpoint endpoint;

    while (!done) {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout
                << "\ncommand list:\n"
                << "help: display this help text\n"
                << "quit: exit the program\n"
                << std::endl;
        } else {
            std::cout << "unrecognized command" << std::endl;
        }
    }

    return 0;
}
```

### step 4

_opening websocket connections_

this step adds two new commands to app_client. the ability to open a new connection and the ability to view information about a previously opened connection. every connection that gets opened will be assigned an integer connection id that the user of the program can use to interact with that connection.

#### new connection metadata object

in order to track information about each connection a `connection_metadata` object is defined. this object stores the numeric connection id and a number of fields that will be filled in as the connection is processed. initially this includes the state of the connection (opening, open, failed, closed, etc), the original uri connected to, an identifying value from the server, and a description of the reason for connection failure/closure. future steps will add more information to this metadata object.

#### update `websocket_endpoint`

the `websocket_endpoint` object has gained some new data members and methods. it now tracks a mapping between connection ids and their associated metadata as well as the next sequential id number to hand out. the `connect()` method initiates a new connection. the `get_metadata` method retrieves metadata given an id.

#### the connect method
a new websocket connection is initiated via a three step process. first, a connection request is created by `endpoint::get_connection(uri)`. next, the connection request is configured. lastly, the connection request is submitted back to the endpoint via `endpoint::connect()` which adds it to the queue of new connections to make.

> ###### terminology `connection_ptr`
> websocket++ keeps track of connection related resources using a reference counted shared pointer. the type of this pointer is `endpoint::connection_ptr`. a `connection_ptr` allows direct access to information about the connection and allows changing connection settings. because of this direct access and their internal resource management role within the library it is not safe to for end applications to use `connection_ptr` except in the specific circumstances detailed below.
>
> **when is it safe to use `connection_ptr`?**
> - after `endpoint::get_connection(...)` and before `endpoint::connect()`: `get_connection` returns a `connection_ptr`. it is safe to use this pointer to configure your new connection. once you submit the connection to `connect` you may no longer use the `connection_ptr` and should discard it immediately for optimal memory management.
> - during a handler: websocket++ allows you to register hooks / callbacks / event handlers for specific events that happen during a connection's lifetime. during the invocation of one of these handlers the library guarantees that it is safe to use a `connection_ptr` for the connection associated with the currently running handler.

> ###### terminology `connection_hdl`
> because of the limited thread safety of the `connection_ptr` the library also provides a more flexible connection identifier, the `connection_hdl`. the `connection_hdl` has type `websocketpp::connection_hdl` and it is defined in `<websocketpp/common/connection_hdl.hpp>`. note that unlike `connection_ptr` this is not dependent on the type or config of the endpoint. code that simply stores or transmits `connection_hdl` but does not use them can include only the header above and can treat its hdls like values.
>
> connection handles are not used directly. they are used by endpoint methods to identify the target of the desired action. for example, the endpoint method that sends a new message will take as a parameter the hdl of the connection to send the message to.
>
> **when is it safe to use `connection_hdl`?**
> `connection_hdl`s may be used at any time from any thread. they may be copied and stored in containers. deleting a hdl will not affect the connection in any way. handles may be upgraded to a `connection_ptr` during a handler call by using `endpoint::get_con_from_hdl()`. the resulting `connection_ptr` is safe to use for the duration of that handler invocation.
>
> **`connection_hdl` faqs**
> - `connection_hdl`s are guaranteed to be unique within a program. multiple endpoints in a single program will always create connections with unique handles.
> - using a `connection_hdl` with a different endpoint than the one that created its associated connection will result in undefined behavior.
> - using a `connection_hdl` whose associated connection has been closed or deleted is safe. the endpoint will return a specific error saying the operation couldn't be completed because the associated connection doesn't exist.
> [todo: more here? link to a connection_hdl faq elsewhere?]

`websocket_endpoint::connect()` begins by calling `endpoint::get_connection()` using a uri passed as a parameter. additionally, an error output value is passed to capture any errors that might occur during. if an error does occur an error notice is printed along with a descriptive message and the -1 / 'invalid' value is returned as the new id.

> ###### terminology: `error handling: exceptions vs error_code`
> websocket++ uses the error code system defined by the c++11 `<system_error>` library. it can optionally fall back to a similar system provided by the boost libraries. all user facing endpoint methods that can fail take an `error_code` in an output parameter and store the error that occured there before returning. an empty/default constructed value is returned in the case of success.
>
> **exception throwing varients**
> all user facing endpoint methods that take and use an `error_code` parameter have a version that throws an exception instead. these methods are identical in function and signature except for the lack of the final ec parameter. the type of the exception thrown is `websocketpp::exception`. this type derives from `std::exception` so it can be caught by catch blocks grabbing generic `std::exception`s. the `websocketpp::exception::code()` method may be used to extract the machine readable `error_code` value from an exception.
>
> for clarity about error handling the app_client example uses exclusively the exception free varients of these methods. your application may choose to use either.

if connection creation succeeds, the next sequential connection id is generated and a `connection_metadata` object is inserted into the connection list under that id. initially the metadata object stores the connection id, the `connection_hdl`, and the uri the connection was opened to.

```cpp
int new_id = m_next_id++;
metadata_ptr metadata(new connection_metadata(new_id, con->get_handle(), uri));
m_connection_list[new_id] = metadata;
```

next, the connection request is configured. for this step the only configuration we will do is setting up a few default handlers. later on we will return and demonstrate some more detailed configuration that can happen here (setting user agents, origin, proxies, custom headers, subprotocols, etc).

> ###### terminology: registering handlers
> websocket++ provides a number of execution points where you can register to have a handler run. which of these points are available to your endpoint will depend on its config. tls handlers will not exist on non-tls endpoints for example. a complete list of handlers can be found at  http://www.zaphoyd.com/websocketpp/manual/reference/handler-list.
>
> handlers can be registered at the endpoint level and at the connection level. endpoint handlers are copied into new connections as they are created. changing an endpoint handler will affect only future connections. handlers registered at the connection level will be bound to that specific connection only.
>
> the signature of handler binding methods is the same for endpoints and connections. the format is: `set_*_handler(...)`. where * is the name of the handler. for example, `set_open_handler(...)` will set the handler to be called when a new connection is open. `set_fail_handler(...)` will set the handler to be called when a connection fails to connect.
>
> all handlers take one argument, a callable type that can be converted to a `std::function` with the correct count and type of arguments. you can pass free functions, functors, and lambdas with matching argument lists as handlers. in addition, you can use `std::bind` (or `boost::bind`) to register functions with non-matching argument lists. this is useful for passing additional parameters not present in the handler signature or member functions that need to carry a 'this' pointer.
>
> the function signature of each handler can be looked up in the list above in the manual. in general, all handlers include the `connection_hdl` identifying which connection this even is associated with as the first parameter. some handlers (such as the message handler) include additional parameters. most handlers have a void return value but some (`validate`, `ping`, `tls_init`) do not. the specific meanings of the return values are documented in the handler list linked above.

`app_client` registers an open and a fail handler. we will use these to track whether each connection was successfully opened or failed. if it successfully opens, we will gather some information from the opening handshake and store it with our connection metadata.

in this example we are going to set connection specific handlers that are bound directly to the metadata object associated with our connection. this allows us to avoid performing a lookup in each handler to find the metadata object we plan to update which is a bit more efficient.

lets look at the parameters being sent to bind in detail:

```cpp
con->set_open_handler(websocketpp::lib::bind(
    &connection_metadata::on_open,
    metadata,
    &m_endpoint,
    websocketpp::lib::placeholders::_1
));
```

`&connection_metadata::on_open` is the address of the `on_open` member function of the `connection_metadata` class. `metadata_ptr` is a pointer to the `connection_metadata` object associated with this class. it will be used as the object on which the `on_open` member function will be called. `&m_endpoint` is the address of the endpoint in use. this parameter will be passed as-is to the `on_open` method. lastly, `websocketpp::lib::placeholders::_1` is a placeholder indicating that the bound function should take one additional argument to be filled in at a later time. websocket++ will fill in this placeholder with the `connection_hdl` when it invokes the handler.

finally, we call `endpoint::connect()` on our configured connection request and return the new connection id.

#### handler member functions

the open handler we registered, `connection_metadata::on_open`, sets the status metadata field to "open" and retrieves the value of the "server" header from the remote endpoint's http response and stores it in the metadata object. servers often set an identifying string in this header.

the fail handler we registered, `connection_metadata::on_fail`, sets the status metadata field to "failed", the server field similarly to `on_open`, and retrieves the error code describing why the connection failed. the human readable message associated with that error code is saved to the metadata object.

#### new commands

two new commands have been set up. "connect [uri]" will pass the uri to the `websocket_endpoint` connect method and report an error or the connection id of the new connection. "show [connection id]" will retrieve and print out the metadata associated with that connection. the help text has been updated accordingly.

```cpp
} else if (input.substr(0,7) == "connect") {
    int id = endpoint.connect(input.substr(8));
    if (id != -1) {
        std::cout << "> created connection with id " << id << std::endl;
    }
} else if (input.substr(0,4) == "show") {
    int id = atoi(input.substr(5).c_str());

    connection_metadata::ptr metadata = endpoint.get_metadata(id);
    if (metadata) {
        std::cout << *metadata << std::endl;
    } else {
        std::cout << "> unknown connection id " << id << std::endl;
    }
}
```

#### build

there are no changes to the build instructions from step 3

#### run

```
enter command: connect not a websocket uri
> connect initialization error: invalid uri
enter command: show 0
> unknown connection id 0
enter command: connect ws://echo.websocket.org
> created connection with id 0
enter command: show 0
> uri: ws://echo.websocket.org
> status: open
> remote server: kaazing gateway
> error/close reason: n/a
enter command: connect ws://wikipedia.org
> created connection with id 1
enter command: show 1
> uri: ws://wikipedia.org
> status: failed
> remote server: apache
> error/close reason: invalid http status.
```

#### code so far

```cpp
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {
public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

    connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri)
      : m_id(id)
      , m_hdl(hdl)
      , m_status("connecting")
      , m_uri(uri)
      , m_server("n/a")
    {}

    void on_open(client * c, websocketpp::connection_hdl hdl) {
        m_status = "open";

        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("server");
    }

    void on_fail(client * c, websocketpp::connection_hdl hdl) {
        m_status = "failed";

        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("server");
        m_error_reason = con->get_ec().message();
    }

    friend std::ostream & operator<< (std::ostream & out, connection_metadata const & data);
private:
    int m_id;
    websocketpp::connection_hdl m_hdl;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;
};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data) {
    out << "> uri: " << data.m_uri << "\n"
        << "> status: " << data.m_status << "\n"
        << "> remote server: " << (data.m_server.empty() ? "none specified" : data.m_server) << "\n"
        << "> error/close reason: " << (data.m_error_reason.empty() ? "n/a" : data.m_error_reason);

    return out;
}

class websocket_endpoint {
public:
    websocket_endpoint () : m_next_id(0) {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
    }

    int connect(std::string const & uri) {
        websocketpp::lib::error_code ec;

        client::connection_ptr con = m_endpoint.get_connection(uri, ec);

        if (ec) {
            std::cout << "> connect initialization error: " << ec.message() << std::endl;
            return -1;
        }

        int new_id = m_next_id++;
        connection_metadata::ptr metadata_ptr(new connection_metadata(new_id, con->get_handle(), uri));
        m_connection_list[new_id] = metadata_ptr;

        con->set_open_handler(websocketpp::lib::bind(
            &connection_metadata::on_open,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));
        con->set_fail_handler(websocketpp::lib::bind(
            &connection_metadata::on_fail,
            metadata_ptr,
            &m_endpoint,
            websocketpp::lib::placeholders::_1
        ));

        m_endpoint.connect(con);

        return new_id;
    }

    connection_metadata::ptr get_metadata(int id) const {
        con_list::const_iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            return connection_metadata::ptr();
        } else {
            return metadata_it->second;
        }
    }
private:
    typedef std::map<int,connection_metadata::ptr> con_list;

    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

    con_list m_connection_list;
    int m_next_id;
};

int main() {
    bool done = false;
    std::string input;
    websocket_endpoint endpoint;

    while (!done) {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout
                << "\ncommand list:\n"
                << "connect <ws uri>\n"
                << "show <connection id>\n"
                << "help: display this help text\n"
                << "quit: exit the program\n"
                << std::endl;
        } else if (input.substr(0,7) == "connect") {
            int id = endpoint.connect(input.substr(8));
            if (id != -1) {
                std::cout << "> created connection with id " << id << std::endl;
            }
        } else if (input.substr(0,4) == "show") {
            int id = atoi(input.substr(5).c_str());

            connection_metadata::ptr metadata = endpoint.get_metadata(id);
            if (metadata) {
                std::cout << *metadata << std::endl;
            } else {
                std::cout << "> unknown connection id " << id << std::endl;
            }
        } else {
            std::cout << "> unrecognized command" << std::endl;
        }
    }

    return 0;
}
```

### step 5

_closing connections_

this step adds a command that allows you to close a websocket connection and adjusts the quit command so that it cleanly closes all outstanding connections before quitting.

#### getting connection close information out of websocket++

> ###### terminology: websocket close codes & reasons
> the websocket close handshake involves an exchange of optional machine readable close codes and human readable reason strings. each endpoint sends independent close details. the codes are short integers. the reasons are utf8 text strings of at most 125 characters. more details about valid close code ranges and the meaning of each code can be found at https://tools.ietf.org/html/rfc6455#section-7.4

the `websocketpp::close::status` namespace contains named constants for all of the iana defined close codes. it also includes free functions to determine whether a value is reserved or invalid and to convert a code to a human readable text representation.

during the close handler call websocket++ connections offer the following methods for accessing close handshake information:

- `connection::get_remote_close_code()`: get the close code as reported by the remote endpoint
- `connection::get_remote_close_reason()`: get the close reason as reported by the remote endpoint
- `connection::get_local_close_code()`: get the close code that this endpoint sent.
- `connection::get_local_close_reason()`: get the close reason that this endpoint sent.
- `connection::get_ec()`: get a more detailed/specific websocket++ `error_code` indicating what library error (if any) ultimately resulted in the connection closure.

*note:* there are some special close codes that will report a code that was not actually sent on the wire. for example 1005/"no close code" indicates that the endpoint omitted a close code entirely and 1006/"abnormal close" indicates that there was a problem that resulted in the connection closing without having performed a close handshake.

#### add close handler

the `connection_metadata::on_close` method is added. this method retrieves the close code and reason from the closing handshake and stores it in the local error reason field.

```cpp
void on_close(client * c, websocketpp::connection_hdl hdl) {
    m_status = "closed";
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    std::stringstream s;
    s << "close code: " << con->get_remote_close_code() << " (" 
      << websocketpp::close::status::get_string(con->get_remote_close_code()) 
      << "), close reason: " << con->get_remote_close_reason();
    m_error_reason = s.str();
}
```

similarly to `on_open` and `on_fail`, `websocket_endpoint::connect` registers this close handler when a new connection is made.

#### add close method to `websocket_endpoint`

this method starts by looking up the given connection id in the connection list.  next a close request is sent to the connection's handle with the specified websocket close code. this is done by calling `endpoint::close`. this is a thread safe method that is used to asynchronously dispatch a close signal to the connection with the given handle. when the operation is complete the connection's close handler will be triggered.

```cpp
void close(int id, websocketpp::close::status::value code) {
    websocketpp::lib::error_code ec;
    
    con_list::iterator metadata_it = m_connection_list.find(id);
    if (metadata_it == m_connection_list.end()) {
        std::cout << "> no connection found with id " << id << std::endl;
        return;
    }
    
    m_endpoint.close(metadata_it->second->get_hdl(), code, "", ec);
    if (ec) {
        std::cout << "> error initiating close: " << ec.message() << std::endl;
    }
}
```

#### add close option to the command loop and help message

a close option is added to the command loop. it takes a connection id and optionally a close code and a close reason. if no code is specified the default of 1000/normal is used. if no reason is specified, none is sent. the `endpoint::send` method will do some error checking and abort the close request if you try and send an invalid code or a reason with invalid utf8 formatting. reason strings longer than 125 characters will be truncated.

an entry is also added to the help system to describe how the new command may be used.

```cpp
else if (input.substr(0,5) == "close") {
    std::stringstream ss(input);
    
    std::string cmd;
    int id;
    int close_code = websocketpp::close::status::normal;
    std::string reason = "";
    
    ss >> cmd >> id >> close_code;
    std::getline(ss,reason);
    
    endpoint.close(id, close_code, reason);
}
```

#### close all outstanding connections in `websocket_endpoint` destructor

until now quitting the program left outstanding connections and the websocket++ network thread in a lurch. now that we have a method of closing connections we can clean this up properly.

the destructor for `websocket_endpoint` now stops perpetual mode (so the run thread exits after the last connection is closed) and iterates through the list of open connections and requests a clean close for each. finally, the run thread is joined which causes the program to wait until those connection closes complete.

```cpp
~websocket_endpoint() {
    m_endpoint.stop_perpetual();
    
    for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
        if (it->second->get_status() != "open") {
            // only close open connections
            continue;
        }
        
        std::cout << "> closing connection " << it->second->get_id() << std::endl;
        
        websocketpp::lib::error_code ec;
        m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
        if (ec) {
            std::cout << "> error closing connection " << it->second->get_id() << ": "  
                      << ec.message() << std::endl;
        }
    }
    
    m_thread->join();
}
```

#### build

there are no changes to the build instructions from step 4

#### run

```
enter command: connect ws://localhost:9002
> created connection with id 0
enter command: close 0 1001 example message
enter command: show 0
> uri: ws://localhost:9002
> status: closed
> remote server: websocket++/0.3.0-alpha4
> error/close reason: close code: 1001 (going away), close reason:  example message
enter command: connect ws://localhost:9002
> created connection with id 1
enter command: close 1 1006
> error initiating close: invalid close code used
enter command: quit
> closing connection 1
```

### step 6

_sending and receiving messages_

- sending a messages
- terminology: websocket opcodes, text vs binary messages
- receiving a message

### step 7

_using tls / secure websockets_

chapter 2: intermediate features
--------------------------------

### step 8

_intermediate level features_

- subprotocol negotiation
- setting and reading custom headers
- ping and pong
- proxies?
- setting user agent
- setting origin
- timers and security
- close behavior
- send one message to all connections


### misc stuff not sure if it should be included here or elsewhere?

core websocket++ control flow.
a handshake, followed by a split into 2 independent control strands
- handshake
-- use information specified before the call to endpoint::connect to construct a websocket handshake request.
-- pass the websocket handshake request to the transport policy. the transport policy determines how to get these bytes to the endpoint playing the server role. depending on which transport policy your endpoint uses this method will be different.
-- receive a handshake response from the underlying transport. this is parsed and checked for conformance to rfc6455. if the validation fails, the fail handler is called. otherwise the open handler is called.
- at this point control splits into two separate strands. one that reads new bytes from the transport policy on the incoming channle, the other that accepts new messages from the local application for framing and writing to the outgoing transport channel.
- read strand
-- read and process new bytes from transport
-- if the bytes contain at least one complete message dispatch each message by calling the appropriate handler. this is either the message handler for data messages, or ping/pong/close handlers for each respective control message. if no handler is registered for a particular message it is ignored.
-- ask the transport layer for more bytes
- write strand
-- wait for messages from the application
-- perform error checking on message input,
-- frame message per rfc6455
-- queue message for sending
-- pass all outstanding messages to the transport policy for output
-- when there are no messages left to send, return to waiting

important observations
handlers run in line with library processing which has several implications applications should be aware of:
