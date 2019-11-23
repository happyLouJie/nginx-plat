#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_http_upstream_conf_t upstream;
}  ngx_http_mytest_conf_t;

typedef struct {
    ngx_http_status_t status;
    ngx_str_t backend_server;
} ngx_http_mytest_ctx_t;

static char* ngx_http_mytest(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
static void* ngx_http_mytest_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_mytest_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf);

static ngx_command_t ngx_http_mytest_command[] = {
    {
        ngx_string("mytest"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_mytest,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_mytest_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_mytest_create_loc_conf,
    ngx_http_mytest_merge_loc_conf
};

ngx_module_t ngx_http_mytest_module = {
    NGX_MODULE_V1,
    &ngx_http_mytest_module_ctx,  /*module context*/
    ngx_http_mytest_command,      /*module command */
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

static ngx_str_t  ngx_http_proxy_hide_headers[] = {
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static void* ngx_http_mytest_create_loc_conf(ngx_conf_t *cf){
    ngx_http_mytest_conf_t *mycf;
    mycf = ngx_palloc(cf->pool, sizeof(ngx_http_mytest_conf_t));
    if (mycf == NULL) {
        return mycf;
    }
    mycf->upstream.connect_timeout = 6000;
    mycf->upstream.send_timeout = 6000;
    mycf->upstream.read_timeout = 6000;
    mycf->upstream.store_access = 0600;
    mycf->upstream.buffering = 0;
    mycf->upstream.bufs.num = 8;
    mycf->upstream.bufs.size = ngx_pagesize;
    mycf->upstream.buffer_size = ngx_pagesize;
    mycf->upstream.busy_buffers_size = 2 * ngx_pagesize;
    mycf->upstream.temp_file_write_size = 2 * ngx_pagesize;
    mycf->upstream.max_temp_file_size = 1024 * 1024 *1024;

    mycf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    mycf->upstream.pass_headers = NGX_CONF_UNSET_PTR;
    return mycf;
}

static char* ngx_http_mytest_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf) {
    ngx_http_mytest_conf_t *parent;
    ngx_http_mytest_conf_t *child;
    ngx_hash_init_t hash;
    parent = prev;
    child = conf;
    hash.max_size = 100;
    hash.bucket_size = 1024;
    hash.name = "proxy_headers_hash";
    if (ngx_http_upstream_hide_headers_hash(cf, &child->upstream, &parent->upstream
    ,ngx_http_proxy_hide_headers, &hash) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}

static ngx_int_t mytest_upstream_create_requset(ngx_http_request_t* r) {
    static ngx_str_t backend_query_line = ngx_string("GET /seachq=%V HTTP/1.1\r\nHost: www.baidu.com\r\nConnection: close\r\n\r\n");
    ngx_int_t query_line_len = backend_query_line.len + r->args.len - 2;
    ngx_buf_t *b = ngx_create_temp_buf(r->pool, query_line_len);
    if (b == NULL) {
        return NGX_ERROR;
    }
    b->last = b->pos + query_line_len;
    ngx_sprintf(b->pos, (char*)backend_query_line.data, &r->args);
    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    if (r->upstream->request_bufs == NULL) {
        return NGX_ERROR;
    }
    r->upstream->request_bufs->buf = b;
    r->upstream->request_bufs->next = NULL;
    r->upstream->request_sent = 0;
    r->upstream->header_sent = 0;
    r->header_hash = 1;
    return NGX_OK;
}


static ngx_int_t mytest_upstream_process_header(ngx_http_request_t *r) {
    ngx_int_t rc;
    ngx_table_elt_t *h;
    ngx_http_upstream_header_t *hh;
    ngx_http_upstream_main_conf_t *umcf;
    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    if (umcf == NULL) {
        return NGX_ERROR;
    }
    for(;;) {
        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
        if (rc == NGX_OK) {
            h = ngx_list_push(&r->upstream->headers_in.headers);
            if (h == NULL) {
                return NGX_ERROR;
            }
            h->hash = r->header_hash;
            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            h->key.data = ngx_palloc(r->pool, h->key.len + 1 + h->value.len + 1 + h->key.len);
            if (h->key.data == NULL) {
                return NGX_ERROR;
            }
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;
            h->value.data = h->key.data + h->key.len + 1;
            h->key.data[h->key.len] = '\0';
            h->value.data[h->value.len] = '\0';
            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash, h->lowcase_key, h->key.len);
            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return NGX_ERROR;
            }
            continue;
        }
        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            if (r->upstream->headers_in.server == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }
                h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash('s', 'e'),'r'),'v'),'e'),'r');
                ngx_str_set(&h->key, "Server");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char*)"server";
            }
            if (r->upstream->headers_in.date == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }
                h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'),'t'),'e');
                ngx_str_set(&h->key, "Date");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char*)"date";
            }
            return NGX_OK;
        }
    }

    if (rc == NGX_AGAIN) {
        return NGX_AGAIN;
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "upstream sent invalid value rc:%i", rc);
    return NGX_HTTP_UPSTREAM_INVALID_HEADER;
}

