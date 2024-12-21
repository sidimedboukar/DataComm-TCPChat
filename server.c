#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <direct.h>

#define MAXUSER 10
#define MAX_IP_ADDRESS_LENGTH 16
#define MAX_BUFFER_SIZE 256
#define MAX_USERNAME_LENGTH 20
#define MAX_MESSAGE_LENGTH 512

struct User
{
    int clilen;
    struct sockaddr_in cli_addr;
    SOCKET newSockfd;
    char username[MAX_USERNAME_LENGTH];
    char ip_address[MAX_IP_ADDRESS_LENGTH];
};

struct User user[MAXUSER], tmpUser;
int userCount = 0;
char tmpCorrectMsg[MAX_MESSAGE_LENGTH];
CRITICAL_SECTION cs;
struct sockaddr_in serv_addr;

void ErrorHandling(const char *msg)
{
    perror(msg);
    exit(1);
}

char *CorruptMessage(const char *message)
{
    char *cmesg = (char *)malloc(strlen(message) + 1);
    if (cmesg == NULL)
    {
        perror("Error allocating memory");
        exit(1);
    }
    strcpy(cmesg, message);
    cmesg[0] = 'X';
    cmesg[3] = 'X';
    cmesg[5] = 'X';
    return cmesg;
}

// Client Listener Thread
DWORD WINAPI ServiceClient(LPVOID userIP)
{
    int myIP = *(int *)userIP, ret, foundInList = 0, i = 0;
    char buffer[MAX_BUFFER_SIZE], uname[MAX_USERNAME_LENGTH], privateMsg[MAX_MESSAGE_LENGTH], publicMsg[MAX_MESSAGE_LENGTH];
    char fullMessage[MAX_MESSAGE_LENGTH], fullCorruptMessage[MAX_MESSAGE_LENGTH];

    while (1)
    {
        int randomChoice = rand() % 2;
        foundInList = 0;
        memset(buffer, 0, sizeof(buffer));
        // Read and wait for message
        ret = recv(user[myIP].newSockfd, buffer, sizeof(buffer) - 1, 0);
        if (ret <= 0)
        {
            printf("ERROR reading from socket\n");
            return 0; // Terminate the thread
        }

        // Check if the client wants to exit
        if (strcmp(buffer, "exit\n") == 0) {
            printf("User %s is exiting.\n", user[myIP].username);
            closesocket(user[myIP].newSockfd); // Close the client socket

            EnterCriticalSection(&cs);
            strcpy(user[myIP].username, "OFFLINE"); // Mark user as offline
            LeaveCriticalSection(&cs);

            // Notify other clients
            char exitMessage[MAX_MESSAGE_LENGTH];
            for (int i = 0; i < userCount; i++) {
                if (i != myIP) {
                    sprintf(exitMessage, "User %s has left the chat.\n", user[myIP].username);
                    send(user[i].newSockfd, exitMessage, strlen(exitMessage), 0);
                }
            }

            return 0; // End the thread for this client
        }

        // Check if the client wants to list users
        if (strcmp(buffer, "list\n") == 0)
        {
            printf("User %s requested the user list.\n", user[myIP].username);
            // Formulate the user list
            char userlist[MAX_MESSAGE_LENGTH] = "----Userlist----\n";
            for (i = 0; i < userCount; i++)
            {
                strcat(userlist, user[i].username);
                strcat(userlist, " (");
                strcat(userlist, user[i].ip_address); // Append IP address
                strcat(userlist, ")\n");
            }
            strcat(userlist, "----------------\n");
            // Send the user list to the client
            ret = send(user[myIP].newSockfd, userlist, strlen(userlist), 0);
            if (ret < 0)
            {
                printf("ERROR writing to socket\n");
                return 1;
            }

            continue;
        }

        if (strncmp(buffer, "MERR", 4) == 0)
        {
            printf("Received MERR from the client. Resending corrected message");
            for (int i = 4; i > 0; i--)
            {
                Sleep(300);
                printf(".");
            }
            printf("\n");

            // Resend the corrected message to the client
            ret = send(user[myIP].newSockfd, tmpCorrectMsg, strlen(tmpCorrectMsg), 0);
            if (ret < 0)
            {
                printf("ERROR writing to socket\n");
                return 1;
            }
            printf("Corrected message sent successfully.\n");
            continue;
        }

        if (buffer[0] == '#')
        {
            printf("<---Public Message--->\n");
            // Assembly of the message
            memset(publicMsg, 0, sizeof(publicMsg));
            strcpy(publicMsg, "-----------\npublic Message from ");
            strcat(publicMsg, user[myIP].username);
            strcat(publicMsg, ": ");

            char *publicMsgStart = strstr(buffer, "#") + 1;
            strcat(publicMsg, publicMsgStart);

            for (i = 0; i < userCount; i++) {
                if (user[i].newSockfd != INVALID_SOCKET) { // Check valid socket
                    ret = send(user[i].newSockfd, publicMsg, strlen(publicMsg), 0);
                    if (ret < 0) {
                        printf("Error sending to user: %s\n", user[i].username);
                        continue;
                    }
                }
            }
            continue;
        }

        //Private Messages
        if (buffer[0] == '@')
        {
            char tmpbuffer[MAX_MESSAGE_LENGTH], u[MAX_USERNAME_LENGTH], m[MAX_MESSAGE_LENGTH];
            unsigned char cr, ch;

            strcpy(tmpbuffer, buffer);

            printf("<---Private Message--->\n");
            if (sscanf(tmpbuffer, "@%[^ ] %[^-]-", u, m))
            {
                //printf(" Username: %s\n", u);
                //printf(" Message: %s\n", m);
            }
            char *token1 = strtok(tmpbuffer, "-");
            token1 = strtok(NULL, "\n");
            token1[strcspn(token1, "\n")] = '\0';

            char tmpt[MAX_USERNAME_LENGTH];
            if (sscanf(token1, "%2hhX&%2hhX|", &cr, &ch))
            {
                //Tokenize the message using '|'
                char *token2 = strtok(token1, "&");
                char *token3 = strtok(token2, "|");
                int tokenCounter = 0;

                while (token2 != NULL)
                {
                    switch (tokenCounter)
                    {
                    case 0:
                        //CRC
                        sscanf(token2, "%2hhX", &cr);
                        break;
                    case 1:
                        //Checksum
                        sscanf(token2, "%2hhX", &ch);
                        break;
                    }
                    token2 = strtok(NULL, "|");
                    tokenCounter++;
                }
            }

            // Search in users
            for (i = 0; i < userCount; i++)
            {
                memset(uname, 0, sizeof(uname));

                uname[0] = '@';
                strcat(uname, user[i].username);

                // Search name
                if (strstr(buffer, uname))
                {
                    // Assembly of the message
                    memset(privateMsg, 0, sizeof(privateMsg));
                    strcpy(privateMsg, "Message from ");
                    strcat(privateMsg, user[myIP].username);
                    strcat(privateMsg, ": ");

                    // Skip the username in the message
                    strcat(privateMsg, m);

                    // Create log file
                    // Create filename from current date, time, and usernames of sender and recipient
                    time_t rawtime;
                    struct tm *timeinfo;
                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    char filename[100];
                    sprintf(filename, "logs/ChatLog-%s_WITH_%s_ON_%d-%d-%2d_AT_%d-%d.txt",
                            user[myIP].username, user[i].username,
                            timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
                            timeinfo->tm_hour, timeinfo->tm_min);

                    // Create logs directory if it doesn't exist
                    _mkdir("logs");

                    FILE *logfile = fopen(filename, "w");
                    if (logfile == NULL) {
                        printf("Error opening log file: %s\n", filename);
                        return 1;
                    }

                    fprintf(logfile, "%s\n", privateMsg); // Write private message to log
                    fclose(logfile); // Close log file

                    char *cmesg = CorruptMessage(m);
                    char valofMethods[MAX_MESSAGE_LENGTH];

                    snprintf(fullMessage, sizeof(fullMessage), "Message from %s: %s|%02X|%02X|", user[myIP].username, m);
                    fullMessage[strcspn(fullMessage, "\n")] = '\0';

                    snprintf(valofMethods, sizeof(valofMethods), "%02X|%02X|", cr, ch);
                    valofMethods[strcspn(valofMethods, "\n")] = '\0';

                    snprintf(fullCorruptMessage, sizeof(fullCorruptMessage), "Message from %s: %s|", user[myIP].username, cmesg);
                    fullCorruptMessage[strcspn(fullCorruptMessage, "\n")] = '\0';

                    // Writing the message to the respective user
                    if (randomChoice == 0)
                    {
                        ret = send(user[i].newSockfd, fullCorruptMessage, strlen(fullCorruptMessage), 0);
                        if (ret < 0)
                        {
                            printf("ERROR writing to socket\n");
                            continue;
                        }
                    }
                    else
                    {
                        ret = send(user[i].newSockfd, fullMessage, strlen(fullMessage), 0);
                        if (ret < 0)
                        {
                            printf("ERROR writing to socket\n");
                            continue;
                        }
                    }

                    ret = send(user[i].newSockfd, valofMethods, strlen(valofMethods), 0);
                    if (ret < 0)
                    {
                        printf("ERROR writing to socket\n");
                        return 1;
                    }

                    foundInList = 1;
                    free(cmesg);
                    break;
                }
            }

            if (foundInList == 1)
            {
                EnterCriticalSection(&cs);
                strcpy(tmpCorrectMsg, fullMessage);
                LeaveCriticalSection(&cs);
            }

            if (foundInList != 1)
            {
                int inList = 0;
                sscanf(buffer, "@%[^ ] OFFLINE", u);
                for (int i = 0; i < userCount; i++)
                {
                    if (strlen(user[i].username) - 8 == strlen(u))
                    {
                        inList = 1;
                        continue;
                    }
                }
                if (inList == 1)
                {
                    printf("OFFLINE User\n");
                    ret = send(user[myIP].newSockfd, "OFFLINE User\n", 12, 0);
                    if (ret < 0)
                    {
                        printf("ERROR writing to socket\n");
                        return 1;
                    }
                }
                else
                {
                    printf("There is no user with that name\n");
                    ret = send(user[myIP].newSockfd, "There is no user with that name\n", 32, 0);
                    if (ret < 0)
                    {
                        printf("ERROR writing to socket\n");
                        return 1;
                    }
                }
            }
            continue;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    WSADATA wsa;
    SOCKET sockfd, newSockfd;
    int portno, i;
    int clilen;
    char buffer[MAX_BUFFER_SIZE], notice[MAX_MESSAGE_LENGTH];
    int ret;
    int userIP[MAXUSER];

    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Failed. Error Code: %d", WSAGetLastError());
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
        ErrorHandling("ERROR opening socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
        ErrorHandling("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(tmpUser.cli_addr);

    printf("\n----- Server is running -----\n\n");
    InitializeCriticalSection(&cs);

    while (1)
    {
        if (userCount < MAXUSER)
        {
            newSockfd = accept(sockfd, (struct sockaddr *)&tmpUser.cli_addr, &clilen);
            if (newSockfd == INVALID_SOCKET)
                ErrorHandling("ERROR on accept");

            strcpy(tmpUser.ip_address, inet_ntoa(tmpUser.cli_addr.sin_addr));
            sprintf(tmpUser.ip_address, "%s:%d", inet_ntoa(tmpUser.cli_addr.sin_addr), userCount);

            // Read username
            memset(buffer, 0, sizeof(buffer));
            ret = recv(newSockfd, buffer, sizeof(buffer) - 1, 0);
            if (ret < 0)
            {
                printf("ERROR reading from socket\n");
                closesocket(newSockfd);
                continue;
            }

            strcpy(tmpUser.username, buffer);
            tmpUser.newSockfd = newSockfd;
            tmpUser.clilen = clilen;
            tmpUser.cli_addr = tmpUser.cli_addr;

            user[userCount] = tmpUser;

            printf("CONN: %s (%s)\n", user[userCount].username, user[userCount].ip_address);

            // Send userlist and commands to the new user
            char userlist[MAX_MESSAGE_LENGTH] = "----Userlist----\n";
            if (userCount == 0)
            {
                char first_client[] = "No one is online but you\n";
                strcat(userlist, first_client);
            }
            else
            {
                for (int i = 0; i < userCount; i++)
                {
                    strcat(userlist, user[i].username);
                    strcat(userlist, " (");
                    strcat(userlist, user[i].ip_address); // Append IP address
                    strcat(userlist, ")\n");
                }
            }
            strcat(userlist, "----------------\n");

            for (int i = 0; i < userCount; i++)
            {
                sprintf(notice, "User %s has joined the server.\n", user[userCount].username);
                send(user[i].newSockfd, notice, strlen(notice), 0);
            }

            // Send userlist to the client
            send(newSockfd, userlist, strlen(userlist), 0);

            // Send commands to the client
            char commands[] = "Available commands:\n\'list\' - Display userlist\n\'exit\' - Disconnect from the server\n- start with \'@\'if you want to send a private message\n- start with \'#\'if you want to send a public message\n";
            send(newSockfd, commands, strlen(commands), 0);

            userIP[userCount] = userCount;

            HANDLE hThread;
            DWORD dwThreadIdArray[MAXUSER];

            hThread = CreateThread(
                          NULL,                   // default security attributes
                          0,                      // use default stack size
                          ServiceClient,          // thread function name
                          &userIP[userCount],     // argument to thread function
                          0,                      // use default creation flags
                          &dwThreadIdArray[userCount]); // returns the thread identifier

            if (hThread == NULL)
            {
                fprintf(stderr, "CreateThread failed (%lu)\n", GetLastError());
                DeleteCriticalSection(&cs);
                return 1;
            }
            userCount++;
        }
    }

    // Cleanup Winsock
    closesocket(sockfd);
    DeleteCriticalSection(&cs);
    WSACleanup();
    return 0;
}
