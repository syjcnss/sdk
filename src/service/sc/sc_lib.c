/*
 *            (C) Copyright 2014, DIPCC . Co., Ltd.
 *                    ALL RIGHTS RESERVED
 *
 *  文件名：sc_task_lib.c
 *
 *  创建时间: 2014年12月16日10:23:53
 *  作    者: Larry
 *  描    述: 每一个群呼任务公共库函数
 *  修改历史:
 */

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

/* include public header files */
#include <dos.h>
#include <time.h>

/* include private header files */

#include <dos/dos_py.h>
#include "sc_def.h"
#include "sc_debug.h"
#include "sc_httpd.h"
#include "sc_http_api.h"


/* 定义开发是内置测试数据 */
#define SC_USE_BUILDIN_DATA 0
#if SC_USE_BUILDIN_DATA
const S8 *g_pszCallerList[] = {
    "075526361510", "075526361511", "075526361512"
};
const S8 *g_pszCalledList[] = {
    "18719065681", "18719065682", "18719065683",
    "18719065611", "18719065612", "18719065613",
    "18719065621", "18719065622", "18719065623",
    "18719065631", "18719065632", "18719065633"
};

const S8 *g_pszSiteList[] = {
    "2000", "2001", "2002"
};
#endif
/* 定义开发是内置测试数据END */

/* define marcos */

/* define enums */

/* define structs */

/* 呼叫任务控制模块控制句柄 */
extern SC_TASK_MNGT_ST   *g_pstTaskMngtInfo;

/* 业务控制模块数据库句柄 */
extern DB_HANDLE_ST      *g_pstSCDBHandle;

/* 业务控制块状态 */
static S8 *g_pszSCBStatus[] =
{
    "IDEL",
    "INIT",
    "AUTH",
    "EXEC",
    "ACTIVE",
    "RELEASE"
    ""
};

/*
 * 函数: S8 *sc_scb_get_status(U32 ulStatus)
 * 功能: 根据ulStatus所指定的状态，获取业务控制块的状态
 * 参数:
 *      U32 ulStatus
 * 返回值:
 */
S8 *sc_scb_get_status(U32 ulStatus)
{
    if (ulStatus >= SC_SCB_BUTT)
    {
        DOS_ASSERT(0);

        return "";
    }

    return g_pszSCBStatus[ulStatus];
}

/* declare functions */
/*
 * 函数: SC_SCB_ST *sc_scb_alloc()
 * 功能: 向业务控制模块申请业务控制块
 * 参数:
 * 返回值:
 *      成功返回业务控制的的指针，失败返回NULL
 */
SC_SCB_ST *sc_scb_alloc()
{
    static U32 ulIndex = 0;
    U32 ulLastIndex;
    SC_SCB_ST   *pstSCB = NULL;

    SC_TRACE_IN(ulIndex, 0, 0, 0);

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallList);

    ulLastIndex = ulIndex;

    for (; ulIndex<SC_MAX_SCB_NUM; ulIndex++)
    {
        if (!sc_scb_is_valid(&g_pstTaskMngtInfo->pstCallSCBList[ulIndex]))
        {
            pstSCB = &g_pstTaskMngtInfo->pstCallSCBList[ulIndex];
            break;
        }
    }

    if (!pstSCB)
    {
        for (ulIndex = 0; ulIndex < ulLastIndex; ulIndex++)
        {
            if (!sc_scb_is_valid(&g_pstTaskMngtInfo->pstCallSCBList[ulIndex]))
            {
                pstSCB = &g_pstTaskMngtInfo->pstCallSCBList[ulIndex];
                break;
            }
        }
    }

    if (pstSCB)
    {
        ulIndex++;
        pthread_mutex_lock(&pstSCB->mutexSCBLock);
        sc_scb_init(pstSCB);
        pstSCB->bValid = 1;
        sc_call_trace(pstSCB, "Alloc SCB.");
        pthread_mutex_unlock(&pstSCB->mutexSCBLock);
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallList);
        SC_TRACE_OUT();
        return pstSCB;
    }


    DOS_ASSERT(0);

    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallList);
    SC_TRACE_OUT();
    return NULL;
}

/*
 * 函数: VOID sc_scb_free(SC_SCB_ST *pstSCB)
 * 功能: 释放pstSCB所指向的业务控制块
 * 参数:
 *      SC_SCB_ST *pstSCB : 需要被释放的业务控制块
 * 返回值:
 */
VOID sc_scb_free(SC_SCB_ST *pstSCB)
{
    SC_TRACE_IN((U64)pstSCB, 0, 0, 0);

    if (!pstSCB)
    {
        DOS_ASSERT(0);
        SC_TRACE_OUT();
        return;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallList);
    pthread_mutex_lock(&pstSCB->mutexSCBLock);
    sc_call_trace(pstSCB, "Free SCB.");
    sc_scb_init(pstSCB);
    pthread_mutex_unlock(&pstSCB->mutexSCBLock);
    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallList);

    SC_TRACE_OUT();
    return;
}

/*
 * 函数: BOOL sc_scb_is_valid(SC_SCB_ST *pstSCB)
 * 功能: 检测pstSCB所指向的业务控制释放已经被分配
 * 参数:
 *      SC_SCB_ST *pstSCB : 当前处理的业务控制块
 * 返回值:
 *      返回业务控制的是否已经被分配
 */
BOOL sc_scb_is_valid(SC_SCB_ST *pstSCB)
{
    BOOL ulValid = DOS_FALSE;

    SC_TRACE_IN(pstSCB, 0, 0, 0);

    if (DOS_ADDR_INVALID(pstSCB))
    {
        return DOS_FALSE;
    }

    pthread_mutex_lock(&pstSCB->mutexSCBLock);
    ulValid = pstSCB->bValid;
    pthread_mutex_unlock(&pstSCB->mutexSCBLock);

    SC_TRACE_OUT();

    return ulValid;
}

/*
 * 函数: U32 sc_scb_init(SC_SCB_ST *pstSCB)
 * 功能: 初始化pstSCB所指向的业务控制块
 * 参数:
 *      SC_SCB_ST *pstSCB : 当前处理的业务控制块
 * 返回值:
 *      成功返回DOS_SUCC，失败返回DOS_FAIL
 */
