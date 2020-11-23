/**
 * TDTCP server/client application.
 *
 * Shawn Chen <shuoshuc@cs.cmu.edu>
 */

#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>

// A newly introduced setsockopt field on SOL_TCP. This avoids installing header
// files from the specific custom kernel.
#define TCP_CURR_TDN_ID 38

// Server side port id.
const int kPORT = 9100;

// Size of a single chunk to send.
const int kCHUNKSIZE = 1024;

// Prints the usage for this program then returns failure.
void printHelpAndExit() {
  std::cout << R"(tdtcp_app usage:
  If running in server mode: ./tdtcp_app server
  If running in client mode: ./tdtcp_app client [server IP])"
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
      printErrorAndExit("tdtcp_client send()");
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
      printErrorAndExit("tdtcp_server read()");
    }
    // 0 means EOF, break out the loop and close server side connection.
    if (nbytes == 0) {
      std::cout << "client side closed connection." << std::endl;
      break;
    }
    auto now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&now), "%D %T %Z")
              << ": tdtcp_server received " << nbytes << " bytes." << std::endl;
  }
  // Close connection on exit.
  close(conn);
}

// tdtcp client that sends chunks of data to tdtcp server with fixed intervals.
int tdtcp_client(std::string ip_addr) {
  struct addrinfo *addr;
  int s =
      getaddrinfo(ip_addr.c_str(), std::to_string(kPORT).c_str(), NULL, &addr);
  if (s != 0) {
    printErrorAndExit("tdtcp_client getaddrinfo: " +
                      std::string(gai_strerror(s)));
  }
  int sfd = socket(addr->ai_family, addr->ai_socktype, 0);
  if (sfd < 0) {
    printErrorAndExit("tdtcp_client socket()");
  }
  if (connect(sfd, addr->ai_addr, addr->ai_addrlen) != 0) {
    printErrorAndExit("tdtcp_client connect()");
  }

  // Initializes a vector buffer of char, all filled with 'A'.
  std::vector<char> buf(kCHUNKSIZE, 'A');
  int tdn = 0;
  while (true) {
    // TDN ID alternates between 0 and 1.
    tdn = tdn % 2;
    int err = setsockopt(sfd, SOL_TCP, TCP_CURR_TDN_ID, &tdn, sizeof(tdn));
    tdn++;

    if (sendAll(sfd, buf) != static_cast<int>(buf.size())) {
      printErrorAndExit("tdtcp_client sendAll()");
    }
    auto now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&now), "%D %T %Z")
              << ": tdtcp_client sent " << buf.size() << " bytes." << std::endl;
    // Sleeps for 1 sec.
    sleep(1);
  }

  return 0;
}

// tdtcp server that accepts connection from tdtcp client and receives data sent
// over.
int tdtcp_server() {
  // Opens a socket for IPv4 Multipath TCP.
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    printErrorAndExit("tdtcp_server socket");
  }

  struct sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sa));
  // socket type IPv4.
  sa.sin_family = AF_INET;
  // binds to fixed port kPORT.
  sa.sin_port = htons(kPORT);
  if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    printErrorAndExit("tdtcp_server bind");
  }
  // Allows up to 5 backlog connections. Connections after 5 will be refused.
  if (listen(sfd, 5) != 0) {
    printErrorAndExit("tdtcp_server listen");
  }

  // Loop to accept *one* new connection and receive from the client. New
  // connection is not accepted unless a previous one is closed or none exists.
  while (true) {
    int conn = accept(sfd, NULL, NULL);
    if (conn < 0) {
      printErrorAndExit("tdtcp_server accept");
    }
    receiveFromClient(conn);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  // argument vector size should be at least 2 if server mode.
  if (argc == 2 && std::string(argv[1]) == "server") {
    return tdtcp_server();
  }
  // argument vector size should be at least 3 if client mode.
  if (argc == 3 && std::string(argv[1]) == "client") {
    // pass the last argument, IP addr to client.
    return tdtcp_client(std::string(argv[2]));
  }

  // If mode is not server or client, print out the usage and exit.
  printHelpAndExit();

  return 0;
}
