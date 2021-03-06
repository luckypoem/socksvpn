
#include "stdafx.h"
#include <windows.h> 
#include <string.h>
#include <process.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#include "Tchar.h"
#include "hookapi.h"
#include "hooksock.h"
#include "procCommClient.h"
#include "entry.h"
#include "proxysocks.h"
#include "logproc.h"

DWORD  g_cur_proc_pid = 0;
char  g_cur_proc_name[256] = {0};
uint16_t g_local_port = 0;
uint32_t g_local_ipaddr = 0x7F000001;
HOOKAPI g_hook_api[HOOK_API_MAX];


HANDLE g_process_hdl;
HMODULE g_local_module;

static bool g_is_injected = false;

static LPFN_sock_ConnectEx GetConnectExPtr() {
    LPFN_sock_ConnectEx lpConnectEx = NULL;
    GUID guid = WSAID_CONNECTEX;
    DWORD dwNumBytes = 0;

    SOCKET s = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP);
    WSAIoctl(s , SIO_GET_EXTENSION_FUNCTION_POINTER , &guid , sizeof(guid) , &lpConnectEx , sizeof(lpConnectEx) , &dwNumBytes , NULL , NULL);
    closesocket(s);
    return lpConnectEx;
}

static LPFN_sock_DisconnectEx GetDisConnectExPtr() {
    LPFN_sock_DisconnectEx lpDisConnectEx = NULL;
    GUID guid = WSAID_DISCONNECTEX;
    DWORD dwNumBytes = 0;

    SOCKET s = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP);
    WSAIoctl(s , SIO_GET_EXTENSION_FUNCTION_POINTER , &guid , sizeof(guid) , &lpDisConnectEx , sizeof(lpDisConnectEx) , &dwNumBytes , NULL , NULL);
    closesocket(s);
    return lpDisConnectEx;
}


static int hook_install()
{
    /*clear hook api array*/
    memset(&g_hook_api[0], 0, sizeof(g_hook_api));

    /*replace connect function*/
    g_hook_api[HOOK_API_CONNECT].szFuncName = "connect";
    g_hook_api[HOOK_API_CONNECT].szDllName = "ws2_32.dll";
    g_hook_api[HOOK_API_CONNECT].pNewProc = (PROC)hook_sock_connect;
	g_hook_api[HOOK_API_CONNECT].type = HOOK_MHOOK;
    if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_CONNECT]))
    {
        MessageBox(NULL,"hook connect failed","Error",MB_OK);
        return -1;
    }
	_LOG_INFO("hook connect ok");

    g_hook_api[HOOK_API_WSA_CONNECT].szFuncName = "WSAConnect";
    g_hook_api[HOOK_API_WSA_CONNECT].szDllName = "ws2_32.dll";
    g_hook_api[HOOK_API_WSA_CONNECT].pNewProc = (PROC)hook_sock_WSAConnect;
	g_hook_api[HOOK_API_WSA_CONNECT].type = HOOK_MHOOK;
	if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_WSA_CONNECT]))
    {
        MessageBox(NULL,"hook WSAConnect failed","Error",MB_OK);
        return -1;
    }
	_LOG_INFO("hook WSAConnect ok");

	PROC pFunc = (PROC)GetConnectExPtr();
	if (NULL != pFunc)
	{
		g_hook_api[HOOK_API_CONNECT_EX].szFuncName = "ConnectEx";
		g_hook_api[HOOK_API_CONNECT_EX].szDllName = "ws2_32.dll";
		g_hook_api[HOOK_API_CONNECT_EX].pOldProc = pFunc;
    	g_hook_api[HOOK_API_CONNECT_EX].pNewProc = (PROC)hook_sock_ConnectEx;
		g_hook_api[HOOK_API_CONNECT_EX].type = HOOK_MHOOK;
		if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_CONNECT_EX]))
		{
			MessageBox(NULL, "hook ConnectEx failed", "Error", MB_OK);
			return -1;
		}
		_LOG_INFO("hook ConnectEx ok");
	}

    g_hook_api[HOOK_API_SEND].szFuncName = "send";
    g_hook_api[HOOK_API_SEND].szDllName = "ws2_32.dll";
    g_hook_api[HOOK_API_SEND].pNewProc = (PROC)hook_sock_send;
	g_hook_api[HOOK_API_SEND].type = HOOK_MHOOK;
	if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_SEND]))
    {
        MessageBox(NULL,"hook send failed","Error",MB_OK);
        return -1;
    }
	_LOG_INFO("hook send ok");

    g_hook_api[HOOK_API_WSA_SEND].szFuncName = "WSASend";
    g_hook_api[HOOK_API_WSA_SEND].szDllName = "ws2_32.dll";
    g_hook_api[HOOK_API_WSA_SEND].pNewProc = (PROC)hook_sock_WSASend;
	g_hook_api[HOOK_API_WSA_SEND].type = HOOK_MHOOK;
	if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_WSA_SEND]))
    {
        MessageBox(NULL,"hook WSASend failed","Error",MB_OK);
        return -1;
    }
	_LOG_INFO("hook WSASend ok");

	pFunc = GetProcAddress(GetModuleHandle(_T("ws2_32.dll")), "WSASendMsg");
	if (pFunc != NULL)
	{
		g_hook_api[HOOK_API_WSA_SENDMSG].szFuncName = "WSASendMsg";
		g_hook_api[HOOK_API_WSA_SENDMSG].szDllName = "ws2_32.dll";
		g_hook_api[HOOK_API_WSA_SENDMSG].pNewProc = (PROC)hook_sock_WSASendMsg;
		g_hook_api[HOOK_API_WSA_SENDMSG].type = HOOK_MHOOK;
		if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_WSA_SENDMSG]))
		{
			MessageBox(NULL, "hook WSASendMsg failed", "Error", MB_OK);
			return -1;
		}
	}
	_LOG_INFO("hook WSASendMsg ok");

	pFunc = (PROC)GetDisConnectExPtr();
	if (NULL != pFunc)
	{
		g_hook_api[HOOK_API_DISCONNECT_EX].szFuncName = "DisconnectEx";
		g_hook_api[HOOK_API_DISCONNECT_EX].szDllName = "ws2_32.dll";
		g_hook_api[HOOK_API_DISCONNECT_EX].pOldProc = pFunc;
		g_hook_api[HOOK_API_DISCONNECT_EX].pNewProc = (PROC)hook_sock_DisconnectEx;
		g_hook_api[HOOK_API_DISCONNECT_EX].type = HOOK_MHOOK;
		if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_DISCONNECT_EX]))
		{
			MessageBox(NULL, "hook DisConnectEx failed", "Error", MB_OK);
			return -1;
		}
		_LOG_INFO("hook DisconnectEx ok");
	}

    g_hook_api[HOOK_API_CLOSE_SOCKET].szFuncName = "closesocket";
    g_hook_api[HOOK_API_CLOSE_SOCKET].szDllName = "ws2_32.dll";
    g_hook_api[HOOK_API_CLOSE_SOCKET].pNewProc = (PROC)hook_sock_closesocket;
	g_hook_api[HOOK_API_CLOSE_SOCKET].type = HOOK_MHOOK;
	if (false == HookAPI(g_local_module, &g_hook_api[HOOK_API_CLOSE_SOCKET]))
    {
        MessageBox(NULL,"hook closesocket failed","Error",MB_OK);
        return -1;
    }
	_LOG_INFO("hook closesocket ok");

    return 0;
}

