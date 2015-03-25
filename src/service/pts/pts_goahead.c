#ifdef  __cplusplus
extern "C"{
#endif

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <limits.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/time.h>
#include <semaphore.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <dos.h>
#include <pt/dos_sqlite3.h>
#include <pt/pts.h>
#include <pt/goahead/webs.h>
#include <pt/goahead/um.h>
#include <pt/goahead/wsIntrn.h>
#include <pt/goahead/websda.h>

#include "pts_msg.h"
#include "pts_web.h"
#include "pts_goahead.h"

/********************************** Defines ***********************************/
/*
 *  The following #defines change the behaviour of security in the absence
 *  of User Management.
 *  Note that use of User management functions require prior calling of
 *  umInit() to behave correctly
 */
#ifndef USER_MANAGEMENT_SUPPORT
#define umGetAccessMethodForURL(url) AM_DIGEST
#define umUserExists(userid) 1
#define umUserCanAccessURL(userid, url) 1
#define umGetUserPassword(userid) websGetPassword()
#define umGetAccessLimitSecure(accessLimit) 0
#define umGetAccessLimit(url) NULL
#endif

#define CREATE_NEW_USER "<a href=\"create_user.html\"><input type=\"button\" value=\"创建新用户\"/></a>&nbsp;&nbsp;&nbsp;&nbsp;\
<input type=\"button\" value=\"修改密码\" onclick=\"change_password()\" />&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"button\" value=\"删除用户\" onclick=\"del_user()\" />"

/*********************************** Locals ***********************************/
/*
 *  Change configuration here
 */

static char_t      *rootWeb = T("html/pts");       /* Root web directory */
static char_t      *password = T("");              /* Security password */
static int          port = WEBS_DEFAULT_PORT;       /* Server port */
static int          retries = 5;                    /* Server port retries */
static int          finished = 0;                   /* Finished flag */
#ifdef _DEBUG
static int      debugSecurity = 1;
#else
static int      debugSecurity = 0;
#endif

BOOL bIsUpdating = DOS_FALSE;   /* 是否正在进行全网 */
PT_PTC_UPGRADE_ST *g_pstUpgrade = NULL;

/****************************** Forward Declarations **************************/

static int  initWebs(int demo);
static void pts_create_user(webs_t wp, char_t *path, char_t *query);
static void pts_change_password(webs_t wp, char_t *path, char_t *query);
static void pts_ping(webs_t wp, char_t *path, char_t *query);
static int  websHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir,
                int arg, char_t *url, char_t *path, char_t *query);