U32 sc_scb_init(SC_SCB_ST *pstSCB)
{
    U32 i;

    SC_TRACE_IN((U64)pstSCB, 0, 0, 0);

    if (!pstSCB)
    {
        SC_TRACE_OUT();
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstSCB->usOtherSCBNo = U16_BUTT;           /* 另外一个leg的SCB编号 */

    pstSCB->usTCBNo = U16_BUTT;                /* 任务控制块编号ID */
    pstSCB->usSiteNo = U16_BUTT;               /* 坐席编号 */

    pstSCB->ulCustomID = U32_BUTT;             /* 当前呼叫属于哪个客户 */
    pstSCB->ulAgentID = U32_BUTT;              /* 当前呼叫属于哪个客户 */
    pstSCB->ulTaskID = U32_BUTT;               /* 当前任务ID */
    pstSCB->ulTrunkID = U32_BUTT;              /* 中继ID */

    pstSCB->ucStatus = SC_SCB_IDEL;            /* 呼叫控制块编号，refer to SC_SCB_STATUS_EN */
    pstSCB->ucServStatus = 0;
    pstSCB->ucTerminationFlag = 0;             /* 业务终止标志 */
    pstSCB->ucTerminationCause = 0;            /* 业务终止原因 */
    pstSCB->ucLegRole = SC_CALL_ROLE_BUTT;
    pstSCB->usHoldCnt = 0;                     /* 被hold的次数 */
    pstSCB->usHoldTotalTime = 0;               /* 被hold的总时长 */
    pstSCB->ulLastHoldTimetamp = 0;            /* 上次hold是的时间戳，解除hold的时候值零 */

    pstSCB->bValid = DOS_FALSE;                /* 是否合法 */
    pstSCB->bTraceNo = DOS_FALSE;              /* 是否跟踪 */
    pstSCB->bBanlanceWarning = DOS_FALSE;      /* 是否余额告警 */
    pstSCB->bNeedConnSite = DOS_FALSE;         /* 接通后是否需要接通坐席 */
    pstSCB->bWaitingOtherRelase = DOS_FALSE;   /* 是否在等待另外一跳退释放 */

    pstSCB->ulCallDuration = 0;                /* 呼叫时长，防止吊死用，每次心跳时更新 */

    pstSCB->szCallerNum[0] = '\0';             /* 主叫号码 */
    pstSCB->szCalleeNum[0] = '\0';             /* 被叫号码 */
    pstSCB->szANINum[0] = '\0';                /* 被叫号码 */
    pstSCB->szDialNum[0] = '\0';               /* 用户拨号 */
    pstSCB->szSiteNum[0] = '\0';               /* 坐席号码 */
    pstSCB->szUUID[0] = '\0';                  /* Leg-A UUID */

    /* 业务类型 列表*/
    pstSCB->ucCurrentSrvInd = 0;               /* 当前空闲的业务类型索引 */
    for (i=0; i<SC_MAX_SRV_TYPE_PRE_LEG; i++)
    {
        pstSCB->aucServiceType[i] = U8_BUTT;
    }

    SC_TRACE_OUT();
    return DOS_SUCC;
}

/*
 * 函数: SC_SCB_ST *sc_scb_get(U32 ulIndex)
 * 功能: 根据业务控制块编号获取业务控制
 * 参数:
 *      U32 ulIndex: 业务控制的标号
 * 返回值:
 *      成功返回业务控制块指向，失败返回NULL
 */
SC_SCB_ST *sc_scb_get(U32 ulIndex)
{
    if (ulIndex >= SC_MAX_SCB_NUM)
    {
        return NULL;
    }

    return &g_pstTaskMngtInfo->pstCallSCBList[ulIndex];
}

/*
 * 函数: static S32 sc_scb_hash_find(VOID *pSymName, HASH_NODE_S *pNode)
 * 功能: 业务控制块hash表中查找一个业务控制块
 * 参数:
 *      VOID *pSymName, HASH_NODE_S *pNode
 * 返回值:
 *      成功返回业务控制块指向，失败返回NULL
 */
static S32 sc_scb_hash_find(VOID *pSymName, HASH_NODE_S *pNode)
{
    SC_SCB_HASH_NODE_ST *pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)pNode;

    if (!pstSCBHashNode)
    {
        return DOS_FAIL;
    }

    if (dos_strncmp(pstSCBHashNode->szUUID, (S8 *)pSymName, sizeof(pstSCBHashNode->szUUID)))
    {
        return DOS_FAIL;
    }

    return DOS_SUCC;
}

/*
 * 函数: static U32 sc_scb_hash_index_get(S8 *pszUUID)
 * 功能: 计算业务控制块hash节点的函数
 * 参数:
 *      S8 *pszUUID 由FS参数的UUID
 * 返回值:
 *      成功返回hash值，失败返回U32_BUTT
 */
static U32 sc_scb_hash_index_get(S8 *pszUUID)
{
    S8   szUUIDHead[16] = { 0 };
    U32  ulIndex;

    SC_TRACE_IN(pszUUID, 0, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    dos_strncpy(szUUIDHead, pszUUID, 8);
    szUUIDHead[8] = '\0';

    if (dos_atoulx(szUUIDHead, &ulIndex) < 0)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    SC_TRACE_OUT();
    return (ulIndex % SC_MAX_SCB_HASH_NUM);
}

U32 sc_scb_hash_tables_add(S8 *pszUUID, SC_SCB_ST *pstSCB)
{
    U32  ulHashIndex;
    SC_SCB_HASH_NODE_ST *pstSCBHashNode;

    SC_TRACE_IN(pszUUID, pstSCB, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    ulHashIndex = sc_scb_hash_index_get(pszUUID);
    if (U32_BUTT == ulHashIndex)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }


    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallHash);
    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)hash_find_node(g_pstTaskMngtInfo->pstCallSCBHash, ulHashIndex, pszUUID, sc_scb_hash_find);
    if (pstSCBHashNode)
    {
        sc_logr_info(SC_TASK, "UUID %s has been added to hash list sometimes before.", pszUUID);
        if (DOS_ADDR_INVALID(pstSCBHashNode->pstSCB)
            && DOS_ADDR_VALID(pstSCB))
        {
            pstSCBHashNode->pstSCB = pstSCB;
        }
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

        SC_TRACE_OUT();
        return DOS_SUCC;
    }

    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)dos_dmem_alloc(sizeof(SC_SCB_HASH_NODE_ST));
    if (!pstSCBHashNode)
    {
        DOS_ASSERT(0);

        sc_logr_warning(SC_TASK, "%s", "Alloc memory fail");
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    HASH_Init_Node((HASH_NODE_S *)&pstSCBHashNode->stNode);
    pstSCBHashNode->pstSCB = NULL;
    pstSCBHashNode->szUUID[0] = '\0';
    sem_init(&pstSCBHashNode->semSCBSyn, 0, 0);

    if (DOS_ADDR_INVALID(pstSCBHashNode->pstSCB)
        && DOS_ADDR_VALID(pstSCB))
    {
        pstSCBHashNode->pstSCB = pstSCB;
    }

    dos_strncpy(pstSCBHashNode->szUUID, pszUUID, sizeof(pstSCBHashNode->szUUID));
    pstSCBHashNode->szUUID[sizeof(pstSCBHashNode->szUUID) - 1] = '\0';

    hash_add_node(g_pstTaskMngtInfo->pstCallSCBHash, (HASH_NODE_S *)pstSCBHashNode, ulHashIndex, NULL);

    sc_logr_warning(SC_TASK, "Add SCB %d with the UUID %s to the hash table.", pstSCB->usSCBNo, pszUUID);

    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

    SC_TRACE_OUT();
    return DOS_SUCC;
}

