/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Bart Vanbrabant <bart dot vanbrabant at zoeloelip dot be>    |
  +----------------------------------------------------------------------+
*/

/* $Id: php_htscanner.h 323705 2012-03-01 15:07:29Z martynas $ */

#ifndef PHP_HTSCANNER_H
#define PHP_HTSCANNER_H

extern zend_module_entry htscanner_module_entry;
#define phpext_htscanner_ptr &htscanner_module_entry

#ifdef PHP_WIN32
#define PHP_HTSCANNER_API __declspec(dllexport)
#else
#define PHP_HTSCANNER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define PHP_HTSCANNER_VERSION "1.0.1"

static PHP_MINIT_FUNCTION(htscanner);
static PHP_MSHUTDOWN_FUNCTION(htscanner);
static PHP_MINFO_FUNCTION(htscanner);

typedef struct _ze_htscanner_cache_entry {
	time_t created_on;
	HashTable *ini_entries;
} htscanner_cache_entry;

ZEND_BEGIN_MODULE_GLOBALS(htscanner)
	char *config_file;
	char *default_docroot;
	unsigned long default_ttl;
	int stop_on_error;
	int verbose;
ZEND_END_MODULE_GLOBALS(htscanner)

#ifdef ZTS
#define HTG(v) TSRMG(htscanner_globals_id, zend_htscanner_globals *, v)
#define htscannerMutexDeclare(x) MUTEX_T x
#define htscannerMutexSetup(x) x = tsrm_mutex_alloc()
#define htscannerMutexShutdown(x) tsrm_mutex_free(x)
#define htscannerMutexLock(x) tsrm_mutex_lock(x)
#define htscannerMutexUnlock(x) tsrm_mutex_unlock(x)
#else
#define HTG(v) (htscanner_globals.v)
#define htscannerMutexDeclare(x)
#define htscannerMutexSetup(x)
#define htscannerMutexShutdown(x)
#define htscannerMutexLock(x)
#define htscannerMutexUnlock(x)
#endif

#endif	/* PHP_HTSCANNER_H */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
