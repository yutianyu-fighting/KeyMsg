#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
 #include <mysql/mysql.h>
 
#include "keymngserverop.h"
#include "keymnglog.h"
#include "deal_mysql.h"

//��keysn�� ����ikeysn�� +1 ---> seckeyid 

int KeyMngsvr_DBOp_GenKeyID(MYSQL *connect, int *keyid)
{
		
}