U32 sc_scb_hash_tables_delete(S8 *pszUUID)
{
    U32  ulHashIndex;
    SC_SCB_HASH_NODE_ST *pstSCBHashNode;

    SC_TRACE_IN(pszUUID, 0, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    ulHashIndex = sc_scb_hash_index_get(pszUUID);
    if (U32_BUTT == ulHashIndex)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallHash);
    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)hash_find_node(g_pstTaskMngtInfo->pstCallSCBHash, ulHashIndex, pszUUID, sc_scb_hash_find);
    if (!pstSCBHashNode)
    {
        DOS_ASSERT(0);

        sc_logr_info(SC_TASK, "Connot find the SCB with the UUID %s where delete the hash node.", pszUUID);
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);
        SC_TRACE_OUT();
        return DOS_SUCC;
    }

    hash_delete_node(g_pstTaskMngtInfo->pstCallSCBHash, (HASH_NODE_S *)pstSCBHashNode, ulHashIndex);
    pstSCBHashNode->pstSCB = NULL;
    pstSCBHashNode->szUUID[sizeof(pstSCBHashNode->szUUID) - 1] = '\0';

    dos_dmem_free(pstSCBHashNode);
    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

    return DOS_SUCC;
}

SC_SCB_ST *sc_scb_hash_tables_find(S8 *pszUUID)
{
    U32  ulHashIndex;
    SC_SCB_HASH_NODE_ST *pstSCBHashNode = NULL;
    SC_SCB_ST           *pstSCB = NULL;

    SC_TRACE_IN(pszUUID, 0, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return NULL;
    }

    ulHashIndex = sc_scb_hash_index_get(pszUUID);
    if (U32_BUTT == ulHashIndex)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return NULL;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallHash);
    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)hash_find_node(g_pstTaskMngtInfo->pstCallSCBHash, ulHashIndex, pszUUID, sc_scb_hash_find);
    if (!pstSCBHashNode)
    {
        sc_logr_info(SC_TASK, "Connot find the SCB with the UUID %s.", pszUUID);
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

        SC_TRACE_OUT();
        return NULL;
    }
    pstSCB = pstSCBHashNode->pstSCB;
    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

    SC_TRACE_OUT();
    return pstSCBHashNode->pstSCB;
}

U32 sc_scb_syn_wait(S8 *pszUUID)
{
    U32  ulHashIndex;
    SC_SCB_HASH_NODE_ST *pstSCBHashNode = NULL;
    SC_SCB_ST           *pstSCB = NULL;

    SC_TRACE_IN(pszUUID, 0, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    ulHashIndex = sc_scb_hash_index_get(pszUUID);
    if (U32_BUTT == ulHashIndex)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallHash);
    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)hash_find_node(g_pstTaskMngtInfo->pstCallSCBHash, ulHashIndex, pszUUID, sc_scb_hash_find);
    if (!pstSCBHashNode)
    {
        sc_logr_info(SC_TASK, "Connot find the SCB with the UUID %s.", pszUUID);
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }
    pstSCB = pstSCBHashNode->pstSCB;
    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

    sem_wait(&pstSCBHashNode->semSCBSyn);

    return DOS_SUCC;

}

U32 sc_scb_syn_post(S8 *pszUUID)
{
    U32  ulHashIndex;
    SC_SCB_HASH_NODE_ST *pstSCBHashNode = NULL;
    SC_SCB_ST           *pstSCB = NULL;

    SC_TRACE_IN(pszUUID, 0, 0, 0);

    if (!pszUUID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    ulHashIndex = sc_scb_hash_index_get(pszUUID);
    if (U32_BUTT == ulHashIndex)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexCallHash);
    pstSCBHashNode = (SC_SCB_HASH_NODE_ST *)hash_find_node(g_pstTaskMngtInfo->pstCallSCBHash, ulHashIndex, pszUUID, sc_scb_hash_find);
    if (!pstSCBHashNode)
    {
        sc_logr_info(SC_TASK, "Connot find the SCB with the UUID %s.", pszUUID);
        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }
    pstSCB = pstSCBHashNode->pstSCB;
    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexCallHash);

    sem_post(&pstSCBHashNode->semSCBSyn);

    SC_TRACE_OUT();
    return DOS_SUCC;

}


/*
 * 函数: BOOL sc_call_check_service(SC_SCB_ST *pstSCB, U32 ulService)
 * 功能: 判断当前业务控制块是否包含ulService所指定的业务
 * 参数:
 *      SC_SCB_ST *pstSCB: 业务控制块
 *      U32 ulService:     业务类型
 * 返回值:
 *      如果包含就返回TRUE，不包含返回DOS_FALSE
 */
BOOL sc_call_check_service(SC_SCB_ST *pstSCB, U32 ulService)
{
    U32 ulIndex;

    if (DOS_ADDR_INVALID(pstSCB) || ulService >= SC_SERV_BUTT)
    {
        return DOS_FALSE;
    }

    for (ulIndex=0; ulIndex<SC_MAX_SRV_TYPE_PRE_LEG; ulIndex++)
    {
        if (ulService == pstSCB->aucServiceType[ulIndex])
        {
            return DOS_TRUE;
        }
    }

    return DOS_FALSE;
}


U32 sc_call_set_trunk(SC_SCB_ST *pstSCB, U32 ulTrunkID)
{
    SC_TRACE_IN((U64)pstSCB, ulTrunkID, 0, 0);

    if (!pstSCB)
    {
        SC_TRACE_OUT();
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pthread_mutex_lock(&pstSCB->mutexSCBLock);
    pstSCB->ulTrunkID = ulTrunkID;
    pthread_mutex_unlock(&pstSCB->mutexSCBLock);

    SC_TRACE_OUT();
    return DOS_SUCC;

}


BOOL sc_tcb_get_valid(SC_TASK_CB_ST *pstTCB)
{
    BOOL bValid = DOS_FALSE;

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        return DOS_FALSE;
    }

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    bValid = pstTCB->ucValid;
    pthread_mutex_lock(&pstTCB->mutexTaskList);

    return bValid;
}

