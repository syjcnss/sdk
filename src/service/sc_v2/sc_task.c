/*
 *            (C) Copyright 2014, DIPCC . Co., Ltd.
 *                    ALL RIGHTS RESERVED
 *
 *  文件名：sc_task.c
 *
 *  创建时间: 2014年12月16日10:23:53
 *  作    者: Larry
 *  描    述: 每一个群呼任务的实现
 *  修改历史:
 */

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

/* include public header files */
#include <dos.h>
#include <esl.h>

/* include private header files */
#include "sc_def.h"
#include "sc_res.h"
#include "sc_debug.h"
#include "sc_db.h"
#include "bs_pub.h"
#include "sc_pub.h"
#include "sc_http_api.h"

/* define marcos */

/* define enums */

/* define structs */


/* 任务列表 refer to struct tagTaskCB*/
SC_TASK_CB           *g_pstTaskList  = NULL;
pthread_mutex_t      g_mutexTaskList = PTHREAD_MUTEX_INITIALIZER;

U32 sc_task_call_result_make_call_before(U32 ulCustomerID, U32 ulTaskID, S8 *szCallingNum, S8 *szCalleeNum, U32 ulSIPRspCode)
{
    SC_DB_MSG_CALL_RESULT_ST *pstCallResult     = NULL;

    pstCallResult = dos_dmem_alloc(sizeof(SC_DB_MSG_CALL_RESULT_ST));
    if (DOS_ADDR_INVALID(pstCallResult))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Analysis call result for task: %u, SIP Code:%u", ulTaskID, ulSIPRspCode);

    dos_memzero(pstCallResult, sizeof(SC_DB_MSG_CALL_RESULT_ST));
    pstCallResult->ulCustomerID = ulCustomerID;
    pstCallResult->ulTaskID = ulTaskID;

    /* 主叫号码 */
    if (DOS_ADDR_VALID(szCallingNum))
    {
        dos_snprintf(pstCallResult->szCaller, sizeof(pstCallResult->szCaller), "%s", szCallingNum);
    }

    /* 被叫号码 */
    if (DOS_ADDR_VALID(szCalleeNum))
    {
        dos_snprintf(pstCallResult->szCallee, sizeof(pstCallResult->szCallee), "%s", szCalleeNum);
    }

    pstCallResult->ulAnswerTimeStamp = time(NULL);
    switch (ulSIPRspCode)
    {
        case CC_ERR_SC_CALLER_NUMBER_ILLEGAL:
        case CC_ERR_SC_CALLEE_NUMBER_ILLEGAL:
            pstCallResult->ulResult = CC_RST_CALLING_NUM_INVALID;
            break;

        default:
            pstCallResult->ulResult = CC_RST_CONNECT_FAIL;
            break;
    }

    pstCallResult->stMsgTag.ulMsgType = SC_MSG_SAVE_CALL_RESULT;

    return sc_send_msg2db((SC_DB_MSG_TAG_ST *)pstCallResult);
}

U16 sc_task_transform_errcode_from_sc2sip(U32 ulErrcode)
{
    U16 usErrcodeSC = CC_ERR_SIP_UNDECIPHERABLE;

    if (ulErrcode >= CC_ERR_BUTT)
    {
        DOS_ASSERT(0);
        return usErrcodeSC;
    }

    if (ulErrcode < 1000 && ulErrcode > 99)
    {
        /* 1000以下为sip错误码，不需要转换 */
        return ulErrcode;
    }

    switch (ulErrcode)
    {
        case CC_ERR_NORMAL_CLEAR:
            usErrcodeSC = CC_ERR_SIP_SUCC;
            break;
        case CC_ERR_NO_REASON:
            usErrcodeSC = CC_ERR_SIP_BUSY_EVERYWHERE;
            break;
        case CC_ERR_SC_SERV_NOT_EXIST:
        case CC_ERR_SC_NO_SERV_RIGHTS:
        case CC_ERR_SC_USER_DOES_NOT_EXIST:
        case CC_ERR_SC_CUSTOMERS_NOT_EXIST:
            usErrcodeSC = CC_ERR_SIP_FORBIDDEN;
            break;
        case CC_ERR_SC_USER_OFFLINE:
        case CC_ERR_SC_USER_HAS_BEEN_LEFT:
        case CC_ERR_SC_PERIOD_EXCEED:
        case CC_ERR_SC_RESOURCE_EXCEED:
            usErrcodeSC = CC_ERR_SIP_TEMPORARILY_UNAVAILABLE;
            break;
        case CC_ERR_SC_USER_BUSY:
            usErrcodeSC = CC_ERR_SIP_BUSY_HERE;
            break;
        case CC_ERR_SC_CB_ALLOC_FAIL:
        case CC_ERR_SC_MEMORY_ALLOC_FAIL:
            usErrcodeSC = CC_ERR_SIP_INTERNAL_SERVER_ERROR;
            break;
        case CC_ERR_SC_IN_BLACKLIST:
        case CC_ERR_SC_CALLER_NUMBER_ILLEGAL:
        case CC_ERR_SC_CALLEE_NUMBER_ILLEGAL:
            usErrcodeSC = CC_ERR_SIP_NOT_FOUND;
            break;
        case CC_ERR_SC_NO_ROUTE:
        case CC_ERR_SC_NO_TRUNK:
            break;
        case CC_ERR_SC_MESSAGE_TIMEOUT:
        case CC_ERR_SC_AUTH_TIMEOUT:
        case CC_ERR_SC_QUERY_TIMEOUT:
            usErrcodeSC = CC_ERR_SIP_REQUEST_TIMEOUT;
            break;
        case CC_ERR_SC_CONFIG_ERR:
        case CC_ERR_SC_MESSAGE_PARAM_ERR:
        case CC_ERR_SC_MESSAGE_SENT_ERR:
        case CC_ERR_SC_MESSAGE_RECV_ERR:
        case CC_ERR_SC_CLEAR_FORCE:
        case CC_ERR_SC_SYSTEM_ABNORMAL:
        case CC_ERR_SC_SYSTEM_BUSY:
        case CC_ERR_SC_SYSTEM_MAINTAINING:
            usErrcodeSC = CC_ERR_SIP_SERVICE_UNAVAILABLE;
            break;
        case CC_ERR_BS_NOT_EXIST:
        case CC_ERR_BS_EXPIRE:
        case CC_ERR_BS_FROZEN:
        case CC_ERR_BS_LACK_FEE:
        case CC_ERR_BS_PASSWORD:
        case CC_ERR_BS_RESTRICT:
        case CC_ERR_BS_OVER_LIMIT:
        case CC_ERR_BS_TIMEOUT:
        case CC_ERR_BS_LINK_DOWN:
        case CC_ERR_BS_SYSTEM:
        case CC_ERR_BS_MAINTAIN:
        case CC_ERR_BS_DATA_ABNORMAL:
        case CC_ERR_BS_PARAM_ERR:
        case CC_ERR_BS_NOT_MATCH:
            usErrcodeSC = CC_ERR_SIP_PAYMENT_REQUIRED;
            break;
        default:
            DOS_ASSERT(0);
            break;
    }

    return usErrcodeSC;
}

