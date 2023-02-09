#include <cstdio>
#include <winsock2.h> //WINSOCK API��ͷ�ļ�
#include <cstring>
#include <ctime>
#include <iostream>
#include<fstream>

using namespace std;

#pragma comment(lib, "ws2_32.lib") // Windows SocketsӦ�ó���ӿ�
#pragma warning(disable : 4996)

const int MAXSIZE = 8192;//���仺������󳤶�
const unsigned char SYN = 0x1;          //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;          //SYN = 0, ACK = 1
const unsigned char ACK_SYN = 0x3;      //SYN = 1, ACK = 1
const unsigned char FIN = 0x4;          //FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x6;      //FIN = 1 ACK = 1
const unsigned char OVER = 0x7;         //������־
double MAXTIMEOUT = CLOCKS_PER_SEC;//���ʱ��

#pragma pack(1)
//���ĸ�ʽ
struct HEADER
{
	u_short sum = 0;//У��ͣ�16λ��
	u_short datasize = 0;//���ݳ��ȣ�16λ��
	unsigned char flag = 0;//FIN��ACK��SYN��8λ��
	unsigned char seq = 0;//�������кţ�8λ��
	HEADER()
	{
		sum = 0;
		datasize = 0;
		flag = 0;
		seq = 0;
	}
};
#pragma pack()

//����⣺����У���
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

//��������
int Connect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//��һ�����֣����գ�
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
			cout << "��һ�����ֳɹ�" << endl;
			break;
		}
	}

	//�ڶ������֣����ͣ�
	header.flag = ACK_SYN;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}
	clock_t start = clock();//����ʱ��

	//���������֣����գ�
	//����Ϊ��������socket
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
			cout << "�ڶ������ֳ�ʱ�������ش�" << endl;
		}
	}
	cout << "�ڶ������ֳɹ�" << endl;
	HEADER temp_header;
	memcpy(&temp_header, buffer, sizeof(temp_header));
	if (temp_header.flag == ACK && CheckSum((u_short*)&temp_header, sizeof(temp_header)) == 0 )
	{
		cout << "���������ֳɹ�" << endl;
	}
	else
	{
		cout << "���ӷ�������" << endl;
		return 0;
	}
	cout << "�������ֽ��������ӳɹ�" << endl;
	mode = 0;
	ioctlsocket(sockServ, FIONBIO, &mode);
	return 1;
}

int DisConnect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
	HEADER header;
	char* buffer = new char[sizeof(header)];
	//��һ�λ��֣����գ�
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
			cout << "��һ�λ��ֳɹ�" << endl;
			break;
		}
	}

	//�ڶ��λ��֣����ͣ�
	header.flag = ACK;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}

	//�����λ��֣����ͣ�
	header.flag = FIN_ACK;
	header.sum = 0;
	header.sum = CheckSum((u_short*)&header, sizeof(header));
	memcpy(buffer, &header, sizeof(header));
	if (sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
	{
		return 0;
	}
	clock_t start = clock();

	//���Ĵλ��֣����գ�
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
			cout << "�ڶ�/���λ��ֳ�ʱ�������ش�" << endl;
		}
	}
	cout << "�ڶ���&�����λ��ֳɹ�" << endl;
	HEADER temp_header;
	memcpy(&temp_header, buffer, sizeof(header));
	if (temp_header.flag == ACK && CheckSum((u_short*)&temp_header, sizeof(temp_header)) == 0)
	{
		cout << "���Ĵλ��ֳɹ�" << endl;
	}
	else
	{
		cout << "���ӷ�������" << endl;
		return 0;
	}

	cout << "�Ĵλ��ֽ��������ӶϿ�" << endl;
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
		if (header.flag == OVER && CheckSum((u_short*)&header, sizeof(header)) == 0)//�ж��Ƿ����
		{
			cout << "�ļ����ճɹ�" << endl;
			break;
		}
		if (header.flag == unsigned char(0) && CheckSum((u_short*)buffer, len) == 0)
		{
			if (seq != int(header.seq))//�ж��Ƿ���ܵ��Ǳ�İ�
			{
				//����ACK
				header.flag = ACK;
				header.datasize = 0;
				header.seq = (unsigned char)(seq-1);
				header.sum = 0;
				header.sum = CheckSum((u_short*)&header, sizeof(header));
				memcpy(buffer, &header, sizeof(header));
				//�ط�ACK
				sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
				cout << "Send to Client\tACK:" << (int)header.seq << "\tSEQ:" << (int)header.seq << endl;
				continue;//�������ݰ�
			}
			seq = (int)header.seq;
			if (seq > 255)
			{
				seq -= 256;//seq:0~255
			}
			//ȡ��buffer�е�����
			cout << "Receive message:" << len - sizeof(header) << "bytes" << "\tFlag:" << int(header.flag) << "\tSEQ:" << int(header.seq) << "\tSum:" << int(header.sum) << endl;
			char* temp = new char[len - sizeof(header)];
			memcpy(temp, buffer + sizeof(header), len - sizeof(header));
			memcpy(message + FileLen, temp, len - sizeof(header));
			FileLen += int(header.datasize);
			//����ACK
			header.flag = ACK;
			header.datasize = 0;
			header.seq = (unsigned char)seq;
			header.sum = 0;
			header.sum = CheckSum((u_short*)&header, sizeof(header));
			memcpy(buffer, &header, sizeof(header));
			//�ط�ACK
			sendto(sockServ, buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
			cout << "Send to Client" << "\tACK:" << int(header.seq) << "\tSEQ:" << int(header.seq) << endl;
			seq++;
			if (seq > 255)
			{
				seq -= 256;
			}
		}
	}
	//����
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
	WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "socket��ʼ��ʧ��" << endl;
		return 0;
	}
	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);         //����socket
	SOCKET router = socket(AF_INET, SOCK_DGRAM, 0);         //����socket
	if (sockSrv == INVALID_SOCKET )
	{
		cout << "socket����ʧ��" << endl;
		return 0;
	}
	SOCKADDR_IN serverAddr;                                   //��socke
	SOCKADDR_IN routerAddr;
	serverAddr.sin_family = AF_INET;                          // IPv4��ַ
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");      //�����ַ
	serverAddr.sin_port = htons(11111);                        //�˿�
	routerAddr.sin_family = AF_INET;
	routerAddr.sin_addr.s_addr = inet_addr("127.0.0.2");
	routerAddr.sin_port = htons(11112);
	if (bind(sockSrv, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == -1) //�󶨷���˵�socket�ʹ���õĵ�ַ
	{
		cout << "��ʧ��" << endl;
		return 0;
	}
	cout << "�������״̬" << endl;
	int len = sizeof(routerAddr);
	if (Connect(sockSrv, routerAddr, len) == 0)
	{
		cout << "��������ʧ��" << endl;
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
	cout << "�ļ��������" << endl;
	if (DisConnect(sockSrv, routerAddr, len) == 0)
	{
		cout << "�Ĵλ���ʧ��" << endl;
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

	server_addr.sin_family = AF_INET;//ʹ��IPV4
	server_addr.sin_port = htons(2456);
	server_addr.sin_addr.s_addr = htonl(2130706433);

	server = socket(AF_INET, SOCK_DGRAM, 0);
	bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));//���׽��֣��������״̬
	cout << "�������״̬���ȴ��ͻ�������" << endl;
	int len = sizeof(server_addr);
	//��������
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
	cout << "�ļ��ѳɹ����ص�����" << endl;
}
*/