#ifdef B_STATS
static void printMemStats(int handle, char_t *fmt, ...);
static void memLeaks();
#endif
int pts_websSecurityHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                        char_t *url, char_t *path, char_t *query);
int pts_set_remark(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_change_user_info(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_create_new_user(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_notify_ptc_switch_pts(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int change_domain(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_upgrade_selected_ptc(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                          char_t *url, char_t *path, char_t *query);
int pts_upgrade_all_ptc(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                          char_t *url, char_t *path, char_t *query);
int pts_del_users(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_recv_from_ipcc_req(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int pts_start_ping(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int cancel_upgrade(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
int get_ping_result(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query);
static int pts_webs_redirect_pts_server(webs_t wp, char_t *urlPrefix, char_t *webDir,
    int arg, char_t *url, char_t *path, char_t *query);
static int pts_webs_auto_redirect(webs_t wp, char_t *urlPrefix, char_t *webDir,
    int arg, char_t *url, char_t *path, char_t *query);
static int pts_create_user_button(int eid, webs_t wp, int argc, char_t **argv);
static int pts_get_ptc_list_from_db(int eid, webs_t wp, int argc, char_t **argv);
static int pts_get_ptc_list_from_db_switch(int eid, webs_t wp, int argc, char_t **argv);
static int pts_get_ptc_list_from_db_config(int eid, webs_t wp, int argc, char_t **argv);
static int pts_get_ptc_list_from_db_upgrades(int eid, webs_t wp, int argc, char_t **argv);
static int pts_user_list(int eid, webs_t wp, int argc, char_t **argv);
static int curr_user(int eid, webs_t wp, int argc, char_t **argv);
static int ptc_upgrade_button(int eid, webs_t wp, int argc, char_t **argv);
static int all_ptc_upgrade_button(int eid, webs_t wp, int argc, char_t **argv);
S32 pts_get_password_from_sqlite_db(char_t *userid, S8 *szWebsPassword);
S32 pts_get_local_ip(S8 *szLocalIp);
S32 pts_send_ptc_list2web_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name);
S32 pts_ptc_list_switch_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name);
S32 pts_ptc_list_config_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name);
S32 pts_ptc_list_upgrades_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name);
void init_crc_tab( void );
void pts_webs_auto_Redirect(webs_t wp, char_t *url);
/*********************************** Code *************************************/
/*
 *  Main -- entry point from LINUX
 */

#define  MAX_FILENAME_LEN      20

#if defined(MIPS)&&defined(UCLINUX)&&!defined(MIPS32)
#define AOS_NTOHL(x) (x)
#define AOS_NTOHS(x) (x)
#define AOS_HTONL(x) (x)
#define AOS_HTONS(x) (x)

#else
#define AOS_NTOHL(x) ((((x) & 0xFF000000)>>24) |  (((x) & 0x00FF0000)>>8) | \
                      (((x) & 0x0000FF00)<<8 ) |  (((x) & 0x000000FF)<<24)  \
                     )
#define AOS_NTOHS(x)  ((((x)& 0xFF00)>>8) |  (((x) & 0x00FF)<<8))
#define AOS_HTONL(x)  AOS_NTOHL(x)
#define AOS_HTONS(x)  AOS_NTOHS(x)
#endif

#define AOS_SUCC 0
#define AOS_FAIL (-1)

#define LOAD_GET_OS_TYPE_FLAG(a) ((a)>>24)
#define LOAD_GET_CPU_TYPE_FLAG(a) (((a)>>16)&0xff)
#define LOAD_GET_PRODUCT_TYPE_FLAG(a) ((a)&0xffff)


enum
{
    LOAD_OS_TYPE_LINUX =0,
    LOAD_OS_TYPE_VXWORKS = 1,
    LOAD_OS_TYPE_WINDOWS = 2,
    LOAD_OS_TYPE_UCLINUX =3,
};

enum
{
    LOAD_CPU_TYPE_ARM =0,
    LOAD_CPU_TYPE_X86,
    LOAD_CPU_TYPE_MIPS,
    LOAD_CPU_TYPE_PPC,
};

typedef enum
{
    LOAD_FILETYPE_PROG       = 1,    /*程序文件*/
    LOAD_FILETYPE_PATCH      = 2,    /*补丁文件*/
    LOAD_FILETYPE_DAT        = 3,    /*数据文件*/
    LOAD_FILETYPE_LOGIC      = 4,    /*LOGIC*/
    LOAD_FILETYPE_IMMEDIATELY= 5,    /*保留给立即加载的MIB使用*/
    LOAD_FILETYPE_TEL_BOOK   = 6,    /*电话区段表*/

    LOAD_FILETYPE_WEBPAGE    = 7,    /*ETG WEB PAGEs*/
    LOAD_FILETYPE_ROUTE      = 8,    /*国际话务路由*/

    LOAD_FILETYPE_SQL_FILE   = 9,    /*内部使用,放在最后.文本文件(不是LDF文件),数据库SQL语句*/
    LOAD_FILETYPE_DIALPLAN_FILE   = 10,    /*内部使用,放在最后.文本文件(不是LDF文件)，拨号表*/
    #define LOAD_FILETYPE_CONFIG  LOAD_FILETYPE_DIALPLAN_FILE
    LOAD_FILETYPE_ALL   =11,
    LOAD_FILETYPE_VOICEDATA =12,
    LOAD_FILETYPE_EXCEPTION_LOG =13,
    LOAD_FILETYPE_DRIVER    =14,
    LOAD_FILETYPE_DSP_FIRMWARE  =15,
    LOAD_FILETYPE_LOGO =16,
    LOAD_FILETYPE_PACKAGE = 32,
    LOAD_FILETYPE_BUTT
} LOAD_FILE_TYPE;


enum
{
    HTTP_TYPE_UPLOAD_WEB = 0,
    HTTP_TYPE_UPLOAD_DB,
    HTTP_TYPE_UPLOAD_DIALPLAN,
    HTTP_TYPE_DOWNLOAD_DB,
    HTTP_TYPE_DOWNLOAD_DIALPLAN,
    HTTP_TYPE_DOWNLOAD_EXCEPTION_LOG,
    HTTP_TYPE_BUTT
};

/*文件标签数据结构*/
typedef  struct tagFileHeadStruct
{
    U32    ulFileLength;                    /* 文件长度,不含tag */
    U32    ulCompressTag;                   /* 是否经过压缩 */
    U32    ulFileType;                      /* 文件类型/软件类型 */
    U32    ulDate;                          /* 日期 */
    U32    ulTime;                          /* 时间 */
    U32    ulVersionID;                     /* 版本 */
    U32    usCRC;                           /* CRC校验码 */
    S8     szFileName[MAX_FILENAME_LEN];    /* 版本文件名 */
} LOAD_FILE_TAG;


typedef struct tagHttpBackupDesc
{
    U8  type;
    U8  ucLoadType;
    U32 ulBuffIndex;
    U32 ulLength;
    U8 * pstSrcAddr;
} HTTP_BACKUP_DESC_S;

#define EXCEPT_FILE_NAME_BACKUP  "/run.log"

#define CRC16_POLY      0x1021
U16     crc_tab[256];               /* Function init_crc_tab() will initialize it */
U8      gucIsTableInit = 0;

U8      g_LangType = 0; //0-en, 1-chn

void aos_task_yield_time()
{
}
void webs_add_user_header(webs_t wp)
{
}
int web_user_is_exists(char *userid)
{
    return 1;
}
char_t *web_getuserpassword(char_t *userid)
{
    return "";
}
#if 1
void defaultTraceHandler(int level, char_t *buf)
{
}
void defaultErrorHandler(int etype, char_t *msg)
{
}
#endif
U32 cfg_webs_post_max_content_len()
{
    //暂时定为3M，足够了
    return 5*1024*1024;
}

VOID *pts_goahead_service(VOID *arg)
{
    int demo = 0;
/*
 *  Initialize the memory allocator. Allow use of malloc and start
 *  with a 60K heap.  For each page request approx 8KB is allocated.
 *  60KB allows for several concurrent page requests.  If more space
 *  is required, malloc will be used for the overflow.
 */
    bopen(NULL, (60 * 1024), B_USE_MALLOC);
    //signal(SIGPIPE, SIG_IGN);

/*
 *  Initialize the web server
 */
    if (initWebs(demo) < 0) {
        return NULL;
    }

#ifdef WEBS_SSL_SUPPORT
    websSSLOpen();
/*  websRequireSSL("/"); */ /* Require all files be served via https */
#endif

/*
 *  Basic event loop. SocketReady returns true when a socket is ready for
 *  service. SocketSelect will block until an event occurs. SocketProcess
 *  will actually do the servicing.
 */
    finished = 0;
    while (!finished) {
        if (socketReady(-1) || socketSelect(-1, 1000)) {
            socketProcess(-1);
        }
        //websCgiCleanup();
        emfSchedProcess();
    }

#ifdef WEBS_SSL_SUPPORT
    websSSLClose();
#endif

#ifdef USER_MANAGEMENT_SUPPORT
    umClose();
#endif

/*
 *  Close the socket module, report memory leaks and close the memory allocator
 */
    websCloseServer();
    socketClose();
#ifdef B_STATS
    memLeaks();
#endif
    bclose();
    return 0;
}

/******************************************************************************/
/*
 *  Initialize the web server.
 */

static int initWebs(int demo)
{
    struct in_addr  intaddr;
    char            host[128], webdir[128];
    char            *cp;
    char_t          wbuf[128];
    S8 szLocalIp[PT_IP_ADDR_SIZE] = {0};
    S8 szServiceRoot[PT_DATA_BUFF_256] = {0};

/*
 *  Initialize the socket subsystem
 */
    socketOpen();

#ifdef USER_MANAGEMENT_SUPPORT
/*
 *  Initialize the User Management database
 */
    umOpen();
    umRestore(T("umconfig.txt"));
#endif

/*
 *  Define the local Ip address, host name, default home page and the
 *  root web directory.
 */
    if (gethostname(host, sizeof(host)) < 0) {
        error(E_L, E_LOG, T("Can't get hostname"));
        return -1;
    }
  /*  if ((hp = gethostbyname(host)) == NULL) {
        error(E_L, E_LOG, T("Can't get host address"));
        return -1;
    }
    memcpy((char *) &intaddr, (char *) hp->h_addr_list[0],
        (size_t) hp->h_length);
        */
    if (pts_get_local_ip(szLocalIp) != DOS_SUCC)
    {
        logr_info("get local ip fail");
    }
    logr_debug("local ip : %s", szLocalIp);
    inet_pton(AF_INET, szLocalIp, (char *) &intaddr);

/*
 *  Set ../web as the root web. Modify this to suit your needs
 *  A "-demo" option to the command line will set a webdemo root
 */
   /* getcwd(dir, sizeof(dir));
    if ((cp = strrchr(dir, '/'))) {
        *cp = '\0';
    }
*/
    if (config_get_service_root(szServiceRoot, sizeof(szServiceRoot)) == NULL)
    {
        exit(-1);
    }

    sprintf(webdir, "%s/%s", szServiceRoot, rootWeb);
    //sprintf(webdir, "%s/%s", "/mnt/hgfs/share/1/pts_sdk", rootWeb);
    logr_debug("webdir : %s", webdir);

/*
 *  Configure the web server options before opening the web server
 */
    websSetDefaultDir(webdir);
    cp = inet_ntoa(intaddr);
    ascToUni(wbuf, cp, min(strlen(cp) + 1, sizeof(wbuf)));
    websSetIpaddr(wbuf);
    ascToUni(wbuf, host, min(strlen(host) + 1, sizeof(wbuf)));
    websSetHost(wbuf);

/*
 *  Configure the web server options before opening the web server
 */
    websSetDefaultPage(T("default.asp"));
    websSetPassword(password);

/*
 *  Open the web server on the given port. If that port is taken, try
 *  the next sequential port for up to "retries" attempts.
 */
    port = config_get_web_server_port();
    websOpenServer(port, retries);

/*
 *  First create the URL handlers. Note: handlers are called in sorted order
 *  with the longest path handler examined first. Here we define the security
 *  handler, forms handler and the default web page handler.
 */
    websUrlHandlerDefine(T(""), NULL, 0, pts_websSecurityHandler, WEBS_HANDLER_FIRST);       /* 认证 */
    websUrlHandlerDefine(T("/goform"), NULL, 0, websFormHandler, 0);
    //websUrlHandlerDefine(T("/cgi-bin"), NULL, 0, websCgiHandler, 0);
    websUrlHandlerDefine(T("/visit_device"), NULL, 0, pts_webs_redirect_pts_server, 0);      /* 手动访问设备 */
    websUrlHandlerDefine(T("/internetIP="), NULL, 0, pts_webs_auto_redirect, 0);             /* 自动访问设备 */
    websUrlHandlerDefine(T("/cgi-bin/ptcSwitchpts"), NULL, 0, pts_notify_ptc_switch_pts, 0); /* 切换pts */
    websUrlHandlerDefine(T("/cgi-bin/change_domain"), NULL, 0, change_domain, 0);            /* 修改主备域名 */
    websUrlHandlerDefine(T("/cgi-bin/cancel_upgrade"), NULL, 0, cancel_upgrade, 0);          /* 停止全网升级 */
    websUrlHandlerDefine(T("/editable_ajax.html"), NULL, 0, pts_set_remark, 0);              /* 设置备注名 */
    websUrlHandlerDefine(T("/goform/HttpSoftUploadSelected"), NULL, 0, pts_upgrade_selected_ptc, 0); /* 升级选中的ptc */
    websUrlHandlerDefine(T("/goform/HttpSoftUploadAll"), NULL, 0, pts_upgrade_all_ptc, 0);   /* 全网升级 */
    websUrlHandlerDefine(T("/cgi-bin/ptsCreateNewUser"), NULL, 0, pts_create_new_user, 0);   /* 创建新用户 */
    websUrlHandlerDefine(T("/cgi-bin/del_users"), NULL, 0, pts_del_users, 0);                /* 删除用户 */
    websUrlHandlerDefine(T("/change_info.html"), NULL, 0, pts_change_user_info, 0);          /* 修改客户的信息 */
    websUrlHandlerDefine(T("/ipcc_internetIP"), NULL, 0, pts_recv_from_ipcc_req, 0);         /* IPCC访问接口 */
    websUrlHandlerDefine(T("/cgi-bin/startPing"), NULL, 0, pts_start_ping, 0);               /* ping测试 */
    websUrlHandlerDefine(T("/cgi-bin/get_ping_result"), NULL, 0, get_ping_result, 0);        /* 获得ping的结果 */
    websUrlHandlerDefine(T(""), NULL, 0, websDefaultHandler, WEBS_HANDLER_LAST);

/*
 *  Now define two test procedures. Replace these with your application
 *  relevant ASP script procedures and form functions.
 */
    websAspDefine(T("ptcList"), pts_get_ptc_list_from_db);                   /* 手动选择列表 */
    websAspDefine(T("ptcListSwitch"), pts_get_ptc_list_from_db_switch);      /* 切换功能的列表 */
    websAspDefine(T("ptcListConfig"), pts_get_ptc_list_from_db_config);      /* 配置ptc的ptc列表 */
    websAspDefine(T("ptcListUpgrades"), pts_get_ptc_list_from_db_upgrades);  /* 升级的ptc列表 */
    websAspDefine(T("createUser"), pts_create_user_button);                  /* admin用户 创建、删除、修改密码功能 */
    websAspDefine(T("ptcUserList"), pts_user_list);                          /* 用户列表 */
    websAspDefine(T("curr_user"), curr_user);                                /* 用户列表 */
    websAspDefine(T("ptc_upgrade_button"), ptc_upgrade_button);              /* ptc升级按钮 */
    websAspDefine(T("all_ptc_upgrade_button"), all_ptc_upgrade_button);      /* ptc全网升级按钮 */

    websFormDefine(T("create_user"), pts_create_user);      /* 创建用户 */
    websFormDefine(T("change_pwd"), pts_change_password);   /* 修改密码 */
    websFormDefine(T("EiaPingGoStart"), pts_ping);          /* ping */

/*
 *  Create the Form handlers for the User Management pages
 */
#ifdef USER_MANAGEMENT_SUPPORT
    formDefineUserMgmt();
#endif

/*
 *  Create a handler for the default home page
 */
    websUrlHandlerDefine(T("/"), NULL, 0, websHomePageHandler, 0);
    return 0;
}

U16 load_calc_crc( U8* pBuffer, U32 ulCount  )
{

    U16 usTemp = 0;

    init_crc_tab();

    while ( ulCount-- != 0 )
    {
        /* The masked statements can make out the REVERSE crc of ccitt-16b
        temp1 = (crc>>8) & 0x00FFL;
        temp2 = crc_tab[ ( (_US)crc ^ *buffer++ ) & 0xff ];
        crc   = temp1 ^ temp2;
        */
        usTemp = (U16)(( usTemp<<8 ) ^ crc_tab[ ( usTemp>>8 ) ^ *pBuffer++ ]);
    }

    return usTemp;
}


void  load_convert_filehead_n2h(LOAD_FILE_TAG * pFileHead)
{
    pFileHead->ulFileLength  = AOS_NTOHL(pFileHead->ulFileLength);
    pFileHead->ulCompressTag = AOS_NTOHL(pFileHead->ulCompressTag);
    pFileHead->ulFileType    = AOS_NTOHL(pFileHead->ulFileType);
    pFileHead->ulDate        = AOS_NTOHL(pFileHead->ulDate);
    pFileHead->ulTime        = AOS_NTOHL(pFileHead->ulTime);
    pFileHead->ulVersionID   = AOS_NTOHL(pFileHead->ulVersionID);
    pFileHead->usCRC         = AOS_NTOHL(pFileHead->usCRC);

}

U32 load_get_cpu_type()
{
#ifdef MIPS
    U32 ulCpuType = LOAD_CPU_TYPE_MIPS;
#else
    U32 ulCpuType = LOAD_CPU_TYPE_ARM;
#endif
    return ulCpuType;
}

U32 load_get_os_type()
{
#ifdef UCLINUX
    U32 ulOsType = LOAD_OS_TYPE_UCLINUX;
#else
    U32 ulOsType = LOAD_OS_TYPE_LINUX;
#endif

    return ulOsType;
}

void init_crc_tab( void )
{
    U16     i, j;
    U16     value;



    if ( gucIsTableInit )
        return;

    for ( i=0; i<256; i++ )
    {
        value = (U16)(i << 8);
        for ( j=0; j<8; j++ )
        {
            if ( (value&0x8000) != 0 )
                value = (U16)((value<<1) ^ CRC16_POLY);
            else
                value <<= 1;
        }

        crc_tab[i] = value;
    }
    gucIsTableInit = 1;

}

void redirect_web_page(webs_t wp, char_t *page)
{
    S8 szBuf[1024];
    if(g_LangType)
    {
        snprintf(szBuf, sizeof(szBuf)-1, "chn%s", page);
    }
    else
    {
        snprintf(szBuf, sizeof(szBuf)-1, "en%s", page);
    }
    szBuf[sizeof(szBuf)-1] = '\0';

    websRedirect(wp, szBuf);
}

int pts_create_new_user(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    S8  szUserName[PT_DATA_BUFF_64] = {0};
    S8  szPassWord[PT_DATA_BUFF_64] = {0};
    S8  pcListBuf[PT_DATA_BUFF_512] = {0};
    S8  achSql[PTS_SQL_STR_SIZE] = {0};
    S8 *pPassWordMd5 = NULL;

    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%s", szUserName, szPassWord);
    if (lResult < 0)
    {
        perror("sscanf");
        dos_strcpy(pcListBuf, "0");
    }
    else
    {
        printf("szUserName = %s, szPassWord = %s\n", szUserName, szPassWord);
        /* 判断username是否已存在 */
        sprintf(achSql, "select count(*) from pts_user where name='%s'", szUserName);
        if (dos_sqlite3_record_is_exist(g_stMySqlite, achSql))  /* 判断是否存在 */
        {
            /* 已存在 */
            sprintf(pcListBuf, "%s existed", szUserName);
        }
        else
        {
            pPassWordMd5 = pts_md5_encrypt(szUserName, szPassWord);
            sprintf(achSql, "INSERT INTO pts_user (\"name\", \"password\") VALUES ('%s', '%s');", szUserName, pPassWordMd5);
            dos_sqlite3_exec(g_stMySqlite, achSql);
            sprintf(pcListBuf, "succ");
            pts_goAhead_free(pPassWordMd5);
            pPassWordMd5 = NULL;
        }
    }

    websError(wp, 200, T("%s"), pcListBuf);
    return 1;
}

int change_domain(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    S8 cDomainType;
    S8 szDomain[PT_DATA_BUFF_128] = {0};
    S8 szPort[PT_DATA_BUFF_16] = {0};
    S8 *pPtcIDStart = NULL;
    S8 *pSelectPtcID = NULL;
    S8 pszPtcID[PTS_SELECT_PTC_MAX_COUNT][PTC_ID_LEN + 1];
    S32 lPtcCount = 0;
    U16 usPort = 0;
    S32 i = 0;
    PT_CTRL_DATA_ST stCtrlData;

    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%[^&]%*[^=]=%[^&]", &cDomainType, szDomain, szPort);

    printf("cDomainType = %c, szDomain = %s, szPort = %s\n", cDomainType, szDomain, szPort);
    pPtcIDStart = dos_strstr(url, "id=") + dos_strlen("id=");
    pSelectPtcID = strtok(pPtcIDStart, "!");
    while (pSelectPtcID)
    {
        dos_strncpy(pszPtcID[lPtcCount], pSelectPtcID, PTC_ID_LEN);
        pszPtcID[lPtcCount][PTC_ID_LEN] = '\0';
        lPtcCount++;
        pSelectPtcID = strtok(NULL, "!");
        if (NULL == pSelectPtcID)
        {
            break;
        }

        if (lPtcCount >= PTS_SELECT_PTC_MAX_COUNT)
        {
            break;
        }
    }

    if (cDomainType == '0')
    {
        if (dos_strcmp(szDomain, "nil"))
        {
            dos_strncpy(stCtrlData.achPtsMajorDomain, szDomain, PT_DATA_BUFF_64);
        }
        else
        {
            dos_memzero(stCtrlData.achPtsMajorDomain, PT_DATA_BUFF_64);
        }

        if (dos_strcmp(szPort, "nil"))
        {
            usPort = atoi(szPort);
            stCtrlData.usPtsMajorPort = dos_htons(usPort);
        }
        else
        {
            stCtrlData.usPtsMajorPort = 0;
        }

        stCtrlData.enCtrlType = PT_CTRL_PTS_MAJOR_DOMAIN;
    }
    else
    {
        if (dos_strcmp(szDomain, "nil"))
        {
            dos_strncpy(stCtrlData.achPtsMinorDomain, szDomain, PT_DATA_BUFF_64);
        }
        else
        {
            dos_memzero(stCtrlData.achPtsMinorDomain, PT_DATA_BUFF_64);
        }

        if (dos_strcmp(szPort, "nil"))
        {
            usPort = atoi(szPort);
            stCtrlData.usPtsMinorPort = dos_htons(usPort);
        }
        else
        {
            stCtrlData.usPtsMinorPort = 0;
        }
        stCtrlData.enCtrlType = PT_CTRL_PTS_MINOR_DOMAIN;
    }

    for (i=0; i<lPtcCount; i++)
    {
        pts_save_msg_into_cache((U8*)pszPtcID[i], PT_DATA_CTRL, (U32)stCtrlData.enCtrlType, (S8 *)&stCtrlData, sizeof(PT_CTRL_DATA_ST), NULL, 0);
        usleep(20);
    }

    websError(wp, 200, T(""));
    return 1;
}

int cancel_upgrade(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    if (g_pstUpgrade != NULL)
    {
        if (g_pstUpgrade->hTmrHandle != NULL)
        {
            dos_tmr_stop(&g_pstUpgrade->hTmrHandle);
            g_pstUpgrade->hTmrHandle = NULL;
        }
        bIsUpdating = DOS_FALSE;
        dos_dmem_free(g_pstUpgrade);
        g_pstUpgrade = NULL;
    }
    websError(wp, 200, T(""));
    return 1;
}

int pts_del_users(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S8 pszUserName[PTS_DELECT_USER_MAX_COUNT][PT_DATA_BUFF_32];
    S8 *pPtcIDStart = NULL;
    S8 *pSelectPtcID = NULL;
    U32 ulUserCount = 0;
    S32 i = 0;
    S8 achSql[PTS_SQL_STR_SIZE] = {0};

    pPtcIDStart = dos_strstr(url, "name=") + dos_strlen("name=");
    pSelectPtcID = strtok(pPtcIDStart, "!");
    while (pSelectPtcID)
    {
        dos_strncpy(pszUserName[ulUserCount], pSelectPtcID, PT_DATA_BUFF_32-1);
        pszUserName[ulUserCount][PT_DATA_BUFF_32 - 1] = '\0';
        ulUserCount++;
        pSelectPtcID = strtok(NULL, "!");
        if (NULL == pSelectPtcID)
        {
            break;
        }

        if (ulUserCount >= PTS_DELECT_USER_MAX_COUNT)
        {
            break;
        }
    }

    for (i=0; i<ulUserCount; i++)
    {
        dos_snprintf(achSql, PTS_SQL_STR_SIZE, "delete from pts_user where name='%s'", pszUserName[i]);
        dos_sqlite3_exec(g_stMySqlite, achSql);
    }

    websError(wp, 200, T(""));
    return 1;
}

/* 通知ptc切换pts */
int pts_notify_ptc_switch_pts(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    S8 szDomain[PT_DATA_BUFF_128] = {0};
    S8 szPort[PT_DATA_BUFF_16] = {0};
    S8 szPtsIp[PT_IP_ADDR_SIZE] = {0};
    U8 paucIPAddr[IPV6_SIZE] = {0};
    S8 pcListBuf[PT_DATA_BUFF_512] = {0};
    PT_CTRL_DATA_ST stCtrlData;
    U16 usPort = 0;
    S8 *pPtcIDStart = NULL;
    S8 *pPtcIDEnd = NULL;
    S8 *pSelectPtcID = NULL;
    S8 pszPtcID[PTS_SELECT_PTC_MAX_COUNT][PTC_ID_LEN + 1];
    S32 lPtcCount = 0;
    S32 i = 0;

    lResult = sscanf(url,"%*[^=]=%*[^=]=%[^&]%*[^=]=%s", szDomain, szPort);
    if (lResult < 0)
    {
        perror("sscanf");
        //dos_strcpy(pcListBuf,"");
    }
    else
    {
        usPort = atoi(szPort);
        printf("domain : %s, port : %d\n", szDomain, usPort);
        /* 域名解析 */
        if (pt_is_or_not_ip(szDomain))
        {
            dos_strncpy(szPtsIp, szDomain, PT_IP_ADDR_SIZE);
        }
        else
        {
            lResult = pt_DNS_analyze(szDomain, paucIPAddr);
            if (lResult <= 0)
            {
                sprintf(pcListBuf, "0&域名错误");
                websError(wp, 200, T(pcListBuf));
                return 1;
            }
            else
            {
                inet_ntop(AF_INET, (void *)(paucIPAddr), szPtsIp, sizeof(szPtsIp));
            }
        }

        pPtcIDStart = dos_strstr(url, "id=") + dos_strlen("id=");
        pPtcIDEnd = dos_strstr(url, "&");
        pPtcIDEnd = '\0';
        pSelectPtcID = strtok(pPtcIDStart, "!");
        while (pSelectPtcID)
        {
            dos_strncpy(pszPtcID[lPtcCount], pSelectPtcID, PTC_ID_LEN);
            pszPtcID[lPtcCount][PTC_ID_LEN] = '\0';
            lPtcCount++;
            pSelectPtcID = strtok(NULL, "!");
            if (NULL == pSelectPtcID)
            {
                break;
            }

            if (lPtcCount >= PTS_SELECT_PTC_MAX_COUNT)
            {
                break;
            }
        }

        logr_debug("domain name is : %s, ip : %s", szDomain, szPtsIp);
        stCtrlData.enCtrlType = PT_CTRL_SWITCH_PTS;
        for (i=0; i<lPtcCount; i++)
        {
            pts_save_msg_into_cache((U8*)pszPtcID[i], PT_DATA_CTRL, PT_CTRL_SWITCH_PTS, (S8 *)&stCtrlData, sizeof(PT_CTRL_DATA_ST), szPtsIp, usPort);
            usleep(20);
        }
        sprintf(pcListBuf, "succ");
        websError(wp, 200, T(pcListBuf));
    }

    return 1;
}

S8* webs_strnstr(S8 * source, S8 * pEnd, S8 *key)
{
    char *pSource;
    U32 ulkeyLen = strlen(key);

    pSource= source;
    do
    {
        if(*pSource == *key
           && memcmp(key, pSource,ulkeyLen) == 0 )
        {
            return pSource;  /*找到了关键标识字*/
        }
        else
        {
            pSource = pSource+1; /*从前次字符串位置后部开始继续搜索 */
        }
    }
    while( pSource < pEnd);   /*找不到关键字符pSch ==NULL 退出循环*/

    return NULL;
}

int http_upload_save(U32 ulLoadType, S8 * pucData, U32 ulDataLen)
{
    FILE * fl;
    #if 0
    //先校验crc是否正确
    memcpy(&stFileTag, pucData, sizeof(stFileTag));
    load_convert_filehead_n2h(&stFileTag);

    if(stFileTag.ulFileLength + sizeof(stFileTag) != ulDataLen)
    {
        return -1;
    }

    if(stFileTag.ulFileType != ulLoadType)
    {
        return -1;
    }

    usCrc = load_calc_crc(pucData + sizeof(stFileTag), stFileTag.ulFileLength);
    if( usCrc != stFileTag.usCRC )
    {
        return -1;
    }

    if(load_get_cpu_type() != LOAD_GET_CPU_TYPE_FLAG(stFileTag.ulCompressTag))
    {
        printf("%s: cpu type is not correct \r\n", __FUNCTION__);
        return -1;
    }

    //判断操作系统
    if(load_get_os_type() != LOAD_GET_OS_TYPE_FLAG(stFileTag.ulCompressTag))
    {
        printf("%s: os type is not correct \r\n", __FUNCTION__);
        return -1;
    }
    #endif
    if(ulLoadType == LOAD_FILETYPE_PACKAGE)
    {
        int fd;

        //unlink("/flash/apps/package.bak");
        fl = fopen("./package", "w+");
        if(NULL == fl)
        {
            printf("%d\n", __LINE__);
            return -1;
        }

        fseek( fl, 0, 0);
        //清除文件内容
        fd = fileno(fl);
        ftruncate(fd, 0);

        fwrite(pucData, ulDataLen, 1, fl);
        fflush(fl);
        fclose(fl);

        return 0;
    }
    else
    {
        return -1;
    }

}

int websReadForUpload(char_t *pStr, U32 ulLen, char_t *boundry, U32 *pulFileLen, S8 **pFileMD5)
{
    U32 length = 0;
    S8 *pStart = NULL, *pEnd = NULL, * pTemp = NULL, *ptr = NULL;
    U32 uploadLength = 0;
    U32 ulLoadType = LOAD_FILETYPE_PACKAGE;
    S8 *pQEnd = pStr + ulLen;
    S32 ret;

    length = dos_strlen(boundry);

    pStart = webs_strnstr(pStr, pQEnd, boundry);
    if(NULL == pStart)
    {
        return -1;
    }

    pStart += length;
    pStart += 2;
    //跳过\r\n

    ptr = pStart;
    //找到行结尾
    pStart = webs_strnstr(pStart, pQEnd, "\r\n");
    if(NULL == pStart)
    {
        return -1;
    }
    pEnd = pStart;
    *pEnd = '\0';

    //校验一下是form-data
    pStart = webs_strnstr(ptr, pQEnd, "Content-Disposition: ");
    if(NULL == pStart)
    {
        *pEnd = '\r';
        return -1;
    }

    pStart += strlen("Content-Disposition: ");
    if(strncmp(pStart, "form-data", 9))
    {
        *pEnd = '\r';
        return -1;
    }

    //恢复尾部字符
    *pEnd = '\r';
    //寻找数据开始位置
    pStart = webs_strnstr(pEnd+2, pQEnd, "\r\n\r\n");
    if(NULL == pStart)
    {
        return -1;
    }
    pStart += 4;
    //记录数据开始位置
    pTemp = pStart;
    //找到第二个boundry开始
    pStart = webs_strnstr(pStart, pQEnd, boundry);
    if(NULL == pStart)
    {
        return -1;
    }
    //回退两个字符，因为我们传入的boundry没哟包含--
    pStart -= 2;
    //回退\r\n
    pStart -= 2;

    uploadLength = pStart-pTemp;
    *pStart = '\0';

    *pulFileLen = uploadLength;
    *pFileMD5 = websMD5(pTemp);

    ret = http_upload_save(ulLoadType, pTemp, uploadLength);
    *pStart = '\r';

    return ret;

}

void websSendFileToPtc(char_t *url, PT_PTC_UPGRADE_ST *pstUpgrade)
{
    U32 ulReadLen = 0;
    U32 ulReadCount = 0;
    FILE *pFileFd = NULL;
    S8 aucBuff[PT_RECV_DATA_SIZE] = {0};
    S8 *pPtcIDStart = NULL;
    S8 *pSelectPtcID = NULL;
    S8 pszPtcIds[PTS_SELECT_PTC_MAX_COUNT][PTC_ID_LEN + 1];
    S32 lPtcCount = 0;
    S32 i = 0;
    S8 szPtcId[PTC_ID_LEN + 1] = {0};
    S8 szPtcVersion[PT_DATA_BUFF_16] = {0};        /* 版本号 */

    /* 获取id */
    pPtcIDStart = dos_strstr(url, "id=") + dos_strlen("id=");
    pSelectPtcID = strtok(pPtcIDStart, "!");
    while (pSelectPtcID)
    {
        sscanf(pSelectPtcID, "%[^|]|%s", szPtcId, szPtcVersion);

        /* 比较版本号，只有不同时，可以升级 */
        if (dos_strcmp(pstUpgrade->szVision, szPtcVersion) != 0)
        {
            dos_strcpy(pszPtcIds[lPtcCount], szPtcId);
            lPtcCount++;
        }
        pSelectPtcID = strtok(NULL, "!");
        if (NULL == pSelectPtcID)
        {
            break;
        }

        if (lPtcCount >= PTS_SELECT_PTC_MAX_COUNT)
        {
            break;
        }
    }

    pFileFd  = fopen("./package", "r");
    if(NULL == pFileFd)
    {
        return;
    }
    for (i=0; i<lPtcCount; i++)
    {
        fseek(pFileFd, 0L, SEEK_SET);
        printf("upgrade ptc sn : %s\n", pszPtcIds[i]);
        pts_save_msg_into_cache((U8*)pszPtcIds[i], PT_DATA_WEB, PT_CTRL_PTC_PACKAGE, (S8 *)pstUpgrade, sizeof(PT_PTC_UPGRADE_ST), NULL, 0);
        do
        {
            ulReadCount = fread(aucBuff, PT_RECV_DATA_SIZE, 1, pFileFd);
            if (0 == ulReadCount)
            {
                ulReadLen = dos_ntohl(pstUpgrade->ulPackageLen)%PT_RECV_DATA_SIZE;
            }
            else
            {
                ulReadLen = PT_RECV_DATA_SIZE;
            }

            pts_save_msg_into_cache((U8*)pszPtcIds[i], PT_DATA_WEB, PT_CTRL_PTC_PACKAGE, aucBuff, ulReadLen, NULL, 0);

        }while (ulReadLen == PT_RECV_DATA_SIZE);
    }

    fclose(pFileFd);
}

void pts_send_ipgrade_package2ptc(S8 *szPtcId, PT_PTC_UPGRADE_ST *pstUpgrade)
{
    S8 aucBuff[PT_RECV_DATA_SIZE] = {0};
    U32 ulReadCount = 0;
    U32 ulReadLen = 0;
    fseek(pstUpgrade->pPackageFileFd, 0L, SEEK_SET);

    pts_save_msg_into_cache((U8 *)szPtcId, PT_DATA_WEB, PT_CTRL_PTC_PACKAGE, (S8 *)pstUpgrade, sizeof(PT_PTC_UPGRADE_ST), NULL, 0);
    do
    {
        ulReadCount = fread(aucBuff, PT_RECV_DATA_SIZE, 1, pstUpgrade->pPackageFileFd);
        if (0 == ulReadCount)
        {
            ulReadLen = dos_ntohl(pstUpgrade->ulPackageLen)%PT_RECV_DATA_SIZE;
        }
        else
        {
            ulReadLen = PT_RECV_DATA_SIZE;
        }

        pts_save_msg_into_cache((U8 *)szPtcId, PT_DATA_WEB, PT_CTRL_PTC_PACKAGE, aucBuff, ulReadLen, NULL, 0);

    }while (ulReadLen == PT_RECV_DATA_SIZE);

}

S32 pts_upgrade_ptcs(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    list_t *pstListHead = *(list_t **)para;
    PTS_UPGRADE_PTC_ST *pstNewPtcNode = (PTS_UPGRADE_PTC_ST *)dos_dmem_alloc(sizeof(PTS_UPGRADE_PTC_ST));
    if (NULL == pstNewPtcNode)
    {
        perror("malloc");
        return 1;
    }
    dos_strncpy(pstNewPtcNode->szPtcId, column_value[1], PTC_ID_LEN);
    pstNewPtcNode->szPtcId[PTC_ID_LEN] = '\0';

    if (NULL == pstListHead)
    {
        dos_list_init(&pstNewPtcNode->stNextNode);
        *(list_t **)para = &pstNewPtcNode->stNextNode;
    }
    else
    {
        dos_list_add_tail(pstListHead, &pstNewPtcNode->stNextNode);
    }

    return 0;
}


void websSendFileToAllPtc(U64 param)
{
    S32 lResult = 0;
    FILE *pFileFd = NULL;
    S8 achSql[PTS_SQL_STR_SIZE] = {0};
    PT_PTC_UPGRADE_ST *pstUpgrade = (PT_PTC_UPGRADE_ST *)param;
    S8 pszPtcIds[PTS_UPGRADE_PTC_COUNT][PTC_ID_LEN + 1];
    list_t *pstListHead = NULL;
    PTS_UPGRADE_PTC_ST *pstPtcNode = NULL;

    pFileFd = fopen("./package", "r");
    if(NULL == pFileFd)
    {
        return;
    }
    pstUpgrade->pPackageFileFd = pFileFd;

    /* 查询数据库, 将ptc都保存到内存中 */
    dos_memzero(pszPtcIds, PTS_UPGRADE_PTC_COUNT*(PTC_ID_LEN+1));
    dos_strncpy(achSql, "select * from ipcc_alias where version!='%s' and ptcType!='PC' and register=1 order by lastLoginTime desc;", PTS_SQL_STR_SIZE);
    lResult = dos_sqlite3_exec_callback(g_stMySqlite, achSql, pts_upgrade_ptcs, (VOID *)&pstListHead);
    if (lResult != DOS_SUCC)
    {
        DOS_ASSERT(0);
    }

    if (pstListHead != NULL)
    {
        while (pstListHead->next != pstListHead)
        {
            pstPtcNode = dos_list_entry(pstListHead, PTS_UPGRADE_PTC_ST, stNextNode);
            printf("ptcid : %s\n", pstPtcNode->szPtcId);
            pts_send_ipgrade_package2ptc(pstPtcNode->szPtcId, pstUpgrade);
            pstListHead = pstListHead->next;
            dos_list_del(pstListHead->prev);
            dos_dmem_free(pstPtcNode);
            pstPtcNode = NULL;
        }

        pstPtcNode = dos_list_entry(pstListHead, PTS_UPGRADE_PTC_ST, stNextNode);
        printf("ptcid : %s\n", pstPtcNode->szPtcId);
        pts_send_ipgrade_package2ptc(pstPtcNode->szPtcId, pstUpgrade);
        dos_list_del(pstListHead->prev);
        dos_dmem_free(pstPtcNode);
        pstPtcNode = NULL;
    }

    fclose(pFileFd);
}

S32 websHttpUploadHandler(webs_t wp, char_t *query, PT_PTC_UPGRADE_ST *pstUpgrade)
{
    int flags;
    char_t * boundry, * ctype, *clen;
    U32 ulQLen = 0;
    int ret;
    U32 ulFileLen = 0;
    S8 *pFileMD5 = NULL;

    flags = websGetRequestFlags(wp);

    if(!(flags & WEBS_POST_REQUEST))
    {
        return DOS_FAIL;
    }
    clen = websGetVar(wp, T("CONTENT_LENGTH"), T(""));
    printf("clen = %s\n", clen);

    if(NULL == clen)
    {
        return DOS_FAIL;
    }

    if(sscanf(clen, "%u", &ulQLen)<1)
    {
        return DOS_FAIL;
    }

    ctype = websGetVar(wp, T("CONTENT_TYPE"), T(""));
    printf("ctype = %s\n", ctype);
    if(NULL == ctype)
    {
        return DOS_FAIL;
    }
    boundry = strstr(ctype, "boundary=");
    if(NULL == boundry)
    {
        return DOS_FAIL;
    }
    boundry += dos_strlen("boundary=");
    if(*boundry)
    {
        ret = websReadForUpload(query, ulQLen, boundry, &ulFileLen, &pFileMD5);
    }

    if(0 != ret)
    {
        return DOS_FAIL;
    }

    /* 发送 MD5验证 */
    pstUpgrade->ulPackageLen = dos_htonl(ulFileLen);
    dos_strncpy(pstUpgrade->szMD5Verify, pFileMD5, PT_MD5_LEN);
    bfreeSafe(B_L, pFileMD5);

    return DOS_SUCC;
}

int pts_upgrade_selected_ptc(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                          char_t *url, char_t *path, char_t *query)
{
    PT_PTC_UPGRADE_ST stUpgrade;
    S8 szFileName[PT_DATA_BUFF_128] = {0};
    S8 *ptr = NULL;
    S32 lResult = 0;

    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path);
    a_assert(query);

    /* 获取文件名 */
    ptr = dos_strstr(query, "filename=");
    if (ptr != NULL)
    {
        lResult = sscanf(ptr, "%*[^\"]\"%[^\"]", szFileName);
        if (lResult != 1)
        {
            goto err_proc;
        }
    }
    else
    {
        goto err_proc;
    }

    lResult = sscanf(szFileName, "%*[^_]_%15s", stUpgrade.szVision);
    if (lResult != 1)
    {
        goto err_proc;
    }
    printf("version : %s\n", stUpgrade.szVision);
    /* 下载文件 */
    lResult = websHttpUploadHandler(wp, query, &stUpgrade);
    if (lResult != DOS_SUCC)
    {
        goto err_proc;
    }

    websSendFileToPtc(url, &stUpgrade);
    websError(wp, 200, T("succ"));
    return 1;

err_proc:
    websError(wp, 200, T("fail"));
    return 1;
}

int pts_upgrade_all_ptc(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                          char_t *url, char_t *path, char_t *query)
{
    PT_PTC_UPGRADE_ST *pstUpgrade = NULL;
    S8 szFileName[PT_DATA_BUFF_128] = {0};
    S8 *ptr = NULL;
    S32 lResult = 0;

    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path);
    a_assert(query);

    pstUpgrade = (PT_PTC_UPGRADE_ST *)dos_dmem_alloc(sizeof(PT_PTC_UPGRADE_ST));
    if (NULL == pstUpgrade)
    {
        perror("malloc");
        goto err_proc;
    }
    pstUpgrade->hTmrHandle = NULL;
    g_pstUpgrade = pstUpgrade;
    /* 获取文件名 */
    ptr = dos_strstr(query, "filename=");
    if (ptr != NULL)
    {
        lResult = sscanf(ptr, "%*[^\"]\"%[^\"]", szFileName);
        if (lResult != 1)
        {
            goto err_proc;
        }
    }
    else
    {
        goto err_proc;
    }

    lResult = sscanf(szFileName, "%*[^_]_%15s", pstUpgrade->szVision);
    if (lResult != 1)
    {
        goto err_proc;
    }
    printf("version : %s\n", pstUpgrade->szVision);

    /* 下载文件 */
    lResult = websHttpUploadHandler(wp, query, pstUpgrade);
    if (lResult != DOS_SUCC)
    {
        goto err_proc;
    }

    /* 开启定时器 */
    lResult = dos_tmr_start(&pstUpgrade->hTmrHandle, PTS_PTC_UPGRADE_TIMER, websSendFileToAllPtc, (U64)pstUpgrade, TIMER_NORMAL_LOOP);
    if (lResult < 0)
    {
        bIsUpdating = DOS_FALSE;
        printf("upgrade all ptc : start timer fail\n");
    }
    else
    {
        bIsUpdating = DOS_TRUE;
    }

    websSendFileToAllPtc((U64)pstUpgrade);

    websError(wp, 200, T("succ"));
    return 1;

err_proc:
    websError(wp, 200, T("fail"));
    return 1;
}

int pts_set_remark(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    U8  aucID[PTC_ID_LEN + 1]  = {0};
    S8  szRemark[PT_DATA_BUFF_512] = {0};
    S8  achSql[PTS_SQL_STR_SIZE] = {0};

    lResult = sscanf(query,"%*[^=]=%*[^=]=%*[^=]=%[^&]%*[^=]=%s", aucID, szRemark);
    if (lResult < 0)
    {
        perror("sscanf");
        //dos_strcpy(pcListBuf,"0");
    }
    else
    {
        aucID[PTC_ID_LEN] = '\0';
        pt_logr_debug("id= %s , remark = %s", aucID, szRemark);

        /* 更新数据库 */
        sprintf(achSql, "update ipcc_alias set remark='%s' where sn='%s';", szRemark, aucID);
        printf("change remark : %s\n", achSql);
        lResult = dos_sqlite3_exec(g_stMySqlite, achSql);
        if (lResult < 0)
        {
            /* 失败 */
            pt_logr_debug("error");
            websDone(wp, 200);
        }
        else
        {
            /* 成功 */
            printf("succ\n");
            websHeader(wp);
            websWrite(wp, T("%s"), szRemark);
            websFooter(wp);
            websDone(wp, 200);
        }
    }

    return 1;
}

int pts_change_user_info(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    U32 ulColumn = 0;
    S8  szName[PT_DATA_BUFF_32]  = {0};
    S8  szValue[PT_DATA_BUFF_256] = {0};
    S8  achSql[PTS_SQL_STR_SIZE] = {0};
    S8  szColumnName[PT_DATA_BUFF_32] = {0};

    lResult = sscanf(query,"%*[^=]=%[^&]&%*[^=]=%*[^=]=%[^&]%*[^=]=%d", szValue, szName, &ulColumn);

    if (dos_strcmp("admin", wp->userName) && dos_strcmp(szName, wp->userName))
    {
        /* 没有权限 */
    }
    else
    {
        switch (ulColumn)
        {
            case 2:
                strcpy(szColumnName, "userName");
                break;
            case 3:
                strcpy(szColumnName, "fixedTel");
                break;
            case 4:
                strcpy(szColumnName, "mobile");
                break;
            case 5:
                strcpy(szColumnName, "mailbox");
                break;
            default:
                goto end;       /* 使用了goto，所有没有用break */
        }


        /* 更新数据库 */
        sprintf(achSql, "update pts_user set %s='%s' where name='%s';", szColumnName, szValue, szName);
        printf("change info : %s\n", achSql);
        lResult = dos_sqlite3_exec(g_stMySqlite, achSql);
        if (lResult < 0)
        {
            /* 失败 */
            pt_logr_debug("error");
        }
        else
        {
            /* 成功 */
            printf("succ\n");
            websHeader(wp);
            websWrite(wp, T("%s"), szValue);
            websFooter(wp);
        }
    }

end:    websDone(wp, 200);
    return 1;
}

int pts_webs_auto_redirect(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    printf("I am here\n");
    pts_webs_auto_Redirect(wp, wp->url);
    return 1;
}

/**
 * 函数：S32 pts_send_ptc_list2web_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
 * 功能：请求ptc列表的回调函数
 * 参数
 * 返回值:
 */

S32 pts_send_ptc_list2web_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    PTS_SQLITE_PARAM_ST* pstSqliteParam = (PTS_SQLITE_PARAM_ST *)para;
    S8 *pBUff = NULL;

    pstSqliteParam->ulResCount++;
    if (pstSqliteParam->ulBuffLen + PT_DATA_BUFF_256 > pstSqliteParam->ulMallocSize)
    {
        pBUff = (S8 *)dos_dmem_alloc(pstSqliteParam->ulMallocSize + PT_DATA_BUFF_1024);
        if (NULL == pBUff)
        {
            return -1;
        }

        dos_memcpy(pBUff, pstSqliteParam->pszBuff, pstSqliteParam->ulBuffLen);
        dos_dmem_free(pstSqliteParam->pszBuff);
        pstSqliteParam->pszBuff = pBUff;
        pstSqliteParam->ulMallocSize += PT_DATA_BUFF_1024;
    }

    pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='radio' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],"
        ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[1], column_value[24]);

    return 0;
}


