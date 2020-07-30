#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
 
#include "poolsocket.h"  
#include "keymngserverop.h"
#include "keymng_msg.h"
#include "keymnglog.h"  

MngServer_Info serverInfo;

int flg = 1;

#define CREATE_DAEMON if(fork()>0)exit(1);setsid();
 
void *start_routine(void * arg)
{
 	int ret;
 	int timeout = 3;
 	int connfd = (int)(long)arg;
 	
 	unsigned char *out = NULL;
 	int outlen = 0;
 	
 	MsgKey_Req *pStruct_req = NULL;
 	int iType = 0;
 	
 	unsigned char *res_outData = NULL;
 	int res_outDataLen = 0;
 	
 	while (1) {
 				
		if (flg == 0) 
			break;	

		//服务器端端接受报文
		ret = sckServer_rev(connfd, timeout, &out, &outlen); 
		if (ret == Sck_ErrPeerClosed) {
			// 检测到 对端关闭，关闭本端。
			printf("----------------ErrPeerClosed 关闭服务器\n");
			break;
		} else if (ret == Sck_ErrTimeOut) {
			if (out != NULL)  sck_FreeMem((void **)&out);
			continue;
		} else if (ret != 0) {
			printf("未知错误\n");
			break;
		}

		// 解码客户端 密钥请求报文 ---> cmdType
		ret = MsgDecode(out, outlen, (void **)&pStruct_req, &iType);
		if (ret != 0) {
			KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "MsgDecode() err:%d", ret);	
			break;	
		}

		switch(pStruct_req->cmdType) {
			case KeyMng_NEWorUPDATE:
				ret = MngServer_Agree(&serverInfo, pStruct_req, &res_outData, &res_outDataLen);
			    break;
			case KeyMng_Check:
				ret = MngServer_Check(&serverInfo, pStruct_req, &res_outData, &res_outDataLen);
			    break;
                /*	
			case 密钥注销:
				mngServer_Agree();
				*/
			default:
				break;
		}
		if (ret != 0) {
            if(pStruct_req->cmdType==KeyMng_NEWorUPDATE)
            {
			    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "MngServer_Agree() err:%d", ret);
            }
            else if(pStruct_req->cmdType==KeyMng_Check)
            {
			    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "MngServer_Check() err:%d", ret);
                printf("KeyMng_Check error\n");
            }
			break;				
		}

	 	//服务器端发送报文
		ret = sckServer_send(connfd, timeout, res_outData, res_outDataLen);
	 	if (ret == Sck_ErrPeerClosed) {
			// 检测到 对端关闭，关闭本端。
			printf("---ErrPeerClosed \n");
			break;
		} else if (ret == Sck_ErrTimeOut) {
			printf("---服务器检测到本端发送数据 超时 \n");
			if (out != NULL) sck_FreeMem((void **)&out);
			if (pStruct_req != NULL) MsgMemFree((void **)&pStruct_req, iType);
			if (res_outData != NULL) MsgMemFree((void **)&res_outData, 0);	
			continue;
		} else if (ret != 0) {
			printf("未知错误\n");
			break;
		}
	}
	
	if (out != NULL) sck_FreeMem((void **)&out);
	if (pStruct_req != NULL) MsgMemFree((void **)&pStruct_req, iType);
	if (res_outData != NULL) MsgMemFree((void **)&res_outData, 0);	
	
	sckServer_close(connfd);
	
 	return NULL;
}

void catchSignal(int signum)
{
	flg = 0;
	printf(" catch signal %d, process is going to die.\n", signum);
	
	return ;
}

int main(void)
{
	int listenfd;
	int ret = 0;
	
	int timeout = 3;
	int connfd = -1;
	
	pthread_t pid;
	
	CREATE_DAEMON
	
	signal(SIGUSR1, catchSignal);
	
	// 服务器信息初始化。
	ret = MngServer_InitInfo(&serverInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "MngServer_InitInfo() err:%d", ret);	
		return ret;
	}
	
	//服务器端初始化
	ret = sckServer_init(serverInfo.serverport, &listenfd);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "sckServer_init() err:%d", ret);	
		return ret;
	}
	
		while (1) {	
		if (flg == 0) 
			break;
		
		ret = sckServer_accept(listenfd, timeout, &connfd);
		if (ret == Sck_ErrTimeOut){
			//KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[2], ret, "---等待客户端连接超时---");
			continue;	
		} else if(ret != 0)  {
			KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "sckServer_accept() err:%d", ret);	
			return ret;
		}
		
		ret = pthread_create(&pid, NULL, start_routine, (void *)connfd);					
	}
 	
 	//服务器端环境释放 
	sckServer_destroy();
	
	//printf("服务器 优雅退出。\n\n");

	return 0;	
}