SC_TASK_CB_ST *sc_tcb_alloc()
{
    static U32 ulIndex = 0;
    U32 ulLastIndex;
    SC_TASK_CB_ST  *pstTCB = NULL;

    SC_TRACE_IN(ulIndex, 0, 0, 0);

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexTCBList);

    ulLastIndex = ulIndex;

    for (; ulIndex<SC_MAX_TASK_NUM; ulIndex++)
    {
        if (!g_pstTaskMngtInfo->pstTaskList[ulIndex].ucValid)
        {
            pstTCB = &g_pstTaskMngtInfo->pstTaskList[ulIndex];
            break;
        }
    }

    if (!pstTCB)
    {
        for (ulIndex = 0; ulIndex < ulLastIndex; ulIndex++)
        {
            if (!g_pstTaskMngtInfo->pstTaskList[ulIndex].ucValid)
            {
                pstTCB = &g_pstTaskMngtInfo->pstTaskList[ulIndex];
                break;
            }
        }
    }

    if (pstTCB)
    {
        pthread_mutex_lock(&pstTCB->mutexTaskList);
        sc_tcb_init(pstTCB);
        pstTCB->ucValid = 1;
        pthread_mutex_unlock(&pstTCB->mutexTaskList);

        sc_task_trace(pstTCB, "Alloc TCB %d.", pstTCB->usTCBNo);

        pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexTCBList);
        SC_TRACE_OUT();
        return pstTCB;
    }

    DOS_ASSERT(0);

    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexTCBList);
    SC_TRACE_OUT();
    return NULL;

}

VOID sc_tcb_free(SC_TASK_CB_ST *pstTCB)
{
    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);
        SC_TRACE_OUT();
        return;
    }

    sc_task_trace(pstTCB, "Free TCB. %d", pstTCB->usTCBNo);

    pthread_mutex_lock(&g_pstTaskMngtInfo->mutexTCBList);

    sc_tcb_init(pstTCB);
    pstTCB->ucValid = 0;

    pthread_mutex_unlock(&g_pstTaskMngtInfo->mutexTCBList);
    SC_TRACE_OUT();
    return;
}

U32 sc_tcb_init(SC_TASK_CB_ST *pstTCB)
{
    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        SC_TRACE_OUT();
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstTCB->ucTaskStatus = SC_TASK_BUTT;
    pstTCB->ucMode = SC_TASK_MODE_BUTT;
    pstTCB->ucPriority = SC_TASK_PRI_NORMAL;
    pstTCB->bTraceON = 0;
    pstTCB->bTraceCallON = 0;

    pstTCB->ulTaskID = U32_BUTT;
    pstTCB->ulCustomID = U32_BUTT;
    pstTCB->ulCurrentConcurrency = 0;
    pstTCB->ulMaxConcurrency = 10;
    pstTCB->usSiteCount = 0;
    pstTCB->ulAgentQueueID = U32_BUTT;
    pstTCB->ucAudioPlayCnt = 0;
    pstTCB->ulCalleeCount = 0;
    pstTCB->usCallerCount = 0;

    dos_list_init(&pstTCB->stCalleeNumQuery);    /* TODO: 释放所有节点 */
    pstTCB->pstCallerNumQuery = NULL;   /* TODO: 初始化所有节点 */
    pstTCB->szAudioFileLen[0] = '\0';
    dos_memzero(pstTCB->astPeriod, sizeof(pstTCB->astPeriod));

    /* 统计相关 */
    pstTCB->ulTotalCall = 0;
    pstTCB->ulCallFailed = 0;
    pstTCB->ulCallConnected = 0;

    SC_TRACE_OUT();
    return DOS_SUCC;
}

SC_TASK_CB_ST *sc_tcb_get_by_id(U32 ulTCBNo)
{
    SC_TRACE_IN((U32)ulTCBNo, 0, 0, 0);

    if (ulTCBNo >= SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return NULL;
    }

    SC_TRACE_OUT();
    return &g_pstTaskMngtInfo->pstTaskList[ulTCBNo];
}

SC_TASK_CB_ST *sc_tcb_find_by_taskid(U32 ulTaskID)
{
    U32 ulIndex;
    SC_TRACE_IN((U32)ulTaskID, 0, 0, 0);

    for (ulIndex=0; ulIndex<SC_MAX_TASK_NUM; ulIndex++)
    {
        if (g_pstTaskMngtInfo->pstTaskList[ulIndex].ucValid
            && g_pstTaskMngtInfo->pstTaskList[ulIndex].ulTaskID == ulTaskID)
        {
            SC_TRACE_OUT();
            return &g_pstTaskMngtInfo->pstTaskList[ulIndex];
        }
    }

    SC_TRACE_OUT();
    return NULL;
}


VOID sc_task_set_current_call_cnt(SC_TASK_CB_ST *pstTCB, U32 ulCurrentCall)
{
    SC_TRACE_IN((U64)pstTCB, ulCurrentCall, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return;
    }

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    pstTCB->ulCurrentConcurrency = ulCurrentCall;
    pthread_mutex_unlock(&pstTCB->mutexTaskList);
}

U32 sc_task_get_current_call_cnt(SC_TASK_CB_ST *pstTCB)
{
    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    return pstTCB->ulCurrentConcurrency;
}

VOID sc_task_set_owner(SC_TASK_CB_ST *pstTCB, U32 ulTaskID, U32 ulCustomID)
{
    SC_TRACE_IN((U64)pstTCB, ulTaskID, ulCustomID, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return;
    }

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return;
    }

    if (0 == ulCustomID || U32_BUTT == ulCustomID)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return;
    }

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    pstTCB->ulTaskID = ulTaskID;
    pstTCB->ulCustomID = ulCustomID;
    pthread_mutex_unlock(&pstTCB->mutexTaskList);
}

