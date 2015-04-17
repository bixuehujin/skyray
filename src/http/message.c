/*
 * message.c
 *
 *  Created on: Apr 15, 2015
 *      Author: hujin
 */

#include "http.h"

zend_class_entry *skyray_ce_HttpMessage;
zend_object_handlers skyray_handler_HttpMessage;

static inline skyray_http_message_t *skyray_http_message_from_obj(zend_object *obj) {
    return (skyray_http_message_t*)((char*)(obj) - XtOffsetOf(skyray_http_message_t, std));
}

zend_object * skyray_http_message_object_new(zend_class_entry *ce)
{
    skyray_http_message_t *intern;
    intern = ecalloc(1, sizeof(skyray_http_message_t) + zend_object_properties_size(ce));

    intern->version = -1;

    zend_hash_init(&intern->headers, 32, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(&intern->iheaders, 32, NULL, ZVAL_PTR_DTOR, 0);

    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);

    intern->std.handlers = &skyray_handler_HttpMessage;
    return &intern->std;
}

void skyray_http_message_object_free(zend_object *object)
{
    skyray_http_message_t *intern = skyray_http_message_from_obj(object);

    zend_hash_destroy(&intern->headers);
    zend_hash_destroy(&intern->iheaders);
    zend_object_std_dtor(&intern->std);
}

static inline void remove_old_header(zend_array *ht, zend_string *name)
{
    zend_string *key;
    zend_hash_internal_pointer_reset(ht);
    while (zend_hash_has_more_elements(ht) == SUCCESS) {
        zend_hash_get_current_key(ht, &key, NULL);

        if (zend_string_equals_ci(key, name)) {
            zend_hash_del(ht, key);
        }
        zend_hash_move_forward(ht);
    }
}

zend_bool skyray_http_message_has_header(skyray_http_message_t *self, zend_string *name)
{
    zend_bool exists;

    zend_string *lname = zend_string_tolower(name);
    exists = zend_hash_exists(&self->iheaders, lname);
    zend_string_release(lname);

    return exists;
}

zval* skyray_http_message_get_header(skyray_http_message_t *self, zend_string *name)
{
    zval *found;
    zend_string *lname = zend_string_tolower(name);

    found = zend_hash_find(&self->iheaders, lname);
    zend_string_release(lname);

    return found;
}


void skyray_http_message_set_header(skyray_http_message_t *self, zend_string *name, zend_string *value)
{
    zval arr;
    zend_string *iname = zend_string_tolower(name);

    array_init(&arr);
    zend_string_addref(value);
    add_next_index_str(&arr, value);

    zend_hash_update(&self->iheaders, iname, &arr);
    zend_string_release(iname);

    zval_add_ref(&arr);

    remove_old_header(&self->headers, name);
    zend_hash_update(&self->headers, name, &arr);
}

void skyray_http_message_add_header(skyray_http_message_t *self, zend_string *name, zend_string *value)
{
    zval *found;
    zend_string *lname = zend_string_tolower(name);

    found = zend_hash_find(&self->iheaders, lname);
    if (found) {
        zend_string_addref(value);
        remove_old_header(&self->headers, name);
        add_next_index_str(found, value);
        zval_add_ref(found);
        zend_hash_update(&self->headers, name, found);

    } else {
        skyray_http_message_set_header(self, name, value);
    }

    zend_string_release(lname);
}

void skyray_http_message_remove_header(skyray_http_message_t *self, zend_string *name)
{
    zend_string *lname = zend_string_tolower(name);
    zend_hash_del(&self->iheaders, lname);

    remove_old_header(&self->headers, lname);

    zend_string_release(lname);
}

SKYRAY_METHOD(HttpMessage, getProtocolVersion)
{
    if (zend_parse_parameters_none() ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));

    if (intern->version >= SR_HTTP_VERSION_10 && intern->version <= SR_HTTP_VERSION_20) {
        RETURN_STRING(http_versions[intern->version]);
    }
}

SKYRAY_METHOD(HttpMessage, setProtocolVersion)
{
    zend_string *version;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &version) ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));

    for (i = 0 ; i <= SR_HTTP_VERSION_20 ; i ++) {
        if (strncmp(http_versions[i], version->val, 3) == 0) {
            intern->version = i;
            break;
        }
    }

    RETURN_ZVAL(getThis(), 1, 0);
}


SKYRAY_METHOD(HttpMessage, getHeaders)
{
    skyray_http_message_t *intern;
    zval zheaders;;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));

    ZVAL_ARR(&zheaders, &intern->headers);
    RETURN_ZVAL(&zheaders, 1, 0);
}

SKYRAY_METHOD(HttpMessage, hasHeader)
{
    zend_string *name;
    skyray_http_message_t *intern;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) ==  FAILURE) {
        return;
    }

    intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    RETURN_BOOL(skyray_http_message_has_header(intern, name));
}

SKYRAY_METHOD(HttpMessage, getHeader)
{
    zend_string *name;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    zval *header = skyray_http_message_get_header(intern, name);
    if (!header) {
        zval empty_arr;
        array_init(&empty_arr);
        RETURN_ARR(Z_ARR(empty_arr));
    }
    RETURN_ZVAL(header, 1, 0);
}

SKYRAY_METHOD(HttpMessage, setHeader)
{
    zend_string *name, *value;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &name, &value) ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    skyray_http_message_set_header(intern, name, value);

    RETURN_ZVAL(getThis(), 1, 0);
}

SKYRAY_METHOD(HttpMessage, addHeader)
{
    zend_string *name, *value;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &name, &value) ==  FAILURE) {
        return;
    }
    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));

    skyray_http_message_add_header(intern, name, value);

    RETURN_ZVAL(getThis(), 1, 0);
}

SKYRAY_METHOD(HttpMessage, removeHeader)
{
    zend_string *name;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    skyray_http_message_remove_header(intern, name);

    RETURN_ZVAL(getThis(), 1, 0);
}

SKYRAY_METHOD(HttpMessage, getBody)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    RETURN_ZVAL(&intern->body, 1, 0);
}

SKYRAY_METHOD(HttpMessage, setBody)
{
    zend_string *body;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &body) ==  FAILURE) {
        return;
    }

    skyray_http_message_t *intern = skyray_http_message_from_obj(Z_OBJ_P(getThis()));
    zval_dtor(&intern->body);
    ZVAL_STR(&intern->body, body);
    RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_setProtocolVersion, 0, 0, 1)
    ZEND_ARG_INFO(0, version)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_name, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_name_value, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_with_body, 0, 0, 1)
    ZEND_ARG_INFO(0, body)
ZEND_END_ARG_INFO()

static const zend_function_entry class_methods[] = {
    SKYRAY_ME(HttpMessage, getProtocolVersion, arginfo_empty, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, setProtocolVersion, arginfo_setProtocolVersion, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, getHeaders, arginfo_empty, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, hasHeader, arginfo_name, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, getHeader, arginfo_name, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, setHeader, arginfo_name_value, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, addHeader, arginfo_name_value, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, removeHeader, arginfo_name, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, getBody, arginfo_empty, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpMessage, setBody, arginfo_with_body, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(skyray_http_message)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "skyray\\http\\Message", class_methods);
    skyray_ce_HttpMessage = zend_register_internal_class(&ce);
    skyray_ce_HttpMessage->create_object = skyray_http_message_object_new;

    memcpy(&skyray_handler_HttpMessage, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    skyray_handler_HttpMessage.free_obj = skyray_http_message_object_free;

    return SUCCESS;
}