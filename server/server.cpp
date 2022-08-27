#include <winsock2.h>	// Ϊ��ʹ��Winsock API����
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <vector>
#include <iostream>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#define MAX_CLNT 256  //����׽���
#define BUF_SIZE 100  //
// ������������WS2_32������
#pragma comment(lib,"WS2_32.lib")
#pragma warning(disable:4996)
using namespace std;

#define MAXLEN 676 //���鳤�� 
double recvpacket[676];
void error_handling(const char* msg);		//��������
DWORD WINAPI ThreadProc(LPVOID lpParam);	//�߳�ִ�к���
void send_msg(char* msg, int len);			//��Ϣ���ͺ���
void grad_handler();                      //FedAvg������

HANDLE g_hEvent;			//�¼��ں˶���
int clnt_cnt = 0;			//ͳ���׽���
int cnt = 0;
double grad_cnt = 0;           //ͳ�ƽ��յ����ݶȸ���
//vector<float> grad_sum1;           //�ݶ�֮��
//vector<float> grad_sum2;           //�ݶ�֮��
double grad_sum[676];
int clnt_socks[MAX_CLNT];	//�����׽���
HANDLE hThread[MAX_CLNT];	//�����߳�


int main()
{
	// ��ʼ��WS2_32.dll������һ��2.2�汾��socket
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	WSAStartup(sockVersion, &wsaData);

	// ����һ���Զ����õģ����ŵģ�û�����Ƶ��¼��ں˶���
	g_hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	// ����socket
	SOCKET serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//��ʽ�׽���
	if (serv_sock == INVALID_SOCKET)
		error_handling("socket ��������");

	// ���sockaddr_in�ṹ
	sockaddr_in my_addr;
	my_addr.sin_family = AF_INET;               //AF_INET���� Internet��TCP/IP����ַ��
	my_addr.sin_port = htons(8888);				//�˿ڣ������ֽ�˳��->�����ֽ�˳��16bits))
	my_addr.sin_addr.S_un.S_addr = INADDR_ANY;	//��ַͨ���

	// ������׽��ֵ�һ�����ص�ַ
	if (bind(serv_sock, (LPSOCKADDR)&my_addr, sizeof(my_addr)) == SOCKET_ERROR)
		error_handling("bind ����");

	// �������ģʽ
	if (listen(serv_sock, 256) == SOCKET_ERROR)		//���������Ϊ256
		error_handling("listen ����");
	printf("Start listen:\n");

	// ѭ�����ܿͻ�����������
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);
	DWORD dwThreadId;	/*�߳�ID*/
	SOCKET clnt_sock;
	//char szText[] = "hello��\n";
	while (TRUE)
	{
		printf("�ȴ�������\n");
		// ����һ��������

		clnt_sock = accept(serv_sock, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (clnt_sock == INVALID_SOCKET)
		{
			printf("accept ����");
			continue;
		}

		/*�ȴ��ں��¼�����״̬����*/
		WaitForSingleObject(g_hEvent, INFINITE);

		hThread[clnt_cnt] =
			CreateThread(NULL, NULL, ThreadProc, (void*)&clnt_sock, 0, &dwThreadId);
		/* Ĭ�ϰ�ȫ����  Ĭ�϶�ջ��С  �߳���ڵ�ַ��ִ���̵߳ĺ�����
		   ���������Ĳ���  ָ���߳���������  �����̵߳�ID��*/

		clnt_socks[clnt_cnt++] = clnt_sock;
		/*�������������ٵ�ʱ�򴴽��̴߳��������ӣ�������������ӵ��׽��������������*/

		SetEvent(g_hEvent);				/*��������*/

		printf(" ���ܵ�һ�����ӣ�%s ִ���߳�ID��%d\r\n", inet_ntoa(remoteAddr.sin_addr), dwThreadId);
	}
	WaitForMultipleObjects(clnt_cnt, hThread, true, INFINITE);
	for (int i = 0; i < clnt_cnt; i++)
	{
		CloseHandle(hThread[i]);
	}
	// �رռ����׽���
	closesocket(serv_sock);
	// �ͷ�WS2_32��
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
		//�������� 
		grad_cnt += 1;
		for (int j = 0; j < 676; j++) {
			double tmp = recvpacket[j];
			grad_sum[j] += tmp;
		}
		int i = 0;
		if (grad_cnt >= 2) {
			grad_handler(); //��������
		    //���ͽ�� 
		    /*�ȴ��ں��¼�����״̬����*/
			WaitForSingleObject(g_hEvent, INFINITE);
			for (i = 0; i < clnt_cnt; i++)
				send(clnt_socks[i], (char*)&grad_sum, sizeof(grad_sum), 0);
			SetEvent(g_hEvent);		/*��������*/
			cnt++;
			printf("Ⱥ���ͳɹ� %d\n", cnt);
			grad_cnt = 0;
			grad_sum[676] = { 0 };
		}
	}
	printf("�ͻ����˳�:%d\n", GetCurrentThreadId());
	/* �ȴ��ں��¼�����״̬���� */
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
	SetEvent(g_hEvent);		/*��������*/
	// �ر�ͬ�ͻ��˵�����
	closesocket(clnt_sock);
	return NULL;
}

void send_msg(char* msg, int len)
{
	int i;
	/*�ȴ��ں��¼�����״̬����*/
	WaitForSingleObject(g_hEvent, INFINITE);
	for (i = 0; i < clnt_cnt; i++)
		send(clnt_socks[i],(char*)&msg, sizeof(msg), 0);
	SetEvent(g_hEvent);		/*��������*/
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
#define MAXLEN 3 //���鳤�� 
int recvpacket[MAXLEN];
int countdata();
void main(){    
//socket�汾     
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
//����socket     
SOCKET sockSrv=socket(AF_INET,SOCK_STREAM,0);    
SOCKADDR_IN addSrv;    
addSrv.sin_addr.S_un.S_addr=htonl(INADDR_ANY);    
addSrv.sin_family=AF_INET;    
addSrv.sin_port=htons(6161);    
bind(sockSrv,(SOCKADDR*)&addSrv,sizeof(SOCKADDR));//��     
listen(sockSrv,5);//����     
int len=sizeof(SOCKADDR);    
SOCKADDR_IN addClient;    
while(1)    
{        
SOCKET sockConn=accept(sockSrv,(SOCKADDR*)&addClient,&len);        
char sendBuf[100];        
sprintf(sendBuf,"���Ѿ��ɹ����ӵ�����������ȴ�...");        
send(sockConn,sendBuf,strlen(sendBuf)+1,0);//���ͻ�ӭ��Ϣ                 
recv(sockConn,(char *)&recvpacket,sizeof(recvpacket),0);//��������                 
int myresult=countdata();//��������              
send(sockConn,(char *)&myresult,sizeof(myresult),0);//���ͽ��         
closesocket(sockConn);          
}   
}
//���� 
int countdata(){    
int s=0;    
for (int i=0;i<MAXLEN;i++)    
{        s=s+recvpacket[i];    }       
return s;
}

*/