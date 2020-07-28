#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "keymnglog.h"
#include "poolsocket.h"
#include "keymng_msg.h"
#include "keymngclientop.h"
#include "keymng_shmop.h"

int MngClient_InitInfo(MngClient_Info *pCltInfo)
{
	int ret = 0;
	 
	strcpy(pCltInfo->clientId, "1111");
	strcpy(pCltInfo->AuthCode, "1111");
	strcpy(pCltInfo->serverId, "0001");
	strcpy(pCltInfo->serverip, "127.0.0.1");
	pCltInfo->serverport = 8001;
	
	pCltInfo->maxnode = 1;
	pCltInfo->shmkey = 0x0111;
	pCltInfo->shmhdl = 0;	
	
	ret = KeyMng_ShmInit(pCltInfo->shmkey, pCltInfo->maxnode, &pCltInfo->shmhdl);
	if (ret != 0) {
		printf("---------客户端创建/打开 共享内存失败-----\n");
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "KeyMng_ShmInit() err:%d", ret);
		return 0;
	}
	
	return 0;
}

int MngClient_Agree(MngClient_Info *pCltInfo)
{
	int i = 0; 
	int ret = 0; 
	int time = 3;
	
	int connfd = -1;
	
	// 存放编码 TLV 完成的 req
	unsigned char	*msgKey_Req_Data = NULL;   
	int 			msgKey_Req_DataLen = 0;
	
	// 存放编码 TLV 完成的 res
	unsigned char	*msgKey_Res_Data = NULL;
	int 			msgKey_Res_DataLen = 0;
	
	MsgKey_Res 		*pStruct_Res = NULL;
	int 			iType = 0;

	// 初始化密钥请求结构体
	MsgKey_Req msgKey_req;
	
	msgKey_req.cmdType = KeyMng_NEWorUPDATE;
	strcpy(msgKey_req.clientId, pCltInfo->clientId);
	strcpy(msgKey_req.AuthCode, pCltInfo->AuthCode);
	strcpy(msgKey_req.serverId, pCltInfo->serverId);	
	
	// 产生随机数         c: abcdefg	s: abcdefg		aabbccddeeffgg	
	for (i = 0; i < 64; i++) {
		msgKey_req.r1[i] = 'a' + i;	
	}
	
	// 编码密钥请求 结构体 req
	ret = MsgEncode(&msgKey_req, ID_MsgKey_Req, &msgKey_Req_Data, &msgKey_Req_DataLen);
	if (ret != 0) {		
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func MsgEncode() err:%d", ret);
		goto END;	
	}
	
	// 初始化建立连接函数
	ret = sckClient_init();
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_init() err:%d", ret);
		goto END;	
	}
		
	// 创建连接。
	ret = sckClient_connect(pCltInfo->serverip, pCltInfo->serverport, time, &connfd);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_connect() err:%d", ret);
		goto END;	
	}
		
	// 发送数据  TLV
	ret = sckClient_send(connfd, time, msgKey_Req_Data, msgKey_Req_DataLen);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_send() err:%d", ret);
		goto END;	
	}
		
	// ---- 等待服务器回发数据
	
	// 接收数据
	ret = sckClient_rev(connfd, time, &msgKey_Res_Data, &msgKey_Res_DataLen);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_rev() err:%d", ret);
		goto END;	
	}	
	
	// 解码密钥应答 结构体 res ---> rv r2
	ret = MsgDecode(msgKey_Res_Data, msgKey_Res_DataLen, (void **)&pStruct_Res, &iType);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func MsgDecode() err:%d", ret);
		goto END;	
	}	
	
	if (pStruct_Res->rv != 0) {
		ret	= -1;
		goto END;
	} else if (pStruct_Res->rv == 0) {
		printf("---当前生成的密钥编号为：%d\n", pStruct_Res->seckeyid);	
	}
	
	// --组织密钥信息结构体	
	NodeSHMInfo nodeSHMInfo;
	
	// --利用 r1 r2 生成密钥
	for (i = 0; i < 64; i++) {
		nodeSHMInfo.seckey[2*i] = msgKey_req.r1[i];
		nodeSHMInfo.seckey[2*i+1] = pStruct_Res->r2[i];
	}
	
	nodeSHMInfo.status = 0;  				//0-有效  1无效
	strcpy(nodeSHMInfo.clientId, msgKey_req.clientId);
	strcpy(nodeSHMInfo.serverId, msgKey_req.serverId);
	nodeSHMInfo.seckeyid = pStruct_Res->seckeyid;

	// --写入共享内存。
	ret = KeyMng_ShmWrite(pCltInfo->shmhdl, pCltInfo->maxnode, &nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "KeyMng_ShmWrite() err:%d", ret);
		goto END;	
	}
	printf("--------------写共享内存完成------\n");
	
