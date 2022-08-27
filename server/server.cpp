#include <winsock2.h>	// 为了使用Winsock API函数
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <vector>
#include <iostream>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#define MAX_CLNT 256  //最大套接字
#define BUF_SIZE 100  //
// 告诉连接器与WS2_32库连接
#pragma comment(lib,"WS2_32.lib")
#pragma warning(disable:4996)
using namespace std;

#define MAXLEN 676 //数组长度 
double recvpacket[676];
void error_handling(const char* msg);		//错误处理函数
DWORD WINAPI ThreadProc(LPVOID lpParam);	//线程执行函数
void send_msg(char* msg, int len);			//消息发送函数
void grad_handler();                      //FedAvg处理函数

HANDLE g_hEvent;			//事件内核对象
int clnt_cnt = 0;			//统计套接字
int cnt = 0;
double grad_cnt = 0;           //统计接收到的梯度个数
//vector<float> grad_sum1;           //梯度之和
//vector<float> grad_sum2;           //梯度之和
double grad_sum[676];
int clnt_socks[MAX_CLNT];	//管理套接字
HANDLE hThread[MAX_CLNT];	//管理线程


int main()
{
	// 初始化WS2_32.dll，请求一个2.2版本的socket
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	WSAStartup(sockVersion, &wsaData);

	// 创建一个自动重置的，受信的，没有名称的事件内核对象
	g_hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	// 创建socket
	SOCKET serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//流式套接字
	if (serv_sock == INVALID_SOCKET)
		error_handling("socket 创建出错！");

	// 填充sockaddr_in结构
	sockaddr_in my_addr;
	my_addr.sin_family = AF_INET;               //AF_INET代表 Internet（TCP/IP）地址族
	my_addr.sin_port = htons(8888);				//端口（本地字节顺序->网络字节顺序（16bits))
	my_addr.sin_addr.S_un.S_addr = INADDR_ANY;	//地址通配符

	// 绑定这个套节字到一个本地地址
	if (bind(serv_sock, (LPSOCKADDR)&my_addr, sizeof(my_addr)) == SOCKET_ERROR)
		error_handling("bind 出错！");

	// 进入监听模式
	if (listen(serv_sock, 256) == SOCKET_ERROR)		//最大连接数为256
		error_handling("listen 出错！");
	printf("Start listen:\n");

	// 循环接受客户的连接请求
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);
	DWORD dwThreadId;	/*线程ID*/
	SOCKET clnt_sock;
	//char szText[] = "hello！\n";
	while (TRUE)
	{
		printf("等待新连接\n");
		// 接受一个新连接

		clnt_sock = accept(serv_sock, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (clnt_sock == INVALID_SOCKET)
		{
			printf("accept 出错！");
			continue;
		}

		/*等待内核事件对象状态受信*/
		WaitForSingleObject(g_hEvent, INFINITE);

		hThread[clnt_cnt] =
			CreateThread(NULL, NULL, ThreadProc, (void*)&clnt_sock, 0, &dwThreadId);
		/* 默认安全属性  默认堆栈大小  线程入口地址（执行线程的函数）
		   传给函数的参数  指定线程立即运行  返回线程的ID号*/

		clnt_socks[clnt_cnt++] = clnt_sock;
		/*当有新连接来临的时候创建线程处理新连接，并将新连接添加到套接字数组里面管理*/

		SetEvent(g_hEvent);				/*设置受信*/

		printf(" 接受到一个连接：%s 执行线程ID：%d\r\n", inet_ntoa(remoteAddr.sin_addr), dwThreadId);
	}
	WaitForMultipleObjects(clnt_cnt, hThread, true, INFINITE);
	for (int i = 0; i < clnt_cnt; i++)
	{
		CloseHandle(hThread[i]);
	}
	// 关闭监听套节字
	closesocket(serv_sock);
	// 释放WS2_32库
	WSACleanup();
	return 0;
}


void error_handling(const char* msg)
{
	printf("%s\n", msg);
	WSACleanup();
	exit(1);
}

DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	int clnt_sock = *((int*)lpParam);
	int str_len = 0, i;
	char msg[BUF_SIZE];

	while ((str_len = recv(clnt_sock, (char*)&recvpacket, sizeof(recvpacket), 0)) != -1)
	{
		//recv(clnt_sock, (char*)&recvpacket, sizeof(recvpacket), 0);
		//接收数组 
		grad_cnt += 1;
		for (int j = 0; j < 676; j++) {
			double tmp = recvpacket[j];
			grad_sum[j] += tmp;
		}
		int i = 0;
		if (grad_cnt >= 2) {
			grad_handler(); //计算数组
		    //发送结果 
		    /*等待内核事件对象状态受信*/
			WaitForSingleObject(g_hEvent, INFINITE);
			for (i = 0; i < clnt_cnt; i++)
				send(clnt_socks[i], (char*)&grad_sum, sizeof(grad_sum), 0);
			SetEvent(g_hEvent);		/*设置受信*/
			cnt++;
			printf("群发送成功 %d\n", cnt);
			grad_cnt = 0;
			grad_sum[676] = { 0 };
		}
	}
	printf("客户端退出:%d\n", GetCurrentThreadId());
	/* 等待内核事件对象状态受信 */
	WaitForSingleObject(g_hEvent, INFINITE);
	for (i = 0; i < clnt_cnt; i++)
	{
		if (clnt_sock == clnt_socks[i])
		{
			while (i++ < clnt_cnt - 1)
				clnt_socks[i] = clnt_socks[i + 1];
			break;
		}
	}
	clnt_cnt--;
	SetEvent(g_hEvent);		/*设置受信*/
	// 关闭同客户端的连接
	closesocket(clnt_sock);
	return NULL;
}

void send_msg(char* msg, int len)
{
	int i;
	/*等待内核事件对象状态受信*/
	WaitForSingleObject(g_hEvent, INFINITE);
	for (i = 0; i < clnt_cnt; i++)
		send(clnt_socks[i],(char*)&msg, sizeof(msg), 0);
	SetEvent(g_hEvent);		/*设置受信*/
}

void grad_handler() {
	for (int i = 0; i < MAXLEN; i++) {
		grad_sum[i] = grad_sum[i] / grad_cnt;
	}
}

/*
* 
#include <Winsock2.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <iostream.h> 
#define MAXLEN 3 //数组长度 
int recvpacket[MAXLEN];
int countdata();
void main(){    
//socket版本     
WORD wVersion;    
WSADATA wsaData;    
int err;    
wVersion=MAKEWORD(1,1);    
err=WSAStartup(wVersion,&wsaData);    
if(err!=0)    
{        
return;    
}    
if((LOBYTE(wsaData.wVersion)!=1)||(HIBYTE(wsaData.wVersion)!=1))    
{        WSACleanup();        return;    }    
//建立socket     
SOCKET sockSrv=socket(AF_INET,SOCK_STREAM,0);    
SOCKADDR_IN addSrv;    
addSrv.sin_addr.S_un.S_addr=htonl(INADDR_ANY);    
addSrv.sin_family=AF_INET;    
addSrv.sin_port=htons(6161);    
bind(sockSrv,(SOCKADDR*)&addSrv,sizeof(SOCKADDR));//绑定     
listen(sockSrv,5);//监听     
int len=sizeof(SOCKADDR);    
SOCKADDR_IN addClient;    
while(1)    
{        
SOCKET sockConn=accept(sockSrv,(SOCKADDR*)&addClient,&len);        
char sendBuf[100];        
sprintf(sendBuf,"你已经成功连接到服务器，请等待...");        
send(sockConn,sendBuf,strlen(sendBuf)+1,0);//发送欢迎消息                 
recv(sockConn,(char *)&recvpacket,sizeof(recvpacket),0);//接收数组                 
int myresult=countdata();//计算数组              
send(sockConn,(char *)&myresult,sizeof(myresult),0);//发送结果         
closesocket(sockConn);          
}   
}
//计算 
int countdata(){    
int s=0;    
for (int i=0;i<MAXLEN;i++)    
{        s=s+recvpacket[i];    }       
return s;
}

*/