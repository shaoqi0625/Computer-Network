#include <iostream>
#include <cstdio>
#include <winsock2.h> //WINSOCK API的头文件
#include <cstring>
#include <ctime>

using namespace std;

#pragma comment(lib, "ws2_32.lib") // Windows Sockets应用程序接口
#pragma warning(disable : 4996)

const int BUF_SIZE = 1000;    //缓冲区大小
const int MAX_NUM = 5;        //最大连接数
SOCKET cliSock[MAX_NUM];      //客户端套接字
SOCKADDR_IN cliAddr[MAX_NUM]; //客户端地址
WSAEVENT cliEvent[MAX_NUM];   //客户端事件
int num = 0;                  //客户端数量
time_t now_time = time(NULL);

DWORD WINAPI serverEventThread(LPVOID IpParameter) //服务端线程
{
    SOCKET *serverSocket = (SOCKET *) IpParameter; // LPVOID为空指针类型，需要先转成SOCKET类型再引用
    while (1) {
        for (int i = 0; i < num + 1; i++) // num+1是因为要包含客户端和服务端
        {
            WSANETWORKEVENTS networkevents;
            WSAEnumNetworkEvents(cliSock[i], cliEvent[i], &networkevents); //查看事件
            //选择事件
            if (networkevents.lNetworkEvents & FD_ACCEPT) //接收
            {
                if (num + 1 < MAX_NUM) {
                    int nextIndex = num + 1;
                    int addlen = sizeof(SOCKADDR);
                    SOCKET newSock = accept(*serverSocket, (SOCKADDR *) &cliAddr[nextIndex], &addlen);
                    if (newSock != INVALID_SOCKET) {
                        num++;
                        cliSock[nextIndex] = newSock;                                                //为新客户端分配socket
                        WSAEVENT newEvent = WSACreateEvent();                                        //为新客户端绑定事件对象
                        WSAEventSelect(cliSock[nextIndex], newEvent,
                                       FD_CLOSE | FD_READ | FD_WRITE); //设置权限：close，read，write
                        cliEvent[nextIndex] = newEvent;
                        tm *t = localtime(&now_time);
                        cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                             << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                        cout << "#" << nextIndex << "Client(IP：" << inet_ntoa(cliAddr[nextIndex].sin_addr)
                             << ") entered the chat room, the current number of connections is:" << num << endl;
                        cout << "---------------------------------------------------------------------------" << endl;
                        char buf[BUF_SIZE] = "Welcome Client";
                        strcat(buf, to_string(nextIndex).c_str());
                        strcat(buf, "(IP：");
                        strcat(buf, inet_ntoa(cliAddr[nextIndex].sin_addr));
                        strcat(buf, ") to the chat room.");
                        for (int j = i; j <= num; j++) {
                            send(cliSock[j], buf, sizeof(buf), 0);
                        }
                    }
                }
            } else if (networkevents.lNetworkEvents & FD_CLOSE) //客户端关闭，断开连接
            {
                num--;
                if (num >= 0) {
                    tm *t = localtime(&now_time);
                    cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                         << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                    cout << "#" << i << "Client (IP：" << inet_ntoa(cliAddr[i].sin_addr)
                         << ") left the chat room, the current number of connections:" << num << endl;
                    cout << "---------------------------------------------------------------------------" << endl;
                    closesocket(cliSock[i]); //释放客户端资源
                    WSACloseEvent(cliEvent[i]);
                    for (int j = i; j <= num; j++) {
                        cliSock[j] = cliSock[j + 1];
                        cliEvent[j] = cliEvent[j + 1];
                        cliAddr[j] = cliAddr[j + 1];
                    } //用顺序表删除元素
                    char buf[BUF_SIZE] = "Client";
                    strcat(buf, to_string(i).c_str());
                    strcat(buf, "(IP：");
                    strcat(buf, inet_ntoa(cliAddr[i].sin_addr));
                    strcat(buf, ") exit the chat room.");
                    for (int j = 1; j <= num; j++) {
                        send(cliSock[j], buf, sizeof(buf), 0);
                    } //向所有客户端发送用户退出聊天室
                }
            } else if (networkevents.lNetworkEvents & FD_READ) //接收消息
            {
                char rece_buf[BUF_SIZE] = {0};
                for (int j = 1; j <= num; j++) {
                    int rece = recv(cliSock[j], rece_buf, sizeof(rece_buf), 0); //接收到的字节数
                    if (strcmp(rece_buf, "exit()") == 0) {
                        num--;
                        cout<<"hi"<<endl;
                        closesocket(cliSock[j]); //释放客户端资源
                        WSACloseEvent(cliEvent[j]);
                        for (int k = j; k <= num; k++) {
                            cliSock[k] = cliSock[k + 1];
                            cliEvent[k] = cliEvent[k + 1];
                            cliAddr[k] = cliAddr[k + 1];
                        } //用顺序表删除元素
                        if (num >= 0) {
                            char buf[BUF_SIZE] = "Client";
                            strcat(buf, to_string(j).c_str());
                            strcat(buf, "(IP：");
                            strcat(buf, inet_ntoa(cliAddr[j].sin_addr));
                            strcat(buf, ") exit the Server.");
                            tm *t = localtime(&now_time);
                            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                            cout << "#" << j << "Client (IP：" << inet_ntoa(cliAddr[j].sin_addr)
                                 << ") left the chat room, the current number of connections:" << num << endl;
                            cout << "---------------------------------------------------------------------------"
                                 << endl;
                            for (int k = 1; k <= num; k++) {
                                send(cliSock[k], buf, sizeof(buf), 0);
                            } //向所有客户端发送用户退出聊天室
                        }
                    } else if (rece > 0) {
                        tm *t = localtime(&now_time);
                        cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                             << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                        cout << "#" << i << ":" << rece_buf << endl;
                        cout << "---------------------------------------------------------------------------" << endl;
                        char send_buf[BUF_SIZE] = {0};
                        strcat(send_buf, "#");
                        strcat(send_buf, to_string(j).c_str());
                        strcat(send_buf, ":");
                        strcat(send_buf, rece_buf);
                        for (int k = 1; k <= num; k++) {
                            send(cliSock[k], send_buf, sizeof(send_buf), 0);
                        } //在客户端显示
                    }
                }
            }
        }
    }
}


