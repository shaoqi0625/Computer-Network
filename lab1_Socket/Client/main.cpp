#include <iostream>
#include <cstdio>
#include <winsock2.h> //WINSOCK API的头文件
#include <cstring>
#include <ctime>

using namespace std;

#pragma comment(lib, "ws2_32.lib") // Windows Sockets应用程序接口
#pragma warning(disable : 4996)

const int BUF_SIZE = 1000;    //缓冲区大小
time_t now_time = time(NULL);

DWORD WINAPI ReceiveMessageThread(LPVOID IpParameter)//接收消息的线程
{
    SOCKET *cliSock = (SOCKET *) IpParameter;
    while (1) {
        char buf[BUF_SIZE] = {0};
        int rece_num = recv(*cliSock, buf, sizeof(buf), 0);
        if (rece_num < 0 || strcmp(buf, "exit()") == 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "Disconnect from the server." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
            closesocket(*cliSock);
            return 0;
        } else if (rece_num > 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << buf << endl;
            cout << "---------------------------------------------------------------------------" << endl;
        }
        memset(buf, 0, BUF_SIZE);
    }
}

DWORD WINAPI SendMessageThread(LPVOID IpParameter) {
    SOCKET *cliSock = (SOCKET *) IpParameter;
    while (1) {
        char buf[BUF_SIZE] = {0};
        cin.getline(buf, BUF_SIZE);
        //cin >> buf;
        int send_num = send(*cliSock, buf, sizeof(buf), 0);
        if (strcmp(buf, "exit()") == 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "Quit the chat room." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
            closesocket(*cliSock);
            return 0;
        } else if (send_num > 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "Message sent successfully." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
        }
        memset(buf, 0, BUF_SIZE);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET cliSock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN cliAddr;
    cliAddr.sin_family = AF_INET;
    cliAddr.sin_addr.s_addr = inet_addr("127.0.0.1");//IP地址
    cliAddr.sin_port = htons(11111);//端口号
    connect(cliSock, (SOCKADDR *) &cliAddr, sizeof(SOCKADDR));
    HANDLE hthread[2];
    hthread[0] = CreateThread(NULL, 0, ReceiveMessageThread, (LPVOID) &cliSock, 0, NULL);
    hthread[1] = CreateThread(NULL, 0, SendMessageThread, (LPVOID) &cliSock, 0, NULL);
    WaitForMultipleObjects(2, hthread, TRUE, INFINITE);
    CloseHandle(hthread[0]);
    CloseHandle(hthread[1]);
    closesocket(cliSock);
    WSACleanup();
    return 0;
}