S32 pts_ptc_list_switch_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    PTS_SQLITE_PARAM_ST* pstSqliteParam = (PTS_SQLITE_PARAM_ST *)para;
    S8 *pBUff = NULL;

    pstSqliteParam->ulResCount++;

    if (pstSqliteParam->ulBuffLen + PT_DATA_BUFF_256 > pstSqliteParam->ulMallocSize)
    {
        pBUff = (S8 *)dos_dmem_alloc(pstSqliteParam->ulMallocSize + PT_DATA_BUFF_1024);
        if (NULL == pBUff)
        {
            return -1;
        }

        dos_memcpy(pBUff, pstSqliteParam->pszBuff, pstSqliteParam->ulBuffLen);
        dos_dmem_free(pstSqliteParam->pszBuff);
        pstSqliteParam->pszBuff = pBUff;
        pstSqliteParam->ulMallocSize += PT_DATA_BUFF_1024;
    }

    if (0 == atoi(column_value[17]))
    {
        pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"\", \"\", \"\", \"%s\"],"
            ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[1]);
    }
    else
    {
        if (0 == atoi(column_value[18]))
        {
             pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s:%s\", \"\", \"\", \"%s\"],"
                ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[17], column_value[20], column_value[1]);
        }
        else
        {
            if (0 == atoi(column_value[19]))
            {
                 pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s:%s\", \"%s:%s\", \"\", \"%s\"],"
                    ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[17], column_value[20], column_value[18], column_value[21], column_value[1]);
            }
            else
            {
                pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s:%s\", \"%s:%s\", \"%s:%s\", \"%s\"],"
                    ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[17], column_value[20], column_value[18], column_value[21], column_value[19], column_value[22], column_value[1]);
            }
        }
    }

    return 0;
}

