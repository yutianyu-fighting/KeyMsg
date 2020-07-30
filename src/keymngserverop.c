#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>

#include "keymngserverop.h"
#include "keymng_msg.h"
#include "keymnglog.h" 
#include "keymng_shmop.h"


int MngServer_InitInfo(MngServer_Info *svrInfo)
{ 
	int ret = 0;
	strcpy(svrInfo->serverId, "0001");
	strcpy(svrInfo->dbuser, "root");
	strcpy(svrInfo->dbpasswd, "123456");
	strcpy(svrInfo->dbname, "datafile");
	svrInfo->dbpoolnum = 8;	
	strcpy(svrInfo->serverip, "127.0.0.1");
	svrInfo->serverport = 8001;
	svrInfo->maxnode = 10;
	svrInfo->shmkey = 0x0001;
	svrInfo->shmhdl = 0;
	
	ret = KeyMng_ShmInit(svrInfo->shmkey, svrInfo->maxnode, &svrInfo->shmhdl);
	if (ret != 0) {
		printf("---------服务器创建/打开 共享内存失败-----\n");
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "服务器 KeyMng_ShmInit() err:%d", ret);
		return ret;
	}
	
	return 0;	
}

int MngServer_Agree(MngServer_Info *svrInfo, MsgKey_Req *msgkeyReq, unsigned char **outData, int *datalen)
{
	int ret = 0;
	int i = 0;
	char buf[512]={0};//存放结果的字段
	
	MsgKey_Res msgKey_Res;
	
	NodeSHMInfo nodeSHMInfo;
	
	// --结合 r1 r2 生成密钥  ---> 成功、失败 rv
	
	if (strcmp(svrInfo->serverId, msgkeyReq->serverId) != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "客户端访问了错误的服务器");
		return -1;	
	}
	
	// 组织 应答结构体 res ： rv r2 clientId serverId  seckeyid
	msgKey_Res.rv = 0; 	//0 成功 1 失败。
	strcpy(msgKey_Res.clientId, msgkeyReq->clientId); 
	strcpy(msgKey_Res.serverId, msgkeyReq->serverId); 
	
	// 生成随机数 r2
	for (i = 0; i < 64; i++) {
		msgKey_Res.r2[i] = 'a' + i;			
	}	
	
	//从数据库 keysn 表中取出 seckeyid
	/*----------------------------------------------------------------------------*/
	//初始化数据库,获取连接
	//MYSQL* connect = msql_conn(svrInfo->dbuser, svrInfo->dbpasswd, svrInfo->dbname);
	//开启事务
	//ret = mysql_BeginTran(connect);
	MYSQL *connect = mysql_init(NULL); 
	connect = mysql_real_connect(connect,"localhost","root","123456","datafile",0,NULL,0);
	ret = KeyMngsvr_DBOp_GenKeyID(connect, buf);
	if(ret!=0)
	{
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "服务器 KeyMngsvr_DBOp_GenKeyID() err:%d", ret);
		return ret;	
	}
	msgKey_Res.seckeyid = atoi(buf);
	
	/*----------------------------------------------------------------------------*/
	
	// 组织密钥节点信息结构体
	for (i = 0; i < 64; i++) {
		nodeSHMInfo.seckey[2*i] = msgkeyReq->r1[i];
		nodeSHMInfo.seckey[2*i+1] = msgKey_Res.r2[i];
	}
	nodeSHMInfo.status = 0;  //0-有效 1无效
	strcpy(nodeSHMInfo.clientId, msgkeyReq->clientId);
	strcpy(nodeSHMInfo.serverId, msgkeyReq->serverId);
	nodeSHMInfo.seckeyid = msgKey_Res.seckeyid;

	// --写入共享内存。
	ret = KeyMng_ShmWrite(svrInfo->shmhdl, svrInfo->maxnode, &nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "服务器 KeyMng_ShmWrite() err:%d", ret);
		return ret;	
	}

	// --写数据库
	
	connect = mysql_init(NULL); 
	ret = KeyMngsvr_DBOp_writeInfo(connect,&nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "服务器 KeyMngsvr_DBOp_writeInfo() err:%d", ret);
		return ret;	
	}
	
	// 编码应答报文  传出
	ret = MsgEncode(&msgKey_Res, ID_MsgKey_Res, outData, datalen);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "serverAgree MsgEncode() err:%d", ret);	
		return ret;
	}
	
	return 0;	
}


int MngServer_Check(MngServer_Info *svrInfo, MsgKey_Req *msgkeyReq, unsigned char **outData, int *datalen)
{
	int ret = 0;
    NodeSHMInfo nodeSHMInfo;
    MsgKey_Res msgkeyres;
    unsigned char* res_Data=NULL;
    int res_len = 0;
    //读出密钥请求结构体中r1（10字节）
    //依据 clientid、serverid 读共享内存 --> seckey 密钥 --> 提取前10字节 --> 比较
    ret = KeyMng_ShmRead(svrInfo->shmhdl,msgkeyReq->clientId,svrInfo->serverId,svrInfo->maxnode,&nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "serverop MngServer_Check KeyMng_ShmRead err:%d", ret);	
		return ret;
	}
    for(int i=0;i<10;i++)
    {
        msgkeyres.r2[i]=nodeSHMInfo.seckey[i];
    }
    //判断密钥是否匹配
    int i;
    for(i=0;i<10;i++)
    {
        if(msgkeyres.r2[i]!=msgkeyReq->r1[i])
        {
            msgkeyres.rv = 1;
            break;
        }
    }
    if(i==10)
    {
        msgkeyres.rv=0;
    }


    //组织密钥应答结构体
    strcpy(msgkeyres.clientId,msgkeyReq->clientId);
    strcpy(msgkeyres.serverId,msgkeyReq->serverId);
    //memset(msgkeyres.r2,0,sizeof(msgkeyres.r2));
    msgkeyres.seckeyid=nodeSHMInfo.seckeyid;
    
    //对应答结构进行编码
    ret = MsgEncode((void*)&msgkeyres,ID_MsgKey_Res,&res_Data,&res_len);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "serverop MngServer_Check MsgEncode err:%d", ret);	
		return ret;
	}
    
    *outData = res_Data;
    *datalen = res_len;
	
	return 0;	
}
