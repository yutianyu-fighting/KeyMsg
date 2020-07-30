# KeyMsg

# 方案子系统

> 1-4：统一组件
>
> 5-6：服务器和客户端
>
> 7：  密钥协商服务配置管理终端
>
> 8-10：接口及脚本

| 编号 |                  子系统                   |  软件平台  |                          主要技术点                          |                备注                |
| :--: | :---------------------------------------: | :--------: | :----------------------------------------------------------: | :--------------------------------: |
|  01  |       统一数据库访问组件libicdbapi        |   Linux    | 1封装proc访问oracle数据库  2基于数据库连接池   3 linux下线程互斥和同步 |          接口的封装和设计          |
|  02  |           统一通讯SocketApi组件           | Linux、Win | 1封装Linux和Win下基本SocketApi  2基于socket 连接池  3 win下和linux下的线程互斥和同步  4 win下和linux下异构通讯 |          接口的封装和设计          |
|  03  |    统一报文编解码组件   Libmessagereal    | Linux、win | 1接口的封装和设计  2 win和linux源码的移植  3win和linux平台下动态库的设计与实现 xml json html  DER |          接口的封装和设计          |
|  04  |           统一共享内存读写组件            | Linux、win | 1接口的封装和设计  2 win和linux进程间通信机制IPC（共享内存、消息队列） |          接口的封装和设计          |
|  05  |       密钥协商服务程序SecMngServer        |   Linux    | 1 linux服务器框架编写  2各种动态库的集成  3 linux业务流的编写和调试 |       Linux服务程序系统开发        |
|  06  |      密钥协商客户端程序SecMngClient       | Linux、win | 1 linux客户端编写  2 Win客户端编写  3各种动态库的集成  4 linux和win业务流的编写和调试 | Linux信息系统开发  Win信息系统开发 |
|  07  | 密钥协商服务配置管理终端SecMngServerAdmin |    Win     | 1MFC框架、视图切分和切换  2各种win动态库的集成  3 win和linux的对接和调试  4 win下odbc驱动连接数据库 |          Win信息系统开发           |
|  08  |           外联接口AppInterface            | Linux、win |                      1接口的封装和设计                       |           接口设计与实现           |
|  09  |            对称密钥加解密接口             | Linux、win |                        1安全基础知识                         |           使用第三方接口           |
|  10  |          安全传输平台数据库脚本           |   Oracle   | 1 sql脚本   2 oracle数据库的安装、启动、关闭基本管理  3安全传输平台sql脚本的实施 |       信息系统数据库解决方案       |

# BER编码规则

编码的作用就是为了跨平台使用，将通信信息转化成字符串，可以跨平台

## 教师结构体的编码与解码

### 编码

**先封装结构体成员，再封装结构体**

```c
typedef struct _Teacher
{
	char name[64]; 
	int age ;
	char *p;
	int plen;
}Teacher;
```

![image-20200721095400655](C:\Users\于天宇\AppData\Roaming\Typora\typora-user-images\image-20200721095400655.png)

### 解码

**先解码结构体，再解码结构体成员**

### 编码解码API

`itcast_asn1_der.h`

```c
教师结构体编码：

 整型：
	DER_ItAsn1_WriteInteger(ITCAST_UINT32 integer, ITASN1_INTEGER **ppDerInteger);

 字符串类型:

	1. DER_ITCAST_String_To_AnyBuf(ITCAST_ANYBUF **pOriginBuf,	unsigned char * strOrigin,int strOriginLen);

	2. DER_ItAsn1_WritePrintableString(ITASN1_PRINTABLESTRING *pPrintString, ITASN1_PRINTABLESTRING **ppDerPrintString);

	int EncodeChar(char *pData, int dataLen, ITCAST_ANYBUF **outBuf);   合并 1. 2.

 结构体类型：
	DER_ItAsn1_WriteSequence(ITASN1_SEQUENCE *pSequence, ITCAST_ANYBUF **ppDerSequence);


int MsgEncode(void *teacher, unsigned char **out, int *outlen, int type);

int MsgDecode(unsigned char *out, int outlen, void ** teacher, int *type);	

int MsgFree(void **teacher, int type);
```

# 动态库制作

**动态库后缀: .so -- libmyname.so**

​	**制作步骤:**

​		1）生成与位置无关的代码(.o)
​			`gcc -fPIC -c *.c -Iinclude`

​				-fPIC（或-fpic）：表示编译为位置独立的代码。位置独立的代码即位置无关代码，在可执行程序加载的时候可	以存放在内存内的任何位置。若不使用该选项则编译后的代码是位置相关的代码，在可执行程序加载时是通过代码拷贝	的方式来满足不同的进程的需要，没有实现真正意义上的位置共享。

