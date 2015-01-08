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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "cpushover.h"

#define CPSH_MAX_URL_LN 128
#define CPSH_RESP_BUFSIZE 512

/* Upper bound of chars needed to convert size_t/time_t to char string of its decimal repr. Note: 2^3 < 10 */
#define SIZSTRBUF (CHAR_BIT * sizeof(size_t))/3 + 2
#define TIMETSTRBUF (CHAR_BIT * sizeof(time_t))/3 + 2

/* Private structs */ 
typedef struct
{
    char *memory;
    size_t size;
} cpsh_memory;

/* Private prototypes */
int npastr(char*, size_t);
size_t cpsh_write_callback(char*, size_t, size_t, void*);

/* Global vars */
static char CPSH_API_TOKEN[CPSH_TOKEN_LN+1];
static char CPSH_API_URL[CPSH_MAX_URL_LN] = "https://api.pushover.net/1/messages.json";

#ifdef CPSH_APPLICATION
int 
main(int argc, char *argv[])
{
    return 0;
}
#endif /*CPSH_APPLICATION*/


/* 
 * Checks if *s is printable-ascii-character zero-terminated string.
 */
int 
npastr(char *s, size_t len)
{
    for (; len > 0 && *s != '\0'; len--, s++)
    {
        if (!isprint(*s)) 
        {
            return CPSH_ERR_ASCII;
        }
    }

    if (*s != '\0')
    {
        return CPSH_ERR_NONTERM;
    }

    else return 0;
}

/*
 * Initializes data
 */
int
cpsh_init(char* token)
{
    if (npastr(token, CPSH_TOKEN_LN))
    {
        return CPSH_ERR_ASCII;
    } 
    else if (strlen(token) != CPSH_TOKEN_LN)
    { 
        return CPSH_ERR_STRLEN;
    }

    strncpy(CPSH_API_TOKEN, token, CPSH_TOKEN_LN);
    CPSH_API_TOKEN[CPSH_TOKEN_LN] = '\0';
    
    return 0;
}


/*
 * Sends pushover message 
 */
int 
cpsh_send(cpsh_message *m)
{
    /* Make sure library has been initialized */
    if (CPSH_API_TOKEN[0] == '\0')
    {
        return CPSH_ERR_INIT;
    }

    int msg_string_ok = 1;
    if (!msg_string_ok) 
    {
        return CPSH_ERR_MSG_FORMAT;
    } 
    else if (m->user[0] == '\0')
    {
        return CPSH_ERR_BLANK_USR;
    } 
    else if (m->message[0] == '\0')
    {
        return CPSH_ERR_BLANK_MSG;
    }

    /* Bound checking */
#define BOUND(VAR, LOW, HIGH) if (VAR < LOW) { VAR = LOW; } else if (VAR > HIGH) { VAR = HIGH; }
    BOUND(m->priority, -2, 2);
    BOUND(m->retry, 30, 86400);
    BOUND(m->expire, 0, 86400);
#undef BOUND

    /* Connection */
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return CPSH_ERR_CURL_INIT;
    }

    /* Set up the message */
    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;
    curl_formadd(&post, &last, CURLFORM_COPYNAME, "token", CURLFORM_COPYCONTENTS, CPSH_API_TOKEN, CURLFORM_END);

    /* Generate HTTPS POST fields from structure in header file */
/* Send conditions (dep) */
#define NODEP 1
#define NEMPT(a) (m-> a != NULL) && (m-> a [0] != '\0')
#define FIELDEQ(a, b) m-> a == b
#define NZERO(a) m-> a != 0 
#define BOUND(a, b, c) (m-> a >= b) && (m-> a <= c)

/* Add fields */
#define GENERATE_CURLFORM(type, name, req, pre, dep) if (dep) { GENERATE_CURLFORM_ ## type(name) } 
#define GENERATE_CURLFORM_CHARPT(name) \
    curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, m-> name, CURLFORM_END);
#define GENERATE_CURLFORM_TIMET(name) \
    char name ## buf [TIMETSTRBUF]; \
    snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
    curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, name ## buf, CURLFORM_END); 
#define GENERATE_CURLFORM_SIZET(name) \
    char name ## buf [SIZSTRBUF]; \
    snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
    curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, name ## buf, CURLFORM_END); 
#define GENERATE_CURLFORM_SIGNCHAR(name) \
    char name ## buf [SIZSTRBUF]; \
    snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
    curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, name ## buf, CURLFORM_END); 

    CPSH_API_FIELDS(GENERATE_CURLFORM)

    /* Prepare post */
    cpsh_memory push_response;
    memset(&push_response, 0, sizeof(push_response));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&push_response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &cpsh_write_callback);
    curl_easy_setopt(curl, CURLOPT_URL, CPSH_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

    /* Perform HTTPS POST */
    CURLcode res;
    res = curl_easy_perform(curl);
    
    curl_formfree(post);
    if (res != CURLE_OK)
    {
        return CPSH_ERR_CURL_POST;
    }

    /* Parse JSON response */
    cJSON *root = cJSON_Parse(push_response.memory);
    int status = cJSON_GetObjectItem(root, "status")->valueint;

    /* Cleanup */
    free(push_response.memory);
    cJSON_Delete(root);

    if (status != 1)
    { 
        return CPSH_ERR_SEND_FAIL;
    }
    else
    {
        return 0;
    }
}

size_t 
cpsh_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t data_length = size * nmemb;
    cpsh_memory *mem = (cpsh_memory *)userdata;
    mem->memory = realloc(mem->memory, mem->size + data_length + 1);
    if (mem->memory == NULL)
    {
        /* Returning a value less than data_length signals an error condition to libcurl */
        return 0;
    }

    memcpy(mem->memory + mem->size, ptr, data_length);
    mem->size += data_length;
    mem->memory[mem->size] = '\0';

    return data_length;
}
