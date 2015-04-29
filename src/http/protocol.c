/*
 * http_protocol.c
 *
 *  Created on: Apr 13, 2015
 *      Author: hujin
 */


#include "http.h"
#include "src/core/stream.h"

zend_class_entry *skyray_ce_HttpProtocol;

zend_string *intern_str_content_type; // Content-Type
zend_string *intern_str_content_length; // Content-Length
zend_string *intern_str_application_json; // application/json
zend_string *intern_str_text_plain; // text/plain
zend_string *intern_str_connection; // Connection
zend_string *intern_str_onRequest;  // onRequest

zend_object_handlers skyray_handler_HttpProtocol;

void skyray_http_protocol_on_request(skyray_http_protocol_t *self);

int longtobase(zend_ulong value, int base, char *buf, char **retval)
{
    static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char *ptr, *end;

    end = ptr = buf + (sizeof(zend_ulong) << 3);

    *ptr = '\0';

    do {
        *--ptr = digits[value % base];
        value /= base;
    } while (ptr > buf && value);

    *retval = ptr;

    return end - ptr;
}

static int http_on_url(http_parser *parser, const char *at, size_t len)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (!skyray_buffer_initialized(&protocol->uri)) {
        skyray_buffer_init_stringl(&protocol->uri, at, len);
    } else {
        skyray_buffer_appendl(&protocol->uri, at, len);
    }

    return 0;
}

static int http_on_status(http_parser *parser, const char *at, size_t len)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    return 0;
}

static int http_on_header_field(http_parser *parser, const char *at, size_t len)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (skyray_buffer_initialized(&protocol->header_value)) {
        skyray_http_message_add_header(&req->message, protocol->header_name.buf, protocol->header_value.buf, 0);
        skyray_buffer_release(&protocol->header_name);
        skyray_buffer_reset(&protocol->header_value);
    }

    if (!skyray_buffer_initialized(&protocol->header_name)) {
        skyray_buffer_init_stringl(&protocol->header_name, at, len);
    } else {
        skyray_buffer_appendl(&protocol->header_name, at, len);
    }

    return 0;
}

static int http_on_header_value(http_parser *parser, const char *at, size_t len)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (!skyray_buffer_initialized(&protocol->header_value)) {
        skyray_buffer_init_stringl(&protocol->header_value, at, len);
    } else {
        skyray_buffer_appendl(&protocol->header_value, at, len);
    }

    return 0;
}

static int http_on_headers_complete(http_parser *parser)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (skyray_buffer_initialized(&protocol->header_value)) {
        skyray_http_message_add_header(&req->message, protocol->header_name.buf, protocol->header_value.buf, 0);
        skyray_buffer_release(&protocol->header_name);
        skyray_buffer_reset(&protocol->header_value);
    }

    ZVAL_STR(&req->uri, protocol->uri.buf);

    req->method = parser->method;
    req->message.version_major = parser->http_major;
    req->message.version_minor = parser->http_minor;

    return 0;
}

static int http_on_body(http_parser *parser, const char *at, size_t len)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (!skyray_buffer_initialized(&protocol->body)) {
        skyray_buffer_init_stringl(&protocol->body, at, len);
    } else {
        skyray_buffer_appendl(&protocol->body, at, len);
    }

    return 0;
}

static int http_on_message_begin(http_parser *parser)
{
    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    zval req;

    skyray_buffer_reset(&protocol->uri);
    skyray_buffer_reset(&protocol->body);

    object_init_ex(&req, skyray_ce_HttpRequest);
    protocol->req = Z_OBJ_P(&req);

    return 0;
}

static int http_on_message_complete(http_parser *parser)
{

    skyray_http_protocol_t *protocol = (skyray_http_protocol_t *)parser;
    skyray_http_request_t *req = skyray_http_request_from_obj(protocol->req);

    if (skyray_buffer_initialized(&protocol->body)) {
        ZVAL_STR(&req->message.raw_body, protocol->body.buf);
    }

    skyray_http_protocol_on_request(protocol);

    return 0;
}

static http_parser_settings http_settings = {
    .on_message_begin       = http_on_message_begin,
    .on_url                 = http_on_url,
    .on_status              = http_on_status,
    .on_header_field        = http_on_header_field,
    .on_header_value        = http_on_header_value,
    .on_headers_complete    = http_on_headers_complete,
    .on_body                = http_on_body,
    .on_message_complete    = http_on_message_complete
};

zend_object * skyray_http_protocol_object_new(zend_class_entry *ce)
{
    skyray_http_protocol_t *intern;
    intern = ecalloc(1, sizeof(skyray_http_protocol_t) + zend_object_properties_size(ce));

    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);

    http_parser_init(&intern->parser, HTTP_REQUEST);

    skyray_buffer_reset(&intern->header_name);
    skyray_buffer_reset(&intern->header_value);

    intern->close = 0;
    intern->stream = NULL;

    intern->std.handlers = &skyray_handler_HttpProtocol;
    return &intern->std;
}

