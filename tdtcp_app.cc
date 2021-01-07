/**
 * TDTCP server/client application.
 *
 * Shawn Chen <shuoshuc@cs.cmu.edu>
 */

#include "icmp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// A newly introduced setsockopt field on SOL_TCP. This avoids installing header
// files from the specific custom kernel.
#define TCP_CURR_TDN_ID 38

// Server side port id.
const int kPORT = 9100;

// Size of a single chunk to send.
const int kCHUNKSIZE = 1024;

// ICMP TDN change interval in seconds.
const int kICMPInterval = 3;

// Prints the usage for this program then returns failure.
void printHelpAndExit() {
  std::cout << R"(tdtcp_app usage:
  If running in server mode: ./tdtcp_app server [client IP]
  If running in client mode: ./tdtcp_app client [server IP])"
            << std::endl;
  std::exit(EXIT_FAILURE);
}

// Prints the error code then returns failure.
void printErrorAndExit(std::string err) {
  std::perror(err.c_str());
  std::exit(EXIT_FAILURE);
}

// Prints the error code but does not exit.
void printError(std::string err) {
  std::perror(err.c_str());
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

uint16_t icmp_checksum(const void* data, size_t len) {
  auto p = reinterpret_cast<const uint16_t*>(data);
  uint32_t sum = 0;
  if (len & 1) {
    // len is odd
    sum = reinterpret_cast<const uint8_t*>(p)[len - 1];
  }
  len /= 2;
  while (len--) {
    sum += *p++;
    if (sum & 0xffff0000) {
      sum = (sum >> 16) + (sum & 0xffff);
    }
  }
  return static_cast<uint16_t>(~sum);
}

void icmp_change_tdn(std::string client_addr, uint8_t tdn_id) {
  // Opens a raw socket for sending ICMP to peer.
  int icmp_sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (icmp_sk < 0) {
    printErrorAndExit("icmp_change_tdn() ICMP socket");
  }

  struct icmphdr icmph;
  memset(&icmph, 0, sizeof(icmph));
  const size_t icmph_size = 8;
  icmph.type = ICMP_ACTIVE_TDN_ID;
  icmph.code = 0;
  icmph.checksum = 0;
  icmph.un.active_tdn.id = tdn_id;
  icmph.checksum = icmp_checksum(&icmph, icmph_size);

  struct sockaddr_in dest_addr;
  // Addr family must be the same as what is specified in the socket.
  dest_addr.sin_family = AF_INET;
  // We don't care about port number since this is an ICMP packet.
  dest_addr.sin_port = 0;
  dest_addr.sin_addr.s_addr = inet_addr(client_addr.c_str());
  if (sendto(icmp_sk, &icmph, icmph_size, 0, (struct sockaddr*)&dest_addr,
             sizeof(dest_addr)) < 0) {
    printError("icmp_change_tdn() ICMP sendto()");
  }
  close(icmp_sk);
}

// Sleeps and sends out ICMP packets periodically.
void icmp_timer(std::string client_addr) {
  uint8_t tdn_id = 1;

  while (true) {
    // Sleeps for 2 sec.
    sleep(kICMPInterval);

    // TDN ID alternates between 0 and 1.
    tdn_id = 1 - tdn_id;
    // Send ICMP to peer to change TDN ID.
    icmp_change_tdn(client_addr, tdn_id);
    auto now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&now), "%D %T %Z")
              << ": sent ICMP ACTIVE_TDN_ID=" << static_cast<int>(tdn_id)
              << " to " << client_addr << std::endl;
  }
}

void receiveFromClient(int conn, std::string client_addr) {
  // Allocates a receive buffer that is twice the size of a chunk in case of
  // network delay or queueing.
  char recvbuf[kCHUNKSIZE];
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
    close(sfd);
    printErrorAndExit("tdtcp_client connect()");
  }

  // Initializes a vector buffer of char, all filled with 'A'.
  std::vector<char> buf(kCHUNKSIZE, 'A');
  while (true) {
    if (sendAll(sfd, buf) != static_cast<int>(buf.size())) {
      close(sfd);
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
int tdtcp_server(std::string client_addr) {
  // Opens a socket for IPv4 TDTCP.
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
    close(sfd);
    printErrorAndExit("tdtcp_server bind");
  }
  // Allows up to 5 backlog connections. Connections after 5 will be refused.
  if (listen(sfd, 5) != 0) {
    close(sfd);
    printErrorAndExit("tdtcp_server listen");
  }

  // Loop to accept *one* new connection and receive from the client. New
  // connection is not accepted unless a previous one is closed or none exists.
  while (true) {
    int conn = accept(sfd, NULL, NULL);
    if (conn < 0) {
      printErrorAndExit("tdtcp_server accept");
    }
    receiveFromClient(conn, client_addr);
    // Close connection on exit.
    close(conn);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  // argument vector size should be at least 3 if server mode.
  if (argc == 3 && std::string(argv[1]) == "server") {
    std::thread t(icmp_timer, std::string(argv[2]));
    tdtcp_server(std::string(argv[2]));
    t.join();
    return 0;
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
