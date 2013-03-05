/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2010 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header 297205 2010-03-30 21:09:07Z johannes $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
extern "C" {
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_drizzle.h"
#include <libdrizzle-5.1/libdrizzle.h>
}
/* If you declare any globals in php_drizzle.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(drizzle)
*/

/* True global resources - no need for thread safety here */
static int le_drizzle;
static int le_drizzle_binlog;
static int le_drizzle_binlog_event;

typedef struct _php_drizzle {
    drizzle_st* conn;
} php_drizzle;

typedef struct _php_drizzle_binlog_callback {
    zval* event_func;
    zval* error_func;
    zval* arg;
} php_drizzle_binlog_callback;

typedef struct _php_drizzle_binlog {
    drizzle_binlog_st* binlog;
    php_drizzle_binlog_callback* callback;
} php_drizzle_binlog;

typedef struct _php_drizzle_binlog_event {
    drizzle_binlog_event_st* event;
} php_drizzle_binlog_event;


/* {{{ drizzle_functions[]
 *
 * Every user visible function must have an entry in drizzle_functions[].
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_drizzle_binlog_init, 0, 0, 3)
    ZEND_ARG_INFO(0, conn)
    ZEND_ARG_INFO(0, event_callback)
    ZEND_ARG_INFO(0, error_callback)
    ZEND_ARG_INFO(1, arg)
ZEND_END_ARG_INFO()

const zend_function_entry drizzle_functions[] = {
    PHP_FE(drizzle_create, NULL)
    PHP_FE(drizzle_quit, NULL)
    PHP_FE(drizzle_binlog_init, arginfo_drizzle_binlog_init)
    PHP_FE(drizzle_binlog_start, NULL)
    PHP_FE(drizzle_binlog_event_type, NULL)
    PHP_FE(drizzle_binlog_event_server_id, NULL)
    PHP_FE(drizzle_binlog_event_length, NULL)
    PHP_FE(drizzle_binlog_event_timestamp, NULL)
    PHP_FE(drizzle_binlog_event_data, NULL)
    PHP_FE_END
};
/* }}} */

/* {{{ drizzle_module_entry
 */
zend_module_entry drizzle_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"drizzle",
	drizzle_functions,
	PHP_MINIT(drizzle),
	PHP_MSHUTDOWN(drizzle),
	PHP_RINIT(drizzle),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(drizzle),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(drizzle),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_DRIZZLE
ZEND_GET_MODULE(drizzle)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("drizzle.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_drizzle_globals, drizzle_globals)
    STD_PHP_INI_ENTRY("drizzle.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_drizzle_globals, drizzle_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_drizzle_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_drizzle_init_globals(zend_drizzle_globals *drizzle_globals)
{
	drizzle_globals->global_value = 0;
	drizzle_globals->global_string = NULL;
}
*/
/* }}} */
static void _drizzle_quit(zend_rsrc_list_entry* rsrc TSRMLS_DC) {
    php_drizzle* drizzle = (php_drizzle*) rsrc->ptr;
    //drizzle_quit(drizzle->conn);
    efree(drizzle);
}

static void _drizzle_binlog_free(zend_rsrc_list_entry* rsrc TSRMLS_DC) {
    php_drizzle_binlog* binlog = (php_drizzle_binlog*) rsrc->ptr;
    drizzle_binlog_free(binlog->binlog);
    efree(binlog->callback);
    efree(binlog);
}

static void _drizzle_binlog_event_free(zend_rsrc_list_entry* rsrc TSRMLS_DC) {
    php_drizzle_binlog_event* event = (php_drizzle_binlog_event*) rsrc->ptr;
    efree(event);
}
/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(drizzle)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
    le_drizzle = zend_register_list_destructors_ex(_drizzle_quit, NULL, "drizzle", module_number);
    le_drizzle_binlog = zend_register_list_destructors_ex(_drizzle_binlog_free, NULL, "drizzle_binlog", module_number);
    le_drizzle_binlog_event = zend_register_list_destructors_ex(_drizzle_binlog_event_free, NULL, "drizzle_binlog_event", module_number);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(drizzle)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(drizzle)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(drizzle)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(drizzle)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "drizzle support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_drizzle_compiled(string arg)
   Return a string to confirm that the module is compiled in */

PHP_FUNCTION(drizzle_create) {
    const char* host;
    const char* user;
    const char* password;
    const char* db;
    int host_len, user_len, password_len, db_len;
    long port;
    php_drizzle* drizzle;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slsss", &host, &host_len, &port, &user, &user_len, &password, &password_len, &db, &db_len)) {
        return;
    }
    drizzle = (php_drizzle*) emalloc(sizeof(php_drizzle));
    drizzle->conn = drizzle_create(host, port, user, password, db, NULL);
    ZEND_REGISTER_RESOURCE(return_value, drizzle, le_drizzle);
}

PHP_FUNCTION(drizzle_quit) {
    zval* zdrizzle;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zdrizzle) == FAILURE) {
        return;
    }
    php_drizzle* drizzle;
    ZEND_FETCH_RESOURCE(drizzle, php_drizzle*, &zdrizzle, -1, "drizzle", le_drizzle);
    long rc = drizzle_quit(drizzle->conn);
    RETURN_LONG(rc);
}

static void _php_drizzle_binlog_event_callback(drizzle_binlog_event_st* event, void* arg) {
    zval* args[2];
    zval retval;
    php_drizzle_binlog_event* ev;
    php_drizzle_binlog_callback* callback = (php_drizzle_binlog_callback*) arg;
    ev = (php_drizzle_binlog_event*) emalloc(sizeof(php_drizzle_binlog_event));
    ev->event = event;
    
    MAKE_STD_ZVAL(args[0]);
    ZEND_REGISTER_RESOURCE(args[0], ev, le_drizzle_binlog_event);

    args[1] = callback->arg;
    Z_ADDREF_P(callback->arg);

    if (call_user_function(EG(function_table), NULL, callback->event_func, &retval, 2, args TSRMLS_CC) == SUCCESS) {
        zval_dtor(&retval);
    }

	zval_ptr_dtor(&(args[0]));
	zval_ptr_dtor(&(args[0]));
}