static S32 sc_task_load_caller_callback(VOID *pArg, S32 lArgc, S8 **pszValues, S8 **pszNames)
{
    SC_TASK_CB_ST            *pstTCB;
    S8                       *pszCustmerID = NULL, *pszCaller = NULL, *pszID = NULL;
    U32                      ulFirstInvalidNode = U32_BUTT, ulIndex = 0;
    U32                      ulCustomerID = 0, ulID = 0;
    BOOL                     blNeedAdd = DOS_TRUE;

    pstTCB = (SC_TASK_CB_ST *)pArg;
    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (DOS_ADDR_INVALID(pszValues) || DOS_ADDR_INVALID(pszNames))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (DOS_ADDR_INVALID(pstTCB->pstCallerNumQuery))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (lArgc < 3)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    for (ulIndex=0; ulIndex<lArgc; ulIndex++)
    {
        if (dos_strcmp(pszNames[ulIndex], "id") == 0)
        {
            pszID = pszValues[ulIndex];
        }
        else if (dos_strcmp(pszNames[ulIndex], "customer") == 0)
        {
            pszCustmerID = pszValues[ulIndex];
        }
        else if (dos_strcmp(pszNames[ulIndex], "caller") == 0)
        {
            pszCaller = pszValues[ulIndex];
        }
    }

    if (DOS_ADDR_INVALID(pszID) || DOS_ADDR_INVALID(pszCustmerID) || DOS_ADDR_INVALID(pszCaller))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if ('\0' == pszID[0] || '\0' == pszCustmerID[0] || '\0' == pszCaller[0])
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (dos_atoul(pszID, &ulID) < 0)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (dos_atoul(pszCustmerID, &ulCustomerID) < 0)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (dos_strlen(pszCaller) >= SC_TEL_NUMBER_LENGTH)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    /* 检测号码是否重复了，并且找到一个空闲的控制块 */
    for (ulIndex=0; ulIndex<SC_MAX_CALLER_NUM; ulIndex++)
    {
        if (!pstTCB->pstCallerNumQuery[ulIndex].bValid
            && U32_BUTT == ulFirstInvalidNode)
        {
            ulFirstInvalidNode = ulIndex;
        }

        if (pstTCB->pstCallerNumQuery[ulIndex].bValid
            && dos_strcmp(pstTCB->pstCallerNumQuery[ulIndex].szNumber, pszCaller) == 0)
        {
            blNeedAdd = DOS_FALSE;
        }
    }

    if (ulFirstInvalidNode >= SC_MAX_CALLER_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    if (!blNeedAdd)
    {
        SC_TRACE_OUT();
        return DOS_SUCC;
    }

    pstTCB->pstCallerNumQuery[ulFirstInvalidNode].bValid = 1;
    pstTCB->pstCallerNumQuery[ulFirstInvalidNode].ulIndexInDB = ulID;
    pstTCB->pstCallerNumQuery[ulFirstInvalidNode].bTraceON = pstTCB->bTraceCallON;
    dos_strncpy(pstTCB->pstCallerNumQuery[ulFirstInvalidNode].szNumber, pszCaller, SC_MAX_CALLER_NUM);
    pstTCB->pstCallerNumQuery[ulFirstInvalidNode].szNumber[SC_MAX_CALLER_NUM - 1] = '\0';
    pstTCB->usCallerCount++;

    SC_TRACE_OUT();
    return DOS_TRUE;
}

U32 sc_task_load_caller(SC_TASK_CB_ST *pstTCB)
{
#if SC_USE_BUILDIN_DATA
    U32 ulIndex;
    S32 lCnt;

    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    if (!g_pstTaskMngtInfo)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    lCnt = 0;

    /* 检测号码是否重复了，并且找到一个空闲的控制块 */
    for (ulIndex=0; ulIndex<sizeof(g_pszCallerList)/sizeof(S8 *); ulIndex++)
    {
        pstTCB->pstCallerNumQuery[ulIndex].bValid   = 1;
        pstTCB->pstCallerNumQuery[ulIndex].bTraceON = 1;
        pstTCB->pstCallerNumQuery[ulIndex].ulIndexInDB = ulIndex;
        dos_strncpy(pstTCB->pstCallerNumQuery[ulIndex].szNumber, g_pszCallerList[ulIndex], SC_TEL_NUMBER_LENGTH);
        pstTCB->pstCallerNumQuery[ulIndex].szNumber[SC_TEL_NUMBER_LENGTH - 1] = '\0';

        lCnt++;
    }

    SC_TRACE_OUT();
    return lCnt;
#else
    S8 szSqlQuery[1024]= { 0 };
    U32 ulLength, ulResult;


    ulLength = dos_snprintf(szSqlQuery
                , sizeof(szSqlQuery)
                , "SELECT tbl_caller.id as id, tbl_caller.customer_id as customer, tbl_caller.cid as caller from tbl_caller WHERE tbl_caller.customer_id=%d"
                , pstTCB->ulCustomID);

    ulResult = db_query(g_pstSCDBHandle
                            , szSqlQuery
                            , sc_task_load_caller_callback
                            , pstTCB
                            , NULL);

    if (ulResult != DOS_SUCC)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    SC_TRACE_OUT();
    return DOS_SUCC;
#endif
}

static S32 sc_task_load_callee_callback(VOID *pArg, S32 lArgc, S8 **pszValues, S8 **pszNames)
{
    SC_TASK_CB_ST *pstTCB = NULL;
    SC_TEL_NUM_QUERY_NODE_ST *pstCallee;
    U32 ulIndex;
    S8 *pszIndex = NULL;
    S8 *pszNum = NULL;

    if (DOS_ADDR_INVALID(pArg)
        || DOS_ADDR_INVALID(pszValues)
        || DOS_ADDR_INVALID(pszNames))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstTCB = (SC_TASK_CB_ST *)pArg;
    pszIndex = pszValues[0];
    pszNum = pszValues[1];

    if (DOS_ADDR_INVALID(pszIndex)
        || DOS_ADDR_INVALID(pszNum)
        || dos_atoul(pszIndex, &ulIndex) < 0)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstCallee = (SC_TEL_NUM_QUERY_NODE_ST *)dos_dmem_alloc(sizeof(SC_TEL_NUM_QUERY_NODE_ST));
    if (DOS_ADDR_INVALID(pstCallee))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    dos_list_node_init(&pstCallee->stLink);
    pstCallee->ulIndex = ulIndex;
    pstCallee->ucTraceON = 0;
    pstCallee->ucCalleeType = SC_CALL_NUM_TYPE_NORMOAL;
    dos_strncpy(pstCallee->szNumber, pszNum, sizeof(pstCallee->szNumber));
    pstCallee->szNumber[sizeof(pstCallee->szNumber) - 1] = '\0';

    pstTCB->ulCalleeCount++;
    pstTCB->ulLastCalleeIndex++;
    dos_list_add_tail(&pstTCB->stCalleeNumQuery, (struct list_s *)pstCallee);

    return 0;
}

U32 sc_task_load_callee(SC_TASK_CB_ST *pstTCB)
{
#if SC_USE_BUILDIN_DATA
    SC_TEL_NUM_QUERY_NODE_ST *pstCalledNode;
    U32 ulIndex;
    S32 lCnt;

    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    lCnt = 0;

    for (ulIndex=0; ulIndex<sizeof(g_pszCalledList)/sizeof(S8 *); ulIndex++)
    {
        pstCalledNode = (SC_TEL_NUM_QUERY_NODE_ST *)dos_dmem_alloc(sizeof(SC_TEL_NUM_QUERY_NODE_ST));
        if (!pstCalledNode)
        {
            break;
        }

        pstCalledNode->ucTraceON = 1;
        pstCalledNode->ucCalleeType = SC_CALL_NUM_TYPE_NORMOAL;
        dos_strncpy(pstCalledNode->szNumber, g_pszCalledList[ulIndex], SC_TEL_NUMBER_LENGTH);
        pstCalledNode->szNumber[SC_TEL_NUMBER_LENGTH - 1] = '\0';

        dos_list_add_head(&pstTCB->stCalleeNumQuery, &pstCalledNode->stLink);

        lCnt++;
    }

    SC_TRACE_OUT();
    return lCnt;
#else
    S8 szSQL[512] = { 0, };

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    pstTCB->ulCalleeCount = 0;

    dos_snprintf(szSQL, sizeof(szSQL)
                    , "SELECT "
                      "  a.id, a.regex_number "
                      "FROM "
                      "  tbl_callee a "
                      "LEFT JOIN "
                      "    (SELECT "
                      "      c.id as id "
                      "    FROM "
                      "      tbl_calleefile c "
                      "    LEFT JOIN "
                      "      tbl_calltask d "
                      "    ON "
                      "      c.id = d.callee_id AND d.id = %d) b "
                      "ON "
                      "  a.calleefile_id = b.id AND a.`status` <> 3 "
                      "LIMIT %d, 1000;"
                    , pstTCB->usTCBNo
                    , pstTCB->ulLastCalleeIndex);

    if (DB_ERR_SUCC != db_query(g_pstSCDBHandle, szSQL, sc_task_load_callee_callback, (VOID *)pstTCB, NULL))
    {
        return DOS_FAIL;
    }

    return DOS_SUCC;
#endif
}

static S32 sc_task_load_period_cb(VOID *pArg, S32 lColumnCount, S8 **aszValues, S8 **aszNames)
{
    S8 *pszStarTime = NULL, *pszEndTime = NULL;
    S32 lStartHour, lStartMin, lStartSecond;
    S32 lEndHour, lEndMin, lEndSecond;
    SC_TASK_CB_ST *pstTCB = NULL;
    U32 ulIndex;

    if (DOS_ADDR_INVALID(pArg)
        || DOS_ADDR_INVALID(aszValues)
        || DOS_ADDR_INVALID(aszNames))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstTCB = (SC_TASK_CB_ST *)pArg;
    pszStarTime = aszValues[0];
    pszEndTime = aszValues[1];

    if (DOS_ADDR_INVALID(pszStarTime)
        || DOS_ADDR_INVALID(pszEndTime))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    if (dos_sscanf(pszStarTime, "%d:%d:%d", &lStartHour, &lStartMin, &lStartSecond) != 3
        || dos_sscanf(pszEndTime, "%d:%d:%d", &lEndHour, &lEndMin, &lEndSecond) != 3)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    if (lStartHour > 23 || lEndHour > 23
        || lStartMin > 59 || lEndMin > 59
        || lStartSecond > 59 || lEndSecond > 59)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    for (ulIndex=0; ulIndex<SC_MAX_PERIOD_NUM; ulIndex++)
    {
        if (!pstTCB->astPeriod[0].ucValid)
        {
            break;
        }
    }

    if (ulIndex >= SC_MAX_PERIOD_NUM)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstTCB->astPeriod[ulIndex].ucValid = DOS_TRUE;
    pstTCB->astPeriod[ulIndex].ucWeekMask = 0xFF;

    pstTCB->astPeriod[ulIndex].ucHourBegin = (U8)lStartHour;
    pstTCB->astPeriod[ulIndex].ucMinuteBegin = (U8)lStartMin;
    pstTCB->astPeriod[ulIndex].ucSecondBegin = (U8)lStartSecond;

    pstTCB->astPeriod[ulIndex].ucHourEnd = (U8)lEndHour;
    pstTCB->astPeriod[ulIndex].ucMinuteEnd = (U8)lEndMin;
    pstTCB->astPeriod[ulIndex].ucSecondEnd= (U8)lEndSecond;

    return DOS_SUCC;
}

U32 sc_task_load_period(SC_TASK_CB_ST *pstTCB)
{
#if SC_USE_BUILDIN_DATA

    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    pstTCB->astPeriod[0].ucValid = 1;

    pstTCB->astPeriod[0].ucWeekMask = 0xFF;

    pstTCB->astPeriod[0].ucHourBegin = 0;
    pstTCB->astPeriod[0].ucMinuteBegin = 0;

    pstTCB->astPeriod[0].ucHourEnd = 23;
    pstTCB->astPeriod[0].ucMinuteEnd = 59;

    SC_TRACE_OUT();
    return 1;

#else
    S8 szSQL[128] = { 0, };

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    dos_snprintf(szSQL, sizeof(szSQL), "SELECT start_time, end_time from tbl_calltask WHERE id=%d;", pstTCB->ulTaskID);

    if (db_query(g_pstSCDBHandle, szSQL, sc_task_load_period_cb, (VOID *)pstTCB, NULL) != DB_ERR_SUCC)
    {
        sc_logr_debug(SC_TASK, "Load task time period for task %d fail;", pstTCB->usTCBNo);

        return DOS_FAIL;
    }

    return DOS_SUCC;
#endif
}

S32 sc_task_load_other_info_cb(VOID *pArg, S32 lColumnCount, S8 **aszValues, S8 **aszNames)
{
    U32 ulTaskMode;
    U32 ulMaxConcurrency;
    SC_TASK_CB_ST *pstTCB;

    if (DOS_ADDR_INVALID(pArg)
        || lColumnCount<= 0
        || DOS_ADDR_INVALID(aszValues)
        || DOS_ADDR_INVALID(aszNames))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstTCB = (SC_TASK_CB_ST *)pArg;

    if (DOS_ADDR_INVALID(aszValues[0])
        || '\0' == aszValues[0][0]
        || dos_atoul(aszValues[0], &ulTaskMode) < 0)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    if (DOS_ADDR_INVALID(aszValues[1])
        || '\0' == aszValues[1][0]
        || dos_atoul(aszValues[1], &ulMaxConcurrency) < 0)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    pstTCB->ucMode = (U8)ulTaskMode;
    pstTCB->ulMaxConcurrency = ulMaxConcurrency;
    return DOS_SUCC;

}

S32 sc_task_load_other_info(SC_TASK_CB_ST *pstTCB)
{
    S8 szSQL[128] = { 0, };

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    dos_snprintf(szSQL, sizeof(szSQL), "SELECT mode,max_concurrent from tbl_calltask WHERE id=%d;", pstTCB->ulTaskID);

    if (db_query(g_pstSCDBHandle, szSQL, sc_task_load_other_info_cb, (VOID *)pstTCB, NULL) != DB_ERR_SUCC)
    {
        sc_logr_debug(SC_TASK, "Load task time period for task %d fail;", pstTCB->usTCBNo);

        return DOS_FAIL;
    }

    return DOS_SUCC;

}

S32 sc_task_load_agent_queue_id_cb(VOID *pArg, S32 lColumnCount, S8 **aszValues, S8 **aszNames)
{
    U32 ulAgentQueueID;

    if (DOS_ADDR_INVALID(pArg)
        || lColumnCount<= 0
        || DOS_ADDR_INVALID(aszValues)
        || DOS_ADDR_INVALID(aszNames))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    if (DOS_ADDR_INVALID(aszValues[0])
        || '\0' == aszValues[0][0]
        || dos_atoul(aszValues[0], &ulAgentQueueID) < 0)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    *(U32 *)pArg = ulAgentQueueID;
    return DOS_SUCC;
}

S32 sc_task_load_agent_number_cb(VOID *pArg, S32 lColumnCount, S8 **aszValues, S8 **aszNames)
{
    U32 ulAgentCount;

    if (DOS_ADDR_INVALID(pArg)
        || lColumnCount<= 0
        || DOS_ADDR_INVALID(aszValues)
        || DOS_ADDR_INVALID(aszNames))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    if (DOS_ADDR_INVALID(aszValues[0])
        || '\0' == aszValues[0][0]
        || dos_atoul(aszValues[0], &ulAgentCount) < 0)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    *(U16 *)pArg = (U16)ulAgentCount;
    return DOS_SUCC;

}

U32 sc_task_load_agent_info(SC_TASK_CB_ST *pstTCB)
{
    /* 初始化坐席队列编号 */
    S8 szSQL[128] = { 0, };

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    dos_snprintf(szSQL, sizeof(szSQL)
                    , "SELECT group_id FROM tbl_calltask WHERE tbl_calltask.id = %d;"
                    , pstTCB->ulTaskID);
    if (db_query(g_pstSCDBHandle, szSQL, sc_task_load_agent_queue_id_cb, (VOID *)&pstTCB->ulAgentQueueID, NULL) != DB_ERR_SUCC)
    {
        DOS_ASSERT(0);
        sc_logr_debug(SC_TASK, "Load agent queue ID for task %d FAIL;", pstTCB->ulTaskID);

        return DOS_FAIL;
    }

    dos_snprintf(szSQL, sizeof(szSQL)
                    , "SELECT count(*) as number FROM tbl_agent WHERE tbl_agent.group1_id=%d OR tbl_agent.group2_id=%d;"
                    , pstTCB->ulAgentQueueID
                    , pstTCB->ulAgentQueueID);
    if (db_query(g_pstSCDBHandle, szSQL, sc_task_load_agent_number_cb, (VOID *)&pstTCB->usSiteCount, NULL) != DB_ERR_SUCC)
    {
        DOS_ASSERT(0);
        sc_logr_debug(SC_TASK, "Load agent number for task %d FAIL;", pstTCB->ulTaskID);

        return DOS_FAIL;
    }

    return DOS_SUCC;
}

U32 sc_task_update_stat(SC_TASK_CB_ST *pstTCB)
{
    return 0;
}

U32 sc_task_load_audio(SC_TASK_CB_ST *pstTCB)
{
#if SC_USE_BUILDIN_DATA
    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return -1;
    }

    pstTCB->ucAudioPlayCnt = 3;
    dos_strncpy(pstTCB->szAudioFileLen, "/usr/local/freeswitch/sounds/mmt1_speex_8k.wav", SC_MAX_AUDIO_FILENAME_LEN);
    pstTCB->szAudioFileLen[SC_MAX_AUDIO_FILENAME_LEN - 1] = '\0';

    SC_TRACE_OUT();
    return 1;
#else
    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }

    dos_strncpy(pstTCB->szAudioFileLen, "/usr/local/freeswitch/sounds/en/us/callie/ivr/8000/ivr-you_lose.wav", sizeof(pstTCB->szAudioFileLen));
    pstTCB->szAudioFileLen[sizeof(pstTCB->szAudioFileLen) - 1] = '\0';
    pstTCB->ucAudioPlayCnt = 3;
    return DOS_SUCC;