S32 pts_ptc_list_config_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    PTS_SQLITE_PARAM_ST* pstSqliteParam = (PTS_SQLITE_PARAM_ST *)para;
    S8 *pBUff = NULL;

    pstSqliteParam->ulResCount++;

    if (pstSqliteParam->ulBuffLen + PT_DATA_BUFF_256 > pstSqliteParam->ulMallocSize)
    {
        pBUff = (S8 *)dos_dmem_alloc(pstSqliteParam->ulMallocSize + PT_DATA_BUFF_1024);
        if (NULL == pBUff)
        {
            return -1;
        }

        dos_memcpy(pBUff, pstSqliteParam->pszBuff, pstSqliteParam->ulBuffLen);
        dos_dmem_free(pstSqliteParam->pszBuff);
        pstSqliteParam->pszBuff = pBUff;
        pstSqliteParam->ulMallocSize += PT_DATA_BUFF_1024;
    }

    pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s:%s\", \"%s:%s\", \"%s\"],"
        ,column_value[1], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[13], column_value[15], column_value[14], column_value[16], column_value[1]);

    return 0;
}

S32 pts_ptc_list_upgrades_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    PTS_SQLITE_PARAM_ST* pstSqliteParam = (PTS_SQLITE_PARAM_ST *)para;
    S8 *pBUff = NULL;

    pstSqliteParam->ulResCount++;

    if (pstSqliteParam->ulBuffLen + PT_DATA_BUFF_256 > pstSqliteParam->ulMallocSize)
    {
        pBUff = (S8 *)dos_dmem_alloc(pstSqliteParam->ulMallocSize + PT_DATA_BUFF_1024);
        if (NULL == pBUff)
        {
            return -1;
        }

        dos_memcpy(pBUff, pstSqliteParam->pszBuff, pstSqliteParam->ulBuffLen);
        dos_dmem_free(pstSqliteParam->pszBuff);
        pstSqliteParam->pszBuff = pBUff;
        pstSqliteParam->ulMallocSize += PT_DATA_BUFF_1024;
    }

    pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s|%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],"
        ,column_value[1], column_value[4], column_value[6], column_value[2], column_value[3], column_value[12], column_value[9], column_value[8], column_value[23], column_value[4], column_value[1]);

    return 0;
}