static void hook_unintall()
{
    for (int ii = 0; ii < HOOK_API_MAX; ii++)
    {
        if (g_hook_api[ii].result)
        {
            UnHookAPI(g_local_module, &g_hook_api[ii]);
			_LOG_INFO("unhook %s ok", g_hook_api[ii].szFuncName);
        }
    }
}

unsigned int WINAPI threadFunc(void * arg)
{
    unsigned long is_start = (unsigned long)arg;

    if (is_start)
    {
		char err_buf[64] = { 0 };
        
        loggger_init((char*)("c://GoProxy//"), g_cur_proc_name, 1 * 512, 6, false);
        logger_set_level(L_INFO);

		WSADATA  Ws;
		if (WSAStartup(MAKEWORD(2, 2), &Ws) != 0)
		{
			char tmpStr[256] = {0};
			_snprintf_s(tmpStr, 256, _T("Init Windows Socket Failed, %d!"), WSAGetLastError());
			MessageBox(NULL, tmpStr, _T("Error"), MB_OK);
			return -1;
		}

        /*get config from pipe*/
        reg_resp_body_t reg_resp_body;
        if(0 != proc_send_reg_req(&reg_resp_body))
        {
			//char tmpStr[256] = { 0 };
			//_snprintf_s(tmpStr, 256, _T("process %u register failed, error %d!"), _getpid(), WSAGetLastError());
			//MessageBox(NULL, tmpStr, _T("Error"), MB_OK);
            return -1;
        }
		
        /*setting*/
        g_local_port = reg_resp_body.local_port;

        if (0 != proxysocks_init())
        {
			_LOG_INFO("process %u init socks failed", _getpid());
			proc_send_unreg_req();
            return -1;
        }

        if (hook_install() != 0)
        {
			_LOG_INFO("process %u install hook failed", _getpid());
            /*hook 失败*/
            hook_unintall();
            proc_send_unreg_req();
            return -1;
        }

		_LOG_INFO("process %u loaded success", _getpid());
        g_is_injected = true;
        return 0;
    }
    else
    {
        hook_unintall();
		proxysocks_free();
		/*等待目标进程退出所有dll函数的使用*/
		Sleep(1000);

		g_is_injected = false;
		_LOG_INFO("process %u unloaded success", _getpid());
		WSACleanup();

        loggger_exit();
        return 0;
    }

    return 0;
}

int injectInit()
{
    /*get local process id and name*/
    char localModDir[MAX_PATH + 1] = { 0 };
	char *ch = NULL;	

    g_cur_proc_pid = _getpid();
    
    g_process_hdl = GetCurrentProcess();

    g_local_module = GetModuleHandle(0);    
    GetModuleFileName(g_local_module, localModDir, MAX_PATH);
    ch = _tcsrchr(localModDir, _T('\\'));
    strncpy(g_cur_proc_name, &ch[1], 255);
    ch = _tcsrchr(g_cur_proc_name, _T('.')); //查找最后一个.出现的位置，并返回.后面的字符（包括.)
    if (ch != NULL)
    {
        ch[0] = 0;
    }

    /*start thread to get config*/
    HANDLE thrd_handle;
    thrd_handle = (HANDLE)_beginthreadex(NULL, 0, threadFunc, (void*)1, 0, NULL);
    /*CloseHandle 只是减少了一个索引，线程真正退出时会释放资源*/
    ::CloseHandle(thrd_handle);

	return 0;
}

void injectFree()
{
    if (g_is_injected == false)
    {
        return;
    }

#if 0
	HANDLE thrd_handle;
    thrd_handle = (HANDLE)_beginthreadex(NULL, 0, threadFunc, (void*)0, 0, NULL);
    /*CloseHandle 只是减少了一个索引，线程真正退出时会释放资源*/
	::CloseHandle(thrd_handle);
#else
	threadFunc((void*)0);
#endif
}