zend_object * skyray_http_protocol_object_clone(zval *obj)
{
    return NULL;
}

void skyray_http_protocol_object_free(zend_object *object)
{
    skyray_http_protocol_t *intern = skyray_http_protocol_from_obj(object);

    if (intern->stream) {
        zend_object_release(intern->stream);
        intern->stream = NULL;
    }

    zend_object_std_dtor(&intern->std);
}

void skyray_http_protocol_negotiate_content_type(skyray_http_protocol_t *self, skyray_http_message_t *message)
{
    zval *zcontent_type = skyray_http_message_get_header(message, intern_str_content_type, 1);

    if (!zcontent_type) {
        if (Z_TYPE_P(&message->raw_body) == IS_STRING || (Z_TYPE_P(&message->raw_body) != IS_STRING && Z_TYPE_P(&message->body) == IS_UNDEF)) {
            skyray_http_message_add_header(message, intern_str_content_type, intern_str_text_plain);
        } else {
            skyray_http_message_add_header(message, intern_str_content_type, intern_str_application_json);
        }
    }
}

void skyray_http_protocol_send_headers(skyray_http_protocol_t *self, skyray_http_message_t *message)
{
    zval *zconnection = NULL;

    zconnection = skyray_http_message_get_header(message, intern_str_connection, 1);

    zend_array *ht = &message->headers;
    zend_array *headers;
    zend_string *name;
    zend_string *value;

    skyray_buffer_t header_s;

    skyray_buffer_init(&header_s, 2048);

    zend_hash_internal_pointer_reset(ht);
    while(zend_hash_has_more_elements(ht) == SUCCESS) {

        zend_hash_get_current_key(ht, &name, NULL);
        headers = Z_ARR_P(zend_hash_get_current_data(ht));

        zend_hash_internal_pointer_reset(headers);
        while(zend_hash_has_more_elements(headers) == SUCCESS) {
            value = Z_STR_P(zend_hash_get_current_data(headers));

            skyray_buffer_appendl(&header_s, name->val, name->len);
            skyray_buffer_appendl(&header_s, ZEND_STRL(": "));
            skyray_buffer_appendl(&header_s, value->val, value->len);
            skyray_buffer_appendl(&header_s, ZEND_STRL("\r\n"));

            zend_hash_move_forward(headers);
        }

        zend_hash_move_forward(ht);
    }

    self->close = 0;

    if (!zconnection) {
        if (http_should_keep_alive(&self->parser)) {
            skyray_buffer_appendl(&header_s, ZEND_STRL("Connection: keep-alive\r\n"));
        } else {
            skyray_buffer_appendl(&header_s, ZEND_STRL("Connection: close\r\n"));
            self->close = 1;
        }
    } else if (strcasecmp(Z_STR_P(zconnection)->val, "close") == 0) {
        self->close = 1;
    }

    self->chunked = 0;

    if (!skyray_http_message_has_header(message, intern_str_content_length)) {
        skyray_buffer_appendl(&header_s, ZEND_STRL("Transfer-Encoding: chunked\r\n"));
        self->chunked = 1;
    }

    skyray_buffer_appendl(&header_s, ZEND_STRL("\r\n"));

    skyray_stream_write(skyray_stream_from_obj(self->stream), header_s.buf);
    skyray_buffer_release(&header_s);
}

void skyray_http_protocol_write(skyray_http_protocol_t *self, skyray_stream_t *stream, zend_string *buf)
{
    char dex[(sizeof(zend_ulong) << 3) + 1], *dexptr;
    int dexlen;

    if (self->chunked) {
        dexlen = longtobase(buf->len, 16, dex, &dexptr);
        skyray_stream_writel(stream, dexptr, dexlen);
        skyray_stream_writel(stream, ZEND_STRL("\r\n"));
        skyray_stream_write(stream, buf);
    } else {
        skyray_stream_write(stream, buf);
    }
    skyray_stream_writel(stream, ZEND_STRL("\r\n"));
}

void skyray_http_protocol_send_body(skyray_http_protocol_t *self, skyray_http_message_t *message)
{
    smart_str buf = {.s = NULL, .a = 0};
    skyray_stream_t *stream = skyray_stream_from_obj(self->stream);

    if (Z_TYPE_P(&message->raw_body) == IS_STRING) {
        skyray_http_protocol_write(self, stream, Z_STR_P(&message->raw_body));
    } else if (Z_TYPE_P(&message->body) != IS_UNDEF) {
        smart_str_alloc(&buf, 1024, 0);
        php_json_encode(&buf, &message->body, PHP_JSON_UNESCAPED_UNICODE);
        smart_str_0(&buf);

        skyray_http_protocol_write(self, stream, buf.s);

        smart_str_free(&buf);
    }
    if (self->chunked) {
        skyray_stream_writel(stream, ZEND_STRL("0\r\n"));
    }
    skyray_stream_writel(stream, ZEND_STRL("\r\n"));
}