DWORD WINAPI ServerSend(LPVOID IpParameter) {
    SOCKET *serverSocket = (SOCKET *) IpParameter; // LPVOID为空指针类型，需要先转成SOCKET类型再引用
    while (1) {
        //cout << "Please type what you want to output to the client:" << endl;
        char sendbuf[BUF_SIZE] = {0};
        int send_num;
        cin.getline(sendbuf, BUF_SIZE);
        //cin >> sendbuf;
        for (int i = 1; i <= num; i++) {
            send_num = send(cliSock[i], sendbuf, sizeof(sendbuf), 0);
        }
        if (strcmp(sendbuf, "exit()") == 0) {
            closesocket(*serverSocket);
            for (int j = 0; j <= num; j++) {
                closesocket(cliSock[j]);
            }
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "Server shutdown." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
            return 0;
        } else if (send_num > 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "Message sent successfully." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
        }
        /*if (send_num == 0) {
            tm *t = localtime(&now_time);
            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                << ":" << t->tm_mday << ":" << t->tm_sec << endl;
            cout << "You cannot send an empty message." << endl;
            cout << "---------------------------------------------------------------------------" << endl;
        }*/
        memset(sendbuf, 0, BUF_SIZE);
    }
}

int main() {
    WSADATA wsaData; //获取版本信息，说明要使用的版本
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);         //创建socket
    sockaddr_in serverAddr;                                   //绑定socke
    serverAddr.sin_family = AF_INET;                          // IPv4地址
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");      //具体地址
    serverAddr.sin_port = htons(11111);                        //端口
    bind(sockSrv, (SOCKADDR *) &serverAddr, sizeof(SOCKADDR)); //绑定服务端的socket和打包好的地址
    WSAEVENT servEvent = WSACreateEvent();                    //创建对象
    WSAEventSelect(sockSrv, servEvent, FD_ALL_EVENTS);        //绑定事件对象，用于监听所有事件
    cliSock[0] = sockSrv;
    cliEvent[0] = servEvent;
    listen(sockSrv, MAX_NUM);
    cout << "The chat server is open. Let's start chatting." << endl;
    HANDLE hThread[2];
    hThread[0] = CreateThread(nullptr, 0, serverEventThread, (LPVOID) &sockSrv, 0, NULL);
    hThread[1] = CreateThread(nullptr, 0, ServerSend, (LPVOID) &sockSrv, 0, NULL);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);
    closesocket(sockSrv);
    WSACleanup();
    return 0;
}