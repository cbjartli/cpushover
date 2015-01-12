cpushover
=========

An API and command-line interface for sending messages via Pushover, written in ANSI C

As per now, only the API is functional, and the CLI itself is yet to be written. If you want to use cpushover in your own application, this is what you do: 

* Include cpushover.h into your project, link libcurl. The linker flags needed can be found by running "curl-config --libs".
* Initialize libcurl through curl_global_init with a sensible set of flags, e.g. curl_global_init(CURL_GLOBAL_DEFAULT); 
* Initialize the API by providing your pushover handle, cpsh_init("yourhandlehere"); 
* Declare the message struct using the typedef cpsh_message, e.g. cpsh_message msg; Remember to set all unused fields to 0, e.g. by memset(&msg, 0, sizeof(msg)); Set the parameters you want. You have to set msg.user and msg.message, the recipient's pushover token and the message body respectively, but all the other parameters can be zero/NULL. 
* Send the message with cpsh_send(&msg); A zero return value indicates success. Anything else indicates an error, and can be decoded using the error constants in the header file. 
* Run  curl_global_cleanup() when you're done. 


This project uses Dave Gamble's cJSON library, http://sourceforge.net/projects/cjson/. 
