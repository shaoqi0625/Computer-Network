#include <iostream>
#include <cstdio>
#include <winsock2.h> //WINSOCK API��ͷ�ļ�
#include <cstring>
#include <ctime>

using namespace std;

#pragma comment(lib, "ws2_32.lib") // Windows SocketsӦ�ó���ӿ�
#pragma warning(disable : 4996)

const int BUF_SIZE = 1000;    //��������С
const int MAX_NUM = 5;        //���������
SOCKET cliSock[MAX_NUM];      //�ͻ����׽���
SOCKADDR_IN cliAddr[MAX_NUM]; //�ͻ��˵�ַ
WSAEVENT cliEvent[MAX_NUM];   //�ͻ����¼�
int num = 0;                  //�ͻ�������
time_t now_time = time(NULL);

DWORD WINAPI serverEventThread(LPVOID IpParameter) //������߳�
{
    SOCKET *serverSocket = (SOCKET *) IpParameter; // LPVOIDΪ��ָ�����ͣ���Ҫ��ת��SOCKET����������
    while (1) {
        for (int i = 0; i < num + 1; i++) // num+1����ΪҪ�����ͻ��˺ͷ����
        {
            WSANETWORKEVENTS networkevents;
            WSAEnumNetworkEvents(cliSock[i], cliEvent[i], &networkevents); //�鿴�¼�
            //ѡ���¼�
            if (networkevents.lNetworkEvents & FD_ACCEPT) //����
            {
                if (num + 1 < MAX_NUM) {
                    int nextIndex = num + 1;
                    int addlen = sizeof(SOCKADDR);
                    SOCKET newSock = accept(*serverSocket, (SOCKADDR *) &cliAddr[nextIndex], &addlen);
                    if (newSock != INVALID_SOCKET) {
                        num++;
                        cliSock[nextIndex] = newSock;                                                //Ϊ�¿ͻ��˷���socket
                        WSAEVENT newEvent = WSACreateEvent();                                        //Ϊ�¿ͻ��˰��¼�����
                        WSAEventSelect(cliSock[nextIndex], newEvent,
                                       FD_CLOSE | FD_READ | FD_WRITE); //����Ȩ�ޣ�close��read��write
                        cliEvent[nextIndex] = newEvent;
                        tm *t = localtime(&now_time);
                        cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                             << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                        cout << "#" << nextIndex << "Client(IP��" << inet_ntoa(cliAddr[nextIndex].sin_addr)
                             << ") entered the chat room, the current number of connections is:" << num << endl;
                        cout << "---------------------------------------------------------------------------" << endl;
                        char buf[BUF_SIZE] = "Welcome Client";
                        strcat(buf, to_string(nextIndex).c_str());
                        strcat(buf, "(IP��");
                        strcat(buf, inet_ntoa(cliAddr[nextIndex].sin_addr));
                        strcat(buf, ") to the chat room.");
                        for (int j = i; j <= num; j++) {
                            send(cliSock[j], buf, sizeof(buf), 0);
                        }
                    }
                }
            } else if (networkevents.lNetworkEvents & FD_CLOSE) //�ͻ��˹رգ��Ͽ�����
            {
                num--;
                if (num >= 0) {
                    tm *t = localtime(&now_time);
                    cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                         << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                    cout << "#" << i << "Client (IP��" << inet_ntoa(cliAddr[i].sin_addr)
                         << ") left the chat room, the current number of connections:" << num << endl;
                    cout << "---------------------------------------------------------------------------" << endl;
                    closesocket(cliSock[i]); //�ͷſͻ�����Դ
                    WSACloseEvent(cliEvent[i]);
                    for (int j = i; j <= num; j++) {
                        cliSock[j] = cliSock[j + 1];
                        cliEvent[j] = cliEvent[j + 1];
                        cliAddr[j] = cliAddr[j + 1];
                    } //��˳���ɾ��Ԫ��
                    char buf[BUF_SIZE] = "Client";
                    strcat(buf, to_string(i).c_str());
                    strcat(buf, "(IP��");
                    strcat(buf, inet_ntoa(cliAddr[i].sin_addr));
                    strcat(buf, ") exit the chat room.");
                    for (int j = 1; j <= num; j++) {
                        send(cliSock[j], buf, sizeof(buf), 0);
                    } //�����пͻ��˷����û��˳�������
                }
            } else if (networkevents.lNetworkEvents & FD_READ) //������Ϣ
            {
                char rece_buf[BUF_SIZE] = {0};
                for (int j = 1; j <= num; j++) {
                    int rece = recv(cliSock[j], rece_buf, sizeof(rece_buf), 0); //���յ����ֽ���
                    if (strcmp(rece_buf, "exit()") == 0) {
                        num--;
                        cout<<"hi"<<endl;
                        closesocket(cliSock[j]); //�ͷſͻ�����Դ
                        WSACloseEvent(cliEvent[j]);
                        for (int k = j; k <= num; k++) {
                            cliSock[k] = cliSock[k + 1];
                            cliEvent[k] = cliEvent[k + 1];
                            cliAddr[k] = cliAddr[k + 1];
                        } //��˳���ɾ��Ԫ��
                        if (num >= 0) {
                            char buf[BUF_SIZE] = "Client";
                            strcat(buf, to_string(j).c_str());
                            strcat(buf, "(IP��");
                            strcat(buf, inet_ntoa(cliAddr[j].sin_addr));
                            strcat(buf, ") exit the Server.");
                            tm *t = localtime(&now_time);
                            cout << (t->tm_year + 1900) << "." << t->tm_mon << "." << t->tm_mday << "." << t->tm_hour
                                 << ":" << t->tm_mday << ":" << t->tm_sec << endl;
                            cout << "#" << j << "Client (IP��" << inet_ntoa(cliAddr[j].sin_addr)
                                 << ") left the chat room, the current number of connections:" << num << endl;
                            cout << "---------------------------------------------------------------------------"
                                 << endl;
                            for (int k = 1; k <= num; k++) {
                                send(cliSock[k], buf, sizeof(buf), 0);
                            } //�����пͻ��˷����û��˳�������
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
                        } //�ڿͻ�����ʾ
                    }
                }
            }
        }
    }
}


DWORD WINAPI ServerSend(LPVOID IpParameter) {
    SOCKET *serverSocket = (SOCKET *) IpParameter; // LPVOIDΪ��ָ�����ͣ���Ҫ��ת��SOCKET����������
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
    WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);         //����socket
    sockaddr_in serverAddr;                                   //��socke
    serverAddr.sin_family = AF_INET;                          // IPv4��ַ
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");      //�����ַ
    serverAddr.sin_port = htons(11111);                        //�˿�
    bind(sockSrv, (SOCKADDR *) &serverAddr, sizeof(SOCKADDR)); //�󶨷���˵�socket�ʹ���õĵ�ַ
    WSAEVENT servEvent = WSACreateEvent();                    //��������
    WSAEventSelect(sockSrv, servEvent, FD_ALL_EVENTS);        //���¼��������ڼ��������¼�
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