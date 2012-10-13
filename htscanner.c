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
  | Authors: Bart Vanbrabant <bart dot vanbrabant at zoeloelip dot be>   |
  |          Pierre-Alain Joye <pierre@php.net>                          |
  +----------------------------------------------------------------------+
*/

/* $Id: htscanner.c 312503 2011-06-26 17:56:58Z derick $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "SAPI.h"
#include "php_htscanner.h"
#include "ext/standard/file.h"
#include "ext/standard/php_string.h"

#if !defined(HAVE_STRTOK_R) || !defined(strtok_r)
# define strtok_r php_strtok_r
#endif

ZEND_DECLARE_MODULE_GLOBALS(htscanner)

int (*php_cgi_sapi_activate)(TSRMLS_D);

#define FILE_BUFFER				1024
#define FILE_SEPARATOR			" \t"

#define STRING(x)				#x
#define MOD_PHP(x)				"mod_php" STRING(x) ".c"

#define TOKEN(tok, sep) { \
	if ((tok = strtok_r(NULL, sep "\r\n", &bufp)) == NULL) \
		continue; \
}

#define RETURN_FAILURE(msg) { \
	if (HTG(stop_on_error) > 0) { \
		if (msg) { \
			zend_error(E_WARNING, "%s", msg); \
		} \
		return FAILURE; \
	} else { \
		return SUCCESS; \
	} \
}

#if HTSCANNER_DEBUG
static void htscanner_debug(char *format, ...) /* {{{ */
{
	va_list args;

	fprintf(stderr, "htscanner: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	putc('\n', stderr);
	fflush(stderr);
}
/* }}} */
#else
# define htscanner_debug(a,...)
#endif

/* {{{ value_hnd
 * Parse an option and try to set the option
 */
static int value_hnd(char *name, char *value, int flag, int mode, HashTable *ini_entries TSRMLS_DC)
{
	size_t name_len, value_len;

	name_len = strlen(name);
	value_len = strlen(value);

	if (flag) {
		/* it's a flag */
		if (!strcasecmp(value, "on") || (value[0] == '1' && value[1] == '\0')) {
			value = "1";
		} else {
			value = "0";
		}
		value_len = 1;
	} else {
		/* it's a value */
		if (!strcasecmp(value, "none")) {
			value = "";
			value_len = 0;
		}
	}
	htscanner_debug("setting: %s = %s (%s)", name, value, flag ? "flag" : "value");

	/* safe_mode & basedir check */
#if PHP_VERSION_ID >= 50399
	if (mode != PHP_INI_SYSTEM && PG(open_basedir)) {
#else
	if (mode != PHP_INI_SYSTEM && (PG(safe_mode) || PG(open_basedir))) {
#endif
		if (!strcmp("error_log", name) ||
				!strcmp("java.class.path", name) ||
				!strcmp("java.home", name) ||
				!strcmp("java.library.path", name) ||
				!strcmp("mail.log", name) ||
				!strcmp("session.save_path", name) ||
				!strcmp("vpopmail.directory", name)) {
#if PHP_VERSION_ID < 50399
			if (PG(safe_mode) && !php_checkuid(value, NULL, CHECKUID_CHECK_FILE_AND_DIR)) {
				htscanner_debug("php_checkuid failed!");
				return FAILURE;
			}
#endif
			if (php_check_open_basedir(value TSRMLS_CC)) {
				htscanner_debug("php_check_open_basedir failed!");
				return FAILURE;
			}
		}
	}

#if PHP_VERSION_ID < 50399
	/* checks that ensure the user does not overwrite certain ini settings when safe_mode is enabled */
	if (mode != PHP_INI_SYSTEM && PG(safe_mode)) {
		if (!strcmp("max_execution_time", name) ||
				!strcmp("memory_limit", name) ||
				!strcmp("child_terminate", name) ||
				!strcmp("open_basedir", name) ||
				!strcmp("safe_mode", name)) {
			htscanner_debug("safe_mode failed!");
			return FAILURE;
		}
	}
#endif
#if PHP_VERSION_ID < 50204
	if (zend_alter_ini_entry(name, name_len + 1, value, value_len, mode, PHP_INI_STAGE_RUNTIME) == FAILURE) {
#else
	if (zend_alter_ini_entry(name, name_len + 1, value, value_len, mode, PHP_INI_STAGE_HTACCESS) == FAILURE) {
#endif
		htscanner_debug("zend_alter_ini_entry failed!");
		if (HTG(verbose)) {
			zend_error(E_WARNING, "Adding option (Name: '%s' Value: '%s') (%lu, %lu) failed!\n", name, value, name_len, value_len);
		}
		return FAILURE;
	}

	if (ini_entries)
		zend_hash_update(ini_entries, name, name_len + 1, value, value_len + 1, NULL);

	return SUCCESS;
}
/* }}} */

/* {{{ parse_config_file
 * Parse the configuration file
 */
static void parse_config_file(char *file, HashTable *ini_entries TSRMLS_DC)
{
	php_stream *stream;
#if PHP_VERSION_ID < 50399
	/* see main/safemode.c:70
	 * for whatever reasons, it is not possible to call this function
	 * without getting an error if the file does not exist, not
	 * when safemode is On.
	 * if one knows why, please let me know.
	 * pierre@php.net
	 */
	if (PG(safe_mode)) {
		struct stat sb;
		if (VCWD_STAT(file, &sb) != 0) {
			return;
		}
	}
#endif

	stream = php_stream_open_wrapper(file, "rb", IGNORE_URL | ENFORCE_SAFE_MODE, NULL);
	if (stream != NULL) {
		char buf[FILE_BUFFER], *bufp, *tok, *value;
		int flag, parse = 1;

		while ((bufp = php_stream_gets(stream, buf, FILE_BUFFER)) != NULL) {
			TOKEN(tok, FILE_SEPARATOR);
			if (!strcasecmp(tok, "<IfModule")) {
				TOKEN(tok, FILE_SEPARATOR);
				parse = !strcmp(tok, MOD_PHP(PHP_MAJOR_VERSION) ">");
			} else if (!strcasecmp(tok, "</IfModule>")) {
				parse = 1;
			} else if (parse && ((flag = !strcasecmp(tok, "php_flag")) || !strcasecmp(tok, "php_value"))) {
				TOKEN(tok, FILE_SEPARATOR);
				if (bufp == NULL)
					continue;

				bufp += strspn(bufp, FILE_SEPARATOR);
				if (*bufp == '\'') {
					TOKEN(value, "'");
				} else if (*bufp == '"') {
					TOKEN(value, "\"");
				} else {
					TOKEN(value, FILE_SEPARATOR);
				}

				value_hnd(tok, value, flag, PHP_INI_PERDIR, ini_entries TSRMLS_CC);
			}
		}
		php_stream_close(stream);
	}
}
/* }}} */

/* True global resources - no need for thread safety here */

/* {{{ htscanner_functions[]
 *
 */
zend_function_entry htscanner_functions[] = {
	{NULL, NULL, NULL}	/* Must be the last line in htscanner_functions[] */
};
/* }}} */

/* {{{ htscanner_module_entry
 */
zend_module_entry htscanner_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"htscanner",
	htscanner_functions,
	PHP_MINIT(htscanner),
	PHP_MSHUTDOWN(htscanner),
	NULL,
	NULL,
	PHP_MINFO(htscanner),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_HTSCANNER_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HTSCANNER
ZEND_GET_MODULE(htscanner)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("htscanner.config_file", ".htaccess", PHP_INI_SYSTEM, OnUpdateString, config_file, zend_htscanner_globals, htscanner_globals)
	STD_PHP_INI_ENTRY("htscanner.default_docroot", "/", PHP_INI_SYSTEM, OnUpdateString, default_docroot, zend_htscanner_globals, htscanner_globals)
#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION > 0)
	STD_PHP_INI_ENTRY("htscanner.default_ttl", "300", PHP_INI_SYSTEM, OnUpdateLong, default_ttl, zend_htscanner_globals, htscanner_globals)
	STD_PHP_INI_ENTRY("htscanner.stop_on_error", "0", PHP_INI_SYSTEM, OnUpdateLong, stop_on_error, zend_htscanner_globals, htscanner_globals)
	STD_PHP_INI_ENTRY("htscanner.verbose", "0", PHP_INI_SYSTEM, OnUpdateLong, verbose, zend_htscanner_globals, htscanner_globals)
#else
	STD_PHP_INI_ENTRY("htscanner.default_ttl", "300", PHP_INI_SYSTEM, OnUpdateInt, default_ttl, zend_htscanner_globals, htscanner_globals)
	STD_PHP_INI_ENTRY("htscanner.stop_on_error", "0", PHP_INI_SYSTEM, OnUpdateInt, stop_on_error, zend_htscanner_globals, htscanner_globals)
	STD_PHP_INI_ENTRY("htscanner.verbose", "0", PHP_INI_SYSTEM, OnUpdateInt, verbose, zend_htscanner_globals, htscanner_globals)
#endif
PHP_INI_END()
/* }}} */

htscannerMutexDeclare(ini_entries_cache_mutex);
static HashTable *ini_entries_cache = NULL;

static void ini_cache_entry_dtor(htscanner_cache_entry *entry) /* {{{ */
{
	zend_hash_destroy(entry->ini_entries);
	free(entry->ini_entries);
}
/* }}} */

static int php_htscanner_create_cache() /* {{{ */
{
	htscannerMutexSetup(ini_entries_cache_mutex);

	ini_entries_cache = malloc(sizeof(HashTable));
	if (!ini_entries_cache) {
		return FAILURE;
	}

	zend_hash_init(ini_entries_cache, 0, NULL, (dtor_func_t) ini_cache_entry_dtor, 1);
	return SUCCESS;
}
/* }}} */

static void php_htscanner_init_globals(zend_htscanner_globals *htscanner_globals) /* {{{ */
{
	htscanner_globals->config_file = NULL;
	htscanner_globals->default_docroot = NULL;
	htscanner_globals->default_ttl = 5*60;
	htscanner_globals->stop_on_error = 0;
	htscanner_globals->verbose = 0;
}
/* }}} */

static int htscanner_main(TSRMLS_D) /* {{{ */
{
	char *doc_root;
	char cwd[MAXPATHLEN + 1];
	size_t cwd_len, doc_root_len;
	htscanner_cache_entry entry, *entry_fetched;
	time_t t;
	HashTable *ini_entries = NULL;

	if (!sapi_module.getenv || !(doc_root = sapi_module.getenv("DOCUMENT_ROOT", sizeof("DOCUMENT_ROOT")-1 TSRMLS_CC))) {
		doc_root = HTG(default_docroot);
	}

	doc_root_len = strlen(doc_root);

	/*
	 * VCWD_GETCWD cannot be used in rinit, cwd is not yet
	 * initialized. Get it from path_translated.
	 */
	if (!SG(request_info).path_translated) {
		RETURN_FAILURE("No path translated, cannot determine the current script\n")
	}

	strncpy(cwd, SG(request_info).path_translated, sizeof(cwd) - 2);
	cwd[sizeof(cwd) - 2] = '\0';

	php_dirname(cwd, strlen(cwd));
	cwd_len = strlen(cwd);
	cwd[cwd_len++] = PHP_DIR_SEPARATOR;
	cwd[cwd_len] = '\0';

	if (!ini_entries_cache && php_htscanner_create_cache() != SUCCESS) {
		RETURN_FAILURE("Cannot create the cache\n");
	}

#if PHP_API_VERSION <= 20041225
	t = time(0);
#else
	t = sapi_get_request_time(TSRMLS_C);
#endif

	htscannerMutexLock(ini_entries_cache_mutex);
	if (zend_hash_find(ini_entries_cache, cwd, cwd_len, (void**)&entry_fetched) == SUCCESS) {
		/* fetch cache and assign */
		if ((unsigned int)(t - entry_fetched->created_on) < HTG(default_ttl)) {
			char *value, *name;
			HashPosition pos;
			uint len;
			ulong num;

			zend_hash_internal_pointer_reset_ex(entry_fetched->ini_entries, &pos);

			while (SUCCESS == zend_hash_get_current_data_ex(entry_fetched->ini_entries, (void**)&value, &pos)) {
				zend_hash_get_current_key_ex(entry_fetched->ini_entries, &name, &len, &num, 0, &pos);
				htscanner_debug("setting: %s = %s (cache hit)", name, value);
#if PHP_VERSION_ID < 50204
				if (zend_alter_ini_entry(name, len, value, strlen(value), PHP_INI_PERDIR, PHP_INI_STAGE_PHP_INI_STAGE_RUNTIME) == FAILURE) {
#else
				if (zend_alter_ini_entry(name, len, value, strlen(value), PHP_INI_PERDIR, PHP_INI_STAGE_HTACCESS) == FAILURE) {
#endif
					char msg[1024];
					snprintf(msg, sizeof (msg), "Adding option from cache (Name: '%s' Value: '%s') failed!\n", name, value);
					htscannerMutexUnlock(ini_entries_cache_mutex);
					RETURN_FAILURE(msg);
				}
				zend_hash_move_forward_ex(entry_fetched->ini_entries, &pos);
			}
			htscannerMutexUnlock(ini_entries_cache_mutex);
			return SUCCESS;
		}
	}

	/* parse, insert, assign */
	if (HTG(default_ttl)) {
		entry.created_on = t;
		entry.ini_entries = ini_entries = malloc(sizeof(HashTable));
		if (ini_entries)
			zend_hash_init(ini_entries, 0, NULL, NULL, 1);
	}

	if (doc_root != NULL) {
		size_t i = 0;

		/* Check whether cwd is a subset of doc_root. */
		if (!strncmp(doc_root, cwd, doc_root_len))
			i = doc_root_len - 1;

		for (; i < cwd_len; i++) {
			if (cwd[i] == PHP_DIR_SEPARATOR) {
				char file[MAXPATHLEN + 1];

				strncpy(file, cwd, i + 1);
				file[i + 1] = '\0';
				strncat(file, HTG(config_file), sizeof(file) - 1 - strlen(file));

				parse_config_file(file, ini_entries TSRMLS_CC);
			}
		}
	}

	if (ini_entries)
		zend_hash_update(ini_entries_cache, cwd, cwd_len, &entry, sizeof(htscanner_cache_entry), NULL);
	htscannerMutexUnlock(ini_entries_cache_mutex);

	return SUCCESS;
}
/* }}} */

/* {{{ sapi_cgi_activate
 * MINIT_FUNCTION replacement in order to modify certain ini entries
 */
static int sapi_cgi_activate(TSRMLS_D)
{
	/* should we call the origin function before or after our stuff?
	 * doesn't really matter right now, as it's null
	 */
	if (php_cgi_sapi_activate) {
		php_cgi_sapi_activate(TSRMLS_C);
	}

	htscanner_main(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

static PHP_MINIT_FUNCTION(htscanner) /* {{{ */
{
	ZEND_INIT_MODULE_GLOBALS(htscanner, php_htscanner_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	/* make sapi cgi call us
	 * this is necessary in order to modify certain
	 * ini entries (register_globals, output_buffering, etc..)
	 */
	php_cgi_sapi_activate = sapi_module.activate;
	sapi_module.activate = sapi_cgi_activate;
	return SUCCESS;
}
/* }}} */

static PHP_MSHUTDOWN_FUNCTION(htscanner) /* {{{ */
{
	if (ini_entries_cache) {
		htscannerMutexLock(ini_entries_cache_mutex);
		zend_hash_destroy(ini_entries_cache);
		free(ini_entries_cache);
		htscannerMutexShutdown(ini_entries_cache_mutex);
	}

	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

PHP_MINFO_FUNCTION(htscanner) /* {{{ */
{
	php_info_print_table_start();
	php_info_print_table_header(2, "htscanner support", "enabled");
	php_info_print_table_row(2, "PECL Module version", PHP_HTSCANNER_VERSION " ($Id: htscanner.c 312503 2011-06-26 17:56:58Z derick $)");
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
