    #include <iostream>
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #include <vector>
    #include <fstream>

    const char* localhost = "127.0.0.1";

    using std::cout, std::cin, std::endl;

    typedef unsigned long long fileSize_t;

    int recvBuf(SOCKET &Client, void *buf, unsigned size) {
        unsigned left = size;
        while (left > 0) {
            unsigned received = recv(Client, reinterpret_cast<char *>(buf) + (size - left), left, 0);
            send(Client, reinterpret_cast<char *>(&received), sizeof(unsigned), 0);
            if (!received) {
                return -1;
            }
            left -= received;
        }
        return 0;
    }

    int main(int argc, char *argv[]) {
        // .\Client.exe 127.0.0.1 4444
        if (argc == 2 && !strcmp(argv[1], "help")) {
            cout << ".\\Server.exe SERVERADDRESS PORT" << endl;
            return 0;
        }

        const char *server = argv[1];
        if (argc == 1) {
            server = localhost;
        }
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

        WSADATA wsData;

        int erStat = WSAStartup(MAKEWORD(2,2), &wsData);
        if ( erStat != 0 ) {
            cout << "Error WinSock version initializaion #";
            cout << WSAGetLastError();
            exit(-1);
        }
        else
            cout << "WinSock initialization is OK" << endl;

        SOCKET ClientSock = socket(AF_INET, SOCK_STREAM, 0);

        if (ClientSock == INVALID_SOCKET) {
            cout << "Error initialization socket # " << WSAGetLastError() << endl;
            closesocket(ClientSock);
            WSACleanup();
            exit(-1);
        }
        else
            cout << "Client socket initialization is OK" << endl;

        in_addr ip_to_num;
        erStat = inet_pton(AF_INET, server, &ip_to_num);
        if (erStat <= 0) {
            cout << "Error in IP translation to special numeric format" << endl;
            exit(-1);
        }

        sockaddr_in servInfo;

        ZeroMemory(&servInfo, sizeof(servInfo));

        servInfo.sin_family = AF_INET;
        servInfo.sin_addr = ip_to_num;	  // Server's IPv4 after inet_pton() function
        servInfo.sin_port = htons(port);

        erStat = connect(ClientSock, (sockaddr*)&servInfo, sizeof(servInfo));

        if (erStat != 0) {
            cout << "Connection to Server is FAILED. Error # " << WSAGetLastError() << endl;
            shutdown(ClientSock, SD_BOTH);
            WSACleanup();
            exit(-1);
        }
        else
            cout << "Connection established SUCCESSFULLY."
                 << endl;


        unsigned size;
        erStat = recvBuf(ClientSock, &size, sizeof(unsigned));
        if (erStat) {
            cout << "Error occurred while receiving filename size" << endl;
            shutdown(ClientSock, SD_BOTH);
            exit(-1);
        }

        std::string filename(size, 0);
        erStat = recvBuf(ClientSock, filename.data(), size);
        if (erStat) {
            cout << "Error occurred while receiving filename" << endl;
            shutdown(ClientSock, SD_BOTH);
            exit(-1);
        }
        cout << "File name: " << filename << endl;

        std::ofstream file;
        file.open(filename, std::ios::binary | std::ios::out);

        fileSize_t fileSize;
        erStat = recvBuf(ClientSock, &fileSize, sizeof(fileSize));
        if (erStat) {
            cout << "Error occurred while receiving file size" << endl;
            shutdown(ClientSock, SD_BOTH);
            file.close();
            exit(-1);
        }
        cout << "File size: " << fileSize << " bytes" << endl;
        unsigned PACKET_SIZE;
        erStat = recvBuf(ClientSock, &PACKET_SIZE, sizeof(PACKET_SIZE));
        if (erStat) {
            cout << "Error occurred while receiving packet size" << endl;
            shutdown(ClientSock, SD_BOTH);
            file.close();
            exit(-1);
        }
        cout << "Packet size: " << PACKET_SIZE << endl;
        unsigned packetNum = fileSize / PACKET_SIZE;
        unsigned leftover = fileSize % PACKET_SIZE;
        char buf[PACKET_SIZE];

        for (int i = 0; i < packetNum; ++i) {
            erStat = recvBuf(ClientSock, buf, PACKET_SIZE);
            if (erStat) {
                cout << "Error occurred while receiving packet #" << i << endl;
                shutdown(ClientSock, SD_BOTH);
                file.close();
                exit(-1);
            }
            file.write(buf, PACKET_SIZE);
        }

        if (leftover) {
            erStat = recvBuf(ClientSock, buf, leftover);
            if (erStat) {
                cout << "Error occurred while receiving last packet" << endl;
                shutdown(ClientSock, SD_BOTH);
                file.close();
                exit(-1);
            }
            file.write(buf, leftover);
        }
        cout << "File received SUCCESSFULLY." << endl;
        file.close();
        shutdown(ClientSock, SD_BOTH);

        exit(0);
    }

