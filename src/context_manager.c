#include <string.h>
#include "context_manager.h"
#include "http.h"

/*
* Enumeration of modules which have a context
*/
typedef enum LoRaMacNvmCtxModule_e
{
    /*!
     * Context for the MAC
     */
    LORAMAC_NVMCTXMODULE_MAC,
    /*!
     * Context for the regions
     */
    LORAMAC_NVMCTXMODULE_REGION,
    /*!
     * Context for the crypto module
     */
    LORAMAC_NVMCTXMODULE_CRYPTO,
    /*!
     * Context for the secure element
     */
    LORAMAC_NVMCTXMODULE_SECURE_ELEMENT,
    /*!
     * Context for the command queue
     */
    LORAMAC_NVMCTXMODULE_COMMANDS,
    /*!
     * Context for class b
     */
    LORAMAC_NVMCTXMODULE_CLASS_B,
    /*!
     * Context for the confirm queue
     */
    LORAMAC_NVMCTXMODULE_CONFIRM_QUEUE,
}LoRaMacNvmCtxModule_t;


static struct lwan_context lwan_ctx;

void context_manager_init(struct context_manager *ctx_mngr)
{
    ctx_mngr->lwan_ctx = get_lwan_ctx();
    memset(ctx_mngr->lwan_ctx, 0 , sizeof(struct lwan_context));
    ctx_mngr->lwan_ctx->network_join_mode = 1;
    ctx_mngr->lwan_ctx->confirmation_mode = 0;
}

struct lwan_context * get_lwan_ctx(void)
{
    return &lwan_ctx;
}