U32 sc_preview_task_call_result(SC_SRV_CB *pstSCB, U32 ulLegNo, U32 ulSIPRspCode)
{
    SC_DB_MSG_CALL_RESULT_ST *pstCallResult     = NULL;
    SC_LEG_CB                *pstCallingLegCB   = NULL;
    SC_LEG_CB                *pstCalleeLegCB    = NULL;
    SC_LEG_CB                *pstHungupLegCB    = NULL;
    SC_AGENT_NODE_ST         *pstAgentCall      = NULL;
    SC_TASK_CB               *pstTCB            = NULL;

    if (DOS_ADDR_INVALID(pstSCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstHungupLegCB = sc_lcb_get(ulLegNo);
    if (DOS_ADDR_INVALID(pstHungupLegCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (0 == pstSCB->stAutoPreview.ulTaskID || U32_BUTT == pstSCB->stAutoPreview.ulTaskID)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstTCB = sc_tcb_find_by_taskid(pstSCB->stAutoPreview.ulTaskID);
    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstCalleeLegCB = sc_lcb_get(pstSCB->stAutoPreview.ulCalleeLegNo);
    if (DOS_ADDR_INVALID(pstCalleeLegCB))
    {
        pstCallingLegCB = sc_lcb_get(pstSCB->stAutoPreview.ulCallingLegNo);
        if (DOS_ADDR_INVALID(pstCallingLegCB))
        {
            return DOS_FAIL;
        }

        /* 坐席振铃超时 */
        pstCallResult = dos_dmem_alloc(sizeof(SC_DB_MSG_CALL_RESULT_ST));
        if (DOS_ADDR_INVALID(pstCallResult))
        {
            DOS_ASSERT(0);

            return DOS_FAIL;
        }

        sc_log(pstSCB->bTrace, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Analysis call result for task: %u, SIP Code:%u", pstSCB->stAutoCall.ulTaskID, ulSIPRspCode);

        dos_memzero(pstCallResult, sizeof(SC_DB_MSG_CALL_RESULT_ST));
        pstCallResult->ulCustomerID = pstSCB->ulCustomerID; /* 客户ID,要求全数字,不超过10位,最高位小于4 */

        /* 坐席ID,要求全数字,不超过10位,最高位小于4 */
        if (U32_BUTT != pstSCB->stAutoPreview.ulAgentID)
        {
            pstCallResult->ulAgentID = pstSCB->stAutoPreview.ulAgentID;
        }
        else
        {
            pstCallResult->ulAgentID = 0;
        }
        pstCallResult->ulTaskID = pstSCB->stAutoPreview.ulTaskID;       /* 任务ID,要求全数字,不超过10位,最高位小于4 */

        pstAgentCall = sc_agent_get_by_id(pstSCB->stAutoPreview.ulAgentID);
        if (DOS_ADDR_VALID(pstAgentCall)
            && DOS_ADDR_VALID(pstAgentCall->pstAgentInfo))
        {
            dos_snprintf(pstCallResult->szAgentNum, sizeof(pstCallResult->szAgentNum), "%s", pstAgentCall->pstAgentInfo->szEmpNo);
        }

        /* 主叫号码 */
        if ('\0' != pstCallingLegCB->stCall.stNumInfo.szOriginalCalling[0])
        {
            dos_snprintf(pstCallResult->szCaller, sizeof(pstCallResult->szCaller), "%s", pstCallingLegCB->stCall.stNumInfo.szOriginalCalling);
        }

        /* 被叫号码 */
        if ('\0' != pstCallingLegCB->stCall.stNumInfo.szOriginalCallee[0])
        {
            dos_snprintf(pstCallResult->szCallee, sizeof(pstCallResult->szCallee), "%s", pstCallingLegCB->stCall.stNumInfo.szOriginalCallee);
        }

        /* 接续时长:从发起呼叫到收到振铃 */
        pstCallResult->ulPDDLen = pstCallingLegCB->stCall.stTimeInfo.ulRingTime - pstCallingLegCB->stCall.stTimeInfo.ulStartTime;
        pstCallResult->ulStartTime = pstCallingLegCB->stCall.stTimeInfo.ulStartTime;
        pstCallResult->ulRingTime = pstCallingLegCB->stCall.stTimeInfo.ulRingTime;                  /* 振铃时长,单位:秒 */
        pstCallResult->ulAnswerTimeStamp = pstCallingLegCB->stCall.stTimeInfo.ulAnswerTime;         /* 应答时间戳 */
        pstCallResult->ulFirstDTMFTime = pstCallingLegCB->stCall.stTimeInfo.ulDTMFStartTime;        /* 第一个二次拨号时间,单位:秒 */
        pstCallResult->ulIVRFinishTime = pstCallingLegCB->stCall.stTimeInfo.ulIVREndTime;           /* IVR放音完成时间,单位:秒 */

        /* 呼叫时长,单位:秒 */
        pstCallResult->ulTimeLen = 0;

        if (pstSCB->stIncomingQueue.ulEnqueuTime != 0)
        {
            if (pstSCB->stIncomingQueue.ulDequeuTime != 0)
            {
                pstCallResult->ulWaitAgentTime = pstSCB->stIncomingQueue.ulDequeuTime - pstSCB->stIncomingQueue.ulEnqueuTime;
            }
            else
            {
                pstCallResult->ulWaitAgentTime = time(NULL) - pstSCB->stIncomingQueue.ulEnqueuTime;
            }
        }

        pstCallResult->ulHoldCnt = pstSCB->stHold.ulHoldCount;                  /* 保持次数 */
        //pstCallResult->ulHoldTimeLen = pstSCB->usHoldTotalTime;              /* 保持总时长,单位:秒 */
        //pstCallResult->usTerminateCause = pstSCB->usTerminationCause;           /* 终止原因 */
        if (ulLegNo == pstSCB->stAutoPreview.ulCallingLegNo)
        {
            pstCallResult->ucReleasePart = SC_CALLING;
        }
        else
        {
            pstCallResult->ucReleasePart = SC_CALLEE;
        }

        pstCallResult->ulResult = CC_RST_AGENT_NO_ANSER;
        goto proc_finished;
    }

    pstCallResult = dos_dmem_alloc(sizeof(SC_DB_MSG_CALL_RESULT_ST));
    if (DOS_ADDR_INVALID(pstCallResult))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    sc_log(pstSCB->bTrace, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Analysis call result for task: %u, SIP Code:%u", pstSCB->stAutoCall.ulTaskID, ulSIPRspCode);

    if (ulSIPRspCode >= CC_ERR_SC_SERV_NOT_EXIST)
    {
         ulSIPRspCode = sc_task_transform_errcode_from_sc2sip(ulSIPRspCode);
    }

    dos_memzero(pstCallResult, sizeof(SC_DB_MSG_CALL_RESULT_ST));
    pstCallResult->ulCustomerID = pstSCB->ulCustomerID; /* 客户ID,要求全数字,不超过10位,最高位小于4 */

    /* 坐席ID,要求全数字,不超过10位,最高位小于4 */
    if (U32_BUTT != pstSCB->stAutoPreview.ulAgentID)
    {
        pstCallResult->ulAgentID = pstSCB->stAutoPreview.ulAgentID;
    }
    else
    {
        pstCallResult->ulAgentID = 0;
    }
    pstCallResult->ulTaskID = pstSCB->stAutoPreview.ulTaskID;       /* 任务ID,要求全数字,不超过10位,最高位小于4 */

    if (pstCalleeLegCB->ulCBNo != ulLegNo)
    {
        pstCalleeLegCB->stCall.stTimeInfo.ulByeTime = pstHungupLegCB->stCall.stTimeInfo.ulByeTime;
    }

    /* 坐席号码(工号) */
    pstAgentCall = sc_agent_get_by_id(pstSCB->stAutoPreview.ulAgentID);

    if (DOS_ADDR_VALID(pstAgentCall)
        && DOS_ADDR_VALID(pstAgentCall->pstAgentInfo))
    {
        dos_snprintf(pstCallResult->szAgentNum, sizeof(pstCallResult->szAgentNum), "%s", pstAgentCall->pstAgentInfo->szEmpNo);
    }

    /* 主叫号码 */
    if ('\0' != pstCalleeLegCB->stCall.stNumInfo.szOriginalCalling[0])
    {
        dos_snprintf(pstCallResult->szCaller, sizeof(pstCallResult->szCaller), "%s", pstCalleeLegCB->stCall.stNumInfo.szOriginalCalling);
    }

    /* 被叫号码 */
    if ('\0' != pstCalleeLegCB->stCall.stNumInfo.szOriginalCallee[0])
    {
        dos_snprintf(pstCallResult->szCallee, sizeof(pstCallResult->szCallee), "%s", pstCalleeLegCB->stCall.stNumInfo.szOriginalCallee);
    }

    /* 接续时长:从发起呼叫到收到振铃 */
    if (0 == pstCalleeLegCB->stCall.stTimeInfo.ulRingTime || 0 == pstCalleeLegCB->stCall.stTimeInfo.ulStartTime)
    {
        pstCallResult->ulPDDLen = 0;
    }
    else
    {
        pstCallResult->ulPDDLen = pstCalleeLegCB->stCall.stTimeInfo.ulRingTime - pstCalleeLegCB->stCall.stTimeInfo.ulStartTime;
    }
    pstCallResult->ulStartTime = pstCalleeLegCB->stCall.stTimeInfo.ulStartTime;
    pstCallResult->ulRingTime = pstCalleeLegCB->stCall.stTimeInfo.ulRingTime;                  /* 振铃时长,单位:秒 */
    pstCallResult->ulAnswerTimeStamp = pstCalleeLegCB->stCall.stTimeInfo.ulAnswerTime;         /* 应答时间戳 */
    pstCallResult->ulFirstDTMFTime = pstCalleeLegCB->stCall.stTimeInfo.ulDTMFStartTime;        /* 第一个二次拨号时间,单位:秒 */
    pstCallResult->ulIVRFinishTime = pstCalleeLegCB->stCall.stTimeInfo.ulIVREndTime;           /* IVR放音完成时间,单位:秒 */

    /* 呼叫时长,单位:秒 */
    if (0 == pstCalleeLegCB->stCall.stTimeInfo.ulByeTime || 0 == pstCalleeLegCB->stCall.stTimeInfo.ulAnswerTime)
    {
        pstCallResult->ulTimeLen = 0;
    }
    else
    {
        pstCallResult->ulTimeLen = pstCalleeLegCB->stCall.stTimeInfo.ulByeTime - pstCalleeLegCB->stCall.stTimeInfo.ulAnswerTime;
    }

    if (pstSCB->stIncomingQueue.ulEnqueuTime != 0)
    {
        if (pstSCB->stIncomingQueue.ulDequeuTime != 0)
        {
            pstCallResult->ulWaitAgentTime = pstSCB->stIncomingQueue.ulDequeuTime - pstSCB->stIncomingQueue.ulEnqueuTime;
        }
        else
        {
            pstCallResult->ulWaitAgentTime = time(NULL) - pstSCB->stIncomingQueue.ulEnqueuTime;
        }
    }

    pstCallResult->ulHoldCnt = pstSCB->stHold.ulHoldCount;                  /* 保持次数 */
    //pstCallResult->ulHoldTimeLen = pstSCB->usHoldTotalTime;              /* 保持总时长,单位:秒 */
    //pstCallResult->usTerminateCause = pstSCB->usTerminationCause;           /* 终止原因 */
    if (ulLegNo == pstSCB->stAutoPreview.ulCallingLegNo)
    {
        pstCallResult->ucReleasePart = SC_CALLING;
    }
    else
    {
        pstCallResult->ucReleasePart = SC_CALLEE;
    }

    pstCallingLegCB = sc_lcb_get(pstSCB->stAutoPreview.ulCallingLegNo);

    pstCallResult->ulResult = CC_RST_BUTT;

    if (CC_ERR_SIP_SUCC == ulSIPRspCode
        || CC_ERR_NORMAL_CLEAR == ulSIPRspCode)
    {
        /* 坐席全忙 */
        if (pstSCB->stIncomingQueue.ulEnqueuTime != 0
            && pstSCB->stIncomingQueue.ulDequeuTime == 0)
        {
            pstCallResult->ulAnswerTimeStamp = pstCallResult->ulStartTime ? pstCallResult->ulStartTime : time(NULL);
            pstCallResult->ulResult = CC_RST_AGNET_BUSY;
            goto proc_finished;
        }

        if (pstCallResult->ulAnswerTimeStamp == 0)
        {
            /* 未接听 */
            pstCallResult->ulAnswerTimeStamp = pstCallResult->ulRingTime;
            pstCallResult->ulResult = CC_RST_NO_ANSWER;
            goto proc_finished;
        }

        if (SC_CALLEE == pstCallResult->ucReleasePart)
        {
            pstCallResult->ulResult = CC_RST_CUSTOMER_HANGUP;
        }
        else
        {
            pstCallResult->ulResult = CC_RST_AGENT_HANGUP;
        }
    }
    else
    {
        switch (ulSIPRspCode)
        {
            case CC_ERR_SIP_NOT_FOUND:
                pstCallResult->ulResult = CC_RST_NOT_FOUND;
                break;

            case CC_ERR_SIP_TEMPORARILY_UNAVAILABLE:
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulRingTime;
                pstCallResult->ulResult = CC_RST_REJECTED;
                break;

            case CC_ERR_SIP_BUSY_HERE:
                if (pstCallResult->ucReleasePart == SC_CALLING)
                {
                    pstCallResult->ulResult = CC_RST_AGENT_NO_ANSER;
                }
                else
                {
                    pstCallResult->ulResult = CC_RST_BUSY;
                }
                break;

            case CC_ERR_SIP_REQUEST_TIMEOUT:
            case CC_ERR_SIP_REQUEST_TERMINATED:
                if (DOS_ADDR_INVALID(pstCalleeLegCB))
                {
                    pstCallResult->ulResult = CC_RST_NO_ANSWER;
                }
                else
                {
                    pstCallResult->ulResult = CC_RST_AGENT_NO_ANSER;
                }
                break;

            case CC_ERR_SC_CALLEE_NUMBER_ILLEGAL:
                pstCallResult->ulResult = CC_RST_CALLING_NUM_INVALID;
                break;

            default:
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulStartTime ? pstCallResult->ulStartTime : time(NULL);
                pstCallResult->ulResult = CC_RST_CONNECT_FAIL;
                break;
        }
    }

proc_finished:

    if (pstCallResult->ulAnswerTimeStamp == 0)
    {
        pstCallResult->ulAnswerTimeStamp = time(NULL);
    }

    if (CC_RST_BUTT == pstCallResult->ulResult)
    {
        pstCallResult->ulResult = CC_RST_CONNECT_FAIL;
    }

    pstCallResult->stMsgTag.ulMsgType = SC_MSG_SAVE_CALL_RESULT;
    return sc_send_msg2db((SC_DB_MSG_TAG_ST *)pstCallResult);
}

U32 sc_task_call_result(SC_SRV_CB *pstSCB, U32 ulLegNo, U32 ulSIPRspCode, U32 ulStatus)
{
    SC_DB_MSG_CALL_RESULT_ST *pstCallResult     = NULL;
    SC_LEG_CB                *pstCallingLegCB   = NULL;
    SC_LEG_CB                *pstCalleeLegCB    = NULL;
    SC_LEG_CB                *pstHungupLegCB    = NULL;
    SC_AGENT_NODE_ST         *pstAgentCall      = NULL;
    SC_TASK_CB               *pstTCB            = NULL;

    if (DOS_ADDR_INVALID(pstSCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstHungupLegCB = sc_lcb_get(ulLegNo);
    if (DOS_ADDR_INVALID(pstHungupLegCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (0 == pstSCB->stAutoCall.ulTaskID || U32_BUTT == pstSCB->stAutoCall.ulTaskID)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstTCB = sc_tcb_find_by_taskid(pstSCB->stAutoCall.ulTaskID);
    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pstCallingLegCB = sc_lcb_get(pstSCB->stAutoCall.ulCallingLegNo);
    if (DOS_ADDR_INVALID(pstCallingLegCB))
    {
        return DOS_FAIL;
    }

    pstCallResult = dos_dmem_alloc(sizeof(SC_DB_MSG_CALL_RESULT_ST));
    if (DOS_ADDR_INVALID(pstCallResult))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    sc_log(pstSCB->bTrace, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Analysis call result for task: %u, SIP Code:%u", pstSCB->stAutoCall.ulTaskID, ulSIPRspCode);

    if (ulSIPRspCode >= CC_ERR_SC_SERV_NOT_EXIST)
    {
         ulSIPRspCode = sc_task_transform_errcode_from_sc2sip(ulSIPRspCode);
    }

    dos_memzero(pstCallResult, sizeof(SC_DB_MSG_CALL_RESULT_ST));
    pstCallResult->ulCustomerID = pstSCB->ulCustomerID; /* 客户ID,要求全数字,不超过10位,最高位小于4 */

    /* 坐席ID,要求全数字,不超过10位,最高位小于4 */
    if (U32_BUTT != pstSCB->stAutoCall.ulAgentID)
    {
        pstCallResult->ulAgentID = pstSCB->stAutoCall.ulAgentID;
    }
    else
    {
        pstCallResult->ulAgentID = 0;
    }
    pstCallResult->ulTaskID = pstSCB->stAutoCall.ulTaskID;       /* 任务ID,要求全数字,不超过10位,最高位小于4 */

    if (pstCallingLegCB->ulCBNo != ulLegNo)
    {
        pstCallingLegCB->stCall.stTimeInfo.ulByeTime = pstHungupLegCB->stCall.stTimeInfo.ulByeTime;
    }

    /* 坐席号码(工号) */
    pstAgentCall = sc_agent_get_by_id(pstSCB->stAutoCall.ulAgentID);

    if (DOS_ADDR_VALID(pstAgentCall)
        && DOS_ADDR_VALID(pstAgentCall->pstAgentInfo))
    {
        dos_snprintf(pstCallResult->szAgentNum, sizeof(pstCallResult->szAgentNum), "%s", pstAgentCall->pstAgentInfo->szEmpNo);
    }

    /* 主叫号码 */
    if ('\0' != pstCallingLegCB->stCall.stNumInfo.szOriginalCalling[0])
    {
        dos_snprintf(pstCallResult->szCaller, sizeof(pstCallResult->szCaller), "%s", pstCallingLegCB->stCall.stNumInfo.szOriginalCalling);
    }

    /* 被叫号码 */
    if ('\0' != pstCallingLegCB->stCall.stNumInfo.szOriginalCallee[0])
    {
        dos_snprintf(pstCallResult->szCallee, sizeof(pstCallResult->szCallee), "%s", pstCallingLegCB->stCall.stNumInfo.szOriginalCallee);
    }

    /* 接续时长:从发起呼叫到收到振铃 */
    if (0 == pstCallingLegCB->stCall.stTimeInfo.ulRingTime || 0 == pstCallingLegCB->stCall.stTimeInfo.ulStartTime)
    {
        pstCallResult->ulPDDLen = 0;
    }
    else
    {
        pstCallResult->ulPDDLen = pstCallingLegCB->stCall.stTimeInfo.ulRingTime - pstCallingLegCB->stCall.stTimeInfo.ulStartTime;
    }
    pstCallResult->ulStartTime = pstCallingLegCB->stCall.stTimeInfo.ulStartTime;
    pstCallResult->ulRingTime = pstCallingLegCB->stCall.stTimeInfo.ulRingTime;                  /* 振铃时长,单位:秒 */
    pstCallResult->ulAnswerTimeStamp = pstCallingLegCB->stCall.stTimeInfo.ulAnswerTime;         /* 应答时间戳 */
    pstCallResult->ulFirstDTMFTime = pstCallingLegCB->stCall.stTimeInfo.ulDTMFStartTime;        /* 第一个二次拨号时间,单位:秒 */
    pstCallResult->ulIVRFinishTime = pstCallingLegCB->stCall.stTimeInfo.ulIVREndTime;           /* IVR放音完成时间,单位:秒 */

    /* 呼叫时长,单位:秒 */
    if (0 == pstCallingLegCB->stCall.stTimeInfo.ulByeTime || 0 == pstCallingLegCB->stCall.stTimeInfo.ulAnswerTime)
    {
        pstCallResult->ulTimeLen = 0;
    }
    else
    {
        pstCallResult->ulTimeLen = pstCallingLegCB->stCall.stTimeInfo.ulByeTime - pstCallingLegCB->stCall.stTimeInfo.ulAnswerTime;
    }

    if (pstSCB->stIncomingQueue.ulEnqueuTime != 0)
    {
        if (pstSCB->stIncomingQueue.ulDequeuTime != 0)
        {
            pstCallResult->ulWaitAgentTime = pstSCB->stIncomingQueue.ulDequeuTime - pstSCB->stIncomingQueue.ulEnqueuTime;
        }
        else
        {
            pstCallResult->ulWaitAgentTime = time(NULL) - pstSCB->stIncomingQueue.ulEnqueuTime;
        }
    }

    pstCallResult->ulHoldCnt = pstSCB->stHold.ulHoldCount;                 /* 保持次数 */
    //pstCallResult->ulHoldTimeLen = pstSCB->usHoldTotalTime;              /* 保持总时长,单位:秒 */
    //pstCallResult->usTerminateCause = pstSCB->usTerminationCause;        /* 终止原因 */
    if (ulLegNo == pstSCB->stAutoCall.ulCallingLegNo)
    {
        pstCallResult->ucReleasePart = SC_CALLING;
    }
    else
    {
        pstCallResult->ucReleasePart = SC_CALLEE;
    }

    pstCalleeLegCB = sc_lcb_get(pstSCB->stAutoCall.ulCalleeLegNo);

    pstCallResult->ulResult = CC_RST_BUTT;

    if (CC_ERR_SIP_SUCC == ulSIPRspCode
        || CC_ERR_NORMAL_CLEAR == ulSIPRspCode)
    {
        /* 坐席全忙 */
        if (pstSCB->stIncomingQueue.ulEnqueuTime != 0
            && pstSCB->stIncomingQueue.ulDequeuTime == 0)
        {
            pstCallResult->ulAnswerTimeStamp = pstCallResult->ulStartTime ? pstCallResult->ulStartTime : time(NULL);
            pstCallResult->ulResult = CC_RST_AGNET_BUSY;
            goto proc_finished;
        }

        if (pstCallResult->ulAnswerTimeStamp == 0
            && DOS_ADDR_INVALID(pstCalleeLegCB))
        {
            /* 未接听 */
            pstCallResult->ulAnswerTimeStamp = pstCallResult->ulRingTime;
            pstCallResult->ulResult = CC_RST_NO_ANSWER;
            goto proc_finished;
        }

        if (pstTCB->ucMode != SC_TASK_MODE_DIRECT4AGETN)
        {
            /* 有可能放音确实没有结束，客户就按键了,所有应该优先处理 */
            if (pstCallResult->ulFirstDTMFTime
                && DOS_ADDR_INVALID(pstCalleeLegCB))
            {
                pstCallResult->ulResult = CC_RST_HANGUP_AFTER_KEY;
                goto proc_finished;
            }

            /* 播放语音时挂断 */
            if (0 == pstCallResult->ulIVRFinishTime)
            {
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulRingTime;
                pstCallResult->ulResult = CC_RST_HANGUP_WHILE_IVR;
                goto proc_finished;
            }

            /* 放音已经结束了，并且呼叫没有在队列，说明呼叫已经被转到坐席了 */
            if (pstCallResult->ulIVRFinishTime && DOS_ADDR_VALID(pstCalleeLegCB))
            {
                /* ANSWER为0，说明坐席没有接通等待坐席时 挂断的 */
                if (DOS_ADDR_VALID(pstCalleeLegCB)
                    && !pstCalleeLegCB->stCall.stTimeInfo.ulAnswerTime)
                {
                    if (SC_CALLEE == pstCallResult->ucReleasePart)
                    {
                        pstCallResult->ulResult = CC_RST_AGENT_NO_ANSER;
                    }
                    else
                    {
                        pstCallResult->ulResult = CC_RST_HANGUP_NO_ANSER;
                    }

                    goto proc_finished;
                }
            }
        }

        if (SC_CALLEE == pstCallResult->ucReleasePart)
        {
            pstCallResult->ulResult = CC_RST_AGENT_HANGUP;
        }
        else
        {
            if (pstTCB->ucMode != SC_TASK_MODE_AUDIO_ONLY
                && ulStatus == SC_AUTO_CALL_EXEC2)
            {
                /* 特殊处理坐席呼叫不同的情况 */
                pstCallResult->ulResult = CC_RST_HANGUP_NO_ANSER;
            }
            else
            {
                pstCallResult->ulResult = CC_RST_CUSTOMER_HANGUP;
            }
        }
    }
    else
    {
        switch (ulSIPRspCode)
        {
            case CC_ERR_SIP_NOT_FOUND:
                pstCallResult->ulResult = CC_RST_NOT_FOUND;
                break;

            case CC_ERR_SIP_TEMPORARILY_UNAVAILABLE:
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulRingTime;
                pstCallResult->ulResult = CC_RST_REJECTED;
                break;

            case CC_ERR_SIP_BUSY_HERE:
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulStartTime;
                pstCallResult->ulResult = CC_RST_BUSY;
                break;

            case CC_ERR_SIP_REQUEST_TIMEOUT:
            case CC_ERR_SIP_REQUEST_TERMINATED:
                if (DOS_ADDR_INVALID(pstCalleeLegCB))
                {
                    pstCallResult->ulResult = CC_RST_NO_ANSWER;
                }
                else
                {
                    pstCallResult->ulResult = CC_RST_AGENT_NO_ANSER;
                }
                break;

            case CC_ERR_SC_CALLEE_NUMBER_ILLEGAL:
                pstCallResult->ulResult = CC_RST_CALLING_NUM_INVALID;
                break;

            default:
                pstCallResult->ulAnswerTimeStamp = pstCallResult->ulStartTime ? pstCallResult->ulStartTime : time(NULL);
                pstCallResult->ulResult = CC_RST_CONNECT_FAIL;
                break;
        }
    }

proc_finished:

    if (pstCallResult->ulAnswerTimeStamp == 0)
    {
        pstCallResult->ulAnswerTimeStamp = time(NULL);
    }

    if (CC_RST_BUTT == pstCallResult->ulResult)
    {
        pstCallResult->ulResult = CC_RST_CONNECT_FAIL;
    }

    pstCallResult->stMsgTag.ulMsgType = SC_MSG_SAVE_CALL_RESULT;
    return sc_send_msg2db((SC_DB_MSG_TAG_ST *)pstCallResult);
}

U32 sc_task_get_mode(U32 ulTCBNo)
{
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return U32_BUTT;
    }

    if (!g_pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        return U32_BUTT;
    }

    return g_pstTaskList[ulTCBNo].ucMode;
}

U32 sc_task_get_playcnt(U32 ulTCBNo)
{
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return 0;
    }

    if (!g_pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        return 0;
    }

    return g_pstTaskList[ulTCBNo].ucAudioPlayCnt;
}

S8 *sc_task_get_audio_file(U32 ulTCBNo)
{
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return NULL;
    }

    if (!g_pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        return NULL;
    }

    return g_pstTaskList[ulTCBNo].szAudioFileLen;
}

U32 sc_task_get_agent_queue(U32 ulTCBNo)
{
    if (ulTCBNo > SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return U32_BUTT;
    }

    if (!g_pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        return U32_BUTT;
    }

    return g_pstTaskList[ulTCBNo].ulAgentQueueID;
}

VOID sc_task_update_calledcnt(U64 ulArg)
{
    SC_DB_MSG_TAG_ST    *pstMsg     = NULL;
    SC_TASK_CB          *pstTCB     = NULL;
    S8                  szSQL[512]  = { 0 };

    pstTCB = (SC_TASK_CB *)ulArg;
    if (DOS_ADDR_INVALID(pstTCB))
    {
        return;
    }

    if (pstTCB->ulCalledCountLast == pstTCB->ulCalledCount)
    {
        return;
    }

    pstTCB->ulCalledCountLast = pstTCB->ulCalledCount;

    dos_snprintf(szSQL, sizeof(szSQL), "UPDATE tbl_calltask SET calledcnt=%u WHERE id=%u", pstTCB->ulCalledCount, pstTCB->ulTaskID);

    pstMsg = (SC_DB_MSG_TAG_ST *)dos_dmem_alloc(sizeof(SC_DB_MSG_TAG_ST));
    if (DOS_ADDR_INVALID(pstMsg))
    {
        DOS_ASSERT(0);

        return;
    }
    pstMsg->ulMsgType = SC_MSG_SAVE_TASK_CALLED_COUNT;
    pstMsg->szData = dos_dmem_alloc(dos_strlen(szSQL) + 1);
    if (DOS_ADDR_INVALID(pstMsg->szData))
    {
        DOS_ASSERT(0);
        dos_dmem_free(pstMsg->szData);

        return;
    }

    dos_strcpy(pstMsg->szData, szSQL);

    sc_send_msg2db(pstMsg);

    return;
}


/*
 * 函数: SC_TEL_NUM_QUERY_NODE_ST *sc_task_get_callee(SC_TASK_CB_ST *pstTCB)
 * 功能: 获取被叫号码
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 返回值: 成功返回被叫号码控制块指针(已经出队列了，所以使用完之后要释放资源)，否则返回NULL
 * 调用该函数之后，如果返回了合法值，需要释放该资源
 */
SC_TEL_NUM_QUERY_NODE_ST *sc_task_get_callee(SC_TASK_CB *pstTCB)
{
    SC_TEL_NUM_QUERY_NODE_ST *pstCallee = NULL;
    list_t                   *pstList = NULL;
    U32                      ulCount = 0;

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);
        return NULL;
    }

    if (dos_list_is_empty(&pstTCB->stCalleeNumQuery))
    {
        ulCount = sc_task_load_callee(pstTCB);
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Load callee number for task %d. Load result : %d", pstTCB->ulTaskID, ulCount);
    }

    if (dos_list_is_empty(&pstTCB->stCalleeNumQuery))
    {
        pstTCB->ucTaskStatus = SC_TASK_STOP;

        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Task %d has finished. or there is no callees.", pstTCB->ulTaskID);

        return NULL;
    }

    while (1)
    {
        if (dos_list_is_empty(&pstTCB->stCalleeNumQuery))
        {
            break;
        }

        pstList = dos_list_fetch(&pstTCB->stCalleeNumQuery);
        if (!pstList)
        {
            continue;
        }

        pstCallee = dos_list_entry(pstList, SC_TEL_NUM_QUERY_NODE_ST, stLink);
        if (!pstCallee)
        {
            continue;
        }

        break;
    }

    pstCallee->stLink.next = NULL;
    pstCallee->stLink.prev = NULL;

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Select callee %s for new call.", pstCallee->szNumber);

    return pstCallee;
}


/*
 * 函数: U32 sc_task_make_call(SC_TASK_CB_ST *pstTCB)
 * 功能: 申请业务控制块，并将呼叫添加到拨号器模块，等待呼叫
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 返回值: 成功返回DOS_SUCC，失败返回DOS_FAIL
 */
U32 sc_task_make_call(SC_TASK_CB *pstTCB)
{
    SC_TEL_NUM_QUERY_NODE_ST    *pstCallee                  = NULL;
    S8                          szCaller[SC_NUM_LENGTH]     = {0};
    SC_SRV_CB                   *pstSCB                     = NULL;
    SC_LEG_CB                   *pstLegCB                   = NULL;
    U32                         ulErrNo                     = CC_ERR_NO_REASON;
    BOOL                        bIsTrace                    = DOS_FALSE;

    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    bIsTrace = pstTCB->bTraceON;
    if (!bIsTrace)
    {
        bIsTrace = sc_customer_get_trace(pstTCB->ulCustomID);
    }

    pstCallee = sc_task_get_callee(pstTCB);
    if (!pstCallee)
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    /* 只要取到了被叫号码，就应该加一 */
    pstTCB->ulCalledCount++;

    if (!bIsTrace)
    {
        bIsTrace = sc_trace_check_callee(pstCallee->szNumber);
    }

    /* 不允许呼叫国际长途 */
    if (pstCallee->szNumber[0] == '0'
        && pstCallee->szNumber[1] == '0')
    {
        /* 外呼时，被叫号码以00开头，禁止呼叫 */
        sc_log(bIsTrace, SC_LOG_SET_FLAG(LOG_LEVEL_WARNING, SC_MOD_EVENT, SC_LOG_DISIST), "callee is %s. Not alloc call", pstCallee->szNumber);
        ulErrNo = CC_ERR_SC_CALLEE_NUMBER_ILLEGAL;
        goto make_call_file;
    }

    /* 判断是否在黑名单中 */
    if (!sc_black_list_check(pstTCB->ulCustomID, pstCallee->szNumber))
    {
        sc_log(bIsTrace, SC_LOG_SET_FLAG(LOG_LEVEL_WARNING, SC_MOD_EVENT, SC_LOG_DISIST), "The destination is in black list. %s", pstCallee->szNumber);
        ulErrNo = CC_ERR_SC_CALLEE_NUMBER_ILLEGAL;
        goto make_call_file;
    }

    if (pstTCB->ucMode != SC_TASK_MODE_CALL_AGNET_FIRST)
    {
        /* 先呼叫坐席的模式，不限获得主叫号码 */
        if (sc_get_number_by_callergrp(pstTCB->ulCallerGrpID, szCaller, SC_NUM_LENGTH) != DOS_SUCC)
        {
            sc_log(bIsTrace, SC_LOG_SET_FLAG(LOG_LEVEL_NOTIC, SC_MOD_TASK, SC_LOG_DISIST), "Get caller from caller group(%u) FAIL.", pstTCB->ulCallerGrpID);
            ulErrNo = CC_ERR_SC_CALLER_NUMBER_ILLEGAL;
            goto make_call_file;
        }

        if (!bIsTrace)
        {
            bIsTrace = sc_trace_check_caller(szCaller);
        }
    }

    /* 申请一个scb，leg */
    pstSCB = sc_scb_alloc();
    if (DOS_ADDR_INVALID(pstSCB))
    {
        DOS_ASSERT(0);
        goto make_call_file;
    }

    pstSCB->bTrace = bIsTrace;

    pstLegCB = sc_lcb_alloc();
    if (DOS_ADDR_INVALID(pstLegCB))
    {
        DOS_ASSERT(0);
        goto make_call_file;
    }

    if (pstTCB->ucMode == SC_TASK_MODE_CALL_AGNET_FIRST)
    {
        /* 先呼叫客户 */
        pstSCB->stAutoPreview.stSCBTag.bValid = DOS_TRUE;
        pstSCB->pstServiceList[pstSCB->ulCurrentSrv] = &pstSCB->stAutoPreview.stSCBTag;
        pstSCB->stAutoPreview.ulCalleeLegNo = pstLegCB->ulCBNo;
        pstSCB->stAutoPreview.ulTaskID = pstTCB->ulTaskID;
        pstSCB->stAutoPreview.ulTcbID = pstTCB->usTCBNo;
        pstSCB->ulCustomerID = pstTCB->ulCustomID;
    }
    else
    {
        pstSCB->stAutoCall.stSCBTag.bValid = DOS_TRUE;
        pstSCB->pstServiceList[pstSCB->ulCurrentSrv] = &pstSCB->stAutoCall.stSCBTag;
        pstSCB->stAutoCall.ulCallingLegNo = pstLegCB->ulCBNo;
        pstSCB->stAutoCall.ulTaskID = pstTCB->ulTaskID;
        pstSCB->stAutoCall.ulTcbID = pstTCB->usTCBNo;
        pstSCB->stAutoCall.ulKeyMode = pstTCB->ucMode;
        pstSCB->ulCustomerID = pstTCB->ulCustomID;

    }

    pstLegCB->stCall.bValid = DOS_TRUE;
    pstLegCB->ulSCBNo = pstSCB->ulSCBNo;

    dos_snprintf(pstLegCB->stCall.stNumInfo.szOriginalCallee, sizeof(pstLegCB->stCall.stNumInfo.szOriginalCallee), pstCallee->szNumber);
    dos_snprintf(pstLegCB->stCall.stNumInfo.szOriginalCalling, sizeof(pstLegCB->stCall.stNumInfo.szOriginalCalling), szCaller);

    pstLegCB->stCall.ucPeerType = SC_LEG_PEER_OUTBOUND;
    sc_scb_set_service(pstSCB, BS_SERV_AUTO_DIALING);

    /* 认证 */
    if (pstTCB->ucMode == SC_TASK_MODE_CALL_AGNET_FIRST)
    {
        pstSCB->stAutoPreview.stSCBTag.usStatus = SC_AUTO_PREVIEW_AUTH;
    }
    else
    {
        pstSCB->stAutoCall.stSCBTag.usStatus = SC_AUTO_CALL_AUTH;
    }

    sc_log_digest_print_only(pstSCB, "Task(%u) callee : %s, caller : %s.", pstTCB->ulTaskID, pstCallee->szNumber, szCaller);

    if (sc_send_usr_auth2bs(pstSCB, pstLegCB) != DOS_SUCC)
    {
        goto make_call_file;
    }

    return DOS_SUCC;

make_call_file:
    if (DOS_ADDR_VALID(pstSCB))
    {
        sc_scb_free(pstSCB);
        pstSCB = NULL;
    }
    if (DOS_ADDR_VALID(pstLegCB))
    {
        sc_lcb_free(pstLegCB);
        pstLegCB = NULL;
    }
    sc_task_call_result_make_call_before(pstTCB->ulCustomID, pstTCB->ulTaskID, szCaller, pstCallee->szNumber, ulErrNo);
    return DOS_FAIL;
}

U32 sc_task_mngt_query_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_DB_MSG_TASK_STATUS_ST    *pstDBTaskStatus    = NULL;
    SC_TASK_CB                  *pstTCB             = NULL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID || U32_BUTT == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (!pstTCB)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    pstDBTaskStatus = dos_dmem_alloc(sizeof(SC_DB_MSG_TASK_STATUS_ST));
    if (DOS_ADDR_INVALID(pstDBTaskStatus))
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    dos_memzero(pstDBTaskStatus, sizeof(SC_DB_MSG_TASK_STATUS_ST));
    pstDBTaskStatus->ulTaskID = pstTCB->ulTaskID;
    /* 坐席个数 */
    sc_agent_group_stat_by_id(pstTCB->ulAgentQueueID, &pstDBTaskStatus->ulTotalAgent, NULL, &pstDBTaskStatus->ulIdleAgent, NULL);
    /* 已经呼叫的号码数 */
    pstDBTaskStatus->ulCalledCount = pstTCB->ulCalledCount;
    /* 当前并发数 */
    pstDBTaskStatus->ulCurrentConcurrency = pstTCB->ulCurrentConcurrency;
    /* 最大并发数 */
    pstDBTaskStatus->ulMaxConcurrency = pstTCB->ulMaxConcurrency;

    pstDBTaskStatus->stMsgTag.ulMsgType = SC_MSG_SACE_TASK_STATUS;

    sc_send_msg2db((SC_DB_MSG_TAG_ST *)pstDBTaskStatus);

    return SC_HTTP_ERRNO_SUCC;
}

/*
 * 函数: VOID *sc_task_runtime(VOID *ptr)
 * 功能: 单个呼叫任务的线程主函数
 * 参数:
 */
VOID *sc_task_runtime(VOID *ptr)
{
    SC_TEL_NUM_QUERY_NODE_ST *pstCallee = NULL;
    SC_TASK_CB      *pstTCB        = NULL;
    list_t          *pstList       = NULL;
    U32             ulTaskInterval = 0;
    S32             lResult        = 0;
    U32             ulMinInterval  = 0;
    U32             blStopFlag     = DOS_FALSE;
    BOOL            blPauseFlag    = DOS_FALSE;

    if (!ptr)
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "%s", "Fail to start the thread for task, invalid parameter");
        pthread_exit(0);
    }

    pstTCB = (SC_TASK_CB *)ptr;
    if (DOS_ADDR_INVALID(pstTCB))
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "%s", "Start task without pointer a TCB.");
        return NULL;
    }

    pstTCB->ucTaskStatus = SC_TASK_WORKING;

    if (sc_serv_ctrl_check(pstTCB->ulCustomID
                                , BS_SERV_AUTO_DIALING
                                , SC_SRV_CTRL_ATTR_TASK_MODE
                                , pstTCB->ucMode
                                , SC_SRV_CTRL_ATTR_INVLID
                                , U32_BUTT))
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Service not allow.(TaskID:%u) ", pstTCB->ulTaskID);

        goto finished;
    }

    /* 开启一个定时器，将已经呼叫过的号码数量，定时写进数据库中 */
    lResult = dos_tmr_start(&pstTCB->pstTmrHandle
                            , SC_TASK_UPDATE_DB_TIMER * 1000
                            , sc_task_update_calledcnt
                            , (U64)pstTCB
                            , TIMER_NORMAL_LOOP);
    if (lResult < 0)
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "Start timer update task(%u) calledcnt FAIL", pstTCB->ulTaskID);
    }

    /*
       这个地方大概的意思是: 假设接续时长为5000ms，需要在这5000ms内部所有坐席需要的呼叫全部震起来，
       因此需要计算一个发起呼叫的时间间隔
       但是呼叫间隔不小于20ms
       至少1000ms检测一次
       如果是不需要坐席的呼叫，则以20CPS的数度发起(当然要现在最低并发数)
    */
    if (pstTCB->usSiteCount * pstTCB->ulCallRate)
    {
        ulMinInterval = 5000 / ceil(1.0 * (pstTCB->usSiteCount * pstTCB->ulCallRate) / 10);
        ulMinInterval = (ulMinInterval < 20) ? 20 : (ulMinInterval > 1000) ? 1000 : ulMinInterval;
    }
    else
    {
        ulMinInterval = 1000 / 20;
    }

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Start run task(%u), Min interval: %ums", pstTCB->ulTaskID, ulMinInterval);

    while (1)
    {
        if (0 == ulTaskInterval)
        {
            ulTaskInterval = ulMinInterval;
        }
        dos_task_delay(ulTaskInterval);
        ulTaskInterval = 0;

        if (!pstTCB->ucValid)
        {
            return NULL;
        }

        /* 根据当前呼叫量，确定发起呼叫的间隔，如果当前任务已经处于受限状态，就要强制调整间隔 */
        if (!sc_task_check_can_call(pstTCB))
        {
            /* 可能会非常快，就不要打印了 */
            /*sc_logr_debug(NULL, SC_TASK, "Cannot make call for reach the max concurrency. Task : %u.", pstTCB->ulTaskID);*/
            continue;
        }

        /* 如果暂停了就继续等待 */
        if (SC_TASK_PAUSED == pstTCB->ucTaskStatus)
        {
            /* 第一次 暂停 时，不结束任务，等待20s */
            if (pstTCB->ulCurrentConcurrency != 0 || !blPauseFlag)
            {
                blPauseFlag = DOS_TRUE;
                sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "Cannot make call for paused status. Task : %u.", pstTCB->ulTaskID);
                ulTaskInterval = 20000;
                continue;
            }

            sc_task_mngt_query_task(pstTCB->ulTaskID, pstTCB->ulCustomID);
            break;
        }
        blPauseFlag = DOS_FALSE;

        /* 如果被停止了，就检测还有没有呼叫，如果有呼叫，就等待，等待没有呼叫时退出任务 */
        if (SC_TASK_STOP == pstTCB->ucTaskStatus)
        {
            /* 第一次 SC_TASK_STOP 时，不结束任务，等待20s */
            if (pstTCB->ulCurrentConcurrency != 0 || !blStopFlag)
            {
                blStopFlag = DOS_TRUE;
                sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "Cannot make call for stoped status. Task : %u, CurrentConcurrency : %u.", pstTCB->ulTaskID, pstTCB->ulCurrentConcurrency);
                ulTaskInterval = 20000;
                continue;
            }

            /* 任务结束了，退出主循环 */
            break;
        }
        blStopFlag = DOS_FALSE;

        /* 检查当前是否在允许的时间段 */
        if (sc_task_check_can_call_by_time(pstTCB) != DOS_TRUE)
        {
            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "Cannot make call for invalid time period. Task : %u. %d", pstTCB->ulTaskID, pstTCB->usTCBNo);
            ulTaskInterval = 20000;
            continue;
        }

        /* 检测当时任务是否可以发起呼叫 */
        if (sc_task_check_can_call_by_status(pstTCB) != DOS_TRUE)
        {
            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "Cannot make call for system busy. Task : %u.", pstTCB->ulTaskID);
            continue;
        }
