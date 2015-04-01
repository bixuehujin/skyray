/*
 * stream.c
 *
 *  Created on: Mar 15, 2015
 *      Author: hujin
 */

#include "stream.h"

zend_class_entry *skyray_ce_Stream;
zend_object_handlers skyray_handler_Stream;

zend_object * skyray_stream_object_new(zend_class_entry *ce)
{
    skyray_stream_t *intern;
    intern = ecalloc(1, sizeof(skyray_stream_t) + zend_object_properties_size(ce));
    intern->fd = 0;
    intern->status = SKYRAY_STREAM_STATUS_OPENING;
    intern->rw_mask = 0;
    ZVAL_NULL(&intern->protocol);

    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);

    intern->std.handlers = &skyray_handler_Stream;
    return &intern->std;
}

void skyray_stream_object_free(zend_object *object)
{
    skyray_stream_t *intern = skyray_stream_from_obj(object);
    zend_object_std_dtor(&intern->std);
}

void skyray_stream_init(skyray_stream_t *self, int fd, zend_object *protocol)
{
    self->fd = fd;
    if (protocol) {
        ZVAL_OBJ(&self->protocol, protocol);
    }
}

int skyray_stream_write(skyray_stream_t *self, zend_string *buffer)
{
    if (self->status == SKYRAY_STREAM_STATUS_CLOSED) {
        skyray_throw_exception("Unable to write data to closed stream.");
        return -1;
    }
    if (!(self->rw_mask & SKYRAY_STREAM_WRITABLE)) {
        skyray_throw_exception("The stream is not writable.");
        return -1;
    }

    int ret = write(self->fd, buffer->val, buffer->len);
    if (ret < 0) {
        skyray_throw_exception_from_errno(errno);
        return -1;
    } else {
        return ret;
    }
}

zend_string * skyray_stream_read(skyray_stream_t * self, zend_bool slient)
{
    if (self->status == SKYRAY_STREAM_STATUS_CLOSED) {
        !slient && skyray_throw_exception("Unable to read data from closed stream.");
        return NULL;
    }

    if (!(self->rw_mask & SKYRAY_STREAM_READABLE)) {
        !slient && skyray_throw_exception("The stream is not readable.");
        return NULL;
    }

    zend_string *buffer = zend_string_alloc(8192, 0);

    int ret = read(self->fd, buffer->val, 8192);
    if (ret < 0) {
        zend_string_free(buffer);
        !slient && skyray_throw_exception_from_errno(errno);
        return NULL;
    }
    buffer->len = ret;
    buffer->val[ret] = '\0';

    if (ret == 0) {
        skyray_stream_on_closed(self);
    }

    return buffer;
}

void skyray_stream_on_data(skyray_stream_t *self, zend_string *buffer)
{
    if (!ZVAL_IS_NULL(&self->protocol)) {
        skyray_protocol_on_data_received(&self->protocol, buffer);
    }
}

void skyray_stream_on_opened(skyray_stream_t *self, int rw_mask)
{
    self->status = SKYRAY_STREAM_STATUS_OPENED;
    self->rw_mask = rw_mask;

    if (!ZVAL_IS_NULL(&self->protocol)) {
        skyray_protocol_on_connect_stream(&self->protocol, &self->std);
        skyray_protocol_on_stream_connected(&self->protocol);
    }
}

void skyray_stream_on_closed(skyray_stream_t *self)
{
    self->status = SKYRAY_STREAM_STATUS_CLOSED;
    self->rw_mask = 0;
    if (!ZVAL_IS_NULL(&self->protocol)) {
        skyray_protocol_on_stream_closed(&self->protocol);
    }
}

zend_bool skyray_stream_close(skyray_stream_t *self)
{
    if (self->status == SKYRAY_STREAM_STATUS_CLOSED) {
        return 1;
    }
    if (close(self->fd) < 0) {
        skyray_throw_exception_from_errno(errno);
        return 0;
    }
    skyray_stream_on_closed(self);

    return 1;
}

SKYRAY_METHOD(stream, __construct)
{

}

SKYRAY_METHOD(stream, write)
{
    zend_string *data = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &data) == FAILURE) {
        return;
    }

    skyray_stream_t * intern = skyray_stream_from_obj(Z_OBJ_P(getThis()));

    int written = skyray_stream_write(intern, data);
    RETURN_BOOL(written != -1);
}

SKYRAY_METHOD(stream, read)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    skyray_stream_t * intern = skyray_stream_from_obj(Z_OBJ_P(getThis()));

    zend_string *buffer = skyray_stream_read(intern, 0);

    if (!buffer) {
        RETURN_NULL();
    }

    RETURN_STR(buffer);
}

SKYRAY_METHOD(stream, close)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    skyray_stream_t * intern = skyray_stream_from_obj(Z_OBJ_P(getThis()));

    skyray_stream_close(intern);

    RETURN_TRUE;
}


ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, 0)
    ZEND_ARG_INFO(0, callable)
    ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_write, 0, 0, 1)
    ZEND_ARG_INFO(0, obj)
ZEND_END_ARG_INFO()


static const zend_function_entry class_methods[] = {
    SKYRAY_ME(stream, __construct, arginfo___construct, ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
    SKYRAY_ME(stream, write, arginfo_write, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SKYRAY_ME(stream, read, arginfo_empty, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    SKYRAY_ME(stream, close, arginfo_empty, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(stream)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "skyray\\core\\Stream", class_methods);
    skyray_ce_Stream = zend_register_internal_class(&ce);
    skyray_ce_Stream->create_object = skyray_stream_object_new;

    memcpy(&skyray_handler_Stream, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    skyray_handler_Stream.offset = XtOffsetOf(skyray_stream_t, std);
    skyray_handler_Stream.free_obj = skyray_stream_object_free;

    return SUCCESS;
}

