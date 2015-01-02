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

/* Length of pushover keys */
#define CPSH_TOKEN_LN 30 
#define CPSH_USER_KEY_LN 30

/* Error codes */
#define CPSH_ERR_INIT       1
#define CPSH_ERR_ASCII      2
#define CPSH_ERR_NONTERM    3
#define CPSH_ERR_STRLEN     4
#define CPSH_ERR_BLANK_USR  5
#define CPSH_ERR_BLANK_MSG  6
#define CPSH_ERR_MSG_FORMAT 7
#define CPSH_ERR_CURL_INIT  8
#define CPSH_ERR_CURL_POST  9
#define CPSH_ERR_SEND_FAIL  10

/* Fields defined in the Pushover API. We generate structs and boilerplate code
   using X-macros. Field format is 
            X(type, name, req, pre, dep) 
   where "type" designates data type, "name" the field name, "req" whether the 
   field is REQuired or OPTional, "pre" input checks done after the user has 
   supplied the entire message, and "dep" dependencies that we need to check 
   before POSTing the data to the API. 

   pre/dep macros: 
   NODEP: No dependency/no check.
   NEMPT(fld): field passes iff fld is a non-NULL and non-empty char* 
       (i.e. fld != NULL and *fld != '\0')
   NZERO(fld): passes iff fld != 0
   FIELDEQ(fld, val): passes iff fld == val
   BOUND(fld, min, max): Field always passes. Checks that fld >= min and 
   fld <= max. Sets fld = min if fld <= min. Similar for max. 

   The control flow is as follows: 
   * The user supplies a cpsh_message m
   * We check that m contains all REQ fields (or return an error code)
   * We check that all fields pass their pre criteria (or return error code)
   * We pass fields to curl iff they pass their dep criteria.
   * We send the message.
*/
#define CPSH_API_FIELDS(X)                                                      \
    X(CHARPT,   user,      REQ, NEMPT(user),              NODEP                )\
    X(CHARPT,   message,   REQ, NEMPT(message),           NODEP                )\
    X(CHARPT,   title,     OPT, NODEP,                    NEMPT(title)         )\
    X(CHARPT,   device,    OPT, NODEP,                    NEMPT(device)        )\
    X(CHARPT,   url,       OPT, NODEP,                    NEMPT(url)           )\
    X(CHARPT,   url_title, OPT, NODEP,                    NEMPT(url_title)     )\
    X(TIMET,    time,      OPT, NODEP,                    NZERO(time)          )\
    X(CHARPT,   sound,     OPT, NODEP,                    NEMPT(sound)         )\
    X(SIGNCHAR, priority,  OPT, BOUND(priority, -2, 2),   NODEP                )\
    X(SIZET,    retry,     OPT, BOUND(retry, 30, 86400),  FIELDEQ(priority, 2) )\
    X(SIZET,    expire,    OPT, BOUND(expire, 30, 86400), FIELDEQ(priority, 2) )

/* Message data struct. Generated X-macro-style from the above. See 
   CPSH_API_FIELDS for member names and data types. */
#define GEN_STRUCT(type, name, req, pre, dep) GEN_STRUCT_ ## type(name) 
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
