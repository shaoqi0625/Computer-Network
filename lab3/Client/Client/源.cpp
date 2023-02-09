#include <cstdio>
#include <winsock2.h> //WINSOCK API的头文件
#include <cstring>
#include <ctime>
#include <iostream>
#include<fstream>
#include <vector>

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
int SlidingWindow = 10;//滑动窗口大小
int window_head;
int window_tail;
int packagenum;
double cwnd = 10;
clock_t start;
int last_seq;
int duplicate_ack;
int state = 0;//0为慢启动阶段，1为拥塞避免阶段
int ssthresh = 20;//阈值

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

struct Argument
{
	SOCKET socket;
	SOCKADDR_IN sock_addr;
	char* message;
	int len;
	Argument(SOCKET socket, SOCKADDR_IN sock_addr, char* message, int len) :socket(socket), sock_addr(sock_addr), message(message), len(len) {};
};


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

int Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//第一次握手（发送）
	header.flag = SYN;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
	{
		return 0;
	}
	//cout << "第一次握手成功" << endl;
	clock_t start_t1 = clock();
	//设置为非阻塞的socket
	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode);

	//第二次握手（接收）
	while (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
	{
		if (clock() - start_t1 > MAXTIMEOUT)//超时，重新传输第一次握手
		{
			header.flag = SYN;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
			start_t1 = clock();
			cout << "第一次握手超时，正在重传" << endl;
		}
	}
	cout << "第一次握手成功" << endl;
	memcpy(&header, buffer, sizeof(header));
	if (header.flag == ACK_SYN  && CheckSum((u_short*)&header, sizeof(header)) == 0)
	{
		cout << "第二次握手成功" << endl;
	}
	else
	{
		cout << "连接发生错误！" << endl;
		return 0;
	}
	mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);

	//第三次握手（发送）
	header.flag = ACK;	
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
	{
		return 0;
	}
	cout << "第三次握手成功" << endl;
	cout << "三次握手结束，连接成功" << endl;
}

int DisConnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//第一次挥手（发送）
	header.flag = FIN;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
	{
		return 0;
	}
	clock_t start_t1 = clock();
	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode);

	//第二次挥手（接收）