#if 1
        /* 发起呼叫 */
        if (sc_task_make_call(pstTCB))
        {
            /* 发送呼叫失败 */
            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "%s", "Make call fail.");

        }
#endif
    }

finished:
    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Task %d finished.", pstTCB->ulTaskID);

    /* 释放相关资源 */
    if (DOS_ADDR_VALID(pstTCB->pstTmrHandle))
    {
        dos_tmr_stop(&pstTCB->pstTmrHandle);
        pstTCB->pstTmrHandle = NULL;
    }

    while (1)
    {
        if (dos_list_is_empty(&pstTCB->stCalleeNumQuery))
        {
            break;
        }

        pstList = dos_list_fetch(&pstTCB->stCalleeNumQuery);
        if (DOS_ADDR_INVALID(pstList))
        {
            break;
        }

        pstCallee = dos_list_entry(pstList, SC_TEL_NUM_QUERY_NODE_ST, stLink);
        if (DOS_ADDR_INVALID(pstCallee))
        {
            continue;
        }

        dos_dmem_free(pstCallee);
        pstCallee = NULL;
    }

#if 0
    if (pstTCB->pstCallerNumQuery)
    {
        dos_dmem_free(pstTCB->pstCallerNumQuery);
        pstTCB->pstCallerNumQuery = NULL;
    }
