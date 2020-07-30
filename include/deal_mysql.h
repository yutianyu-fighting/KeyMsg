#ifndef _DEAL_MYSQL_H_
#define _DEAL_MYSQL_H_

#include <mysql/mysql.h> //数据库
#include "keymng_shmop.h"

#define BEGIN_TRAN 		"START TRANSACTION"
#define SET_TRAN			"SET AUTOCOMMIT=0"  
#define UNSET_TRAN		"SET AUTOCOMMIT=1"
#define COMMIT_TRAN		"COMMIT"
#define ROLLBACK_TRAN	"ROLLBACK"

/* -------------------------------------------*/
/**
 * @brief  数据库事务控制
 *
 * @mysql_BeginTran 开启事务
 * @mysql_Rollback  事务回滚
 * @mysql_Commit    提交事务
 */
/* -------------------------------------------*/
int mysql_BeginTran(MYSQL *mysql);

int mysql_Rollback(MYSQL *mysql);

int mysql_Commit(MYSQL *mysql);



/* -------------------------------------------*/
/**
 * @brief  打印操作数据库出错时的错误信息
 *
 * @param conn       (in)    连接数据库的句柄
 * @param title      (int)   用户错误提示信息
 *
 */
/* -------------------------------------------*/
void print_error(MYSQL *conn, const char *title);

/* -------------------------------------------*/
/**
 * @brief  连接数据库
 *
 * @param user_name	 (in)   数据库用户
 * @param passwd     (in)   数据库密码
 * @param db_name    (in)   数据库名称
 *
 * @returns   
 *          成功：连接数据库的句柄
 *			失败：NULL
 */
/* -------------------------------------------*/
MYSQL* msql_conn(char *user_name, char* passwd, char *db_name);


/* -------------------------------------------*/
/**
 * @brief  处理数据库查询结果
 *
 * @param conn	     (in)   连接数据库的句柄
 * @param res_set    (in)   数据库查询后的结果集
 *
 */
/* -------------------------------------------*/
void process_result_test(MYSQL *conn, MYSQL_RES *res_set);

//处理数据库查询结果，结果保存在buf，只处理一条记录
int process_result_one(MYSQL *conn, char *sql_cmd, char *buf);

//从数据库 keysn 表中取出 seckeyid --将自增后的 seckeyid 存入 keysn表中
int KeyMngsvr_DBOp_GenKeyID(MYSQL* connect,char* buf);

//将写入共享内存中的 NodeSHMInfo 写入数据库 表 seckeyinfo 中
int KeyMngsvr_DBOp_writeInfo(MYSQL* connect,NodeSHMInfo* nodeSHMInfo);

#endif
