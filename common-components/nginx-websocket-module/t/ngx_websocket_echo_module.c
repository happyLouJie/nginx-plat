/*
 * @Author: pingox
 * @Copyright: pngox
 * @Github: https://github.com/pingox
 * @EMail: cczjp89@gmail.com
 */

#include "ngx_websocket.h"

static char *
ngx_websocket_echo_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void
ngx_websocket_connect_handler(ngx_websocket_session_t *ws);
static void
ngx_websocket_recv_handler(ngx_websocket_session_t *ws,
                            ngx_str_t *msg, u_char opcode);
static void
ngx_websocket_disconnect_handler(ngx_websocket_session_t *ws);

static ngx_http_module_t  ngx_websocket_echo_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


static ngx_command_t  ngx_websocket_echo_commands[] = {

    { ngx_string("websocket_echo"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_websocket_echo_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};

ngx_module_t  ngx_websocket_echo_module = {
    NGX_MODULE_V1,
    &ngx_websocket_echo_module_ctx,        /* module context */
    ngx_websocket_echo_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *
ngx_websocket_echo_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_websocket_loc_conf_t   *wlcf;

    wlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_websocket_module);
    wlcf->connect_handler = ngx_websocket_connect_handler;

    return NGX_CONF_OK;
}

static void
ngx_websocket_connect_handler(ngx_websocket_session_t *ws)
{
    ngx_http_request_t *r;
    r = ws->r;
    ngx_str_t uri;
    uri.data = r->uri_start;
    uri.len = r->uri_end - r->uri_start;
    ngx_str_t request;
    request.data = r->request_start;
    request.len = r->request_end - r->request_start;
    ngx_log_error(NGX_LOG_INFO, ws->log, 0, "request:%v uri:%v", &request, &uri);
    if (r->args_start) {
        ngx_str_t args;
        args.data = r->args_start;
        args.len = r->uri_end - r->args_start;
        ngx_log_error(NGX_LOG_INFO, ws->log, 0, "args:%v r->args:%v", &args, &r->args);
    }

    if (r->uri_ext) {
        ngx_str_t ext;
        ext.data = r->uri_ext;
        ext.len = r->uri_end - r->uri_ext;
        ngx_log_error(NGX_LOG_INFO, ws->log, 0, "uri_ext:%v", &ext);
    }


    if (r->args.len) {
        uri.len = r->args_start - r->uri_start;
        ngx_log_error(NGX_LOG_INFO, ws->log, 0, "uri:%v r->args:%v", &uri, &r->args);
    }

    u_char* p;
    u_char* end;
    if (r->args.len) {
        end = r->args_start - 1;
    } else {
        end = r->uri_end;
    }
    p = r->uri_start;
    p = ngx_strlchr(p, end, '/');
    u_char *name = NULL;
    while(p && p < end) {
        u_char* tmp = p;
        if (end -p > 5)
        if (tmp[1] == 'l' && tmp[2] == 'i' && tmp[3] == 'v' && tmp[4] == 'e' && tmp[5] == '_') {
            name = p + 1;
            break;
        }
        p = ngx_strlchr(p+1, end, '/');
    }

    if (name) {
        ngx_str_t na;
        na.data = name;
        na.len = end - name;
        ngx_log_error(NGX_LOG_INFO, ws->log, 0, "name:%v", &na);
    }

    ngx_log_error(NGX_LOG_INFO, ws->log, 0,
        "websocket-echo: connect_handler| new client connection.");

    ws->recv_handler = ngx_websocket_recv_handler;
    ws->disconnect_handler = ngx_websocket_disconnect_handler;
}

static void
ngx_websocket_recv_handler(ngx_websocket_session_t *ws,
                                ngx_str_t *msg, u_char opcode)
{
    switch (opcode) {
        case NGX_WEBSOCKET_OPCODE_TEXT:
        case NGX_WEBSOCKET_OPCODE_BINARY:
            ngx_log_error(NGX_LOG_DEBUG, ws->log, 0,
                "websocket-echo: recv_handler| recv: %V", msg);
            ngx_websocket_send_message(ws, msg, opcode);
        break;

        case NGX_WEBSOCKET_OPCODE_PONG:
            ngx_log_error(NGX_LOG_DEBUG, ws->log, 0,
                "websocket-echo: recv_handler| getting pong ...");
        break;

        default:
            ngx_log_error(NGX_LOG_WARN, ws->log, 0,
                "websocket-echo: recv_handler| drop message, opcode %d", opcode);
    }
}

static void
ngx_websocket_disconnect_handler(ngx_websocket_session_t *ws)
{
    ngx_log_error(NGX_LOG_INFO, ws->log, 0,
        "websocket-echo: disconnect_handler| client disconnect.");
}