#endif

    pthread_mutex_destroy(&pstTCB->mutexTaskList);

    /* 群呼任务结束后，将呼叫的被叫号码数量，改为被叫号码的总数量 */
    pstTCB->bThreadRunning = DOS_FALSE;
    sc_task_update_calledcnt((U64)pstTCB);
    sc_task_save_status(pstTCB->ulTaskID, blStopFlag ? SC_TASK_STATUS_DB_STOP : SC_TASK_STATUS_DB_PAUSED, NULL);

    sc_tcb_free(pstTCB);
    pstTCB = NULL;

    return NULL;
}

/*
 * 函数: U32 sc_task_start(SC_TASK_CB_ST *pstTCB)
 * 功能: 启动呼叫化任务
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 成功返回DOS_SUCC，否则返回DOS_FAIL
 */
U32 sc_task_start(SC_TASK_CB *pstTCB)
{
    if (!pstTCB
        || !pstTCB->ucValid)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (pstTCB->bThreadRunning)
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Task %u already running.", pstTCB->ulTaskID);
    }
    else
    {
        if (pthread_create(&pstTCB->pthID, NULL, sc_task_runtime, pstTCB) < 0)
        {
            DOS_ASSERT(0);

            pstTCB->bThreadRunning = DOS_FALSE;

            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Start task %d faild", pstTCB->ulTaskID);

            return DOS_FAIL;
        }

        pstTCB->bThreadRunning = DOS_TRUE;
    }

    sc_task_save_status(pstTCB->ulTaskID, SC_TASK_STATUS_DB_START, NULL);

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Start task %d finished.", pstTCB->ulTaskID);

    return DOS_SUCC;
}