L: while (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
{
	if (clock() - start_t1 > MAXTIMEOUT)
	{
		header.flag = FIN;
		header.sum = 0;
		header.sum = CheckSum((u_short*)&header, sizeof(header));
		memcpy(buffer, &header, sizeof(header));
		sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
		start_t1 = clock();
		cout << "第一次挥手超时，正在重传" << endl;
	}
}
cout << "第一次挥手成功" << endl;
memcpy(&header, buffer, sizeof(header));
if (header.flag == ACK && CheckSum((u_short*)&header, sizeof(header)) == 0)
{
	cout << "第二次挥手成功" << endl;
}
else
{
	cout << "连接发生错误！" << endl;
	return 0;
}
start_t1 = clock();

//第三次挥手（接收）
while (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
{
	if (clock() - start_t1 > MAXTIMEOUT)
	{
		header.flag = FIN;
		header.sum = 0;
		header.sum = CheckSum((u_short*)&header, sizeof(header));
		memcpy(buffer, &header, sizeof(header));
		sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
		start_t1 = clock();
		cout << "第三次挥手超时，正在重传" << endl;
		goto L;
	}
}
memcpy(&header, buffer, sizeof(header));
if (header.flag == FIN_ACK && CheckSum((u_short*)&header, sizeof(header)) == 0)
{
	cout << "第三次挥手成功" << endl;
}
else
{
	cout << "连接发生错误！" << endl;
	return 0;
}
mode = 0;
ioctlsocket(socketClient, FIONBIO, &mode);

//第四次挥手（发送）
header.flag = ACK;
header.sum = 0;
header.sum = CheckSum((u_short*)&header, sizeof(header));
if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
{
	return 0;
}
cout << "第四次挥手成功" << endl;
cout << "四次挥手结束，连接断开" << endl;
return 1;
}

//lab3-1

void SendPacket(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{
	HEADER header;
	char* buffer = new char[MAXSIZE + sizeof(header)];
	header.datasize = len;
	header.seq = unsigned char(order);//序列号
	header.flag = unsigned char(0x0);
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), message, sizeof(header) + len);
	header.sum = CheckSum((u_short*)buffer, sizeof(header) + len);
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
	cout << "Send message:" << len << "bytes" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
	clock_t start = clock();
	//接收ACK等信息
	while (1)
	{
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				header.datasize = len;
				header.seq = unsigned char(order);
				header.flag = unsigned char(0x0);
				memcpy(buffer, &header, sizeof(header));
				memcpy(buffer + sizeof(header), message, sizeof(header) + len);
				//header.sum = 0;
				header.sum = CheckSum((u_short*)buffer, sizeof(header) + len);
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				cout << "发送超时，正在重传" << endl;
				cout<<"Resend message:"<<len<<"bytes"<<"\tFlag:"<<int(header.flag)<<"\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.seq == unsigned char(order) && header.flag == ACK)
		{
			cout<<"发送成功"<< "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
			break;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}

//发送整个文件
void Send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
	int PacketNum;
	int seqnum = 0;
	if (len % MAXSIZE == 0)
	{
		PacketNum = len / MAXSIZE;
	}
	else
	{
		PacketNum = len / MAXSIZE + 1;
	}
	for (int i = 0; i < PacketNum; i++)
	{
		int templen;
		if (PacketNum == i + 1)
		{
			templen = len - i * MAXSIZE;
		}
		else
		{
			templen = MAXSIZE;
		}
		//cout << message<<endl;
		SendPacket(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, templen, seqnum);
		seqnum++;
		if (seqnum > 255)
		{
			seqnum -= 256;
		}
	}
	//发送结束信息
	HEADER header;
	char* buffer = new char[sizeof(header)];
	header.flag = OVER;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
	cout << "发送结束" << endl;
	clock_t start = clock();
	while (1)
	{
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				char* buffer = new char[sizeof(header)];
				header.flag = OVER;
				header.sum = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				cout << "结束超时，正在重传" << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == OVER)
		{
			cout << "发送成功，对方已成功接收文件" << endl;
			break;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);
}


//lab3-2
//GBN
//发送单个数据包
void SendPacket_GBN(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int order)
{
	HEADER header;
	char* buffer = new char[MAXSIZE + sizeof(header)];
	header.datasize = len;
	header.seq = unsigned char(order);//序列号
	header.flag = unsigned char(0x0);
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), message, sizeof(header) + len);
	header.sum = CheckSum((u_short*)buffer, sizeof(header) + len);
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
	cout << "Send message:" << len << "bytes" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
	/*
	clock_t start = clock();
	//接收ACK等信息
	while (1)
	{
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				header.datasize = len;
				header.seq = unsigned char(order);
				header.flag = unsigned char(0x0);
				memcpy(buffer, &header, sizeof(header));
				memcpy(buffer + sizeof(header), message, sizeof(header) + len);
				//header.sum = 0;
				header.sum = CheckSum((u_short*)buffer, sizeof(header) + len);
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				cout << "发送超时，正在重传" << endl;
				cout<<"Resend message:"<<len<<"bytes"<<"\tFlag:"<<int(header.flag)<<"\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.seq == unsigned char(order) && header.flag == ACK)
		{
			cout<<"发送成功"<< "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
			break;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
	*/
}

