#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <vector>

const char* localhost = "127.0.0.1";

const unsigned PACKET_SIZE = 4096;

using std::cout, std::cin, std::endl;

typedef unsigned long long fileSize_t;

int sendBuf(SOCKET &Client, const void *buf, unsigned size) {
    int left = size;
    while(left > 0) {
        send(Client, reinterpret_cast<const char*>(buf) + (size - left), left, 0);
        unsigned sent;
        int erStat = recv(Client, reinterpret_cast<char *>(&sent), sizeof(unsigned), 0);
        if (!sent || !erStat) {
            return 1;
        }
        left -= sent;
    }
    return 0;
}

void sendFile(SOCKET Client, std::string filepath) {
    std::string errorFileName = "err";
    errorFileName += std::to_string(time(NULL)) + ".txt";
    std::ofstream errors(errorFileName.c_str());

    std::ifstream file;
    file.open(filepath, std::ios::binary | std::ios::in);
    if (!file) {
        errors << "Error occurred while opening file" << endl;
        shutdown(Client, SD_BOTH);
        return;
    }

    int name_begin = filepath.rfind("\\") + 1;
    std::string filename = filepath.substr(name_begin, filepath.size() - name_begin);

    unsigned size = filename.size();
    int erStat = sendBuf(Client, &size, sizeof(size));
    erStat += sendBuf(Client, filename.c_str(), size);
    if (erStat) {
        errors << "Error occurred while sending filename" << endl;
        shutdown(Client, SD_BOTH);
        file.close();
        return;
    }

    file.seekg (0, std::ios::end);
    fileSize_t fileSize = file.tellg();
    file.seekg (0, std::ios::beg);

    erStat = sendBuf(Client, &fileSize, sizeof(fileSize));
    if (erStat) {
        errors << "Error occurred while sending file size" << endl;
        shutdown(Client, SD_BOTH);
        file.close();
        return;
    }
    erStat = sendBuf(Client, &PACKET_SIZE, sizeof(PACKET_SIZE));
    if (erStat) {
        errors << "Error occurred while sending packet size" << endl;
        shutdown(Client, SD_BOTH);
        file.close();
        return;
    }

    unsigned packetNum = fileSize / PACKET_SIZE;
    unsigned leftover = fileSize % PACKET_SIZE;
    char buf[PACKET_SIZE];
    for (int i = 0; i < packetNum; ++i) {
        file.read(buf, PACKET_SIZE);
        erStat = sendBuf(Client, buf, PACKET_SIZE);
        if (erStat) {
            errors << "Error occurred while sending packet #" << i << endl;
            shutdown(Client, SD_BOTH);
            file.close();
            return;
        }
    }
    if (leftover) {
        file.read(buf, leftover);
        erStat = sendBuf(Client, buf, leftover);
        if (erStat) {
            errors << "Error occurred while sending last packet" << endl;
            shutdown(Client, SD_BOTH);
            file.close();
            return;
        }
    }

    errors.close();
    remove(errorFileName.c_str());

    shutdown(Client, SD_BOTH);
    file.close();
}

int main(int argc, const char *argv[]) {
    // .\Server.exe "file.txt" PORT
    if (argc == 2 && !strcmp(argv[1], "help")) {
        cout << ".\\Server.exe FILEPATH PORT" << endl;
        return 0;
    }

    const char *&filename = argv[1];
    if (argc == 1) {
        cout << "Enter file name" << endl;
        exit(-1);
    }

    std::ifstream file;
    file.open(filename, std::ios::binary | std::ios::in);
    if (!file) {
        cout << "Wrong file name" << endl;
        exit(-1);
    }
    file.close();

    unsigned long port = 4444;
    if (argc > 2) {
        char *eptr = nullptr;
        errno = 0;
        port = strtoul(argv[2], &eptr, 10);
        if (errno || *eptr || eptr == argv[2] || (unsigned) port != port) {
            cout << "Wrong port number" << endl;
            exit(-1);
        }
    }

    int erStat = WSAStartup(MAKEWORD(2,2), nullptr);
    if ( erStat != 0 ) {
        cout << "Error WinSock version initializaion #";
        cout << WSAGetLastError();
        exit(-1);
    }
    else
        cout << "WinSock initialization is OK" << endl;

    SOCKET ServSock = socket(AF_INET, SOCK_STREAM, 0);

    if (ServSock == INVALID_SOCKET) {
        cout << "Error initialization socket # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        exit(-1);
    }
    else
        cout << "Server socket initialization is OK" << endl;

    in_addr ip_to_num;
    erStat = inet_pton(AF_INET, localhost, &ip_to_num);
    if (erStat <= 0) {
        cout << "Error in IP translation to special numeric format" << endl;
        exit(-1);
    }

    sockaddr_in servInfo;
    ZeroMemory(&servInfo, sizeof(servInfo));

    servInfo.sin_family = PF_INET;
    servInfo.sin_addr = ip_to_num;
    servInfo.sin_port = htons(port);

    erStat = bind(ServSock, (sockaddr*)&servInfo, sizeof(servInfo));
    if ( erStat != 0 ) {
        cout << "Error Socket binding to Server info. Error # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        exit(-1);
    }
    else
        cout << "Binding socket to Server info is OK" << endl;

    erStat = listen(ServSock, SOMAXCONN);

    if ( erStat != 0 ) {
        cout << "Can't start to listen to. Error # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        exit(-1);
    }
    else {
        cout << "Listening..." << endl;
    }

    while (true) {
        int clientInfo_size = sizeof(sockaddr_in);

        SOCKET ClientConn = accept(ServSock,nullptr, &clientInfo_size);

        if (ClientConn == INVALID_SOCKET) {
            cout << "Client detected, but can't connect to a client. Error # " << WSAGetLastError() << endl;
            continue;
        } else
            cout << "Connection to a client established successfully" << endl;

        std::string file = {filename};
        std::jthread thr;
        thr = std::jthread(sendFile, ClientConn, file);
        // jthread(func, args...)
        thr.detach();
    }

    closesocket(ServSock);

    exit(0);
}

