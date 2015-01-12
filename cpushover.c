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
#include "cpushover.h"
#include "cJSON.h"

/* Needed for some preprocessor evaluations later on */
#define EMPTY() 
#define DEFER(a) a EMPTY()
#define EVAL(...) __VA_ARGS__

/* Upper bound of chars needed to convert to char string of decimal repr. Note: 2^3 < 10 */
#define SIZSTRBUF (CHAR_BIT * sizeof(size_t))/3 + 2
#define TIMETSTRBUF (CHAR_BIT * sizeof(time_t))/3 + 2

/* Private structs */ 
typedef struct
{
    char *memory;
    size_t size;
} cpsh_memory;

typedef struct
{
    int initialized;
    char api_token[CPSH_TOKEN_LN+1];
    char api_url[CPSH_MAX_API_URL_LN+1];
} cpsh_config;

/* Private prototypes */
int pr_ascii_len(char*);
size_t cpsh_write_callback(char*, size_t, size_t, void*);
int cpsh_validate_input(cpsh_message*);

/* Global configuration */
cpsh_config config;

#ifdef CPSH_APPLICATION
int 
main(int argc, char *argv[])
{
    return 0;
}
#endif /*CPSH_APPLICATION*/


/* 
   Checks if *s is printable-ascii-character string. 
   Return values: -1 if string contains non-printable ascii char, 0 if s is NULL, 
   string length otherwise
 */
int 
pr_ascii_len(char *s)
{
    if (s == NULL) return 0;

    int len;
    for (len = 0; *s != '\0'; len++, s++)
    {
        if (!isprint(*s)) return -1;
    }

    return len;
}

/*
 * Initializes data
 */
int
cpsh_init(char* token)
{
    if (pr_ascii_len(token) != CPSH_TOKEN_LN) return CPSH_ERR_INIT;
    strcpy(config.api_token, token);
    strcpy(config.api_url, CPSH_DEFAULT_API_URL);
    config.initialized = 1;
    return 0;
}


/*
 * Sends pushover message 
 */
int 
cpsh_send(cpsh_message *m)
{
    /* Make sure library has been initialized */
    if (!config.initialized)
    {
        return CPSH_ERR_INIT;
    }

    /* Validate input */
    int input_valid;
    if ((input_valid = cpsh_validate_input(m)))
    {
        return input_valid;
    }

    /* Connection */
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return CPSH_ERR_CURL_INIT;
    }

    /* Set up the message */
    struct curl_httppost *post = NULL;
    struct curl_httppost *last = NULL;
    curl_formadd(&post, &last, CURLFORM_COPYNAME, "token", CURLFORM_COPYCONTENTS, \
            config.api_token, CURLFORM_END);

    /* Generate HTTPS POST fields from structure in header file */

    /* Dependencies */
    #define DEP_NODEP 1
    #define DEP_NEMPTY(field) (m-> field != NULL) && (m-> field [0] != '\0')
    #define DEP_NZERO(field) m-> field != 0
    #define DEP_FIELDEQ(field, val) m-> field == val

    /* Add fields */
    #define GENERATE_CURLFORM(type, name, check, dep) if (DEP_ ## dep) \
        { GENERATE_CURLFORM_ ## type(name) } 
    #define GENERATE_CURLFORM_CHARPT(name) \
        if ((m-> name != NULL) && (m-> name [0] != '\0')) \
            curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, \
                    m-> name, CURLFORM_END);
    #define GENERATE_CURLFORM_TIMET(name) \
        char name ## buf [TIMETSTRBUF]; \
        snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
        curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, \
                name ## buf, CURLFORM_END); 
    #define GENERATE_CURLFORM_SIZET(name) \
        char name ## buf [SIZSTRBUF]; \
        snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
        curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, \
                name ## buf, CURLFORM_END); 
    #define GENERATE_CURLFORM_SIGNCHAR(name) \
        char name ## buf [SIZSTRBUF]; \
        snprintf(name ## buf, sizeof(name ## buf), "%li", (long) m-> name); \
        curl_formadd(&post, &last, CURLFORM_COPYNAME, #name, CURLFORM_COPYCONTENTS, \
                name ## buf, CURLFORM_END); 

    CPSH_API_FIELDS(GENERATE_CURLFORM)

    /* Prepare post */
    cpsh_memory push_response;
    memset(&push_response, 0, sizeof(push_response));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&push_response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &cpsh_write_callback);
    curl_easy_setopt(curl, CURLOPT_URL, config.api_url);
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

int
cpsh_validate_input(cpsh_message *m)
{
    
    #define FLAT_STLEN(a, b) STLEN, a, b
    #define FLAT_NODEP NODEP, N/A, N/A 
    #define FLAT_BOUND(a, b) BOUND, a, b 
    #define FLAT_NORBOUND(a, b) NORBOUND, a, b 
    #define VAL_STLEN(name, a, b) (pr_ascii_len(m-> name) >= a) && (pr_ascii_len(m-> name) <= b)
    #define VAL_NODEP(name, a, b) 1
    #define VAL_BOUND(name, a, b) ((m-> name >= a) && (m-> name <= b))
    #define VAL_NORBOUND(name, a, b) ((m-> name == 0) || (VAL_BOUND(name, a, b)))
    #define GEN_VAL(name, val, a, b) if (! VAL_ ## val(name, a, b)) { return CPSH_ERR_MSG_FORMAT; } 
    #define VALIDATE_FIELDS(type, name, check, dep) EVAL(DEFER(GEN_VAL)(name, FLAT_ ## check))

    CPSH_API_FIELDS(VALIDATE_FIELDS)
    
    return 0;
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