//发送整个文件
void Send_GBN(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
	int GBN_Head = -1;//滑动窗口头部
	int GBN_Tail = 0;//滑动窗口尾部
	int PacketNum;
	if (len % MAXSIZE == 0)
	{
		PacketNum = len / MAXSIZE;
	}
	else
	{
		PacketNum = len / MAXSIZE + 1;
	}
	/*
	for (int i = 0; i < PacketNum; i++)
	{
		int templen;
		if (PacketNum == i + 1)
		{
			templen = len - i * MAXSIZE;
		}
		else
		{
			templen = MAXSIZE;
		}
		//cout << message<<endl;
		SendPacket(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, templen, seqnum);
		seqnum++;
		if (seqnum > 255)
		{
			seqnum -= 256;
		}
	}
	*/
	HEADER header;
	char* buffer = new char[sizeof(header)];
	clock_t start_send;
	while (GBN_Head < PacketNum - 1)//没有发送结束
	{
		if (GBN_Tail - GBN_Head < SlidingWindow && GBN_Tail != PacketNum)
		{
			int templen;
			if (PacketNum == GBN_Tail + 1)
			{
				templen = len - GBN_Tail * MAXSIZE;
			}
			else
			{
				templen = MAXSIZE;
			}
			SendPacket_GBN(socketClient, servAddr, servAddrlen, message + GBN_Tail * MAXSIZE, templen, (GBN_Tail%256));
			cout << "剩余滑动窗口大小" << SlidingWindow - (GBN_Tail - GBN_Head)<<endl;
			GBN_Tail++;
			start = clock();
		}
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		if (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen)>0)//缓冲区接收到信息
		{
			memcpy(&header, buffer, sizeof(header));//读取缓冲区
			if ((int(CheckSum((u_short*)&header, sizeof(header)))!=0) || header.flag != ACK)//确认校验和以及ACK
			{
				GBN_Tail = GBN_Head + 1;
				cout << "GBN重传" << endl;
				continue;
			}
			else//说明收到ACK，若header.seq在GBN_Head之后，则说明中间部分都已被确认，调整GBN_Head位置
			{
				if (int(header.seq) >= GBN_Head % 256)
				{
					GBN_Head = GBN_Head + int(header.seq) - GBN_Head % 256;
					cout << "Send message(GBN) Confirmed:" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << endl;
				}
				else if ((GBN_Head % 256 > 256 - SlidingWindow - 1) && (int(header.seq) < SlidingWindow))
				{
					GBN_Head = GBN_Head + 256 - GBN_Head % 256 + int(header.seq);
					cout << "Send message(GBN) Confirmed:" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << endl;
				}
			}
		}
		else
		{
			if (clock() - start > MAXTIMEOUT)
			{
				GBN_Tail = GBN_Head + 1;
				cout << "GBN重传" << endl;
			}
		}
		mode = 0;
		ioctlsocket(socketClient, FIONBIO, &mode);
	}

	//发送结束信息
	header.flag = OVER;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
	cout << "发送结束" << endl;
	start = clock();
	while (1)
	{
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				char* buffer = new char[sizeof(header)];
				header.flag = OVER;
				header.sum = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				cout << "结束超时，正在重传" << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == OVER)
		{
			cout << "发送成功，对方已成功接收文件" << endl;
			break;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);
}


//lab3-3(单线程)

void sendpackage3_3(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int order)
{
	HEADER header;
	char* buffer = new char[MAXSIZE + sizeof(header)];
	header.datasize = len;
	header.seq = unsigned char(order);//序列号
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), message, sizeof(header) + len);
	u_short check = CheckSum((u_short*)buffer, sizeof(header) + len);//计算校验和
	header.sum = check;
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
	cout << "Send message " << len << " bytes!" << " flag:" << int(header.flag) << " SEQ:" << int(header.seq) << " SUM:" << int(header.sum) << " windows:" << cwnd << endl;
}