S32 pts_user_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    PTS_SQLITE_PARAM_ST* pstSqliteParam = (PTS_SQLITE_PARAM_ST *)para;
    S8 *pBUff = NULL;

    pstSqliteParam->ulResCount++;

    if (pstSqliteParam->ulBuffLen + PT_DATA_BUFF_256 > pstSqliteParam->ulMallocSize)
    {
        pBUff = (S8 *)dos_dmem_alloc(pstSqliteParam->ulMallocSize + PT_DATA_BUFF_1024);
        if (NULL == pBUff)
        {
            return -1;
        }

        dos_memcpy(pBUff, pstSqliteParam->pszBuff, pstSqliteParam->ulBuffLen);
        dos_dmem_free(pstSqliteParam->pszBuff);
        pstSqliteParam->pszBuff = pBUff;
        pstSqliteParam->ulMallocSize += PT_DATA_BUFF_1024;
    }

    pstSqliteParam->ulBuffLen += snprintf(pstSqliteParam->pszBuff+pstSqliteParam->ulBuffLen, pstSqliteParam->ulMallocSize-pstSqliteParam->ulBuffLen, "[\"<INPUT name='PtcCheckBox'  type='checkbox' value='%s' >\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"],"
        ,column_value[0], column_value[0], column_value[2], column_value[3], column_value[4], column_value[5], column_value[6]);

    return 0;
}


static int pts_search_db(int eid, webs_t wp, int argc, char_t **argv, S8 szPtcListFields[][PT_DATA_BUFF_32], U32 ulFieldCount, my_sqlite_callback psqlites_callback, S8 *szTableName, S8 *szOtherCondition, U32 ulOtherLen)
{
    S8 *achSql = NULL;
    PTS_SQLITE_PARAM_ST stSqliteParam;
    S8 *pszBuff = NULL;
    //S8 szPtcListFields[PTS_VISIT_FILELDS_COUNT][PT_DATA_BUFF_16] = {"", "sn", "name", "remark", "ptcType", "internetIP", "intranetIP", "szMac", "lastLoginTime"};
    S8 szLimit[PT_DATA_BUFF_16] = {0};
    S8 szOrder[PT_DATA_BUFF_64] = {0};
    S8 *szWhere = NULL;
    S8 *szDisplayStart = NULL;
    S8 *szDisplayLen = NULL;
    S8 *szSortCol = NULL;
    S8 *szSortDir = NULL;
    S8 *szSearch = NULL;
    S8 *szEcho = NULL;
    S32 lSortCol = 0;
    S32 i = 0;
    S32 lResult = 0;

    pszBuff = (S8 *)dos_dmem_alloc(PT_DATA_BUFF_1024 * 5);
    if (NULL == pszBuff)
    {
        DOS_ASSERT(0);
        goto error;
    }
    achSql = (S8 *)dos_dmem_alloc(PT_DATA_BUFF_1024 * 2);
    if (NULL == achSql)
    {
        DOS_ASSERT(0);
        goto error;
    }
    szWhere = (S8 *)dos_dmem_alloc(PT_DATA_BUFF_1024);
    if (NULL == szWhere)
    {
        DOS_ASSERT(0);
        goto error;
    }

    //dos_memzero(pszBuff, PT_DATA_BUFF_1024 * 5);
    dos_memzero(achSql, PT_DATA_BUFF_1024 * 2);
    dos_memzero(szWhere, PT_DATA_BUFF_1024);
    stSqliteParam.ulResCount = 0;
    stSqliteParam.ulBuffLen = 0;
    stSqliteParam.pszBuff = pszBuff;
    pszBuff = NULL;
    stSqliteParam.ulMallocSize = PT_DATA_BUFF_1024 * 5;

    /* 组装sql语句 */
    szDisplayStart = websGetVar(wp, T("iDisplayStart"), T(""));
    szDisplayLen = websGetVar(wp, T("iDisplayLength"), T(""));
    szSortCol = websGetVar(wp, T("iSortCol_0"), T(""));
    szSortDir = websGetVar(wp, T("sSortDir_0"), T(""));
    szSearch = websGetVar(wp, T("sSearch"), T(""));
    szEcho = websGetVar(wp, T("sEcho"), T(""));

    if (szDisplayStart[0] != '\0' && atoi(szDisplayLen) != -1)
    {
        snprintf(szLimit, PT_DATA_BUFF_16, "LIMIT %s, %s", szDisplayStart, szDisplayLen);
    }

    lSortCol = atoi(szSortCol);
    if (lSortCol != 0)
    {
        snprintf(szOrder, PT_DATA_BUFF_64, "ORDER BY %s %s", szPtcListFields[lSortCol], szSortDir);
    }

    if (szSearch[0] != '\0')
    {
        U32 ulLen = 0;

        if (szOtherCondition != NULL)
        {
            strcpy(szWhere, "WHERE (");
            ulLen = dos_strlen("WHERE (");
        }
        else
        {
            strcpy(szWhere, "WHERE ");
            ulLen = dos_strlen("WHERE ");
        }

        for (i=1; i<ulFieldCount; i++)
        {
            ulLen += dos_snprintf(szWhere+ulLen, PT_DATA_BUFF_1024-ulLen, "%s LIKE '%%%s%%' OR ", szPtcListFields[i], szSearch);
            if (ulLen + 1 >= PT_DATA_BUFF_1024)
            {
                DOS_ASSERT(0);
                goto error;
            }
        }

        szWhere[dos_strlen(szWhere) - 3] = '\0';
        ulLen -= 3;
        if (szOtherCondition != NULL)
        {
            if (ulLen + ulOtherLen + 3 <= PT_DATA_BUFF_1024)
            {
                dos_snprintf(szWhere+ulLen, PT_DATA_BUFF_1024-ulLen, ") %s ", szOtherCondition);
            }
        }
    }
    else
    {
        if (szOtherCondition != NULL)
        {
            sprintf(szWhere, "WHERE %s ", szOtherCondition);
        }
    }

    //websWrite(wp, T("%s"), "{\"sEcho\":1,\"iTotalRecords\":\"5\",\"iTotalDisplayRecords\":\"2\",\"aaData\":[");
    websWrite(wp, T("%s"), "{\"aaData\":[");
    dos_snprintf(achSql, PT_DATA_BUFF_1024*2, "select * from %s %s %s %s;", szTableName, szWhere, szOrder, szLimit);
    printf("%s\n", achSql);
    lResult = dos_sqlite3_exec_callback(g_stMySqlite, achSql, psqlites_callback, (VOID *)&stSqliteParam);
    if (lResult != DOS_SUCC)
    {
        DOS_ASSERT(0);
        goto error;
    }

    printf("!!!!!!!!!strlen : %d\n", dos_strlen(stSqliteParam.pszBuff));
    stSqliteParam.pszBuff[stSqliteParam.ulBuffLen - 1] = '\0';
    websWrite(wp, T("%s],"), stSqliteParam.pszBuff);
    websWrite(wp, T("\"sEcho\":\"%s\", \"iTotalDisplayRecords\":\"%d\"}"), szEcho, stSqliteParam.ulResCount);

    if (stSqliteParam.pszBuff)
    {
        dos_dmem_free(stSqliteParam.pszBuff);
        stSqliteParam.pszBuff = NULL;
    }
    if (achSql)
    {
        dos_dmem_free(achSql);
        achSql = NULL;
    }
    if (szWhere)
    {
        dos_dmem_free(szWhere);
        szWhere = NULL;
    }

    return websWrite(wp, T("%s"), "");

error:
    if (stSqliteParam.pszBuff)
    {
        dos_dmem_free(stSqliteParam.pszBuff);
        stSqliteParam.pszBuff = NULL;
    }
    if (achSql)
    {
        dos_dmem_free(achSql);
        achSql = NULL;
    }
    if (szWhere)
    {
        dos_dmem_free(szWhere);
        szWhere = NULL;
    }
    return websWrite(wp, T("%s"), "{\"aaData\":[],\"sEcho\":\"%s\", \"iTotalDisplayRecords\":\"0\"}", szEcho);
}