​		2）将.o打包生成.so文件
​			`gcc -shared  *.o -o libmytest.so`

​		或者直接一步：`gcc hello.c -fPIC -shared -o libmyhello.so`

​		3）使用 main.c   lib  include
​			`gcc main.c -Llib -lmytest -Iinlude -o app`

   **应用程序不能执行, 动态链接器连接不到自己制作的库**

1. 临时设置的方法:
   `export LD_LIBRARY_PATH=./lib` 

2. 永久设置的方法1

   帮助动态连接器 指定加载 动态库目录位置。`export LD_LIBRARY_PATH=./lib` ; 写入 ~/.bash_profile 中

3. 永久设置的方法2:

   1. 找到动态链接库的配置文件: `sudo vi /etc/ld.so.conf`

   2. 在该文件中添加动态库的目录(绝对路径)

   3. 更新: `sudo ldconfig -v`

       

# Socket连接池

## 出现的问题

开辟不同的线程时，当给线程的回调函数传递主线程的结构体指针时，会遇到子线程的栈空间读取主线程栈空间的问题，可能使得子线程们读取到的值出现问题，为避免此问题，最好为每个子线程单独开辟一块堆空间，使得每个子线程拥有每个独立的数据--内存冗余法


# 共享内存

## 常用基础API

`int shmget(key_t key, size_t size, int shmflg);`

```
key_t key：16进制 非0数据。 *0x0001*  ----描述当前共享内存 状态。 shared --- private
size_t size： 共享内存大小 
int shmflg：状态：读写执行权限 -- 8进制  0644 | IPC_CREAT | IPC_EXCL -->存在报错不存在创建
返回值：成功 shmid， 失败-1， 设置errorno 
```

`void *shmat(int shmid, const void *shmaddr, int shmflg);`

```
shmid： shmget 返回值	
shmaddr： 建议传入地址。默认NULL内核自动分配。
shmflg： SHM_RND 读写。 SHM_RDONLY 只读。
返回值：成功 映射内存首地址， 失败（void *）-1， 设置errorno
```

`int shmdt(const void *shmaddr);`

```
取消 进程与共享内存的 关联关系。
返回值：成功 0， 失败-1， 设置errorno
```

`int shmctl(int shmid, int cmd, struct shmid_ds *buf);`

```
shmid：shmget 返回值
cmd：IPC_RMID 删除 共享内存 （引用计数 变为 0）
对应 参数3 传 NULL
返回值：成功 0， 失败-1， 设置errorno
```

## Linux内核管理共享内存方法

​	**key值 + 引用计数 技术**

> ------ Shared Memory Segments --------
> key        shmid      owner      perms      bytes      nattch     status  
>
> 0x00000018 5734417    test04     644        256        4        
>
> 0x00000000 5734417    test04     644        256        3          dest  
>
> key 0x0018 ---> 0x0000  PRIVATE 状态。

# 密钥协商

## 密钥协商客户端

> 	```c
> 		显示菜单
> 		接收用户选择。  num。
> 		客户端信息初始化。
> 		
> 		switch(num) {
> 			
> 			case 1协商：
> 		
> 				密钥协商。 建立连接、封装req结构体、编码、发送、接收、解码、生成密钥、写共享内存。
> 		
> 				_Agree();
> 		
> 			case 2校验：
> 				
> 				密钥校验。 建立连接、封装req结构体、编码、发送、接收、解码、生成密钥、写共享内存。
> 		
> 				_Check();	
> 		
> 			case 查看：
> 		
> 				密钥查看。 建立连接、封装req结构体、编码、发送、接收、解码、生成密钥、写共享内存。
> 		
> 				_View();
> 			。。。。
> 		}
> 		
> 		将结果展示给用户。
> 	```

### 模块划分

**业务逻辑**

`keymngclient.c` 

**业务功能**

```c
	keymngclientop.c    		-->   		keymngclientop.h
	
	实现 客户端信息初始化。					客户端信息初始化声明。  			

	实现 客户端密钥协商。							。。。。

	实现 客户端密钥校验。

	struct clientInfo { serverIp, serverPort, clientID, serverID, Autocode, shmkey, shmid, maxnode }

	int keymng_Agree(struct clientInfo *pInfo)
	{
		//组织密钥请求结构体

		//编码密钥请求结构体

		//初始化连接

		//建立连接

		//发送请求报文

		//接收应答报文

		//解析应答报文

		//生成密钥

		//写共享内存。
	}

	int keymng_Check(struct clientInfo *pInfo);

	int keymng_Revoke(struct clientInfo *pInfo);
```

## 密钥协商服务器

