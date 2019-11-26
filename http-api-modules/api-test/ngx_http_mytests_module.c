#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
     ngx_str_t stock[6];
} ngx_http_mytests_ctx_t;


static char* ngx_http_mytests(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
static ngx_int_t  ngx_http_mytests_handler(ngx_http_request_t *r);
static void mytest_post_handler(ngx_http_request_t *r);

static ngx_command_t ngx_http_mytests_commands[] = {
    {
        ngx_string("mytests"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_mytests,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command

};


static ngx_http_module_t  ngx_http_mytests_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configration */
    NULL                                   /* merge location configration */
};


ngx_module_t  ngx_http_mytests_module = {
    NGX_MODULE_V1,
    &ngx_http_mytests_module_ctx,           /* module context */
    ngx_http_mytests_commands,               /* module directives */
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


static char* ngx_http_mytests(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
     //找到mytest配置项所属的配置块
     ngx_http_core_loc_conf_t *clcf;
     clcf=ngx_http_conf_get_module_loc_conf(cf,ngx_http_core_module);
     /*
         HTTP框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时
         如果请求的主机域名、URI与mytest配置项所在的配置块相匹配，
         就将调用我们事先的ngx_http_mytest_handler方法处理这个请求
     */
     clcf->handler=ngx_http_mytests_handler;
     return NGX_CONF_OK;
}

//子请求结束时的回调方法
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r,
    void *data,ngx_int_t rc)
{
    //获取父请求
    ngx_http_request_t *pr=r->parent;
    //获取上下文
    ngx_http_mytests_ctx_t *myctx=ngx_http_get_module_ctx(pr,ngx_http_mytests_module);
    
    pr->headers_out.status=r->headers_out.status;
    //查看返回码，如果为NGX_HTTP_OK，则意味访问成功，接着开始解析HTTP包体
    if(r->headers_out.status==NGX_HTTP_OK)
    {
        int flag=0;
        //上游响应会保存在buffer缓冲区中
        ngx_buf_t *pRecvBuf=&r->upstream->buffer;
        /*
            解析上游服务器的相应，并将解析出的值赋到上下文结构体myctx->stock数组中
            新浪服务器的返回大致如下:
            var hq_str_s_sh000009=" 上证 380,3356.355,-5.725,-0.17,266505,2519967"
        */
        for(;pRecvBuf->pos!=pRecvBuf->last;pRecvBuf->pos++)
        {
            if(*pRecvBuf->pos==','||*pRecvBuf->pos=='\"')
            {
                if(flag>0)
                {
                    myctx->stock[flag-1].len=pRecvBuf->pos-myctx->stock[flag-1].data;
                }
                flag++;
                myctx->stock[flag-1].data=pRecvBuf->pos+1;
            }
            if(flag>6)
                break;
        }
    }
    //设置父请求的回调方法
    pr->write_event_handler=mytest_post_handler;
    return NGX_OK;
}

static ngx_int_t  ngx_http_mytests_handler(ngx_http_request_t *r) {
         //创建HTTP上下文
     ngx_http_mytests_ctx_t *myctx=ngx_http_get_module_ctx(r,ngx_http_mytests_module);
     if(myctx==NULL)
     {
         myctx=ngx_palloc(r->pool,sizeof(ngx_http_mytests_ctx_t));
         if(myctx==NULL)
         {
             return NGX_ERROR;
         }
         ngx_http_set_ctx(r,myctx,ngx_http_mytests_module);
     }
     //ngx_http_post_subrequest_t结构体会决定子请求的回调方法
     ngx_http_post_subrequest_t *psr=ngx_palloc(r->pool,sizeof(ngx_http_post_subrequest_t));
     if(psr==NULL){
         return NGX_HTTP_INTERNAL_SERVER_ERROR;
     }
     //设置子请求回调方法为mytest_subrequest_post_handler
     psr->handler=mytest_subrequest_post_handler;
     //将data设为myctx上下文，这样回调mytest_subrequest_post_handler时传入的data参数就是myctx
     psr->data=myctx;
     //子请求的URI前缀是/list
     ngx_str_t sub_prefix=ngx_string("/lis=");
     ngx_str_t sub_location;
     sub_location.len=sub_prefix.len+r->args.len;
     sub_location.data=ngx_palloc(r->pool,sub_location.len);
     ngx_snprintf(sub_location.data,sub_location.len,
                  "%V%V",&sub_prefix,&r->args);
     //sr就是子请求
     ngx_http_request_t *sr;
     //调用ngx_http_subrequest创建子请求
     ngx_int_t rc=ngx_http_subrequest(r,&sub_location,NULL,&sr,psr,NGX_HTTP_SUBREQUEST_IN_MEMORY);
     if(rc!=NGX_OK){
         return NGX_ERROR;
     }
     return NGX_DONE;

}


static void mytest_post_handler(ngx_http_request_t *r) {
         //如果没有返回200，则直接把错误码发送回用户
     if(r->headers_out.status!=NGX_HTTP_OK)
     {
         ngx_http_finalize_request(r,r->headers_out.status);
         return;
     }
     //取出上下文
     ngx_http_mytests_ctx_t *myctx=ngx_http_get_module_ctx(r,ngx_http_mytests_module);
     //定义发给用户的HTTP包体内容
     ngx_str_t output_format=ngx_string("stock[%V],Today current price:%V,volumn:%V");
     //计算待发送包体的长度
     int bodylen=output_format.len+myctx->stock[0].len+
                 myctx->stock[1].len+myctx->stock[4].len-6;
     r->headers_out.content_length_n=bodylen;
     //在内存池上分配内存以保存将要发送的包体
     ngx_buf_t *b=ngx_create_temp_buf(r->pool,bodylen);
     ngx_snprintf(b->pos,bodylen,(char *)output_format.data,
                  &myctx->stock[0],&myctx->stock[1],&myctx->stock[4]);
     b->last=b->pos+bodylen;
     b->last_buf=1;
     
     ngx_chain_t out;
     out.buf=b;
     out.next=NULL;
     //设置Content-Type
     static ngx_str_t type=ngx_string("text/plain;charset=GBK");
     r->headers_out.content_type=type;
     r->headers_out.status=NGX_HTTP_OK;
     
     r->connection->buffered|=NGX_HTTP_WRITE_BUFFERED;
     ngx_int_t ret=ngx_http_send_header(r);
     ret=ngx_http_output_filter(r,&out);
     
     ngx_http_finalize_request(r,ret);
}