static int pts_get_ptc_list_from_db(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szPtcListFields[PTS_VISIT_FILELDS_COUNT][PT_DATA_BUFF_32] = {"", "lastLoginTime", "name", "remark", "ptcType", "internetIP", "intranetIP", "szMac", "sn", "heartbeatTime"};

    return pts_search_db(eid, wp, argc, argv, szPtcListFields, PTS_VISIT_FILELDS_COUNT, pts_send_ptc_list2web_callback, "ipcc_alias", "register = 1", dos_strlen("register = 1"));
}

static int pts_get_ptc_list_from_db_switch(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szPtcListFields[PTS_SWITCH_FILELDS_COUNT][PT_DATA_BUFF_32] = {"", "lastLoginTime", "name", "remark", "ptcType", "internetIP", "intranetIP", "szMac", "szPtsHistoryIp1", "szPtsHistoryIp2", "szPtsHistoryIp3", "sn"};

    return pts_search_db(eid, wp, argc, argv, szPtcListFields, PTS_SWITCH_FILELDS_COUNT, pts_ptc_list_switch_callback, "ipcc_alias", "register = 1 AND ptcType != 'PC'", dos_strlen("register = 1 AND ptcType != 'PC'"));
}


static int pts_get_ptc_list_from_db_config(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szPtcListFields[PTS_CONFIG_FILELDS_COUNT][PT_DATA_BUFF_32] = {"", "lastLoginTime", "name", "remark", "ptcType", "internetIP", "intranetIP", "szMac", "achPtsMajorDomain", "achPtsMinorDomain", "sn"};

    return pts_search_db(eid, wp, argc, argv, szPtcListFields, PTS_CONFIG_FILELDS_COUNT, pts_ptc_list_config_callback, "ipcc_alias", "register = 1 AND ptcType != 'PC'", dos_strlen("register = 1 AND ptcType != 'PC'"));
}

static int pts_get_ptc_list_from_db_upgrades(int eid, webs_t wp, int argc, char_t **argv)
{

    S8 szPtcListFields[PTS_UPGRADES_FILELDS_COUNT][PT_DATA_BUFF_32] = {"", "lastLoginTime", "name", "remark", "ptcType", "internetIP", "intranetIP", "szMac", "version", "sn"};

    return pts_search_db(eid, wp, argc, argv, szPtcListFields, PTS_UPGRADES_FILELDS_COUNT, pts_ptc_list_upgrades_callback, "ipcc_alias", "register = 1 AND ptcType != 'PC'", dos_strlen("register = 1 AND ptcType != 'PC'"));
}

static int pts_user_list(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szPtcListFields[PTS_USER_COUNT][PT_DATA_BUFF_32] = {"", "name", "userName", "fixedTel", "mobile", "mailbox", "regiestTime"};

    return pts_search_db(eid, wp, argc, argv, szPtcListFields, PTS_USER_COUNT, pts_user_callback, "pts_user", NULL, 0);
}


static int pts_create_user_button(int eid, webs_t wp, int argc, char_t **argv)
{
    if (dos_strcmp(wp->userName, "admin") == 0)
    {
        /* admin用户，可创建用户 */
        return websWrite(wp, T("%s"), CREATE_NEW_USER);
    }
    else
    {
        return websWrite(wp, T(""));
    }
}

static int curr_user(int eid, webs_t wp, int argc, char_t **argv)
{
    return websWrite(wp, T("%s"), wp->userName);
}

static int ptc_upgrade_button(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szButtonHtml[PT_DATA_BUFF_128] = {0};
    if (bIsUpdating)
    {
        dos_snprintf(szButtonHtml, PT_DATA_BUFF_128, "<input id='upgrade' type='submit' value='升级' onclick='return form_check_package()' disabled />");
    }
    else
    {
        dos_snprintf(szButtonHtml, PT_DATA_BUFF_128, "<input id='upgrade' type='submit' value='升级' onclick='return form_check_package()' />");
    }
    return websWrite(wp, T("%s"), szButtonHtml);
}

static int all_ptc_upgrade_button(int eid, webs_t wp, int argc, char_t **argv)
{
    S8 szButtonHtml[PT_DATA_BUFF_256] = {0};
    if (bIsUpdating)
    {
        dos_snprintf(szButtonHtml, PT_DATA_BUFF_256, "<input id='upgrade_all' type='submit' value='升级' onclick='return form_check_package_all()' disabled />\
            <input id='upgrade_end' type='button' value='结束' onclick='cancel_upgrade()' style=\"display:block\" />");
    }
    else
    {
        dos_snprintf(szButtonHtml, PT_DATA_BUFF_256, "<input id='upgrade_all' type='submit' value='升级' onclick='return form_check_package_all()' />\
            <input id='upgrade_end' type='button' value='结束' onclick='cancel_upgrade()' style=\"display:none\" />");
    }
    return websWrite(wp, T("%s"), szButtonHtml);
}

/******************************************************************************/
/*
 *  Test form for posted data (in-memory CGI). This will be called when the
 *  form in web/forms.asp is invoked. Set browser to "localhost/forms.asp" to test.
 */

static void pts_create_user(webs_t wp, char_t *path, char_t *query)
{
    char_t  *szName, *szPassWd, *szUserName, *szFixedTel, *szMobile, *szMailbox;
    //S32 lResult = 0;
    S8  pcListBuf[PT_DATA_BUFF_512] = {0};
    S8  achSql[PTS_SQL_STR_SIZE] = {0};
    S8 *pPassWordMd5 = NULL;

    szName = websGetVar(wp, T("name"), T(""));
    szPassWd = websGetVar(wp, T("password"), T(""));
    szUserName = websGetVar(wp, T("userName"), T(""));
    szFixedTel = websGetVar(wp, T("fixedTel"), T(""));
    szMobile = websGetVar(wp, T("mobile"), T(""));
    szMailbox = websGetVar(wp, T("mailbox"), T(""));

    printf("szUserName : %s\n", szUserName);

    sprintf(achSql, "select count(*) from pts_user where name='%s'", szName);
    if (dos_sqlite3_record_is_exist(g_stMySqlite, achSql))  /* 判断是否存在 */
    {
        /* 已存在 */
        sprintf(pcListBuf, "%s existed", szName);
    }
    else
    {
        pPassWordMd5 = pts_md5_encrypt(szName, szPassWd);
        sprintf(achSql, "INSERT INTO pts_user (\"name\", \"password\", \"userName\", \"fixedTel\", \"mobile\", \"mailbox\") VALUES ('%s', '%s', '%s', '%s', '%s', '%s');", szName, pPassWordMd5, szUserName, szFixedTel, szMobile, szMailbox);
        dos_sqlite3_exec(g_stMySqlite, achSql);
        sprintf(pcListBuf, "succ");
        pts_goAhead_free(pPassWordMd5);
        pPassWordMd5 = NULL;
    }

    websRedirect(wp, "pts_user.html");
}


static void pts_change_password(webs_t wp, char_t *path, char_t *query)
{
    S32 lResult = 0;
    S8 pcListBuf[PT_DATA_BUFF_128] = {0};
    S8 achSql[PTS_SQL_STR_SIZE] = {0};
    S8 *szWebsPassword = NULL;
    S8 *pPassWordMd5 = NULL;
    S8 *pNewPassWordMd5 = NULL;

    char_t  *szName, *szPassWd, *szPassWdOld;

    szName = websGetVar(wp, T("name"), T(""));
    szPassWdOld = websGetVar(wp, T("password"), T(""));
    szPassWd = websGetVar(wp, T("new_password"), T(""));

    /* 判断user是否存在 */
    sprintf(achSql, "select count(*) from pts_user where name='%s'", szName);
    if (!dos_sqlite3_record_is_exist(g_stMySqlite, achSql))  /* 判断是否存在 */
    {
        /* 不存在 */
        sprintf(pcListBuf, "%s is not existed", szName);
    }
    else
    {
        /* 判断原密码是否正确 */
        szWebsPassword = (S8 *)dos_dmem_alloc(PT_DATA_BUFF_128);
        if (NULL == szWebsPassword)
        {
           sprintf(pcListBuf, "%s malloc fail", szName);
        }
        else
        {
            lResult = pts_get_password_from_sqlite_db(szName, szWebsPassword);
            if (lResult != DOS_SUCC)
            {
                dos_dmem_free(szWebsPassword);
                sprintf(pcListBuf, "%s get password fail", szName);
            }
            else
            {
                pPassWordMd5 = pts_md5_encrypt(szName, szPassWdOld);
                if (strcmp(szWebsPassword, pPassWordMd5) == 0)
                {
                     /* 修改密码 */
                    pNewPassWordMd5 = pts_md5_encrypt(szName, szPassWd);
                    sprintf(achSql, "update pts_user set password='%s' where name='%s';", pNewPassWordMd5, szName);
                    dos_sqlite3_exec(g_stMySqlite, achSql);
                    sprintf(pcListBuf, "%s succ", szName);
                }
                else
                {
                    sprintf(pcListBuf, "%s password error", szName);
                }
                pts_goAhead_free(pPassWordMd5);
                pPassWordMd5 = NULL;
                pts_goAhead_free(pNewPassWordMd5);
                pNewPassWordMd5 = NULL;
                dos_dmem_free(szWebsPassword);
                szWebsPassword = NULL;
            }
        }
    }

    websRedirect(wp, "pts_user.html");
}