/*
 * 函数: U32 sc_task_stop(SC_TASK_CB_ST *pstTCB)
 * 功能: 停止呼叫化任务
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 成功返回DOS_SUCC，否则返回DOS_FAIL
 */
U32 sc_task_stop(SC_TASK_CB *pstTCB)
{
    if (!pstTCB)
    {
        DOS_ASSERT(0);


        return DOS_FAIL;
    }

    if (!pstTCB->ucValid)
    {
        DOS_ASSERT(0);

        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "Cannot stop the task. TCB Valid:%d, TCB Status: %d", pstTCB->ucValid, pstTCB->ucTaskStatus);

        return DOS_FAIL;
    }

    sc_task_save_status(pstTCB->ulTaskID, SC_TASK_STATUS_DB_STOP, NULL);

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    pstTCB->ucTaskStatus = SC_TASK_STOP;
    pthread_mutex_unlock(&pstTCB->mutexTaskList);

    return DOS_SUCC;
}

/*
 * 函数: U32 sc_task_continue(SC_TASK_CB_ST *pstTCB)
 * 功能: 恢复呼叫化任务
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 成功返回DOS_SUCC，否则返回DOS_FAIL
 */
U32 sc_task_continue(SC_TASK_CB *pstTCB)
{
    if (!pstTCB)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (!pstTCB->ucValid
        || (pstTCB->ucTaskStatus != SC_TASK_PAUSED && pstTCB->ucTaskStatus != SC_TASK_STOP))
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "Cannot continue the task. TCB Valid:%d, TCB Status: %d", pstTCB->ucValid, pstTCB->ucTaskStatus);

        return DOS_FAIL;
    }

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    pstTCB->ucTaskStatus = SC_TASK_WORKING;
    pthread_mutex_unlock(&pstTCB->mutexTaskList);

    /* 开始任务 */
    return sc_task_start(pstTCB);
}

