extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

class ngx_http_mytest_cpp_module_t
{
private:
    /* data */
public:
    ngx_http_mytest_cpp_module_t(/* args */);
    ~ngx_http_mytest_cpp_module_t();
};

ngx_http_mytest_cpp_module_t::ngx_http_mytest_cpp_module_t(/* args */)
{
    ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, "init in cpp module");
}

ngx_http_mytest_cpp_module_t::~ngx_http_mytest_cpp_module_t()
{
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "destory cpp module");
}


//extern "C" {
static char* ngx_http_cpp_mytest(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
//static ngx_int_t ngx_http_mytest_cpp_handler(ngx_http_request_t *r);
//}

static ngx_command_t ngx_http_mytest_cpp_command[] = {
    {
        ngx_string("mytest_cpp"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_cpp_mytest,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_mytest_cpp_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

ngx_module_t ngx_http_mytest_cpp_module = {
    NGX_MODULE_V1,
    &ngx_http_mytest_cpp_module_ctx,  /*module context*/
    ngx_http_mytest_cpp_command,      /*module command */
    NGX_HTTP_MODULE,              /*module type */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};


static ngx_int_t ngx_http_mytest_cpp_handler(ngx_http_request_t *r) {
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "mytest cpp method:%V uri:%V args:%V line:%V"
    ,&r->method_name, &r->uri, &r->args, &r->request_line);
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    ngx_int_t rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    ngx_http_mytest_cpp_module_t test;
    ngx_str_t type = ngx_string("text/plain");
    ngx_str_t respone = ngx_string("hello world");
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = respone.len;
    r->headers_out.content_type = type;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    ngx_buf_t *b;
    b = ngx_create_temp_buf(r->pool, respone.len);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_memcpy(b->pos, respone.data, respone.len);
    b->last = b->pos + respone.len;
    //声明最后一块缓冲区
    b->last_buf = 1;
    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;
    //发送包体，发送结束后http框架会调用ngx_http_finalize_request方法结束请求。
    return ngx_http_output_filter(r, &out);
}

static char* ngx_http_cpp_mytest(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    ngx_http_core_loc_conf_t *clcf;
    clcf = (ngx_http_core_loc_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_mytest_cpp_handler;
    return NGX_CONF_OK;
}