static void _php_drizzle_binlog_error_callback(drizzle_return_t ret, drizzle_st* conn, void* arg) {
    zval* args[2];
    zval retval;

    php_drizzle_binlog_callback* callback = (php_drizzle_binlog_callback*) arg;
    
    MAKE_STD_ZVAL(args[0]);
    ZVAL_LONG(args[0], ret);

    args[1] = callback->arg;
    Z_ADDREF_P(callback->arg);

    if (call_user_function(EG(function_table), NULL, callback->error_func, &retval, 2, args TSRMLS_CC) == SUCCESS) {
        zval_dtor(&retval);
    }

	zval_ptr_dtor(&(args[0]));
	zval_ptr_dtor(&(args[0]));
}

PHP_FUNCTION(drizzle_binlog_init) {
    zval* zdrizzle;
    zval* zevent_callback;
    zval* zerror_callback;
    zval* zarg = NULL;
    php_drizzle* drizzle;
    php_drizzle_binlog* binlog;
    char* event_func_name;
    char* error_func_name;
    php_drizzle_binlog_callback* callback;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rzz|z", &zdrizzle, &zevent_callback, &zerror_callback, &zarg) == FAILURE) {
        return;
    }

    if (!zend_is_callable(zevent_callback, 0, &event_func_name TSRMLS_CC)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "'%s' is not a valid callback", event_func_name);
        efree(event_func_name);
        RETURN_FALSE;
    }
    if (!zend_is_callable(zerror_callback, 0, &error_func_name TSRMLS_CC)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "'%s' is not a valid callback", error_func_name);
        efree(error_func_name);
        RETURN_FALSE;
    }
    efree(event_func_name);
    efree(error_func_name);

    zval_add_ref(&zevent_callback);
    zval_add_ref(&zerror_callback);

    if (zarg) {
        zval_add_ref(&zarg);
    } else {
        ALLOC_INIT_ZVAL(zarg);
    }

    ZEND_FETCH_RESOURCE(drizzle, php_drizzle*, &zdrizzle, -1, "drizzle", le_drizzle);

    callback = (php_drizzle_binlog_callback*) emalloc(sizeof(php_drizzle_binlog_callback));
    callback->event_func = zevent_callback;
    callback->error_func = zerror_callback;
    callback->arg = zarg;

    binlog = (php_drizzle_binlog*) emalloc(sizeof(php_drizzle_binlog));
    binlog->binlog = drizzle_binlog_init(drizzle->conn, _php_drizzle_binlog_event_callback, _php_drizzle_binlog_error_callback, callback, true);
    binlog->callback = callback;

    ZEND_REGISTER_RESOURCE(return_value, binlog, le_drizzle_binlog);
}

PHP_FUNCTION(drizzle_binlog_start) {
    zval* zbinlog;
    long server_id;
    const char* log_file;
    int log_file_len;
    long log_pos;
    php_drizzle_binlog* binlog;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlsl", &zbinlog, &server_id, &log_file, &log_file_len, &log_pos) == FAILURE) {
        return;
    }
    ZEND_FETCH_RESOURCE(binlog, php_drizzle_binlog*, &zbinlog, -1, "drizzle_binlog", le_drizzle_binlog);
    long rc = drizzle_binlog_start(binlog->binlog, server_id, log_file, log_pos);
    RETURN_LONG(rc);
}

PHP_FUNCTION(drizzle_binlog_free) {
    zval* zbinlog;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zbinlog) == FAILURE) {
        return;
    }
    zend_list_delete(Z_RESVAL_P(zbinlog));
    RETURN_TRUE;
}

#define DRIZZLE_FETCH_BINLOG_EVENT() \
zval* zbinlog_event;\
php_drizzle_binlog_event* binlog_event;\
if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zbinlog_event) == FAILURE) {\
    return;\
}\
ZEND_FETCH_RESOURCE(binlog_event, php_drizzle_binlog_event*, &zbinlog_event, -1, "drizzle_binlog_event", le_drizzle_binlog_event);

PHP_FUNCTION(drizzle_binlog_event_type) {
    DRIZZLE_FETCH_BINLOG_EVENT();
    long event_type = drizzle_binlog_event_type(binlog_event->event);
    RETURN_LONG(event_type);
}

PHP_FUNCTION(drizzle_binlog_event_server_id) {
    DRIZZLE_FETCH_BINLOG_EVENT();
    long server_id = drizzle_binlog_event_server_id(binlog_event->event);
    RETURN_LONG(server_id);
}

PHP_FUNCTION(drizzle_binlog_event_length) {
    DRIZZLE_FETCH_BINLOG_EVENT();
    long event_length = drizzle_binlog_event_length(binlog_event->event);
    RETURN_LONG(event_length);
}

PHP_FUNCTION(drizzle_binlog_event_timestamp) {
    DRIZZLE_FETCH_BINLOG_EVENT();
    long timestamp = drizzle_binlog_event_timestamp(binlog_event->event);
    RETURN_LONG(timestamp);
}

PHP_FUNCTION(drizzle_binlog_event_data) {
    DRIZZLE_FETCH_BINLOG_EVENT();
    const char* data = (char*) drizzle_binlog_event_data(binlog_event->event);
    long event_length = drizzle_binlog_event_length(binlog_event->event);
    RETURN_STRINGL(data, event_length, 1);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