/*
 * 函数: U32 sc_task_pause(SC_TASK_CB_ST *pstTCB)
 * 功能: 暂停呼叫化任务
 * 参数:
 *      SC_TASK_CB_ST *pstTCB: 任务控制块
 * 成功返回DOS_SUCC，否则返回DOS_FAIL
 */
U32 sc_task_pause(SC_TASK_CB *pstTCB)
{
    if (!pstTCB)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (!pstTCB->ucValid
        || pstTCB->ucTaskStatus != SC_TASK_WORKING)
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "Cannot stop the task. TCB Valid:%d, TCB Status: %d", pstTCB->ucValid, pstTCB->ucTaskStatus);

        return DOS_FAIL;
    }

    sc_task_save_status(pstTCB->ulTaskID, SC_TASK_STATUS_DB_PAUSED, NULL);

    pthread_mutex_lock(&pstTCB->mutexTaskList);
    pstTCB->ucTaskStatus = SC_TASK_PAUSED;
    pthread_mutex_unlock(&pstTCB->mutexTaskList);

    return DOS_SUCC;
}

U32 sc_task_concurrency_add(U32 ulTCBNo)
{
    if (ulTCBNo >= SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    if (!g_pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    pthread_mutex_lock(&g_pstTaskList[ulTCBNo].mutexTaskList);
    g_pstTaskList[ulTCBNo].ulCurrentConcurrency++;
    if (g_pstTaskList[ulTCBNo].ulCurrentConcurrency > g_pstTaskList[ulTCBNo].ulMaxConcurrency)
    {
        DOS_ASSERT(0);
    }
    pthread_mutex_unlock(&g_pstTaskList[ulTCBNo].mutexTaskList);

    return DOS_SUCC;
}

U32 sc_task_concurrency_minus(U32 ulTCBNo)
{
    if (ulTCBNo >= SC_MAX_TASK_NUM)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }

    /* 如果任务结束后，还有一通电话正在通话中，
        当这同电话挂断时，下面这个条件不成立，
        则 ulCurrentConcurrency 会为1， 则这个任务永远也不会退出了
    if (!g_pstTaskMngtInfo->pstTaskList[ulTCBNo].ucValid)
    {
        DOS_ASSERT(0);

        SC_TRACE_OUT();
        return DOS_FAIL;
    }
    */

    pthread_mutex_lock(&g_pstTaskList[ulTCBNo].mutexTaskList);
    if (g_pstTaskList[ulTCBNo].ulCurrentConcurrency > 0)
    {
        g_pstTaskList[ulTCBNo].ulCurrentConcurrency--;
    }
    else
    {
        DOS_ASSERT(0);
    }
    pthread_mutex_unlock(&g_pstTaskList[ulTCBNo].mutexTaskList);

    return DOS_SUCC;
}

/*
 * 函数: U32 sc_task_mngt_continue_task(U32 ulTaskID, U32 ulCustomID)
 * 功能: 启动一个已经暂停的任务
 * 参数:
 *      U32 ulTaskID   : 任务ID
 *      U32 ulCustomID : 任务所属客户ID
 * 返回值
 *      返回HTTP API错误码
 * 该函数切勿阻塞
 **/
U32 sc_task_mngt_continue_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_TASK_CB *pstTCB = NULL;
    U32 ulRet = DOS_FAIL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (!pstTCB)
    {
        /* 暂停的任务不一定加载了，如果找不到应该重新加载 */
        if (sc_task_load(ulTaskID) != DOS_SUCC)
        {
            DOS_ASSERT(0);
            return SC_HTTP_ERRNO_INVALID_DATA;
        }

        pstTCB = sc_tcb_find_by_taskid(ulTaskID);
        if (!pstTCB)
        {
            DOS_ASSERT(0);
            return SC_HTTP_ERRNO_INVALID_DATA;
        }

        ulRet = sc_task_load_callee(pstTCB);
        if (DOS_SUCC != ulRet)
        {
            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "SC Task Load Callee FAIL.(TaskID:%u, usNo:%u)", ulTaskID, pstTCB->usTCBNo);
            return SC_HTTP_ERRNO_INVALID_DATA;
        }
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "SC Task Load callee SUCC.(TaskID:%u, usNo:%u)", ulTaskID, pstTCB->usTCBNo);
    }

    if (pstTCB->ucTaskStatus == SC_TASK_INIT)
    {
        /* 需要加载任务 */
        if (DOS_SUCC != sc_task_load(ulTaskID))
        {
            DOS_ASSERT(0);

            return SC_HTTP_ERRNO_CMD_EXEC_FAIL;
        }
    }

    /* 获取呼叫任务最大并发数 */
    pstTCB->usSiteCount = sc_agent_group_agent_count(pstTCB->ulAgentQueueID);
    pstTCB->ulMaxConcurrency = ceil(1.0 * (pstTCB->usSiteCount * pstTCB->ulCallRate) / 10);
    if (0 == pstTCB->ulMaxConcurrency)
    {
        pstTCB->ulMaxConcurrency = SC_MAX_TASK_MAX_CONCURRENCY;
    }

    if (pstTCB->ucTaskStatus != SC_TASK_PAUSED
        && pstTCB->ucTaskStatus != SC_TASK_STOP)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_TASK_STATUS;
    }

    sc_task_continue(pstTCB);

    return SC_HTTP_ERRNO_SUCC;
}