static ngx_int_t mytest_process_status_line(ngx_http_request_t *r) {
    size_t len;
    ngx_int_t rc;
    ngx_http_upstream_t *u;
    ngx_http_mytest_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_mytest_module);
    if (ctx == NULL) {
        return NGX_ERROR;
    }
    u = r->upstream;
    rc = ngx_http_parse_header_line(r, &u->buffer, 1);
    if (rc == NGX_AGAIN) {
        return rc;
    }
    if (rc == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "upstream sent no valid HTTP/1.0 header");
        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;
        return NGX_OK;
    }
    // if (u->state) {
    //     u->state->status = ctx->status.code;
    //     u->state->status = ctx->status.code;
    // }
    //u->headers_in.status_n = ctx->status.code;
    len = ctx->status.end - ctx->status.start;
    u->headers_in.status_line.len = len;
    u->headers_in.status_line.data =  ngx_pnalloc(r->pool, len);
    if (u->headers_in.status_line.data == NULL) {
        return NGX_ERROR;
    }
    ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);
    u->process_header = mytest_upstream_process_header;
    return mytest_upstream_process_header(r);
}


static void mytest_upstream_finalize_request(ngx_http_request_t* r, ngx_int_t rc) {
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "upstream finalize rc:%i", rc);
}


static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r) {
    ngx_http_mytest_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_mytest_module);
    if (ctx == NULL) {
        ctx = ngx_palloc(r->pool, sizeof(ngx_http_mytest_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
        ngx_http_set_ctx(r, ctx, ngx_http_mytest_module);
    }
    if (ngx_http_upstream_create(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_upstream_create failed");
        return NGX_ERROR;
    }
    ngx_http_mytest_conf_t *mycf = ngx_http_get_module_loc_conf(r, ngx_http_mytest_module);
    ngx_http_upstream_t *u = r->upstream;
    u->conf = &mycf->upstream;
    u->buffering = mycf->upstream.buffering;
    u->resolved = ngx_palloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "create resolved failed %s", strerror(errno));
        return NGX_ERROR;
    }
    struct sockaddr_in backend_sock_addr;
    struct hostent *host = gethostbyname("www.baidu.com");
    if (host == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "gethost failed:%s", strerror(errno));
        return NGX_ERROR;
    }
    backend_sock_addr.sin_family = AF_INET;
    backend_sock_addr.sin_port = htons((in_port_t)80);
    char* ip = inet_ntoa(*(struct in_addr*)(host->h_addr_list[0]));
    backend_sock_addr.sin_addr.s_addr = inet_addr(ip);
    ctx->backend_server.data = (u_char*)ip;
    ctx->backend_server.len = ngx_strlen(ip);

    u->resolved->sockaddr = (struct sockaddr*)&backend_sock_addr;
    u->resolved->socklen = sizeof(struct sockaddr_in);
    u->resolved->naddrs = 1;
    u->resolved->port = 80;

    u->create_request = mytest_upstream_create_requset;
    u->finalize_request = mytest_upstream_finalize_request;
    u->process_header = mytest_process_status_line;
    r->main->count++;
    ngx_http_upstream_init(r);
    return NGX_DONE;
}

#if 0
static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r) {
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "mytest method:%V uri:%V args:%V line:%V"
    ,&r->method_name, &r->uri, &r->args, &r->request_line);
    // ngx_int_t rc = ngx_http_send_header(r);
    // if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
    //     return rc;
    // }
    // //return ngx_http_output_filter(r, NULL);
    // return NGX_HTTP_OK;
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    ngx_int_t rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }


    #if 0
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
    #endif


    //send file
    ngx_buf_t *b;
    b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
    u_char* filename = (u_char*)"hello.txt";
    b->in_file = 1;
    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    b->file->fd = ngx_open_file(filename, NGX_FILE_RDONLY|NGX_FILE_NONBLOCK, NGX_FILE_OPEN, 0);
    b->file->log = r->connection->log;
    b->file->name.data = filename;
    b->file->name.len = ngx_strlen(filename);
    if (b->file->fd <= 0) {
        return NGX_HTTP_NOT_FOUND;
    }
    if (ngx_file_info(filename, &b->file->info) == NGX_FILE_ERROR){
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    r->headers_out.content_length_n = b->file->info.st_size;
    b->file_pos = 0;
    b->file_last = b->file->info.st_size;
    //ngx_str_t type = ngx_string("application/octet-stream");
    // r->headers_out.content_type.data = type.data;
    // r->headers_out.content_type.len = type.len;
    //r->headers_out.headers.
    //r->headers_out.
    ngx_pool_cleanup_t *cn = ngx_pool_cleanup_add(r->pool, sizeof(ngx_pool_cleanup_file_t));
    if (cn == NULL) {
        return NGX_ERROR;
    }
    cn->handler = ngx_pool_cleanup_file;
    ngx_pool_cleanup_file_t *clnf = cn->data;
    clnf->fd = b->file->fd;
    clnf->log = r->connection->log;
    clnf->name = filename;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }
    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;
    //发送包体，发送结束后http框架会调用ngx_http_finalize_request方法结束请求。
    return ngx_http_output_filter(r, &out);
}
#endif

static char* ngx_http_mytest(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_mytest_handler;
    return NGX_CONF_OK;
}