static void pts_ping(webs_t wp, char_t *path, char_t *query)
{
    char_t *szPingDestPtc, *szPingNum, *szPingPacketSize;
    U32 ulPingNum = 0;
    U32 ulPingPacketSize = 0;
    PT_PING_PACKET_ST stPingPacket;
    S8 acBuff[PT_SEND_DATA_SIZE] = {0};

    szPingDestPtc = websGetVar(wp, T("PingDest"), T(""));
    szPingNum = websGetVar(wp, T("PingNum"), T(""));
    szPingPacketSize = websGetVar(wp, T("PingPacketSize"), T(""));
    ulPingNum = atoi(szPingNum);
    ulPingPacketSize = atoi(szPingPacketSize);

    stPingPacket.ulPacketSize = ulPingPacketSize;
    stPingPacket.ulPingNum = ulPingNum;
    stPingPacket.pWpHandle = (void *)wp;
    printf("szPingDestPtc : %s, szPingNum : %s, szPingPacketSize : %s\n", szPingDestPtc, szPingNum, szPingPacketSize);
    websHeader(wp);
    websWrite(wp, T("Pinging %s with %s bytes of data:<br/>"), szPingDestPtc, szPingPacketSize);
    while (ulPingNum--)
    {
        printf("wp addr : %p\n", wp);
        stPingPacket.ulSeq = stPingPacket.ulPingNum - ulPingNum;
        gettimeofday(&stPingPacket.stStartTime, NULL);
        dos_memcpy(acBuff, &stPingPacket, sizeof(stPingPacket));
        pts_save_msg_into_cache((U8*)szPingDestPtc, PT_DATA_CTRL, PT_CTRL_PING, acBuff, sizeof(stPingPacket), NULL, 0);
        sleep(1);
    }
    websFooter(wp);
    websDone(wp, 200);
}

/******************************************************************************/
/*
 *  Home page handler
 */

static int websHomePageHandler(webs_t wp, char_t *urlPrefix, char_t *webDir,
    int arg, char_t *url, char_t *path, char_t *query)
{
/*
 *  If the empty or "/" URL is invoked, redirect default URLs to the home page
 */
    if (*url == '\0' || gstrcmp(url, T("/")) == 0) {
        websRedirect(wp, WEBS_DEFAULT_HOME);
        return 1;
    }
    return  0;
}

void pts_websResponse(webs_t wp, int code, char_t *message, char_t *redirect, char_t *setcookie)
{
    char_t      *date;

    a_assert(websValid(wp));

/*
 *  IE3.0 needs no Keep Alive for some return codes.
 */
    wp->flags &= ~WEBS_KEEP_ALIVE;

/*
 *  Only output the header if a header has not already been output.
 */
    if ( !(wp->flags & WEBS_HEADER_DONE)) {
        wp->flags |= WEBS_HEADER_DONE;
/*
 *      Redirect behaves much better when sent with HTTP/1.0
 */
        if (redirect != NULL) {
            websWrite(wp, T("HTTP/1.0 %d %s\r\n"), code, websErrorMsg(code));
        } else {
            websWrite(wp, T("HTTP/1.1 %d %s\r\n"), code, websErrorMsg(code));
        }
/*
 *      The Server HTTP header below must not be modified unless
 *      explicitly allowed by licensing terms.
 */
#ifdef WEBS_SSL_SUPPORT
        websWrite(wp, T("Server: %s/%s %s/%s\r\n"),
            WEBS_NAME, WEBS_VERSION, SSL_NAME, SSL_VERSION);
#else
        websWrite(wp, T("Server: %s/%s\r\n"), WEBS_NAME, WEBS_VERSION);
#endif
/*
 *      Timestamp/Date is usually the next to go
 */
        if ((date = websGetDateString(NULL)) != NULL) {
            websWrite(wp, T("Date: %s\r\n"), date);
            bfree(B_L, date);
        }
/*
 *      If authentication is required, send the auth header info
 */
        if (wp->flags & WEBS_KEEP_ALIVE) {
            websWrite(wp, T("Connection: keep-alive\r\n"));
        }
        if (setcookie != NULL)
        {
            websWrite(wp, T(setcookie));
        }
        websWrite(wp, T("Pragma: no-cache\r\nCache-Control: max-age=0, private, no-store, no-cache, must-revalidate\r\n"));
        websWrite(wp, T("Content-Type: text/html\r\n"));
        websWrite(wp, T("Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"));
/*
 *      We don't do a string length here as the message may be multi-line.
 *      Ie. <CR><LF> will count as only one and we will have a content-length
 *      that is too short.
 *
 *      websWrite(wp, T("Content-Length: %s\r\n"), message);
 */
        if (redirect) {
            websWrite(wp, T("Location: %s\r\n"), redirect);
        }
        websWrite(wp, T("\r\n"));
    }

/*
 *  If the browser didn't do a HEAD only request, send the message as well.
 */
    if ((wp->flags & WEBS_HEAD_REQUEST) == 0 && message && *message) {
        websWrite(wp, T("%s\r\n"), message);
    }
    websDone(wp, code);
}

void pts_websRedirect(webs_t wp, char_t *url)
{
    char_t  *msgbuf, *urlbuf, *redirectFmt;
    S8 szDestIp[PT_IP_ADDR_SIZE] = {0};
    S8 szSetCookie[PT_DATA_BUFF_256] = {0};
    PTS_SERV_SOCKET_ST stServSocket;
    U16 usDestPort = 0;
    S8 szDestPort[PT_DATA_BUFF_16] = {0};
    S8 szPtcID[PTC_ID_LEN + 1] = {0};
    websRec stWpCpy;
    S32 lResult = 0;
    S32 i = 0;

    a_assert(websValid(wp));
    a_assert(url);

    websStats.redirects++;
    msgbuf = urlbuf = NULL;

/*
 *  Some browsers require a http://host qualified URL for redirection
 */
    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%[^&]%*[^=]=%s", szPtcID, szDestIp, szDestPort);
    if (lResult < 0)
    {
        websError(wp, 403, T("%s"), strerror(errno));
        return;
    }
    szPtcID[PTC_ID_LEN] = '\0';
    usDestPort = atoi(szDestPort);

    redirectFmt = T("http://%s:%d/%s");

    for (i=0; i<PTS_WEB_SERVER_MAX_SIZE; i++)
    {
        if (g_lPtsServSocket[i].lSocket <= 0)
        {
            break;
        }
    }

    if (i < PTS_WEB_SERVER_MAX_SIZE)
    {
        pts_create_web_serv_socket(i);
        stServSocket = g_lPtsServSocket[i];
    }
    else
    {
        websError(wp, 403, T("port is use up\nPlease try again later\n"));
        return;
    }

    pt_logr_debug("now use port is %d\n", stServSocket.usServPort);
    fmtAlloc(&urlbuf, WEBS_MAX_URL + 80, redirectFmt
        , wp->host, stServSocket.usServPort, T(""));
    url = urlbuf;
/*
 *  Add human readable message for completeness. Should not be required.
 */
    fmtAlloc(&msgbuf, WEBS_MAX_URL + 80,
        T("<html><head>\r\n\
        This document has moved to a new <a href=\"%s\">location</a>.\r\n\
        Please update your documents to reflect the new location.\r\n\
        </body></html>\r\n"), urlbuf);


    stWpCpy = *wp;
    snprintf(szSetCookie, PT_DATA_BUFF_256, "Set-Cookie: ptsId_%d=%s!%s!%d;\r\n"
        , stServSocket.usServPort, szPtcID, szDestIp, usDestPort);
    pts_websResponse(&stWpCpy, 302, msgbuf, url, szSetCookie);

    bfreeSafe(B_L, msgbuf);
    bfreeSafe(B_L, urlbuf);
}

void pts_webs_auto_Redirect(webs_t wp, char_t *url)
{
    char_t  *msgbuf, *urlbuf, *redirectFmt;
    S8 aucDestID[PTC_ID_LEN+1] = {0};
    S8 szSetCookie[PT_DATA_BUFF_256] = {0};
    PTS_SERV_SOCKET_ST stServSocket;
    U16 usDestPort = 0;
    S8 pDestInternetIp[PT_DATA_BUFF_64] = {0};
    S8 pDestIntranetIp[PT_DATA_BUFF_64] = {0};
    S8 pDestPort[PT_DATA_BUFF_64] = {0};
    S32 i = 0;
    S32 lResult = 0;

    a_assert(websValid(wp));
    a_assert(url);

    websStats.redirects++;
    msgbuf = urlbuf = NULL;

    printf("url = %s, host : %s\n", url, wp->host);

/*
 *  Some browsers require a http://host qualified URL for redirection
 */
    if (*url == '/') {
            url++;
        }

    /* 解析url */
    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%[^&]%*[^=]=%[^&]", pDestInternetIp, pDestIntranetIp, pDestPort);
    if (lResult != 3)
    {
        perror("sscanf");
        websError(wp, 403, T("param error!\n"));
        return;
    }
    printf(" %s, %s, %s\n", pDestInternetIp, pDestIntranetIp, pDestPort);
    /* 查找ptc */
    lResult = pts_find_ptc_by_dest_addr(pDestInternetIp, pDestIntranetIp, aucDestID);
    if (lResult != DOS_SUCC)
    {
        /* 没有找到ptc */
        websError(wp, 403, T("not found ptc\n"));
        return;
    }

    usDestPort = atoi(pDestPort);
    redirectFmt = T("http://%s:%d/%s");

    for (i=0; i<PTS_WEB_SERVER_MAX_SIZE; i++)
    {
        if (g_lPtsServSocket[i].lSocket <= 0)
        {
            break;
        }
    }

    if (i < PTS_WEB_SERVER_MAX_SIZE)
    {
        pts_create_web_serv_socket(i);
        stServSocket = g_lPtsServSocket[i];
    }
    else
    {
        websError(wp, 403, T("port is use up\nPlease try again later\n"));
        return;
    }

    pt_logr_debug("now use port is %d\n", stServSocket.usServPort);
    fmtAlloc(&urlbuf, WEBS_MAX_URL + 80, redirectFmt
        , wp->host, stServSocket.usServPort, T(""));
    url = urlbuf;
/*
 *  Add human readable message for completeness. Should not be required.
 */
    fmtAlloc(&msgbuf, WEBS_MAX_URL + 80,
        T("<html><head>\r\n\
        This document has moved to a new <a href=\"%s\">location</a>.\r\n\
        Please update your documents to reflect the new location.\r\n\
        </body></html>\r\n"), urlbuf);

    snprintf(szSetCookie, PT_DATA_BUFF_256, "Set-Cookie: ptsId_%d=%s!%s!%d;\r\n"
        , stServSocket.usServPort, aucDestID, pDestIntranetIp, usDestPort);

    pts_websResponse(wp, 302, msgbuf, url, szSetCookie);
    bfreeSafe(B_L, msgbuf);
    bfreeSafe(B_L, urlbuf);
}

int pts_recv_from_ipcc_req(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    /* 身份验证 */
    printf("!!!!url = %s\n", wp->url);
    S8 *pStrrchr = NULL;
    S8 szUrlVerify[PT_DATA_BUFF_256] = {0};
    S8 *pLocalMd5 = NULL;
    S8 *pMd5 = NULL;

    pStrrchr = strrchr(wp->url, '&');
    if (pStrrchr != NULL)
    {
        pStrrchr[0] = '\0';
        dos_snprintf(szUrlVerify, PT_DATA_BUFF_256, "%s&Authentication", wp->url);
        pLocalMd5 = websMD5(szUrlVerify);
        printf("url : %s, pLocalMd5 : %s\n", szUrlVerify, pLocalMd5);
        pMd5 = strchr(pStrrchr+1, '=');
        if (pMd5 != NULL)
        {
            if (0 == dos_strcmp(pLocalMd5, pMd5+1))
            {
                bfreeSafe(B_L, pLocalMd5);
                pStrrchr[0] = '&';
                pts_webs_auto_Redirect(wp, wp->url);
                return 1;
            }
        }

        bfreeSafe(B_L, pLocalMd5);
    }

    return 0;
}

/* ping 超时 */
void pts_ping_timeout(U64 param)
{
    S32 lResult = 0;
    PTS_PING_TIMEOUT_PARAM_ST *pstTimeOutParam = (PTS_PING_TIMEOUT_PARAM_ST *)param;
    //U32 ulSeq = pstTimeOutParam->ulSeq;
    //U32 ulDataField = ulSeq & 7;
    U32 ulDataField = 0;
    S8 achSql[PTS_SQL_STR_SIZE] = {0};
    S32 lCurPosition = -1;

    sprintf(achSql,"select curr_position from ping_result where sn='%.*s' limit 1 ", PTC_ID_LEN, pstTimeOutParam->szPtcId);
    lResult = dos_sqlite3_exec_callback(g_stMySqlite, achSql, pts_get_curr_position_callback, (VOID *)&lCurPosition);
    if (lResult != DOS_SUCC)
    {
        DOS_ASSERT(0);
        return;
    }

    if (lCurPosition < 0)
    {
        dos_snprintf(achSql, PTS_SQL_STR_SIZE, "INSERT INTO ping_result (\"id\", \"sn\", \"curr_position\", \"timer0\") VALUES (NULL, '%.*s', 0, -1)", PTC_ID_LEN, pstTimeOutParam->szPtcId);
    }
    else
    {   ulDataField = lCurPosition + 1;
        if (ulDataField > 7)
        {
            ulDataField = 0;
        }

        dos_snprintf(achSql, PTS_SQL_STR_SIZE, "update ping_result set curr_position=%d, timer%d=-1 where sn='%.*s'", ulDataField, ulDataField, PTC_ID_LEN, pstTimeOutParam->szPtcId);
    }
    printf("save %s\n", achSql);
    lResult = dos_sqlite3_exec(g_stMySqlite, achSql);

    return;
}

