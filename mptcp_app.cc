/**
 * MPTCP experiments: server/client application.
 *
 * Shawn Chen <shuoshuc@cs.cmu.edu>
 */

#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>

// Special protocol value representing MPTCP.
const int IPPROTO_MPTCP = 262;

// Server side port id.
const int kPORT = 9100;

// Size of a single chunk to send.
const int kCHUNKSIZE = 1024;

// Prints the usage for this program then returns failure.
void printHelpAndExit() {
  std::cout << R"(mptcp_app usage:
  If running in server mode: ./mptcp_app server
  If running in client mode: ./mptcp_app client [server IP])"
            << std::endl;
  std::exit(EXIT_FAILURE);
}

// Prints the error code then returns failure.
void printErrorAndExit(std::string err) {
  std::perror(err.c_str());
  std::exit(EXIT_FAILURE);
}

int sendAll(int socket, std::vector<char> &buf) {
  int bytes_sent = 0, nbytes = 0;
  size_t bytes_left = buf.size();
  while (bytes_sent < static_cast<int>(buf.size())) {
    nbytes = send(socket, buf.data() + bytes_sent, bytes_left, 0);
    if (nbytes < 0) {
      printErrorAndExit("mptcp_client send()");
    }
    bytes_sent += nbytes;
    bytes_left -= nbytes;
  }

  return bytes_sent;
}

void receiveFromClient(int conn) {
  // Allocates a receive buffer that is twice the size of a chunk in case of
  // network delay or queueing.
  char recvbuf[2 * kCHUNKSIZE];
  while (true) {
    int nbytes = read(conn, recvbuf, sizeof(recvbuf));
    if (nbytes < 0) {
      printErrorAndExit("mptcp_server read()");
    }
    auto now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << ctime(&now) << ": mptcp_server received " << nbytes
              << " bytes." << std::endl;
  }
  // Close connection on exit.
  close(conn);
}

// mptcp client that sends chunks of data to mptcp server with fixed intervals.
int mptcp_client(std::string ip_addr) {
  struct addrinfo *addr;
  int s =
      getaddrinfo(ip_addr.c_str(), std::to_string(kPORT).c_str(), NULL, &addr);
  if (s != 0) {
    printErrorAndExit("mptcp_client getaddrinfo: " +
                      std::string(gai_strerror(s)));
  }
  int sfd = socket(addr->ai_family, addr->ai_socktype, IPPROTO_MPTCP);
  if (sfd < 0) {
    printErrorAndExit("mptcp_client socket()");
  }
  if (connect(sfd, addr->ai_addr, addr->ai_addrlen) != 0) {
    printErrorAndExit("mptcp_client connect()");
  }

  // Initializes a vector buffer of char, all filled with 'A'.
  std::vector<char> buf(kCHUNKSIZE, 'A');
  while (true) {
    if (sendAll(sfd, buf) != static_cast<int>(buf.size())) {
      printErrorAndExit("mptcp_client sendAll()");
    }
    auto now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << ctime(&now) << ": mptcp_client sent " << buf.size()
              << " bytes." << std::endl;
    // Sleeps for 1 sec.
    sleep(1);
  }

  return 0;
}

// mptcp server that accepts connection from mptcp client and receives data sent
// over.
int mptcp_server() {
  // Opens a socket for IPv4 Multipath TCP.
  int sfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_MPTCP);
  if (sfd < 0) {
    printErrorAndExit("mptcp_server socket");
  }

  struct sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sa));
  // socket type IPv4.
  sa.sin_family = AF_INET;
  // binds to fixed port kPORT.
  sa.sin_port = htons(kPORT);
  if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    printErrorAndExit("mptcp_server bind");
  }
  // Allows up to 5 backlog connections. Connections after 5 will be refused.
  if (listen(sfd, 5) != 0) {
    printErrorAndExit("mptcp_server listen");
  }

  // Loop to accept *one* new connection and receive from the client. New
  // connection is not accepted unless a previous one is closed or none exists.
  while (true) {
    int conn = accept(sfd, NULL, NULL);
    if (conn < 0) {
      printErrorAndExit("mptcp_server accept");
    }
    receiveFromClient(conn);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  // argument vector size should be at least 2 if server mode.
  if (argc == 2 && std::string(argv[1]) == "server") {
    return mptcp_server();
  }
  // argument vector size should be at least 3 if client mode.
  if (argc == 3 && std::string(argv[1]) == "client") {
    // pass the last argument, IP addr to client.
    return mptcp_client(std::string(argv[2]));
  }

  // If mode is not server or client, print out the usage and exit.
  printHelpAndExit();

  return 0;
}