/*
 * 函数: U32 sc_task_mngt_start_task(U32 ulTaskID, U32 ulCustomID)
 * 功能: 暂停一个在运行的任务
 * 参数:
 *      U32 ulTaskID   : 任务ID
 *      U32 ulCustomID : 任务所属客户ID
 * 返回值
 *      返回HTTP API错误码
 * 该函数切勿阻塞
 **/
U32 sc_task_mngt_pause_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_TASK_CB *pstTCB = NULL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (!pstTCB)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (pstTCB->ucTaskStatus != SC_TASK_WORKING)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_TASK_STATUS;
    }

    sc_task_pause(pstTCB);

    return SC_HTTP_ERRNO_SUCC;

}

U32 sc_task_mngt_delete_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_TASK_CB *pstTCB = NULL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID || U32_BUTT == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (!pstTCB)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    sc_task_stop(pstTCB);
    return SC_HTTP_ERRNO_SUCC;

}


/*
 * 函数: U32 sc_task_mngt_start_task(U32 ulTaskID, U32 ulCustomID)
 * 功能: 启动一个新的任务
 * 参数:
 *      U32 ulTaskID   : 任务ID
 *      U32 ulCustomID : 任务所属客户ID
 * 返回值
 *      返回HTTP API错误码
 * 该函数切勿阻塞
 **/
U32 sc_task_mngt_start_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_TASK_CB *pstTCB = NULL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID || U32_BUTT == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    if (DOS_SUCC != sc_task_load(ulTaskID))
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_CMD_EXEC_FAIL;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (DOS_ADDR_INVALID(pstTCB))
    {
        DOS_ASSERT(0);
        return DOS_FAIL;
    }

    /* 获取呼叫任务最大并发数 */
    pstTCB->usSiteCount = sc_agent_group_agent_count(pstTCB->ulAgentQueueID);
    pstTCB->ulMaxConcurrency = ceil(1.0 * (pstTCB->usSiteCount * pstTCB->ulCallRate) / 10);
    if (0 == pstTCB->ulMaxConcurrency)
    {
        pstTCB->ulMaxConcurrency = SC_MAX_TASK_MAX_CONCURRENCY;
    }

    if (sc_task_start(pstTCB) != DOS_SUCC)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_CMD_EXEC_FAIL;
    }

    return SC_HTTP_ERRNO_SUCC;
}


