#include "httpd.h"
#include "http_config.h"
#include <stdio.h>

module MODULE_VAR_EXPORT htscanner_module;

static const char *dummy(cmd_parms *params, void *config, char *name, char *value)
{
    return (NULL);
}

static command_rec htscanner_cmds[] =
{
    { "php_flag", dummy, NULL, OR_OPTIONS, TAKE2, "PHP Flag Modifier"},
    { "php_value", dummy, NULL, OR_OPTIONS, TAKE2, "PHP Value Modifier"},
    { NULL }
};

module MODULE_VAR_EXPORT htscanner_module =
{
    STANDARD_MODULE_STUFF,
    NULL,		/* initializer */
    NULL,		/* dir config creator */
    NULL,		/* dir config merger */
    NULL,		/* server config creator */
    NULL,		/* server config merger */
    htscanner_cmds,	/* command table */
    NULL,		/* handlers */
    NULL,		/* filename translation */
    NULL,		/* check_user_id */
    NULL,		/* check auth */
    NULL,		/* check access */
    NULL,		/* type_checker */
    NULL,		/* fixups */
    NULL,		/* logger */
    NULL,		/* header parser */
    NULL,		/* child_init */
    NULL,		/* child_exit */
    NULL		/* post read-request */
};