END:
	if (msgKey_Req_Data != NULL) 
		MsgMemFree((void **)&msgKey_Req_Data, 0);
	if (msgKey_Res_Data != NULL) 
		MsgMemFree((void **)&msgKey_Res_Data, 0);
	if (pStruct_Res != NULL) 
		MsgMemFree((void **)&pStruct_Res, iType);
	
	return ret;	
}

int MngClient_Check(MngClient_Info *pCltInfo)
{
    int ret=0;
    int listenfd,connfd;
    int timeout = 3;

    NodeSHMInfo nodeSHMInfo;
    memset(&nodeSHMInfo,0,sizeof(nodeSHMInfo));
    
    MsgKey_Req req;//请求结构体
    unsigned char *outData=NULL;
    int outDataLen=0;

    unsigned char* resData = NULL;
    int resDataLen = 0 ;

    MsgKey_Res* pStruct_Res = NULL;
    int iType = 0;

    do
    {
	    //读共享内存 --> seckey 密钥 --> 提取前10字节 --> req.r1[];
        ret = KeyMng_ShmRead(pCltInfo->shmhdl,pCltInfo->clientId,pCltInfo->serverId,pCltInfo->maxnode,&nodeSHMInfo);
        if(ret!=0)
        {
            KeyMng_Log(__FILE__,__LINE__,KeyMngLevel[4],ret,"KeyMng_ShmRead error:%d",ret);
            break;
        }

        //组织密钥请求结构体
        req.cmdType = KeyMng_Check;
        strcpy(req.clientId,pCltInfo->clientId);
        strcpy(req.serverId,pCltInfo->serverId);
        strcpy(req.AuthCode,pCltInfo->AuthCode);
        //取密钥前10位
        for(int i=0;i<10;i++)
        {
            req.r1[i]=nodeSHMInfo.seckey[i];
        }
        //对请求报文进行编码
        ret = MsgEncode((void*)&req,ID_MsgKey_Req,&outData,&outDataLen);
        if(ret!=0)
        {
            KeyMng_Log(__FILE__,__LINE__,KeyMngLevel[4],ret,"serv MngClient_Check MsgEncode  error:%d",ret);
            break;
        }

	    // 初始化建立连接函数
	    ret = sckClient_init();
	    if (ret != 0) {
		    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_init() err:%d", ret);
		    break;	
	    }
		
	    // 创建连接。
	    ret = sckClient_connect(pCltInfo->serverip, pCltInfo->serverport, timeout, &connfd);
	    if (ret != 0) {
		    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_connect() err:%d", ret);
		    break;	
	    }
		
	    // 发送数据  TLV
	    ret = sckClient_send(connfd, timeout, outData, outDataLen);
	    if (ret != 0) {
		    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_send() err:%d", ret);
		    break;	
	    }
		
	    // ---- 等待服务器回发数据
	
    	// 接收数据
	    ret = sckClient_rev(connfd, timeout, &resData, &resDataLen);
	    if (ret != 0) {
		    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func sckClient_rev() err:%d", ret);
		    break;	
	    }	
	
	    // 解码密钥应答 结构体 res ---> rv r2
	    ret = MsgDecode(resData, resDataLen, (void **)&pStruct_Res, &iType);
	    if (ret != 0) {
		    KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "func MsgDecode() err:%d", ret);
		    break;	
    	}

        //密钥校验
        /*if(memcmp(pStruct_Res->r2,nodeSHMInfo.seckey,10)==0)
        {
            printf("密钥匹配，校验成功！\n");
        }
        else
        {
            printf("密钥不匹配，效验失败！\n");
        }*/

        if(pStruct_Res->rv==0)//0--校验成功 1--校验失败
        {
            printf("密钥匹配，校验成功！\n");
        }
        else
        {
            printf("密钥不匹配，效验失败！\n");
            return -1;
        }
        
    }while(0);

    
	if (outData != NULL) 
		MsgMemFree((void **)&outData, 0);
	if (resData != NULL) 
		MsgMemFree((void **)&resData, 0);
	if (pStruct_Res != NULL) 
		MsgMemFree((void **)&pStruct_Res, iType);
	
	return ret;	
}