#endif
}

U32 sc_task_check_can_call_by_time(SC_TASK_CB_ST *pstTCB)
{
    time_t     now;
    struct tm  *timenow;
    U32 ulWeek, ulHour, ulMinute, ulIndex;

    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);
        SC_TRACE_OUT();
        return DOS_FALSE;
    }

    time(&now);
    timenow = localtime(&now);

    ulWeek = timenow->tm_wday;
    ulHour = timenow->tm_hour;
    ulMinute = timenow->tm_min;

    printf("Current Time %02d:%02d\r\n", ulHour, ulMinute);

    for (ulIndex=0; ulIndex<SC_MAX_PERIOD_NUM; ulIndex++)
    {
        if (!pstTCB->astPeriod[ulIndex].ucValid)
        {
            continue;
        }

        printf("Time peroid: %02d:%02d-%02d:%02d\r\n"
                , pstTCB->astPeriod[ulIndex].ucHourBegin, pstTCB->astPeriod[ulIndex].ucMinuteBegin
                , pstTCB->astPeriod[ulIndex].ucHourEnd, pstTCB->astPeriod[ulIndex].ucMinuteEnd);

        if (!((pstTCB->astPeriod[ulIndex].ucWeekMask >> ulWeek) & 0x01))
        {
            continue;
        }

        if ((ulHour < pstTCB->astPeriod[ulIndex].ucHourBegin
            || ulHour > pstTCB->astPeriod[ulIndex].ucHourEnd))
        {
            continue;
        }

        if (ulMinute < pstTCB->astPeriod[ulIndex].ucMinuteBegin
            || ulMinute > pstTCB->astPeriod[ulIndex].ucMinuteEnd)
        {
            continue;
        }

        SC_TRACE_OUT();
        return DOS_TRUE;
    }

    DOS_ASSERT(0);
    SC_TRACE_OUT();
    return DOS_FALSE;
}