/*
 * 函数: U32 sc_task_mngt_stop_task(U32 ulTaskID, U32 ulCustomID)
 * 功能: 停止一个在运行的任务
 * 参数:
 *      U32 ulTaskID   : 任务ID
 *      U32 ulCustomID : 任务所属客户ID
 * 返回值
 *      返回HTTP API错误码
 * 该函数切勿阻塞
 **/
U32 sc_task_mngt_stop_task(U32 ulTaskID, U32 ulCustomID)
{
    SC_TASK_CB *pstTCB = NULL;

    if (0 == ulTaskID || U32_BUTT == ulTaskID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (0 == ulCustomID || U32_BUTT == ulCustomID)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_USR;
    }

    pstTCB = sc_tcb_find_by_taskid(ulTaskID);
    if (!pstTCB)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_DATA;
    }

    if (pstTCB->ucTaskStatus == SC_TASK_STOP)
    {
        DOS_ASSERT(0);
        return SC_HTTP_ERRNO_INVALID_TASK_STATUS;
    }

    sc_task_stop(pstTCB);

    return SC_HTTP_ERRNO_SUCC;
}


/*
 * 函数 : sc_task_write_stat
 * 功能 : 保存任务运行状态
 * 参数 :
 * 返回值: 成功返回DOS_SUCC,否则返回DOS_FAIL
 */
U32 sc_task_write_stat(U32 ulType, VOID *ptr)
{
    U32 i;

    for (i=0; i<SC_MAX_TASK_NUM; i++)
    {
        if (g_pstTaskList[i].ucValid)
        {
            sc_task_mngt_query_task(g_pstTaskList[i].ulTaskID, g_pstTaskList[i].ulCustomID);
        }
    }

    return DOS_SUCC;
}


/*
 * 函数: VOID sc_task_mngt_cmd_process(SC_TASK_CTRL_CMD_ST *pstCMD)
 * 功能: 处理任务控制，呼叫控制API
 * 参数:
 *      SC_TASK_CTRL_CMD_ST *pstCMD: API命令数据
 * 返回值
 * 该函数切勿阻塞
 **/
U32 sc_task_mngt_cmd_proc(U32 ulAction, U32 ulCustomerID, U32 ulTaskID)
{
    U32 ulRet = SC_HTTP_ERRNO_INVALID_REQUEST;

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Process CMD, Action:%u, Task: %u, CustomID: %u"
                    , ulAction, ulTaskID, ulCustomerID);

    switch (ulAction)
    {
        case SC_API_CMD_ACTION_ADD:
        {
            /* DO Nothing */
            ulRet = SC_HTTP_ERRNO_SUCC;
            break;
        }
        case SC_API_CMD_ACTION_UPDATE:
        {
            /* 如果任务没有被加载到内存，就不要更新了 */
            if (sc_tcb_find_by_taskid(ulTaskID))
            {
                //if (sc_task_and_callee_load(ulTaskID) != DOS_SUCC)
                //{
                //    ulRet = SC_HTTP_ERRNO_INVALID_DATA;
                //}

                if (sc_task_load(ulTaskID) != DOS_SUCC)
                {
                    ulRet = SC_HTTP_ERRNO_INVALID_DATA;
                }

                ulRet = SC_HTTP_ERRNO_SUCC;
            }
            else
            {
                ulRet = SC_HTTP_ERRNO_SUCC;
            }
            break;
        }
        case SC_API_CMD_ACTION_START:
        {
            ulRet = sc_task_mngt_start_task(ulTaskID, ulCustomerID);
            break;
        }
        case SC_API_CMD_ACTION_STOP:
        {
            ulRet = sc_task_mngt_stop_task(ulTaskID, ulCustomerID);
            break;
        }
        case SC_API_CMD_ACTION_CONTINUE:
        {
            ulRet = sc_task_mngt_continue_task(ulTaskID, ulCustomerID);
            break;
        }
        case SC_API_CMD_ACTION_PAUSE:
        {
            ulRet = sc_task_mngt_pause_task(ulTaskID, ulCustomerID);
            break;
        }
        case SC_API_CMD_ACTION_DELETE:
        {
            ulRet = sc_task_mngt_delete_task(ulTaskID, ulCustomerID);
            break;
        }
        case SC_API_CMD_ACTION_STATUS:
        {
            ulRet = sc_task_mngt_query_task(ulTaskID, ulCustomerID);
            break;
        }
        default:
        {
            sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Action templately not support. ACTION: %d", ulAction, ulAction);
            break;
        }
    }

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_DEBUG, SC_MOD_TASK), "CMD Process finished. Action: %u, ErrCode:%u"
                    , ulAction, ulRet);

    return ulRet;
}


/*
 * 函数: U32 sc_task_mngt_start()
 * 功能: 启动呼叫控制模块，同时启动已经被加载的呼叫任务
 * 参数:
 * 返回值: 成功返回DOS_SUCC， 失败返回DOS_FAIL
 **/
U32 sc_task_mngt_start()
{
    SC_TASK_CB    *pstTCB = NULL;
    U32              ulIndex;

    for (ulIndex = 0; ulIndex < SC_MAX_TASK_NUM; ulIndex++)
    {
        pstTCB = &g_pstTaskList[ulIndex];

        if (pstTCB->ucValid && SC_TCB_HAS_VALID_OWNER(pstTCB))
        {
            if (DOS_SUCC != sc_task_and_callee_load(pstTCB->ulTaskID))
            {
                sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Task init fail. Custom ID: %d, Task ID: %d", pstTCB->ulCustomID, pstTCB->ulTaskID);
                sc_tcb_free(pstTCB);
                continue;
            }
            
            if (pstTCB->ucMode != SC_TASK_MODE_AUDIO_ONLY)
            {
                /* 获取呼叫任务最大并发数 */
                pstTCB->usSiteCount = sc_agent_group_agent_count(pstTCB->ulAgentQueueID);
                pstTCB->ulMaxConcurrency = ceil(1.0 * (pstTCB->usSiteCount * pstTCB->ulCallRate) / 10);
            }
            
            if (0 == pstTCB->ulMaxConcurrency)
            {
                pstTCB->ulMaxConcurrency = SC_MAX_TASK_MAX_CONCURRENCY;
            }

            if (pstTCB->ucTaskStatus != SC_TASK_WORKING)
            {
                continue;
            }

            if (sc_task_start(pstTCB) != DOS_SUCC)
            {
                sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_NOTIC, SC_MOD_TASK), "Task start fail. Custom ID: %d, Task ID: %d", pstTCB->ulCustomID, pstTCB->ulTaskID);

                sc_tcb_free(pstTCB);
                continue;
            }
        }
    }

    sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_INFO, SC_MOD_TASK), "Start call task mngt service finished.");

    return DOS_SUCC;
}


/*
 * 函数: U32 sc_task_mngt_init()
 * 功能: 初始化呼叫管理模块
 * 参数:
 * 返回值: 成功返回DOS_SUCC， 失败返回DOS_FAIL
 **/
U32 sc_task_mngt_init()
{
    SC_TASK_CB      *pstTCB = NULL;
    U32             ulIndex = 0;

    /* 初始化呼叫控制块和任务控制块 */
    g_pstTaskList = (SC_TASK_CB *)dos_smem_alloc(sizeof(SC_TASK_CB) * SC_MAX_TASK_NUM);
    if (!g_pstTaskList)
    {
        DOS_ASSERT(0);

        return DOS_FAIL;
    }
    dos_memzero(g_pstTaskList, sizeof(SC_TASK_CB) * SC_MAX_TASK_NUM);
    for (ulIndex = 0; ulIndex < SC_MAX_TASK_NUM; ulIndex++)
    {
        pstTCB = &g_pstTaskList[ulIndex];
        pstTCB->usTCBNo = ulIndex;
        pthread_mutex_init(&pstTCB->mutexTaskList, NULL);
        pstTCB->ucTaskStatus = SC_TASK_BUTT;
        pstTCB->ulTaskID = U32_BUTT;
        pstTCB->ulCustomID = U32_BUTT;

        dos_list_init(&pstTCB->stCalleeNumQuery);
    }

    /* 加载群呼任务 */
    if (sc_task_mngt_load_task() != DOS_SUCC)
    {
        sc_log(DOS_FALSE, SC_LOG_SET_MOD(LOG_LEVEL_ERROR, SC_MOD_TASK), "Load call task fail.");
        return DOS_FAIL;
    }

    return DOS_SUCC;
}

/*
 * 函数: U32 sc_task_mngt_shutdown()
 * 功能: 停止任务管理模块
 * 参数:
 * 返回值: 成功返回DOS_SUCC， 失败返回DOS_FAIL
 **/
U32 sc_task_mngt_stop()
{
    return DOS_SUCC;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */


