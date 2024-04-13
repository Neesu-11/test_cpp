#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include <unordered_map>
std::unordered_map<std::string, std::vector<int>> chatrooms;

const int BUFFER_SIZE = 1024;
const int MAX_CLIENTS = 10;
const int PORT = 8888;

std::vector<int> clientSockets(MAX_CLIENTS, 0);

void handleChatroomSelection(int clientSocket, int clientIndex) {
    // Send list of available chatrooms to the client
    std::string chatroomList;
    for (const auto& chatroom : chatrooms) {
        chatroomList += chatroom.first + ";";
    }
    send(clientSocket, chatroomList.c_str(), chatroomList.length(), 0);

    // Receive selected chatroom from client
    char selectedChatroom[BUFFER_SIZE];
    recv(clientSocket, selectedChatroom, BUFFER_SIZE, 0);
    std::string selectedChatroomName(selectedChatroom);

    // Add client to the selected chatroom
    chatrooms[selectedChatroomName].push_back(clientIndex);

    std::cout << "Client " << clientIndex << " joined chatroom: " << selectedChatroomName << std::endl;

    // Handle chatroom messages
    handleChatroomMessages(clientSocket, clientIndex, selectedChatroomName);
}
void handleChatroomMessages(int clientSocket, int clientIndex, const std::string& chatroomName) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cout << "Client " << clientIndex << " disconnected from chatroom " << chatroomName << std::endl;
            // Remove client from the chatroom
            chatrooms[chatroomName].erase(std::remove(chatrooms[chatroomName].begin(), chatrooms[chatroomName].end(), clientIndex), chatrooms[chatroomName].end());
            break;
        }

        buffer[bytesReceived] = '\0';
        std::cout << "Client " << clientIndex << " in chatroom " << chatroomName << " says: " << buffer << std::endl;

        // Broadcast the message to all clients in the chatroom
        for (int i : chatrooms[chatroomName]) {
            if (i != clientIndex) {
                send(clientSockets[i], buffer, bytesReceived, 0);
            }
        }
    }
}


void handleClient(int clientSocket, int clientIndex) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cout << "Client " << clientIndex << " disconnected.\n";
            close(clientSocket);
            clientSockets[clientIndex] = 0;
            break;
        }

        buffer[bytesReceived] = '\0';
        std::cout << "Client " << clientIndex << " says: " << buffer << std::endl;

        // Broadcast the message to all clients except the sender
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clientSockets[i] != 0 && i != clientIndex) {
                send(clientSockets[i], buffer, bytesReceived, 0);
            }
        }
    }
}

std::unordered_map<std::string, std::string> userCredentials = {
    {"user1", "password1"},
    {"user2", "password2"}
};

// Function to authenticate users
bool authenticateUser(const std::string& username, const std::string& password) {
    auto it = userCredentials.find(username);
    if (it != userCredentials.end() && it->second == password) {
        return true;  // Authentication successful
    }
    return false;     // Authentication failed
}

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error: Could not create socket\n";
        return EXIT_FAILURE;
    }

    // Prepare the sockaddr_in structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error: Bind failed\n";
        return EXIT_FAILURE;
    }

    // Listen
    if (listen(serverSocket, MAX_CLIENTS) == -1) {
        std::cerr << "Error: Listen failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Server started. Waiting for connections...\n";

    int clientIndex = 0;

    while (true) {
    // Accept connection from an incoming client
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket == -1) {
        std::cerr << "Error: Accept failed\n";
        continue;
    }

        std::cout << "New connection established. Client IP: " << inet_ntoa(clientAddr.sin_addr)
                  << ", Port: " << ntohs(clientAddr.sin_port) << std::endl;
        
    

        // Authenticate the client
        char credentials[BUFFER_SIZE];
        recv(clientSocket, credentials, BUFFER_SIZE, 0);

        std::string credentialsStr(credentials);
        size_t delimiterPos = credentialsStr.find(':');
        std::string username = credentialsStr.substr(0, delimiterPos);
        std::string password = credentialsStr.substr(delimiterPos + 1);

        if (!authenticateUser(username, password)) {
            std::cerr << "Authentication failed for " << username << std::endl;
            close(clientSocket);
            continue;
        }


        std::cout << "User " << username << " authenticated successfully.\n";
        // Authentication successful, send authentication result to the client
        send(clientSocket, "authenticated", strlen("authenticated"), 0);



        // Add client socket to the vectorc
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clientSockets[i] == 0) {
                clientSockets[i] = clientSocket;
                clientIndex = i;
                break;
            }
        }

        // Create thread to handle client
        std::thread clientThread(handleClient, clientSocket, clientIndex);
        clientThread.detach(); // Detach the thread to allow it to run independently
        
        std::thread chatroomSelectionThread(handleChatroomSelection, clientSocket, clientIndex);
        chatroomSelectionThread.detach();
       
    }

    close(serverSocket);
    return EXIT_SUCCESS;
}