U32 sc_task_check_can_call_by_status(SC_TASK_CB_ST *pstTCB)
{
    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);
        SC_TRACE_OUT();
        return DOS_FALSE;
    }

    if (pstTCB->ulCurrentConcurrency >= (pstTCB->usSiteCount * SC_MAX_CALL_MULTIPLE))
    {
        SC_TRACE_OUT();
        return DOS_FALSE;
    }

    SC_TRACE_OUT();
    return DOS_TRUE;
}

U32 sc_task_get_call_interval(SC_TASK_CB_ST *pstTCB)
{
    U32 ulPercentage;
    U32 ulInterval;

    SC_TRACE_IN((U64)pstTCB, 0, 0, 0);

    if (!pstTCB)
    {
        DOS_ASSERT(0);
        SC_TRACE_OUT();
        return 1000;
    }

    if (pstTCB->ulCurrentConcurrency)
    {
        ulPercentage = ((pstTCB->usSiteCount * SC_MAX_CALL_MULTIPLE) * 100) / pstTCB->ulCurrentConcurrency;
    }
    else
    {
        ulPercentage = 0;
    }

    if (ulPercentage < 40)
    {
        ulInterval = 300;
    }
    else if (ulPercentage < 80)
    {
        ulInterval = 500;
    }
    else
    {
        ulInterval = 1000;
    }

    SC_TRACE_OUT();
    return ulInterval;
}

