#include "xmsger.h"


static void* recv_loop(void *ptr)
{

    return NULL;
}


static void* send_loop(void *ptr)
{
    xmsger_loop(ptr);
    return NULL;
}

bool xmsger_connect(xmsger_ptr msger, const char *host, uint16_t port)
{
    __xlogd("xmsger_connect enter\n");

        xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->addr = (__xipaddr_ptr)malloc(sizeof(struct __xipaddr));
    __xbreak(msg->addr == NULL);

    __xbreak(!__xapi->udp_make_ipaddr(host, port, (__xipaddr_ptr)msg->addr));
    
    msg->type = MSG_TYPE_MESSAGE;
    msg->channel = NULL;
    
    __atom_add(msger->len, 12);
    __xbreak(xpipe_write(msger->spipe, &msg, sizeof(void*)) != sizeof(void*));

    if (__is_false(msger->working)){
        __xlogd("xmsger_connect notify\n");
        __xapi->mutex_notify(msger->mtx);
    }

    __xlogd("xmsger_connect exit\n");

    return true;

Clean:

    if (msg){
        if (msg->addr){
            free(msg->addr);
        }        
        free(msg);
    }

    return false;
}


xmsger_ptr xmsger_create(xmsgsocket_ptr msgsock, xmsglistener_ptr listener)
{
    __xlogd("xmsger_create enter\n");

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));
    
    msger->running = true;
    msger->readable = true;
    msger->msgsock = msgsock;
    msger->listener = listener;
    
    msger->squeue = (xchannellist_ptr)malloc(sizeof(struct xchannellist));
    __xbreak(msger->squeue == NULL);

    msger->squeue->len = 0;
    msger->squeue->lock = false;
    msger->squeue->head.prev = NULL;
    msger->squeue->end.next = NULL;
    msger->squeue->head.next = &msger->squeue->end;
    msger->squeue->end.prev = &msger->squeue->head;

    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->peers = xtree_create();
    __xbreak(msger->peers == NULL);

    msger->mtx = __xapi->mutex_create();
    __xbreak(msger->mtx == NULL);

    msger->spipe = xpipe_create(sizeof(struct xtask_enter) * 1024);
    __xbreak(msger->spipe == NULL);

    msger->rpipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->spipe == NULL);

    msger->spid = __xapi->process_create(send_loop, msger);
    __xbreak(msger->spid == NULL);

    msger->rpid = __xapi->process_create(recv_loop, msger);
    __xbreak(msger->rpid == NULL);

    __xlogd("xmsger_create exit\n");

    return msger;

Clean:

    xmsger_free(&msger);
    return NULL;
}


void xmsger_free(xmsger_ptr *pptr)
{
    __xlogd("xmsger_free enter\n");

    if (pptr && *pptr){

        xmsger_ptr msger = *pptr;
        *pptr = NULL;

        __set_false(msger->running);

        if (msger->mtx){
            __xapi->mutex_broadcast(msger->mtx);
        }

        if (msger->spid){
            __xlogd("xmsger_free send process\n");
            __xapi->process_free(msger->spid);
        }

        if (msger->rpid){
            __xlogd("xmsger_free recv process\n");
            __xapi->process_free(msger->rpid);
        }

        if (msger->spipe){
            __xlogd("xmsger_free send pipe\n");
            xpipe_free(&msger->spipe);
        }

        if (msger->rpipe){
            __xlogd("xmsger_free recv pipe\n");
            xpipe_free(&msger->rpipe);
        }        

        if (msger->peers){
            __xlogd("xmsger_free clear peers\n");
            xtree_clear(msger->peers, free_channel);
            __xlogd("xmsger_free peers\n");
            xtree_free(&msger->peers);
        }

        if (msger->squeue){
            __xlogd("xmsger_free send queue\n");
            free(msger->squeue);
        }

        if (msger->mtx){
            __xlogd("xmsger_free mutex\n");
            __xapi->mutex_free(msger->mtx);
        }

        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}