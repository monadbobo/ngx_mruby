/*
// ngx_http_mruby_filter.c - ngx_mruby mruby filter functions
//
// See Copyright Notice in ngx_http_mruby_module.c
*/

#include <nginx.h>
#include <ngx_http.h>

#include "ngx_http_mruby_module.h"
#include "ngx_http_mruby_state.h"
#include "ngx_http_mruby_filter_handler.h"

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt   ngx_http_next_body_filter;

static ngx_int_t ngx_http_mruby_header_filter(ngx_http_request_t *r);
static ngx_int_t ngx_http_mruby_body_filter(ngx_http_request_t *r, ngx_chain_t *in);
static void ngx_http_mruby_filter_cleanup(void *data);

void ngx_http_mruby_header_filter_init(void)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter  = ngx_http_mruby_header_filter;
}

void ngx_http_mruby_body_filter_init(void)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter  = ngx_http_mruby_body_filter;
}
ngx_int_t ngx_http_mruby_header_filter_handler(ngx_http_request_t *r)
{
    ngx_http_mruby_main_conf_t *mmcf = ngx_http_get_module_main_conf(r, ngx_http_mruby_module);
    ngx_http_mruby_loc_conf_t  *mlcf = ngx_http_get_module_loc_conf(r,  ngx_http_mruby_module);
    ngx_int_t rc;
    NGX_MRUBY_STATE_REINIT_IF_NOT_CACHED(
        mlcf->cached,
        mmcf->state,
        mlcf->header_filter_inline_code,
        ngx_http_mruby_state_reinit_from_file
    );
    rc = ngx_mrb_run(r, mmcf->state, mlcf->header_filter_code, 1);
    if (rc == NGX_DECLINED || rc == NGX_ERROR) {
        return NGX_ERROR;
    }
    return NGX_OK;
}

ngx_int_t ngx_http_mruby_header_filter_inline_handler(ngx_http_request_t *r)
{
    ngx_http_mruby_main_conf_t *mmcf = ngx_http_get_module_main_conf(r, ngx_http_mruby_module);
    ngx_http_mruby_loc_conf_t  *mlcf = ngx_http_get_module_loc_conf(r,  ngx_http_mruby_module);
    ngx_int_t rc;
    rc = ngx_mrb_run(r, mmcf->state, mlcf->header_filter_inline_code, 1);
    if (rc == NGX_DECLINED || rc == NGX_ERROR) {
        return NGX_ERROR;
    }
    return NGX_OK;
}

ngx_int_t ngx_http_mruby_body_filter_handler(ngx_http_request_t *r)
{
    printf("%s\n", __FUNCTION__);
    return NGX_OK;
}

ngx_int_t ngx_http_mruby_body_filter_inline_handler(ngx_http_request_t *r)
{
    printf("%s\n", __FUNCTION__);
    return NGX_OK;
}

static ngx_int_t ngx_http_mruby_header_filter(ngx_http_request_t *r)
{
    ngx_http_mruby_loc_conf_t *mlcf;
    ngx_http_mruby_ctx_t     *ctx;
    ngx_pool_cleanup_t *cln;
    ngx_int_t rc;
  
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_mruby_module);

    if (mlcf->body_filter_handler) {
        r->filter_need_in_memory = 1;
    }

    if (mlcf->header_filter_handler == NULL) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_mruby_module);
    if (ctx) {
        ngx_http_set_ctx(r, NULL, ngx_http_mruby_module);
        return ngx_http_next_header_filter(r);
    }

    if ((ctx = ngx_pcalloc(r->pool, sizeof(*ctx))) == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to allocate memory from r->pool %s:%d", __FUNCTION__, __LINE__);
        return NGX_ERROR;
    }

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to allocate memory from r->pool %s:%d", __FUNCTION__, __LINE__);
        return NGX_ERROR;
    }
    cln->handler = ngx_http_mruby_filter_cleanup;
    cln->data    = ctx;

    rc = mlcf->header_filter_handler(r);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_http_next_header_filter(r);
}

static ngx_int_t ngx_http_mruby_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    return NGX_OK;
}

static void ngx_http_mruby_filter_cleanup(void *data)
{
    ngx_http_mruby_ctx_t *ctx;
    ctx = (ngx_http_mruby_ctx_t *)data;
    ngx_memzero(ctx, sizeof(ngx_http_mruby_ctx_t));
}