void send3_3(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
	int sock_len = sizeof(servAddr);
	HEADER header;
	char* buffer = new char[sizeof(header)];
	int packagenum = len / MAXSIZE;
	if (len % MAXSIZE != 0)
	{
		packagenum += 1;
	}
	int window_head = -1;
	int window_tail = 0;
	int last_seq = 0;
	clock_t start;
	int duplicate_ack = 0;
	while (window_head < packagenum - 1)
	{
		//发送数据包
		if (window_tail - window_head <= cwnd && window_tail != packagenum)
		{
			int packagelen = MAXSIZE;
			int seq = window_tail % 256;
			if (window_tail == packagenum - 1)
			{
				packagelen = len - (packagenum - 1) * MAXSIZE;
			}
			cout << "head: " << window_head << "  tail: " << window_tail << endl;
			sendpackage3_3(socketClient, servAddr, servAddrlen,message+window_tail * MAXSIZE, packagelen, seq);
			start = clock();
			window_tail++;
		}

		//接收ACK
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		if (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &sock_len) > 0)
		{
			memcpy(&header, buffer, sizeof(header));
			if (CheckSum((u_short*)&header, sizeof(header)) != 0 || header.flag != ACK)
			{
				window_tail = window_head + 1;
				cout << "GBN(message incorrect)!" << endl;
				continue;
			}
			else
			{
				//重复ACK的判断
				if (last_seq == (int)header.seq)
				{
					duplicate_ack++;
				}
				else
				{
					last_seq = (int)header.seq;
					duplicate_ack = 0;
				}
				//窗口大小改变
				if (state == 0)
				{
					cwnd++;
					if (cwnd >= ssthresh)
					{
						state = 1;
					}
				}
				else
				{
					cwnd += 1.0 / cwnd;
				}
				if (int(header.seq) >= window_head % 256)
				{
					window_head = window_head + int(header.seq) - window_head % 256;
					cout << "[Send has been confirmed]  Flag: " << int(header.flag) << "   SEQ: " << int(header.seq) << "   SUM: " << int(header.sum) << "  CWND: " << int(cwnd) << endl;
				}
				else if (window_head % 256 > 256 - cwnd - 1 && int(header.seq) < cwnd)
				{
					window_head = window_head + 256 - window_head % 256 + int(header.seq);
					cout << "[Send has been confirmed]  Flag: " << int(header.flag) << "   SEQ: " << int(header.seq) << "   SUM: " << int(header.sum) << "  CWND: " << int(cwnd) << endl;
				}
			}
			//三次重复ACK
			if (duplicate_ack >= 3)
			{
				duplicate_ack = 0;
				ssthresh = cwnd / 2;
				cwnd = ssthresh + 3;
				state = 1;
				window_tail = window_head + 1;
			}
		}
		else
		{
			//超时
			if (clock() - start > MAXTIMEOUT)
			{
				ssthresh = cwnd / 2;
				cwnd = 1;
				state = 0;
				window_tail = window_head + 1;
				start = clock();
				cout << "GBN(time out)!" << endl;
			}
		}
		mode = 0;
		ioctlsocket(socketClient, FIONBIO, &mode);
	}

	//发送OVER
	header.flag = OVER;
	header.sum = 0;
	header.seq = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, sock_len);
	cout << "send over!" << endl;
	start = clock();

	//接收OVER
	while (true)
	{
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &sock_len) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				header.flag = OVER;
				header.sum = 0;
				header.seq = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, sizeof(header), 0, (sockaddr*)&servAddr, sock_len);
				cout << "send over!" << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == OVER)
		{
			cout << "message received!" << endl;
			break;
		}
		else
		{
			continue;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);
}


//lab3-3(多线程)
void send_package3_3(SOCKET & socket, SOCKADDR_IN & sock_addr, char* message, int len, int seq)
{
	int sock_len = sizeof(sock_addr);
	HEADER header;
	char* buffer = new char[MAXSIZE + sizeof(header)];   
	header.flag = 0;
	header.sum = 0;
	header.seq = seq;
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), message, len);
	header.sum = CheckSum((u_short*)buffer, len + sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	sendto(socket, buffer, len + sizeof(header), 0, (sockaddr*)&sock_addr, sock_len);
	cout << "[Send message] " << len << " bytes!" << "  FLAG: " << int(header.flag) << "   SEQ: " << int(header.seq) << "   SUM: " << int(header.sum) << "  CWND: " << cwnd << endl;
}

DWORD WINAPI SendPackageThread(LPVOID lpPara)
{
	Argument arg = *(Argument*)lpPara;
	SOCKET socket = arg.socket;
	SOCKADDR_IN sock_addr = arg.sock_addr;
	char* message = arg.message;
	int len = arg.len;
	int sock_len = sizeof(sock_addr);
	while (window_head < packagenum - 1)
	{
		//发送数据包
		if (window_tail - window_head <= cwnd && window_tail != packagenum)
		{
			int packagelen = MAXSIZE;
			int seq = window_tail % 256;
			if (window_tail == packagenum - 1)
			{
				packagelen = len - (packagenum - 1) * MAXSIZE;
			}
			cout << "head: " << window_head << "  tail: " << window_tail << endl;
			send_package3_3(socket, sock_addr, message+window_tail * MAXSIZE, packagelen, seq);
			start = clock();
			window_tail++;
		}
	}
	return 0;
}

