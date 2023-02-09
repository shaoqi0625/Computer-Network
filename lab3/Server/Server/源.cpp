#include <cstdio>
#include <winsock2.h> //WINSOCK API的头文件
#include <cstring>
#include <ctime>
#include <iostream>
#include<fstream>

using namespace std;

#pragma comment(lib, "ws2_32.lib") // Windows Sockets应用程序接口
#pragma warning(disable : 4996)

const int MAXSIZE = 8192;//传输缓冲区最大长度
const unsigned char SYN = 0x1;          //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;          //SYN = 0, ACK = 1
const unsigned char ACK_SYN = 0x3;      //SYN = 1, ACK = 1
const unsigned char FIN = 0x4;          //FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x6;      //FIN = 1 ACK = 1
const unsigned char OVER = 0x7;         //结束标志
double MAXTIMEOUT = CLOCKS_PER_SEC;//最大时延

#pragma pack(1)
//报文格式
struct HEADER
{
	u_short sum = 0;//校验和（16位）
	u_short datasize = 0;//数据长度（16位）
	unsigned char flag = 0;//FIN，ACK，SYN（8位）
	unsigned char seq = 0;//传输序列号（8位）
	HEADER()
	{
		sum = 0;
		datasize = 0;
		flag = 0;
		seq = 0;
	}
};
#pragma pack()

//差错检测：计算校验和
u_short CheckSum(u_short* message, int size)
{
	int count = (size + 1) / 2;
	u_short* buffer = (u_short*)malloc(size + 1);
	memset(buffer, 0, size + 1);
	memcpy(buffer, message, size);
	u_long sum = 0;
	while (count != 0)
	{
		sum += *buffer;
		buffer++;
		count--;
		if (sum & 0xffff0000)
		{
			sum &= 0xffff;
			sum++;
		}
	}
	return ~(sum & 0xffff);
}

//建立连接
int Connect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//第一次握手（接收）
	while (1)
	{
		if (recvfrom(sockServ, buffer, sizeof(header), 0, (SOCKADDR*)&ClientAddr, &ClientAddrLen) == -1)
		{
			//cout << 1 << endl;
			return 0;
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == SYN && CheckSum((u_short*)&header, sizeof(header)) == 0)
		{
			cout << "第一次握手成功" << endl;
			break;
		}
	}

	//第二次握手（发送）
	header.flag = ACK_SYN;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}
	clock_t start = clock();//发送时间

	//第三次握手（接收）
	//设置为非阻塞的socket
	u_long mode = 1;
	ioctlsocket(sockServ, FIONBIO, &mode);
	while (recvfrom(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
	{
		if (clock() - start > MAXTIMEOUT)
		{
			header.flag = ACK_SYN;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
			{
				return 0;
			}
			cout << "第二次握手超时，正在重传" << endl;
		}
	}
	cout << "第二次握手成功" << endl;
	HEADER temp_header;
	memcpy(&temp_header, buffer, sizeof(temp_header));
	if (temp_header.flag == ACK && CheckSum((u_short*)&temp_header, sizeof(temp_header)) == 0 )
	{
		cout << "第三次握手成功" << endl;
	}
	else
	{
		cout << "连接发生错误！" << endl;
		return 0;
	}
	cout << "三次握手结束，连接成功" << endl;
	mode = 0;
	ioctlsocket(sockServ, FIONBIO, &mode);
	return 1;
}

int DisConnect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//第一次挥手（接收）
	while (1)
	{
		recvfrom(sockServ, buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
		/*
		if (recvfrom(sockServ, buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen) == -1)
		{
			return 0;
		}
		*/
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == FIN && CheckSum((u_short*)&header, sizeof(header)) == 0)
		{
			cout << "第一次挥手成功" << endl;
			break;
		}
	}

	//第二次挥手（发送）
	header.flag = ACK;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}

	//第三次挥手（发送）
	header.flag = FIN_ACK;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}
	clock_t start = clock();

	//第四次挥手（接收）
	while (recvfrom(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
	{
		if (clock() - start > MAXTIMEOUT)
		{
			header.flag = ACK;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
			{
				return 0;
			}
			header.flag = FIN_ACK;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
			{
				return 0;
			}
			cout << "第二/三次挥手超时，正在重传" << endl;
		}
	}
	cout << "第二次&第三次挥手成功" << endl;
	HEADER temp_header;
	memcpy(&temp_header, buffer, sizeof(header));
	if (temp_header.flag == ACK && CheckSum((u_short*)&temp_header, sizeof(temp_header)) == 0)
	{
		cout << "第四次挥手成功" << endl;
	}
	else
	{
		cout << "连接发生错误！" << endl;
		return 0;
	}

	cout << "四次挥手结束，连接断开" << endl;
	return 1;
}

int ReceiveMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* message)
{
	HEADER header;
	int FileLen = 0;
	char* buffer = new char[MAXSIZE + sizeof(header)];
	int seq = 0;
	while (1)
	{
		int len = recvfrom(sockServ, buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == OVER && CheckSum((u_short*)&header, sizeof(header)) == 0)//判断是否结束
		{
			cout << "文件接收成功" << endl;
			break;
		}
		if (header.flag == unsigned char(0) && CheckSum((u_short*)buffer, len) == 0)
		{
			if (seq != int(header.seq))//判断是否接受的是别的包
			{
				//返回ACK
				header.flag = ACK;
				header.datasize = 0;
				header.seq = (unsigned char)(seq-1);
				header.sum = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				//重发ACK
				sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
				cout << "Send to Client\tACK:" << (int)header.seq << "\tSEQ:" << (int)header.seq << endl;
				continue;//丢弃数据包
			}
			seq = (int)header.seq;
			if (seq > 255)
			{
				seq -= 256;//seq:0~255
			}
			//取出buffer中的内容
			cout << "Receive message:" << len - sizeof(header) << "bytes" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
			char* temp = new char[len - sizeof(header)];
			memcpy(temp, buffer + sizeof(header), len - sizeof(header));
			memcpy(message + FileLen, temp, len - sizeof(header));
			FileLen += int(header.datasize);
			//返回ACK
			header.flag = ACK;
			header.datasize = 0;
			header.seq = (unsigned char)seq;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			//重发ACK
			sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
			cout << "Send to Client" << "\tACK:" << int(header.seq) << "\tSEQ:" << int(header.seq) << endl;
			seq++;
			if (seq > 255)
			{
				seq -= 256;
			}
		}
	}
	//结束
	header.flag = OVER;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}
	return FileLen;
}

