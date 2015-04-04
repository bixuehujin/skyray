#include "skyray.h"

#include "core/reactor.h"
#include "core/stream.h"
#include "processing/process.h"

zend_class_entry * skyray_ce_SkyrayException;
zend_class_entry * skyray_ce_ProtocolInterface;

PHP_MINIT_FUNCTION(skyray_interfaces)
{
    zend_class_entry ce;

    ZEND_BEGIN_ARG_INFO_EX(arginfo_dataReceived, 0, 0, 1)
        ZEND_ARG_INFO(0, data)
    ZEND_END_ARG_INFO()

    ZEND_BEGIN_ARG_INFO_EX(arginfo_connectStream, 0, 0, 1)
        ZEND_ARG_INFO(0, stream)
    ZEND_END_ARG_INFO()

    static const zend_function_entry class_methods[] = {
        SKYRAY_ABSTRACT_ME(ProtocolInterface, connectStream, arginfo_connectStream)
        SKYRAY_ABSTRACT_ME(ProtocolInterface, streamConnected, arginfo_empty)
        SKYRAY_ABSTRACT_ME(ProtocolInterface, dataReceived, arginfo_dataReceived)
        SKYRAY_ABSTRACT_ME(ProtocolInterface, streamClosed, arginfo_empty)
        PHP_FE_END
    };


    INIT_CLASS_ENTRY(ce, "skyray\\core\\ProtocolInterface", class_methods);
    skyray_ce_ProtocolInterface = zend_register_internal_interface(&ce);

    return SUCCESS;
}

PHP_MINIT_FUNCTION(skyray_exceptions)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "skyray\\core\\SkyrayException", NULL);
    skyray_ce_SkyrayException = zend_register_internal_class_ex(&ce, zend_exception_get_default());

    return SUCCESS;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(skyray)
{
    PHP_MINIT(skyray_exceptions)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(skyray_interfaces)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(skyray_reactor)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(skyray_timer)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(stream_client)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(stream)(INIT_FUNC_ARGS_PASSTHRU);
    PHP_MINIT(process)(INIT_FUNC_ARGS_PASSTHRU);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(skyray)
{

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(skyray)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "skyray support", "enabled");
    php_info_print_table_end();
}
/* }}} */


ZEND_BEGIN_ARG_INFO_EX(arginfo_hash_1, 0, 0, 2)
    ZEND_ARG_INFO(0, algo)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, raw_output)
ZEND_END_ARG_INFO()


/* {{{ hash_module_entry
 */
zend_module_entry skyray_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "skyray",
    NULL,
    PHP_MINIT(skyray),
    PHP_MSHUTDOWN(skyray),
    NULL, /* RINIT */
    NULL, /* RSHUTDOWN */
    PHP_MINFO(skyray),
#if ZEND_MODULE_API_NO >= 20010901
    "0.0.1", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */


#ifdef COMPILE_DL_SKYRAY
ZEND_GET_MODULE(skyray)
#endif