DWORD WINAPI ReceiveACKThread(LPVOID lpPara)
{
	Argument arg = *(Argument*)lpPara;
	SOCKET socket = arg.socket;
	SOCKADDR_IN sock_addr = arg.sock_addr;
	char* message = arg.message;
	int len = arg.len;
	int sock_len = sizeof(sock_addr);
	HEADER header;
	char* buffer = new char[sizeof(header)];
	while (window_head < packagenum - 1)
	{
		//接收ACK
		u_long mode = 1;
		ioctlsocket(socket, FIONBIO, &mode);
		if (recvfrom(socket, buffer, sizeof(header), 0, (sockaddr*)&sock_addr, &sock_len) > 0)
		{
			memcpy(&header, buffer, sizeof(header));
			if (!(header.flag == ACK && CheckSum((u_short*)&header, sizeof(header)) == 0))
			{
				window_tail = window_head + 1;
				cout << "GBN(message incorrect)!" << endl;
				continue;
			}
			else
			{
				//重复ACK的判断
				if (last_seq == (int)header.seq)
				{
					duplicate_ack++;
				}
				else
				{
					last_seq = (int)header.seq;
					duplicate_ack = 0;
				}
				//窗口大小改变
				if (state == 0)//慢开始算法
				{
					cwnd++;
					if (cwnd >= ssthresh)
					{
						state = 1;//拥塞避免算法
					}
				}
				else
				{
					cwnd += 1.0 / cwnd;
				}
				if (int(header.seq) >= window_head % 256)
				{
					window_head = window_head + int(header.seq) - window_head % 256;
					cout << "[Send has been confirmed]  Flag: " << int(header.flag) << "   SEQ: " << int(header.seq) << "   SUM: " << int(header.sum) << "  CWND: " << int(cwnd) << endl;
				}
				else if (window_head % 256 > 256 - cwnd - 1 && int(header.seq) < cwnd)
				{
					window_head = window_head + 256 - window_head % 256 + int(header.seq);
					cout << "[Send has been confirmed]  Flag: " << int(header.flag) << "   SEQ: " << int(header.seq) << "   SUM: " << int(header.sum) << "  CWND: " << int(cwnd) << endl;
				}
			}
			//三次重复ACK
			if (duplicate_ack >= 3)
			{
				duplicate_ack = 0;
				ssthresh = cwnd / 2;
				cwnd = ssthresh + 3;
				state = 1;
				window_tail = window_head + 1;
			}
		}
		else
		{
			//超时
			if (clock() - start > MAXTIMEOUT)
			{
				ssthresh = cwnd / 2;
				cwnd = 1;
				state = 0;
				window_tail = window_head + 1;
				start = clock();
				cout << "GBN(time out)!" << endl;
			}
		}
		mode = 0;
		ioctlsocket(socket, FIONBIO, &mode);
	}
	return 0;
}

void Send3_3(SOCKET& socket, SOCKADDR_IN& sock_addr, char* message, int len)
{
	int sock_len = sizeof(sock_addr);
	HEADER header;
	char* buffer = new char[sizeof(header)];
	window_head = -1;
	window_tail = 0;
	last_seq = 0;
	duplicate_ack = 0;
	start = clock();
	packagenum = len / MAXSIZE;
	if (len % MAXSIZE != 0)
	{
		packagenum += 1;
	}
	Argument arg = Argument(socket, sock_addr, message, len);
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, SendPackageThread, (LPVOID)&arg, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, ReceiveACKThread, (LPVOID)&arg, 0, NULL);
	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	//发送OVER
	header.flag = OVER;
	header.sum = 0;
	header.seq = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	sendto(socket, buffer, sizeof(header), 0, (sockaddr*)&sock_addr, sock_len);
	cout << "send over!" << endl;
	start = clock();

	//接收OVER
	while (true)
	{
		u_long mode = 1;
		ioctlsocket(socket, FIONBIO, &mode);
		while (recvfrom(socket, buffer, sizeof(header), 0, (sockaddr*)&sock_addr, &sock_len) <= 0)
		{
			if (clock() - start > MAXTIMEOUT)
			{
				header.flag = OVER;
				header.sum = 0;
				header.seq = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				sendto(socket, buffer, sizeof(header), 0, (sockaddr*)&sock_addr, sock_len);
				cout << "send over!" << endl;
				start = clock();
			}
		}
		memcpy(&header, buffer, sizeof(header));
		if (header.flag == ACK && CheckSum((u_short*)&header, sizeof(header)) == 0)
		{
			cout << "message received!" << endl;
			break;
		}
		else
		{
			continue;
		}
	}
	u_long mode = 0;
	ioctlsocket(socket, FIONBIO, &mode);
}