S8 *sc_task_get_audio_file(U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return NULL;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return NULL;
    }

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].szAudioFileLen;
}


U32 sc_task_audio_playcnt(U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucAudioPlayCnt;
}

U32 sc_task_concurrency_add(U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->pstTaskList[ulTCBNo].mutexTaskList);
    g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulCurrentConcurrency++;
    if (g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulCurrentConcurrency
        > g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulMaxConcurrency)
    {
        DOS_ASSERT(0);
    }
    pthread_mutex_unlock(&g_pstTaskMngtInfo->pstTaskList[ulTCBNo].mutexTaskList);

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucAudioPlayCnt;
}

U32 sc_task_concurrency_minus (U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return 0;
    }

    pthread_mutex_lock(&g_pstTaskMngtInfo->pstTaskList[ulTCBNo].mutexTaskList);
    if (g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulCurrentConcurrency > 0)
    {
        g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulCurrentConcurrency--;
    }
    else
    {
        DOS_ASSERT(0);
    }
    pthread_mutex_unlock(&g_pstTaskMngtInfo->pstTaskList[ulTCBNo].mutexTaskList);

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucAudioPlayCnt;
}



U32 sc_task_get_timeout_for_noanswer(U32 ulTCBNo)
{
    return 10;
}


U32 sc_task_callee_set_recall(SC_TASK_CB_ST *pstTCB, U32 ulIndex)
{
    S8 szSQL[128];

    dos_snprintf(szSQL, sizeof(szSQL), "UPDATE tbl_callee SET tbl_callee.`status`=\"waiting\" WHERE tbl_callee.id = 0;");

    if (DB_ERR_SUCC != db_query(g_pstSCDBHandle, szSQL, NULL, NULL, NULL))
    {
        DOS_ASSERT(0);

        sc_logr_notice(SC_TASK, "Set callee to waiting status FAIL. index:%d", ulIndex);

        return DOS_FALSE;
    }

    return DOS_TRUE;
}

U32 sc_task_get_mode(U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucMode;
}

U32 sc_task_get_agent_queue(U32 ulTCBNo)
{
    SC_TRACE_IN(ulTCBNo, 0, 0, 0);
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return U32_BUTT;
    }

    return g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ulAgentQueueID;
}


SC_SYS_STATUS_EN sc_check_sys_stat()
{
    return SC_SYS_NORMAL;
}

//------------------------------------------------------

U32 sc_http_gateway_update_proc(U32 ulAction, U32 ulGatewayID)
{
   U32   ulRet = 0;
   
   if (ulAction >= SC_API_CMD_ACTION_BUTT)
   {
      DOS_ASSERT(0);
      return DOS_FAIL;
   }
   
   switch(ulAction)
   {
      case SC_API_CMD_ACTION_GATEWAY_ADD:
      case SC_API_CMD_ACTION_GATEWAY_UPDATE:
#if INCLUDE_SERVICE_PYTHON
         ulRet = py_exec_func("router", "make_route", "(k)", (U64)ulGatewayID);
         if (DOS_SUCC != ulRet)
         {
            DOS_ASSERT(0);
            return DOS_FAIL;
         }
#endif
         break;
      case SC_API_CMD_ACTION_GATEWAY_DELETE: 
#if INCLUDE_SERVICE_PYTHON
         ulRet = py_exec_func("router", "del_route", "(k)", (U64)ulGatewayID);
         if (DOS_SUCC != ulRet)
         {
            DOS_ASSERT(0);
            return DOS_FAIL;
         }
#endif
         break;
      default:
         break;
   }

   return DOS_SUCC;
}

U32 sc_http_sip_update_proc(U32 ulAction, U32 ulSIPID, U32 ulAgentID, U32 ulCustomerID)
{
   U32 ulRet = 0;

   if (ulAction >= SC_API_CMD_ACTION_BUTT)
   {
      DOS_ASSERT(0);
      return DOS_FAIL;
   }

   switch(ulAction)
   {
      case SC_API_CMD_ACTION_SIP_ADD:
      case SC_API_CMD_ACTION_SIP_UPDATE:
#if INCLUDE_SERVICE_PYTHON
          ulRet = py_exec_func("sip_mgnt", "add_sip","(i)", ulSIPID);
          if (ulRet != DOS_SUCC)
          {
              DOS_ASSERT(0);
              return DOS_FAIL;
          }
#endif
          break;
      case SC_API_CMD_ACTION_SIP_DELETE:
#if INCLUDE_SERVICE_PYTHON
          ulRet = py_exec_func("sip_mgnt", "del_sip_from_group","(i,i)", ulAgentID, ulCustomerID);
          if (ulRet != DOS_SUCC)
          {
              DOS_ASSERT(0);
              return DOS_FAIL;
          }
#endif
          break;
      default:
          break;
   }

   return DOS_SUCC;
}


U32 sc_http_route_update_proc(U32 ulAction, U32 ulRouteID)
{
    if (ulAction > SC_API_CMD_ACTION_BUTT)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    switch(ulAction)
    {
        case SC_API_CMD_ACTION_ROUTE_ADD:
        case SC_API_CMD_ACTION_ROUTE_DELETE:
        case SC_API_CMD_ACTION_ROUTE_UPDATE:
            break;
        default:
            break;
    }
    return DOS_SUCC;
}

U32 sc_http_gw_group_update_proc(U32 ulAction, U32 ulGwGroupID)
{
    if (ulAction > SC_API_CMD_ACTION_BUTT)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    switch(ulAction)
    {
        case SC_API_CMD_ACTION_GW_GROUP_ADD:
        case SC_API_CMD_ACTION_GW_GROUP_DELETE:
            break;
        default:
            break;
    }
    return DOS_SUCC;
}

U32 sc_http_did_update_proc(U32 ulAction, U32 ulDidID)
{
    if (ulAction > SC_API_CMD_ACTION_BUTT)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    switch(ulAction)
    {
        case SC_API_CMD_ACTION_DID_ADD:
        case SC_API_CMD_ACTION_DID_DELETE:
        case SC_API_CMD_ACTION_DID_UPDATE:
            break;
        default:
            break;
    }
    return DOS_SUCC;
}


//--------------------------------------------------

#ifdef __cplusplus
}
#endif /* __cplusplus */