int pts_start_ping(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    S8 szPtcID[PTC_ID_LEN + 1] = {0};
    S8 szPingPacketSize[PT_DATA_BUFF_10] = {0};
    S8 szPacketSeq[PT_DATA_BUFF_10] = {0};
    S8 acBuff[PT_SEND_DATA_SIZE] = {0};
    PT_PING_PACKET_ST stPingPacket;
    PTS_PING_TIMEOUT_PARAM_ST stTimeOutParam;

    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%[^&]%*[^=]=%[^&]", szPtcID, szPingPacketSize, szPacketSeq);
    if (lResult != 3)
    {

    }
    else
    {
        dos_strcpy(stTimeOutParam.szPtcId, szPtcID);
        stTimeOutParam.ulSeq = atoi(szPacketSeq);

        /* 超时 */
        lResult = dos_tmr_start(&stPingPacket.hTmrHandle, PTS_PING_TIMEOUT, pts_ping_timeout, (U64)&stTimeOutParam, TIMER_NORMAL_NO_LOOP);
        if (PT_SAVE_DATA_FAIL == lResult)
        {
            pt_logr_debug("pts_save_into_recv_cache : start timer fail");
            return 0;
        }

        stPingPacket.pWpHandle = (void *)wp;
        stPingPacket.ulSeq = atoi(szPacketSeq);
        gettimeofday(&stPingPacket.stStartTime, NULL);
        dos_memcpy(acBuff, &stPingPacket, sizeof(stPingPacket));
        pts_save_msg_into_cache((U8*)szPtcID, PT_DATA_CTRL, PT_CTRL_PING, acBuff, sizeof(stPingPacket), NULL, 0);
    }
    websError(wp, 200, T(""));
    return 0;
}

S32 pts_get_ping_result_callback(VOID *para, S32 n_column, S8 **column_value, S8 **column_name)
{
    webs_t wp = (webs_t)para;
    S32 lCurrPosition = atoi(column_value[2]);
    double lTime = atof(column_value[lCurrPosition + 3]);
    S8 szResult[PT_DATA_BUFF_64] = {0};

    if (lTime < 0)
    {
        dos_snprintf(szResult, PT_DATA_BUFF_64, "time out\n");
    }
    else if (lTime < 1000)
    {
        dos_snprintf(szResult, PT_DATA_BUFF_64, "time < 1 ms\n");
    }
    else
    {
        dos_snprintf(szResult, PT_DATA_BUFF_64, "time is %.2f ms\n", lTime/1000);
    }

    websWrite(wp, T("%s"), szResult);

    return 0;
}

int get_ping_result(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
        char_t *url, char_t *path, char_t* query)
{
    S32 lResult = 0;
    S8 szPtcID[PTC_ID_LEN + 1] = {0};
    S8 szPacketSeq[PT_DATA_BUFF_10] = {0};
    S8 achSql[PTS_SQL_STR_SIZE] = {0};
    U32 ulSeq = 0;
    //U32 ulDataField = 0;

    lResult = sscanf(url,"%*[^=]=%[^&]%*[^=]=%[^&]", szPtcID, szPacketSeq);
    if (lResult != 2)
    {

    }
    else
    {
        /* 查询数据库 */
        ulSeq = atoi(szPacketSeq);
        //ulDataField = ulSeq & 7;
        dos_snprintf(achSql, PTS_SQL_STR_SIZE, "select * from ping_result where sn='%s';", szPtcID);
        lResult = dos_sqlite3_exec_callback(g_stMySqlite, achSql, pts_get_ping_result_callback, (VOID *)wp);
        if (lResult != DOS_SUCC)
        {
            DOS_ASSERT(0);
        }
        websDone(wp, 200);
    }

    return 0;
}

static int pts_webs_redirect_pts_server(webs_t wp, char_t *urlPrefix, char_t *webDir,
    int arg, char_t *url, char_t *path, char_t *query)
{
    pts_websRedirect(wp, wp->url);
    return 1;
}


/******************************************************************************/

#ifdef B_STATS
static void memLeaks()
{
    int     fd;

    if ((fd = gopen(T("leak.txt"), O_CREAT | O_TRUNC | O_WRONLY, 0666)) >= 0) {
        bstats(fd, printMemStats);
        close(fd);
    }
}

/******************************************************************************/
/*
 *  Print memory usage / leaks
 */

static void printMemStats(int handle, char_t *fmt, ...)
{
    va_list     args;
    char_t      buf[256];

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    write(handle, buf, strlen(buf));
}
#endif

/******************************************************************************/

int pts_websSecurityHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
                        char_t *url, char_t *path, char_t *query)
{
    char_t          *type, *userid, *password, *accessLimit;
    int             flags, nRet;
    accessMeth_t    am;
    a_assert(websValid(wp));
    a_assert(url && *url);
    a_assert(path && *path);
/*
 *  Get the critical request details
 */
    type = websGetRequestType(wp);
    password = websGetRequestPassword(wp);
    userid = websGetRequestUserName(wp);
    flags = websGetRequestFlags(wp);

    if (!dos_strncmp(wp->url, "/ipcc_internetIP", dos_strlen("/ipcc_internetIP")))
    {
        /* 退出登陆 */
        return 0;
    }
/*
 *  Get the access limit for the URL.  Exit if none found.
 */
    accessLimit = umGetAccessLimit(path);
/*
 *  Get the access limit for the URL
 */
    am = umGetAccessMethodForURL(accessLimit);

    nRet = 0;
    if ((flags & WEBS_LOCAL_REQUEST) && (debugSecurity == 0)) {
/*
 *      Local access is always allowed (defeat when debugging)
 */
    } else if (am == AM_NONE) {
/*
 *      URL is supposed to be hidden!  Make like it wasn't found.
 */
        websStats.access++;
        websError(wp, 404, T("Page Not Found"));
        nRet = 1;
    } else  if (userid && *userid) {
        if (!umUserExists(userid)) {
            websStats.access++;
            websError(wp, 401, T("Access Denied\nUnknown User"));
            trace(3, T("SEC: Unknown user <%s> attempted to access <%s>\n"),
                userid, path);
            nRet = 1;
        } else if (!umUserCanAccessURL(userid, accessLimit)) {
            websStats.access++;
            websError(wp, 403, T("Access Denied\nProhibited User"));
            nRet = 1;
        } else if (password && * password) {
            char_t * userpass = umGetUserPassword(userid);
            if (userpass) {
                if (gstrcmp(password, userpass) != 0) {
                    websStats.access++;
                    websError(wp, 401, T("Access Denied\nWrong Password"));
                    trace(3, T("SEC: Password fail for user <%s>")
                                T("attempt to access <%s>\n"), userid, path);
                    nRet = 1;
                } else {
/*
 *                  User and password check out.
 */
                }

                bfree (B_L, userpass);
            }
#ifdef DIGEST_ACCESS_SUPPORT
        } else if (flags & WEBS_AUTH_DIGEST) {

            char_t *digestCalc;
            S32 lResult = 0;
            /* 根据用户名，在数据库中查找对应的密码 */
            S8 *szWebsPassword = (S8 *)dos_dmem_alloc(PT_DATA_BUFF_128);
            if (NULL == szWebsPassword)
            {
                websError(wp, 401, T("Access Denied\nMalloc Fail"));
                nRet = 1;
                perror("dos_dmem_alloc");
                return nRet;
            }

            lResult = pts_get_password_from_sqlite_db(userid, szWebsPassword);
            if (lResult != DOS_SUCC)
            {
                dos_dmem_free(szWebsPassword);
                websError(wp, 401, T("Access Denied\nUnknown User"));
                nRet = 1;
                return nRet;
            }
            a_assert(wp->digest);
            a_assert(wp->nonce);
            a_assert(wp->password);
            wp->password = szWebsPassword;
            digestCalc = ptsWebsCalcDigest(wp);
            dos_dmem_free(szWebsPassword);
            wp->password = NULL;
            szWebsPassword = NULL;
            a_assert(digestCalc);
            if (gstrcmp(wp->digest, digestCalc) != 0) {
                bfree (B_L, digestCalc);
                digestCalc = websCalcUrlDigest(wp);
                a_assert(digestCalc);
                if (gstrcmp(wp->digest, digestCalc) != 0) {
                    websStats.access++;

                    websError(wp, 401, T("Access Denied\nWrong Password"));
                    nRet = 1;
                }
            }

            bfree (B_L, digestCalc);
#endif
        } else {
/*
 *          No password has been specified
 */
#ifdef DIGEST_ACCESS_SUPPORT
            if (am == AM_DIGEST) {
                wp->flags |= WEBS_AUTH_DIGEST;
            }
#endif
            websStats.errors++;
            websError(wp, 401,
                T("Access to this document requires a password"));
            nRet = 1;
        }
    } else if (am != AM_FULL) {
/*
 *      This will cause the browser to display a password / username
 *      dialog
 */
#ifdef DIGEST_ACCESS_SUPPORT
        if (am == AM_DIGEST) {
            wp->flags |= WEBS_AUTH_DIGEST;
        }
#endif
        websStats.errors++;
        websError(wp, 401, T("Access to this document requires a User ID"));
        nRet = 1;
    }

    if (accessLimit != NULL)
    {
        bfree(B_L, accessLimit);
    }
    return nRet;
}

S32 pts_get_password_from_sqlite_db(char_t *userid, S8 *szWebsPassword)
{
    S8 achSql[PTS_SQL_STR_SIZE] = {0};

    if (NULL == userid || NULL == szWebsPassword)
    {
        return DOS_FAIL;
    }

    dos_memzero(szWebsPassword, WEBS_MAX_PASS);
    sprintf(achSql, "select password from pts_user where name='%s';", userid);
    dos_sqlite3_exec_callback(g_stMySqlite, achSql, pts_get_password_callback, (VOID *)szWebsPassword);
    if (dos_strlen(szWebsPassword) > 0)
    {
        return DOS_SUCC;
    }

    return DOS_FAIL;
}

S32 pts_get_local_ip(S8 *szLocalIp)
{
    S32 i = 0;
    S8 *ip = NULL;
    S8 buf[PT_DATA_BUFF_512];
    struct ifreq *ifreq;
    struct ifconf ifconf;
    S32 lSocket = 0;

    /* 初始化ifconf */
    ifconf.ifc_len = PT_DATA_BUFF_512;
    ifconf.ifc_buf = buf;

    lSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (lSocket < 0)
    {
        perror("create socket error!");
        return DOS_FAIL;
    }

    /* 获取所有接口信息 */
    ioctl(lSocket, SIOCGIFCONF, &ifconf);

    /* 接下来一个一个的获取IP地址 */
    ifreq = (struct ifreq*)buf;
    i = ifconf.ifc_len/sizeof(struct ifreq);
    for (; i>0; i--)
    {
        ip = inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr);

        /* 排除127.0.0.1，继续下一个 */
        if(dos_strcmp(ip, "127.0.0.1") == 0)
        {
            ifreq++;
            continue;
        }
        dos_strcpy(szLocalIp, ip);

        return DOS_SUCC;
    }

    return DOS_FAIL;
}

S8 *pts_md5_encrypt(S8 *szUserName, S8 *szPassWord)
{
    S8 *a1 = NULL;
    S8 *a1prime = NULL;
    if (NULL == szUserName || NULL == szPassWord)
    {
        return NULL;
    }

    fmtAlloc(&a1, 255, T("%s:%s:%s"), szUserName, websGetRealm(), szPassWord);
    a_assert(a1);
    printf("!!!!%s\n", a1);
    a1prime = websMD5(a1);

    bfreeSafe(B_L, a1);
    a_assert(a1prime);

    return a1prime;
}

VOID pts_goAhead_free(S8 *pPoint)
{
    a_assert(pPoint);
    bfreeSafe(B_L, pPoint);
}

#ifdef  __cplusplus
}
#endif  /* end of __cplusplus */

