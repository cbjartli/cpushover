/*
  Copyright (c) 2015 Christian Bjartli

  Permission is hereby granted, free of charge, to any person obtaining a copy 
  of this software and associated documentation files (the "Software"), to deal 
  in the Software without restriction, including without limitation the rights 
  to use, copy, modify, merge, publish, distribute, sublicense, and/or 
  sell copies of the Software, and to permit persons to whom the Software is 
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all 
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
  SOFTWARE.
*/

#ifndef PUSHOVER_H
#define PUSHOVER_H

#include <time.h>
#include <curl/curl.h>

#define CPSH_TOKEN_LN 30
#define CPSH_MAX_API_URL_LN 64
#define CPSH_DEFAULT_API_URL "https://api.pushover.net/1/messages.json"

/* Error codes */
#define CPSH_ERR_INIT       1
#define CPSH_ERR_NONTERM    2
#define CPSH_ERR_STRLEN     3
#define CPSH_ERR_BLANK_USR  4
#define CPSH_ERR_BLANK_MSG  5
#define CPSH_ERR_MSG_FORMAT 6
#define CPSH_ERR_CURL_INIT  7
#define CPSH_ERR_CURL_POST  8
#define CPSH_ERR_SEND_FAIL  9

/* This is a single-point-of-truth for the fields defined in the Pushover API. 
   We generate structs and necessary code using X-macros.  Format: 
   X(type, name, val, dep), where "type" is data type, "name" field name, "val" 
   checks done to see if input is valid, and "dep" dependencies on other fields. */
#define CPSH_API_FIELDS(X)                                            \
    X(CHARPT,   user,      STLEN(30, 30),       NODEP                )\
    X(CHARPT,   message,   STLEN(1, 1024),      NODEP                )\
    X(CHARPT,   title,     STLEN(0, 250),       NODEP                )\
    X(CHARPT,   device,    STLEN(0, 25),        NODEP                )\
    X(CHARPT,   url,       STLEN(0, 512),       NODEP                )\
    X(CHARPT,   url_title, STLEN(0, 100),       NEMPTY(url)          )\
    X(TIMET,    time,      NODEP,               NZERO(time)          )\
    X(CHARPT,   sound,     STLEN(0, 16),        NODEP                )\
    X(SIGNCHAR, priority,  BOUND(-2, 2),        NODEP                )\
    X(SIZET,    retry,     NORBOUND(30, 86400), FIELDEQ(priority, 2) )\
    X(SIZET,    expire,    NORBOUND(30, 86400), FIELDEQ(priority, 2) )

/* Message data struct. Generated X-macro-style from the above. See 
   CPSH_API_FIELDS for member names and data types. */
#define GEN_STRUCT(type, name, check, dep) GEN_STRUCT_ ## type(name) 
#define GEN_STRUCT_CHARPT(name) char* name;
#define GEN_STRUCT_TIMET(name) time_t name; 
#define GEN_STRUCT_SIGNCHAR(name) signed char name; 
#define GEN_STRUCT_SIZET(name) size_t name;
typedef struct
{
    CPSH_API_FIELDS(GEN_STRUCT)
} cpsh_message;

/* Init interface. Call cpsh_init with your Pushover API key */
int cpsh_init(char*);

/* Send message. */
int cpsh_send(cpsh_message*);
#endif