void skyray_http_protocol_response(skyray_http_protocol_t *self, skyray_http_response_t *response)
{
    skyray_stream_t *stream = skyray_stream_from_obj(self->stream);

    char buffer[1024] = {0};
    int len = sprintf(buffer, "HTTP/%d.%d %d %s\r\n",
            response->message.version_major ? response->message.version_major : 1,
            response->message.version_minor ? response->message.version_minor : 1,
            response->code, "Ok");

    skyray_stream_writel(stream, buffer, len);

    skyray_http_protocol_negotiate_content_type(self, &response->message);

    skyray_http_protocol_send_headers(self, &response->message);

    skyray_http_protocol_send_body(self, &response->message);

    if (self->close) {
        skyray_stream_close(stream);
    }
}


void skyray_http_protocol_on_request(skyray_http_protocol_t *self)
{
    zval function_name, zprotocol, retval;
    zval params[1];

    ZVAL_STR(&function_name, intern_str_onRequest);
    ZVAL_OBJ(&zprotocol, &self->std);
    ZVAL_OBJ(&params[0], self->req);

    call_user_function(EG(function_table), &zprotocol, &function_name, &retval, 1, params);

    if (EG(exception)) {
        skyray_handle_uncaught_exception(EG(exception));
        object_init_ex(&retval, skyray_ce_HttpResponse);

        //TODO 500
    }else if (Z_TYPE_P(&retval) == IS_NULL) {
        goto clean;
    } else if (Z_TYPE_P(&retval) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(&retval), skyray_ce_HttpResponse)) {
        zval_ptr_dtor(&retval);
        object_init_ex(&retval, skyray_ce_HttpResponse);

        //TODO 500
    }

    skyray_http_protocol_response(self, (skyray_http_response_t *)Z_OBJ(retval));
    zval_ptr_dtor(&retval);
clean:
    zend_object_release(self->req);
}

SKYRAY_METHOD(HttpProtocol, connectStream)
{
    zval *zstream;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &zstream, skyray_ce_Stream) ==  FAILURE) {
        return;
    }

    skyray_http_protocol_t *intern = skyray_http_protocol_from_obj(Z_OBJ_P(getThis()));
    assert(intern->stream == NULL);
    intern->stream = Z_OBJ_P(zstream);
    zval_addref_p(zstream);
}


SKYRAY_METHOD(HttpProtocol, streamConnected)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
}

SKYRAY_METHOD(HttpProtocol, dataReceived)
{
    zend_string *data;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &data) ==  FAILURE) {
        return;
    }

    skyray_http_protocol_t *intern = skyray_http_protocol_from_obj(Z_OBJ_P(getThis()));

    http_parser_execute(&intern->parser, &http_settings, data->val, data->len);
}

SKYRAY_METHOD(HttpProtocol, streamClosed)
{
    if (zend_parse_parameters_none() ==  FAILURE) {
        return;
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_connectStream, 0, 0, 1)
    ZEND_ARG_INFO(0, stream)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dataReceived, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

static const zend_function_entry class_methods[] = {
    SKYRAY_ME(HttpProtocol, connectStream, arginfo_connectStream, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpProtocol, streamConnected, arginfo_empty, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpProtocol, dataReceived, arginfo_dataReceived, ZEND_ACC_PUBLIC)
    SKYRAY_ME(HttpProtocol, streamClosed, arginfo_empty, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(skyray_http_protocol)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "skyray\\http\\ServerProtocol", class_methods);
    skyray_ce_HttpProtocol = zend_register_internal_class(&ce);
    zend_class_implements(skyray_ce_HttpProtocol, 1, skyray_ce_ProtocolInterface);

    skyray_ce_HttpProtocol->create_object = skyray_http_protocol_object_new;

    memcpy(&skyray_handler_HttpProtocol, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    skyray_handler_HttpProtocol.offset = XtOffsetOf(skyray_http_protocol_t, std);
    skyray_handler_HttpProtocol.free_obj = skyray_http_protocol_object_free;
    skyray_handler_HttpProtocol.clone_obj = skyray_http_protocol_object_clone;

    intern_str_application_json = zend_new_interned_string(zend_string_init(ZEND_STRL("application/json"), 1));
    intern_str_text_plain       = zend_new_interned_string(zend_string_init(ZEND_STRL("text/plain"), 1));
    intern_str_content_type     = zend_new_interned_string(zend_string_init(ZEND_STRL("Content-Type"), 1));
    intern_str_content_length   = zend_new_interned_string(zend_string_init(ZEND_STRL("Content-Length"), 1));
    intern_str_connection       = zend_new_interned_string(zend_string_init(ZEND_STRL("Connection"), 1));
    intern_str_onRequest        = zend_new_interned_string(zend_string_init(ZEND_STRL("onRequest"), 1));

    return SUCCESS;
}
