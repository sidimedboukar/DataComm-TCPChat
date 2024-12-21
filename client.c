#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

unsigned char CRC8Table[256];
void generateCRC8Table()
{
    // Generate CRC8 lookup table
    for (int i = 0; i < 256; i++)
    {
        unsigned char crc = i;
        for (int j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x80) ? 0x07 : 0);
        }
        CRC8Table[i] = crc;
    }
}

unsigned char calculateCRC8(const char *data, size_t len)
{
    unsigned char crc = 0;
    for (size_t i = 0; i < len; i++)
    {
        crc = CRC8Table[crc ^ data[i]];
    }
    return crc;
}

unsigned char calculateChecksum(const char *data, size_t len)
{
    unsigned char checksum = 0;
    for (size_t i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

void ErrorHandling(const char *msg)
{
    perror(msg);
    return;
}

// Thread function to receive and process messages
DWORD WINAPI ReceiveChat(LPVOID lpParam)
{
    SOCKET sockfd = *((SOCKET *)lpParam);
    int ret,CorruptMessageFlag;
    char buffer[256];
    char username[256], message[256], tmpmessage[256];
    unsigned char checksum, crc, tmpchecksum, tmpcrc;
    int waitingForCorrection;

    printf("\n<---Connected--->\n");

    unsigned char CRC8Table[256];
    generateCRC8Table(CRC8Table);

    // Infinite reading from the server
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        ret = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (ret <= 0)
        {
            printf("ERROR reading from socket\n");
            closesocket(sockfd);
            break;
        }
        if (waitingForCorrection)
        {
            // Check if the server is sending the corrected message
            buffer[strcspn(buffer, "|")] = '\0';
            printf("-----------------\nReceived corrected message from server...\n%s\n", buffer);
            CorruptMessageFlag = 0;
            waitingForCorrection = 0;
            continue;
        }

        if (sscanf(buffer, "Message from %[^:]: %256[^|]|", username, message) && CorruptMessageFlag == 0 )
        {
            printf("-----------------\nReceived message from %s: %s\n", username, message);
            continue;
        }

        if (sscanf(buffer, "%02hhX|%02hhX|", &crc, &checksum))
        {
            //Tokenize the message using '|'
            char *token = strtok(buffer, "|");
            int tokenCounter = 0;

            while (token != NULL)
            {
                switch (tokenCounter)
                {
                case 0:
                    //CRC
                    sscanf(token, "%2hhX", &crc);
                    sscanf(token, "%2hhX", &tmpcrc);
                    break;
                case 1:
                    //Checksum
                    sscanf(token, "%2hhX", &checksum);
                    sscanf(token, "%2hhX", &tmpchecksum);
                    break;
                }
                token = strtok(NULL, "|");
                tokenCounter++;
            }
            memset(buffer, 0, sizeof(buffer));

            // Uncomment the following lines for debugging purposes
            // printf("checksum for correct have to be: %02X\n", tmpchecksum);
            // printf("CRC for correct have to be: %02X\n", tmpcrc);

            unsigned char checksum_message = calculateChecksum(message, strlen(message));
            //generateCRC8Table();
            unsigned char crc_message = calculateCRC8(message, strlen(message));

            // printf("Receved Corrupt message checksum is: %02X\n", checksum_message);
            // printf("Receved Corrupt message CRC is: %02X\n", crc_message);

            if (checksum_message != tmpchecksum || crc_message != tmpcrc)
            {
                CorruptMessageFlag =1;
            }
        }

        if (CorruptMessageFlag == 1)
        {
            // Send "MERR" to the server
            printf("***Corrupt message***\n");
            printf("MERR - Witing corrected message from server again");
            for(int i=4; i>0; i--)
            {
                Sleep(300);
                printf(". ");
            }
            printf("\n");

            CorruptMessageFlag =0;
            waitingForCorrection = 1;
            ret = send(sockfd, "MERR", 4, 0);
            if (ret < 0)
            {
                printf("ERROR writing to socket\n");
                break;
            }
            continue;
        }
        else
        {
            printf("%s\n", buffer);
            continue;
        }
    }
    return 0;
}


int main(int argc, char *argv[])
{
    WSADATA wsa;
    SOCKET sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256],uname[20];

    HANDLE hThread;

    if (argc < 4)
    {
        fprintf(stderr, "usage %s hostname port username\n", argv[0]);
        return 1;
    }

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Failed. Error Code: %d", WSAGetLastError());
        return 1;
    }

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
        ErrorHandling("ERROR opening socket");

    // Get the server's IP address
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    if (strcmp(buffer, "exit\n") == 0) {
        printf("Exiting chat...\n");
        send(sockfd, buffer, strlen(buffer), 0); // Notify the server
        closesocket(sockfd); // Close the socket
        WSACleanup();        // Clean up Winsock
        return 0;            // Exit the client program
    }

    // Set up server address struct
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
        ErrorHandling("ERROR connecting");

    // Register @server
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, argv[3]);

    // send name
    int ret = send(sockfd, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));

    // Start Listener Thread
    hThread = CreateThread(
                  NULL,         // default security attributes
                  0,            // use default stack size
                  ReceiveChat,  // thread function name
                  &sockfd,      // argument to thread function
                  0,            // use default creation flags
                  NULL);        // returns the thread identifier

    if (hThread == NULL)
    {
        fprintf(stderr, "CreateThread failed (%lu)\n", GetLastError());
        return 1;
    }

    unsigned char CRC8Table[256];
    generateCRC8Table(CRC8Table);

    while (1)
    {
        // Write messages endlessly
        if (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            if (buffer[0] != '\n')
            {
                if(buffer[0] == '@')
                {
                    //Skip the message
                    char *message_start;

                    if(strstr(buffer," ")==0) continue;
                    else message_start = strstr(buffer," ");

                    message_start[strcspn(message_start, "\n")] = '\0';
                    message_start++;

                    unsigned char checksum_m = calculateChecksum(message_start, strlen(message_start));
                    //generateCRC8Table();
                    unsigned char crc_m = calculateCRC8(message_start, strlen(message_start));

                    char checkmethod[10];
                    snprintf(checkmethod, sizeof(checkmethod), "-%02X&%02X|",crc_m,checksum_m);
                    checkmethod[strcspn(checkmethod, "\n")] = '\0';
                    strcat(buffer,checkmethod);

                    buffer[strcspn(buffer, "\n")] = '\0';
                    //printf("\n%s\n",buffer);
                }
                ret = send(sockfd, buffer, strlen(buffer), 0);
                if (ret < 0)
                {
                    printf("ERROR writing to socket\n");
                    break;
                }
            }
        }
        memset(buffer, 0, sizeof(buffer));
    }

    // Cleanup Winsock
    closesocket(sockfd);
    WSACleanup();

    return 0;
}