```
void *callback(void *arg)
	{
		// 接收客户端 密钥请求报文 --- TLV 

		// 解码客户端 密钥请求报文 ---> cmdType

		switch(cmdType) {
			case 密钥协商:
				mngServer_Agree();

			case 密钥校验:
				mngServer_Agree();
				
			case 密钥注销:
				mngServer_Agree();
			default：
				break;
		}
		// 发送应答报文
	}
	
	keymngserver.c    业务逻辑
	{
		// 服务器信息初始化。
			
		// 监听客户端连接请求

		while(1) {

		// 启动线程 与客户端 进行通信 fd

			pthread_create();
		}
	}
==================================================================
	keymngserverop.c 		keymngserverop.h		功能实现


	实现 服务器信息初始化。		服务器信息初始化声明。  			

	实现 服务器密钥协商。		。。。。

	实现 服务器密钥校验。	

	实现 服务器密钥注销。

	struct serverInfo {serverIp, serverPort, clientID, serverID, Autocode, shmkey, shmid, maxnode, username, passwd, dbname} 不变

	int MngServer_Agree(MngServer_Info *svrInfo, MsgKey_Req *msgkeyReq, unsigned char **outData, int *datalen);
	{
		// 生成随机数 r2

		// 结合 r1 r2 生成密钥  ---> 成功、失败 rv

		// 组织 应答结构体 res ： rv r2 clientId serverId  seckeyid

		// 写共享内存

		// 写数据库

		// 编码应答报文  传出
	}

```

# 密钥校验

```
服务器：
	int MngServer_check()
	{

		//读出 密钥请求结构体中 r1[] (10字节的密钥数据)

		//依据 clientid、serverid 读共享内存 --> seckey 密钥 --> 提取前10字节 --> 比较
		
		//根据比较结果填充 res.rv 

		//组织密钥应答结构体 res 其他成员变量

		//编码密钥应答结构体 --> 传出。
	}
	
客户端：

	校验方法：
		1. a1b2c3 --> 再加密（非对称加密）--> jqk678

		2. 片段校验法： 0-10 密钥信息一致  --> 一致。

	int MngServer_check()
	{

		//读共享内存 --> seckey 密钥 --> 提取前10字节 --> req.r1[];

		//组织密钥请求结构体（校验的事）

		//编码密钥请求结构体 req 

		//初始化连接 --> listenfd

		//建立连接 --> connfd

		//发送请求报文 -->  TLV send

		//接收应答报文 --> fwq TLV res

		//解析应答报文 --> TLV --> Struct --> rv --> 0 一致 1 不一致。

		//给用户返回校验结果。	
	}

```

## 数据库操作

```
密钥协商：

	——Agree：

开启事务
		1. 从数据库 keysn 表中取出 seckeyid 。 ---读数据库。select --keysn

		2. 将 自增后的 seckeyid 存入 keysn表中。---写数据库。update --keysn--seckeyid

		3. 将写入共享内存中的 NodeSHMInfo 写入数据库 表 seckeyinfo 中。---写数据库。insert --seckeyinfo 	
关闭事务


密钥校验：

	——Check：

		--- 读数据库。 select --seckeyinfo 

		封装/实现函数：int KeyMngsvr_DBOp_ReadSecKey(void *dbhdl, NodeSHMInfo **pNodeInfo);

密钥注销：

	——Revoke：

		--- 写数据库。 update --seckeyinfo --》  status  0-->1 

=============================================================================

密钥注销：

	keymngserver.c

	// 接收客户端连接 

	// 创建线程 ， 接收数据包 TLV

	// 解码密钥请求结构体

	switch （cmdType）{
		
		case KeyMng_Revoke：

			MngServer_Revoke();

			break;
	}

	// 发送应答报文 TLV


服务器：		keymngserverop.c


int MngServer_Revoke(MngServer_Info *svrInfo, MsgKey_Req *msgkeyReq, unsigned char **outData, int *datalen)
{
	// 取出 msgkeyReq 中的 clientID、serverID

	// 注销当前密钥： 根据 clietid 、 serverID --> seckeyid 
	
	// 根据 seckid 修改数据库中的 对应 status 0 --> 1  (update)

		封装/实现函数：----写数据库。 update --seckeyinfo --》  status  0-->1   注意事务的管理。

	// 根据结果 修改 rv

	// 修改共享内存中的 密钥 status  0-->1  

	// 组织应答报文结构体。  传出	
}	

	
客户端：		keymngclientop.c

int MngClient_Revoke(MngClient_Info *pCltInfo)
{
	// 组织 请求报文 req		cmdType--> KeyMng_Revoke

	// 编码 请求报表  TLV

	// 发送 请求报文
			
	// 接收 应答报文

	// 解码 应答报文
	
	// 修改共享内存中的 密钥 status  0-->1 

	// 返回结果给用户。
	
	return 0;	
}

```