int main()
{
	WSADATA wsaData; //获取版本信息，说明要使用的版本
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "socket初始化失败" << endl;
		return 0;
	}
	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);         //创建socket
	SOCKET router = socket(AF_INET, SOCK_DGRAM, 0);         //创建socket
	if (sockSrv == INVALID_SOCKET )
	{
		cout << "socket创建失败" << endl;
		return 0;
	}
	SOCKADDR_IN serverAddr;                                   //绑定socke
	SOCKADDR_IN routerAddr;
	serverAddr.sin_family = AF_INET;                          // IPv4地址
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");      //具体地址
	serverAddr.sin_port = htons(11111);                        //端口
	routerAddr.sin_family = AF_INET;
	routerAddr.sin_addr.s_addr = inet_addr("127.0.0.2");
	routerAddr.sin_port = htons(11112);
	if (bind(sockSrv, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) //绑定服务端的socket和打包好的地址
	{
		cout << "绑定失败" << endl;
		return 0;
	}
	cout << "进入监听状态" << endl;
	int len = sizeof(routerAddr);
	if (Connect(sockSrv, routerAddr, len) == 0)
	{
		cout << "三次握手失败" << endl;
		return 0;
	}
	char* name = new char[20];
	char* data = new char[10000000];
	int namelen = ReceiveMessage(sockSrv, routerAddr, len, name);
	int datalen = ReceiveMessage(sockSrv, routerAddr, len, data);
	string filename;
	for (int i = 0; i < namelen; i++)
	{
		filename = filename + name[i];
	}
	cout << "文件传输完成" << endl;
	if (DisConnect(sockSrv, routerAddr, len) == 0)
	{
		cout << "四次挥手失败" << endl;
		return 0;
	}
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < datalen; i++)
	{
		fout << data[i];
	}
	fout.close();
	closesocket(sockSrv);
	WSACleanup();
	return 0;
	}
	
	/*
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	SOCKADDR_IN server_addr;
	SOCKET server;

	server_addr.sin_family = AF_INET;//使用IPV4
	server_addr.sin_port = htons(2456);
	server_addr.sin_addr.s_addr = htonl(2130706433);

	server = socket(AF_INET, SOCK_DGRAM, 0);
	bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));//绑定套接字，进入监听状态
	cout << "进入监听状态，等待客户端上线" << endl;
	int len = sizeof(server_addr);
	//建立连接
	Connect(server, server_addr, len);
	char* name = new char[20];
	char* data = new char[100000000];
	int namelen = ReceiveMessage(server, server_addr, len, name);
	int datalen = ReceiveMessage(server, server_addr, len, data);
	string a;
	for (int i = 0; i < namelen; i++)
	{
		a = a + name[i];
	}
	DisConnect(server, server_addr, len);
	ofstream fout(a.c_str(), ofstream::binary);
	for (int i = 0; i < datalen; i++)
	{
		fout << data[i];
	}
	fout.close();
	cout << "文件已成功下载到本地" << endl;
}
*/