int main()
{
	WSADATA wsaData; //获取版本信息，说明要使用的版本
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "socket初始化失败" << endl;
		return 0;
	}
	SOCKET sockCli = socket(AF_INET, SOCK_DGRAM, 0);         //创建socket
	SOCKET sockRouter = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockCli == INVALID_SOCKET || sockRouter == INVALID_SOCKET)
	{
		cout << "socket创建失败" << endl;
		return 0;
	}
	sockaddr_in clientAddr;                                   //绑定socke
	sockaddr_in routerAddr;
	clientAddr.sin_family = AF_INET;                          // IPv4地址
	clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");      //具体地址
	clientAddr.sin_port = htons(11111);                        //端口
	routerAddr.sin_family = AF_INET;
	routerAddr.sin_addr.s_addr = inet_addr("127.0.0.2");
	routerAddr.sin_port = htons(11112);
	int len = sizeof(routerAddr);
	if (Connect(sockRouter, routerAddr, len) == 0)
	{
		cout << "三次握手失败" << endl;
		return 0;
	}
	string Filename;
	char* buffer = new char[100000000];
	int index = 0;
	cout << "请输入文件名称" << endl;
	cin >> Filename;
	ifstream fin(Filename.c_str(), ifstream::binary);
	unsigned char temp = fin.get();
	while (fin)
	{
		buffer[index] = temp;
		index++;
		temp = fin.get();
	}
	fin.close();
	//Send(sockRouter, routerAddr, len, (char*)(Filename.c_str()), Filename.length());
	//Send_GBN(sockRouter, routerAddr, len, (char*)(Filename.c_str()), Filename.length());
	send3_3(sockRouter, routerAddr, len, (char*)(Filename.c_str()), Filename.length());
	clock_t start_t = clock();
	//Send(sockRouter, routerAddr, len, buffer, index);
	//Send_GBN(sockRouter, routerAddr, len, buffer, index);
	send3_3(sockRouter, routerAddr, len, buffer, index);
	clock_t end_t = clock();
	double t = 1.0 * (end_t - start_t) / CLOCKS_PER_SEC;
	cout << "总传输时间为" << t << "s" << endl;
	cout << "吞吐率为:" << (float)sizeof(buffer) /t << "byte/s" << endl;
	if (DisConnect(sockRouter, routerAddr, len) == 0)
	{
		cout << "四次挥手失败" << endl;
	}
	closesocket(sockCli);
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
	int len = sizeof(server_addr);
	//建立连接
	if (Connect(server, server_addr, len) == -1)
	{
		return 0;
	}
	
	string filename;
	cout << "请输入文件名称" << endl;
	cin >> filename;
	ifstream fin(filename.c_str(), ifstream::binary);//以二进制方式打开文件
	char* buffer = new char[10000000];
	int index = 0;
	unsigned char temp = fin.get();
	while (fin)
	{
		buffer[index++] = temp;
		temp = fin.get();
	}
	fin.close();
	Send(server, server_addr, len, (char*)(filename.c_str()), filename.length());
	clock_t start = clock();
	Send(server, server_addr, len, buffer, index);
	clock_t end = clock();
	cout << "传输总时间为:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
	cout << "吞吐率为:" << ((float)index) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
	DisConnect(server, server_addr, len);
}
*/