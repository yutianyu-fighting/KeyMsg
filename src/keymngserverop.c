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
		printf("---------����������/�� �����ڴ�ʧ��-----\n");
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "������ KeyMng_ShmInit() err:%d", ret);
		return ret;
	}
	
	return 0;	
}

int MngServer_Agree(MngServer_Info *svrInfo, MsgKey_Req *msgkeyReq, unsigned char **outData, int *datalen)
{
	int ret = 0;
	int i = 0;
	char buf[512]={0};//��Ž�����ֶ�
	
	MsgKey_Res msgKey_Res;
	
	NodeSHMInfo nodeSHMInfo;
	
	// --��� r1 r2 ������Կ  ---> �ɹ���ʧ�� rv
	
	if (strcmp(svrInfo->serverId, msgkeyReq->serverId) != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "�ͻ��˷����˴���ķ�����");
		return -1;	
	}
	
	// ��֯ Ӧ��ṹ�� res �� rv r2 clientId serverId  seckeyid
	msgKey_Res.rv = 0; 	//0 �ɹ� 1 ʧ�ܡ�
	strcpy(msgKey_Res.clientId, msgkeyReq->clientId); 
	strcpy(msgKey_Res.serverId, msgkeyReq->serverId); 
	
	// ��������� r2
	for (i = 0; i < 64; i++) {
		msgKey_Res.r2[i] = 'a' + i;			
	}	
	
	//�����ݿ� keysn ����ȡ�� seckeyid
	/*----------------------------------------------------------------------------*/
	//��ʼ�����ݿ�,��ȡ����
	//MYSQL* connect = msql_conn(svrInfo->dbuser, svrInfo->dbpasswd, svrInfo->dbname);
	//��������
	//ret = mysql_BeginTran(connect);
	MYSQL *connect = mysql_init(NULL); 
	connect = mysql_real_connect(connect,"localhost","root","123456","datafile",0,NULL,0);
	ret = KeyMngsvr_DBOp_GenKeyID(connect, buf);
	if(ret!=0)
	{
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "������ KeyMngsvr_DBOp_GenKeyID() err:%d", ret);
		return ret;	
	}
	msgKey_Res.seckeyid = atoi(buf);
	
	/*----------------------------------------------------------------------------*/
	
	// ��֯��Կ�ڵ���Ϣ�ṹ��
	for (i = 0; i < 64; i++) {
		nodeSHMInfo.seckey[2*i] = msgkeyReq->r1[i];
		nodeSHMInfo.seckey[2*i+1] = msgKey_Res.r2[i];
	}
	nodeSHMInfo.status = 0;  //0-��Ч 1��Ч
	strcpy(nodeSHMInfo.clientId, msgkeyReq->clientId);
	strcpy(nodeSHMInfo.serverId, msgkeyReq->serverId);
	nodeSHMInfo.seckeyid = msgKey_Res.seckeyid;

	// --д�빲���ڴ档
	ret = KeyMng_ShmWrite(svrInfo->shmhdl, svrInfo->maxnode, &nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "������ KeyMng_ShmWrite() err:%d", ret);
		return ret;	
	}

	// --д���ݿ�
	
	connect = mysql_init(NULL); 
	ret = KeyMngsvr_DBOp_writeInfo(connect,&nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "������ KeyMngsvr_DBOp_writeInfo() err:%d", ret);
		return ret;	
	}
	
	// ����Ӧ����  ����
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
    //������Կ����ṹ����r1��10�ֽڣ�
    //���� clientid��serverid �������ڴ� --> seckey ��Կ --> ��ȡǰ10�ֽ� --> �Ƚ�
    ret = KeyMng_ShmRead(svrInfo->shmhdl,msgkeyReq->clientId,svrInfo->serverId,svrInfo->maxnode,&nodeSHMInfo);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "serverop MngServer_Check KeyMng_ShmRead err:%d", ret);	
		return ret;
	}
    for(int i=0;i<10;i++)
    {
        msgkeyres.r2[i]=nodeSHMInfo.seckey[i];
    }
    //�ж���Կ�Ƿ�ƥ��
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


    //��֯��ԿӦ��ṹ��
    strcpy(msgkeyres.clientId,msgkeyReq->clientId);
    strcpy(msgkeyres.serverId,msgkeyReq->serverId);
    //memset(msgkeyres.r2,0,sizeof(msgkeyres.r2));
    msgkeyres.seckeyid=nodeSHMInfo.seckeyid;
    
    //��Ӧ��ṹ���б���
    ret = MsgEncode((void*)&msgkeyres,ID_MsgKey_Res,&res_Data,&res_len);
	if (ret != 0) {
		KeyMng_Log(__FILE__, __LINE__, KeyMngLevel[4], ret, "serverop MngServer_Check MsgEncode err:%d", ret);	
		return ret;
	}
    
    *outData = res_Data;
    *datalen = res_len;
	
	return 0;	
}
