#include"CNN.h"
#include"dataSet.h"
#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <stdlib.h>
#include <WS2tcpip.h>
#include <vector>
#include <string>
#include <typeinfo>
#include <ATen/ATen.h>
#include <torch/script.h>
#include<opencv2/opencv.hpp>
#include<torch/torch.h>
using namespace std;

#pragma comment(lib,"WS2_32.lib")
#pragma warning(disable:4996)
#define BUF_SIZE 256
#define NAME_SIZE 30
#define _WINSOCK_DEPRECATED_NO_WARNINGS 0

DWORD WINAPI ThreadProc(LPVOID lpParam);
void error_handling(const char* msg);

int main()
{
	HANDLE hThread[2];
	DWORD dwThreadId;
	// 初始化WS2_32.dll
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	WSAStartup(sockVersion, &wsaData);

	printf("Press 'Enter' to start training your model:");
	//scanf_s("%s", name, 30);
	getchar();	//接收换行符
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
		error_handling("Failed socket()");

	// 填写远程地址信息
	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	// 如果计算机没有联网，直接使用本地地址127.0.0.1
	servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if (connect(sock, (sockaddr*)&servAddr, sizeof(servAddr)) == -1)
		error_handling("Failed connect()");
	printf("connect success\n");
	/*
		NULL,		// 默认安全属性
		NULL,		// 默认堆栈大小
		send_msg,	// 线程入口地址（执行线程的函数）
		&sock,		// 传给函数的参数
		0,		    // 指定线程立即运行
		&dwThreadId	// 返回线程的ID号
	*/
	hThread[0] =
		CreateThread(NULL, NULL, ThreadProc, &sock, 0, &dwThreadId);

	// 等待线程运行结束
	WaitForMultipleObjects(1, hThread, true, INFINITE);
	CloseHandle(hThread[0]);
	//CloseHandle(hThread[1]);
	printf(" Thread Over,Enter anything to over.\n");
	getchar();
	// 关闭套节字
	closesocket(sock);
	// 释放WS2_32库
	WSACleanup();
    std::cout << "Test output";
    return 0;
}

DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	int sock = *((int*)lpParam);
	char name_msg[NAME_SIZE + BUF_SIZE];
	double numlist[676];

	std::string path_train = "D:\\hymenoptera_data\\train"; 
	auto custom_dataset_train = dataSetClc(path_train, ".jpg").map(torch::data::transforms::Stack<>());
	auto data_loader_train = torch::data::make_data_loader<torch::data::samplers::RandomSampler>(std::move(custom_dataset_train), 1);

	float loss_train = 0; float loss_val = 0;
	float acc_train = 0; float acc_val = 0; float best_acc = 0;
	/* 第一次训练一个CNN网络 */
	auto cnn = plainCNN(3, 1);
	torch::Device device = torch::Device(torch::kCPU);
	for (int epoch = 0; epoch < 2; epoch++){
		//auto cnn_input = torch::randint(255, { 1,3,224,224 });
		torch::optim::Adam optimizer_cnn(cnn.parameters(), 0.0003);
		auto cnn_target = torch::zeros({ 1,1,26,26 });
		float batch_index_train = 0;
		acc_train = 0; loss_train = 0;
		for (auto& batch : *data_loader_train) {
			auto cnn_input = batch.data; 
			auto cnn_target = batch.target.squeeze(); 
			cnn_input = cnn_input.to(torch::kFloat).to(device);
			cnn_target = cnn_target.to(torch::kFloat).to(device);
			optimizer_cnn.zero_grad();
			auto out = cnn.forward(cnn_input);
			auto acc = out.argmax(1).eq(cnn_target).sum();
			acc_train = acc_train + acc.template item<float>()+batch_index_train * 2;
			auto loss = torch::mse_loss(out, cnn_target);
			out.retain_grad();
			loss.backward();
			at::Tensor t = out.grad()[0][0].clone();
			//vector<double> g (t.data_ptr<double>(), t[26][26].data_ptr());
			for (int j = 0; j < 26; j++) {
				for (int k = 0; k < 26; k++) {
					numlist[j * k + k] = t[j][k].item<double>();
				}
			}
			send(sock, (char*)&numlist, sizeof(numlist), 0);
			vector<double> re;
			double recvlist[676];
			recv(sock, (char*)&recvlist, sizeof(recvlist), 0);
			at::TensorOptions opts = at::TensorOptions().dtype(at::kDouble);
			for (int m = 0; m < 676; m++) {
				re.push_back(recvlist[m]);
			}
			out.grad()[0][0] = at::from_blob(re.data(), { 26,26 }, opts).clone();
			optimizer_cnn.step();
			loss_train += loss.item<float>();
			batch_index_train++;
			int e = epoch + 1;
			cout << " Epoch: " << e << "\t| Train Loss: " << loss_train / batch_index_train << "\t| Train Acc: " << 0.001*acc_train / batch_index_train << endl;
		}
	}
	return NULL;
}

void error_handling(const char* msg)
{
	printf("%s\n", msg);
	WSACleanup();
	exit(1);
}

/*
#include <Winsock2.h>
#include <stdio.h>
#include <iostream.h>
#include <stdlib.h>
#define MAXLEN 3
int sendpacket[MAXLEN];
void getdata();
void main(){
WORD wVersion;
WSADATA wsaData;
int err;
wVersion=MAKEWORD(1,1);
err=WSAStartup(wVersion,&wsaData);
if(err!=0)    {        return;    }
if((LOBYTE(wsaData.wVersion)!=1)||(HIBYTE(wsaData.wVersion)!=1))
{        WSACleanup();        return;    }
SOCKET sockClient=socket(AF_INET,SOCK_STREAM,0);
SOCKADDR_IN addSrv;
addSrv.sin_addr.S_un.S_addr=inet_addr("127.0.0.1");
addSrv.sin_family=AF_INET;
addSrv.sin_port=htons(6161);
connect(sockClient,(SOCKADDR*)&addSrv,sizeof(SOCKADDR));
char recvBuf[100];
recv(sockClient,recvBuf,100,0);
printf("%s/n",recvBuf);
getdata();
send(sockClient,(char *)&sendpacket,sizeof(sendpacket),0);
int myresult;
recv(sockClient,(char *)&myresult,sizeof(myresult),0);
cout<<myresult<<endl;
closesocket(sockClient);
WSACleanup();
}
void getdata(){
cout<<"请输入"<<MAXLEN<<"个整数:"<<endl;
for (int i=0;i<MAXLEN;i++)
{
cin>>sendpacket[i];
}
}
*/
