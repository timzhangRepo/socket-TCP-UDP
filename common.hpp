#include <cctype>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <iterator>
#include <pthread.h>
#include <algorithm>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unordered_map>
#include <unordered_set>
using namespace std;

#define ID 915
#define MAX_LEN 1024
#define SERVER_A_PORT (30000 + ID)
#define SERVER_B_PORT (31000 + ID)
#define SERVER_MAIN_UDP_PORT (32000 + ID)
#define SERVER_MAIN_TCP_PORT (33000 + ID)

#define DATA1_FILE "data1.txt"
#define DATA2_FILE "data2.txt"

void pexit(const char *errmsg) {
    perror(errmsg);
    exit(EXIT_FAILURE);
}

using Graph = unordered_map<int, unordered_set<int>>;
using ContryGraph = unordered_map<string, Graph>;

ContryGraph read_graph(const string& file) {
    ContryGraph cg;

    ifstream fin(file);
    string country;
    string line;

    Graph g;
    while (getline(fin, line)) {
        if (isalpha(line[0])) {
            if (!g.empty()) cg[country] = g;
            g.clear();
            country = line;
        } else {
            istringstream iss(line);
            vector<int> ps(istream_iterator<int>{iss}, istream_iterator<int>());
            for (int i = 1; i < (int)ps.size(); ++i) {
                g[ps.front()].insert(ps[i]);
                g[ps[i]].insert(ps.front());
            }
        }
    }

    if (!g.empty()) cg[country] = g;

    return cg;
}

int intersect_size(const unordered_set<int>& a, const unordered_set<int> &b) {
    int tot = 0;
    for (auto i: a) {
        if (b.count(i)) ++tot;
    }
    return tot;
}

int recommendation(Graph& g, int u) {
    // all connected
    if (g[u].size() + 1 == g.size()) return -1;

    int max_common = -1;
    int max_node = -1;

    for (auto& p: g) {
        int n = p.first;
        // connected
        if (n == u || g[u].count(n)) continue;
        int common_count = intersect_size(g[u], g[n]);
        if (common_count > max_common || (common_count == max_common && n < max_node)) {
            max_common = common_count;
            max_node = n;
        }
    }

    if (max_common > 0) return max_node;

    int max_degree = -1;
    max_node = -1;
    for (auto& p: g) {
        int n = p.first;
        if (n == u || g[u].count(n)) continue;

        int degree = (int)p.second.size();
        if (degree > max_degree || (degree == max_degree && n < max_node)) {
            max_degree = degree;
            max_node = n;
        }
    }

    return max_node;
}

enum class SType : char {
    CLIENT,
    SERVER,
};

struct Socket {
    string name;
    int fd;
    bool debug;
    struct sockaddr_in sock;

    Socket() { }

    Socket(const string &name, int port) : name(name), fd(-1), debug(false) {
        bzero(&sock, sizeof(struct sockaddr_in));
        sock.sin_family = AF_INET;
        sock.sin_addr.s_addr = htonl(INADDR_ANY);
        sock.sin_port = htons(port);

        debug_print("socket initialization done");
    }

    void debug_print(const string& msg) const {
        if (debug) {
            cout << "[" << name << "] " << msg << endl;
        }
    }

    virtual void send(const string& msg) const = 0;
    virtual string recv() const = 0;
};

struct UDPSocket : public Socket {
    UDPSocket(const string &name, int port, SType type) : Socket(name, port) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            pexit("ERROR listen failed");
        }

        if (type == SType::SERVER) {
            bind();
        }
    }

    void bind() {
        if (::bind(fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_in))) {
            pexit("ERROR bind failed");
        }
        debug_print("bind completed");
    }

    void send(const string& msg) const final {
        debug_print("send \"" + msg + "\"");

        int len = (int)msg.length();

        sendto(fd, (char *)&len, sizeof(int), MSG_CONFIRM,
                (const struct sockaddr *)&sock, sizeof(struct sockaddr_in));

        sendto(fd, msg.c_str(), msg.length(), MSG_CONFIRM,
                (const struct sockaddr *)&sock, sizeof(struct sockaddr_in));
    }

    string recv() const final {
        char buf[MAX_LEN];
        socklen_t len;

        int size = 0;
        recvfrom(fd, (char *)&size, sizeof(int),
                MSG_WAITALL, (struct sockaddr *)&sock, &len); 

        for (int rd = 0; rd < size; ) {
            rd += recvfrom(fd, (char *)buf+rd, size-rd,
                MSG_WAITALL, (struct sockaddr *)&sock, &len); 
        }
        buf[size] = '\0';

        debug_print("recv \"" + string(buf) + "\"");

        return buf;
    }
};

struct TCPSocket : public Socket {
    typedef void (*CALLBACK)(const TCPSocket&);

    TCPSocket(const string &name, int port, SType type) : Socket(name, port) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            pexit("ERROR socket failed");
        }

        if (type == SType::SERVER) {
            bind_and_listen();
        } else {
            connect();
        }
    }

    TCPSocket(const string &name, int fd) : Socket() {
        this->name = name;
        this->fd = fd;
        this->debug = false;
    }

    void bind_and_listen() {
        if (bind(fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_in))) {
            pexit("ERROR bind failed");
        }
        debug_print("bind completed");

        if (listen(fd, 5)) {
            pexit("ERROR listen failed");
        }
        debug_print("listen completed");
    }

    void connect() {
        if (::connect(fd, (struct sockaddr *)&sock, sizeof(struct sockaddr_in))) { 
            pexit("ERROR connect failed");
        }
        debug_print("connect completed");
    }

    int run(CALLBACK callback) {
        int id = 0;
        int max_fd = fd + 1;
        fd_set fds;
        FD_ZERO(&fds);

        while (true) {
            FD_SET(fd, &fds);
            int rc = select(max_fd, &fds, NULL, NULL, NULL);
            if (rc < 0) {
                pexit("ERROR select failed");
            }
            if (0 == rc) {
                continue;
            }
            if (FD_ISSET(fd, &fds)) {
                struct sockaddr_in sock;
                socklen_t len = sizeof(struct sockaddr_in);
                int client_fd = accept(fd, (struct sockaddr *)&sock, &len);
                if (client_fd < 0) {
                    pexit("ERROR accept failed");
                }

                TCPSocket tcp_client("client" + to_string(++id), client_fd);

                callback(tcp_client);
            }
        }
    }

    void send(const string& msg) const final {
        debug_print("send \"" + msg + "\"");

        int size = msg.length();
        ::send(fd, (char*)&size, sizeof(int), 0);
        ::send(fd, msg.c_str(), msg.length(), 0);
    }

    string recv() const final {
        char buf[MAX_LEN];
        int size = 0;
        ::recv(fd, (char*)&size, sizeof(int), 0);
        for (int rd = 0; rd < size; ) {
            rd += ::recv(fd, buf+rd, size-rd, 0);
        }
        buf[size] = '\0';
        debug_print("recv \"" + string(buf) + "\"");
        return buf;
    }
};

void send_countries(const Socket& sock, ContryGraph& cg) {
    sock.send(to_string(cg.size()));
    for (auto& p: cg) {
        sock.send(p.first);
    }
}

vector<string> recv_countries(const Socket& sock) {
    vector<string> cs;

    int num_countries = stoi(sock.recv());
    for (int i = 0; i < num_countries; ++i) {
        cs.emplace_back(sock.recv());
    }

    return cs;
}

void run_data_server(const char *name, int port, const char *data_file) {
    cout << "The server " << name << " is up and running using UDP on port " << port << endl;

    ContryGraph cg = read_graph(data_file);

    UDPSocket udp_server(string("server")+name, port, SType::SERVER);
    UDPSocket udp_server_main("servermain UDP client", SERVER_MAIN_UDP_PORT, SType::CLIENT);

    string msg = udp_server.recv();
    send_countries(udp_server_main, cg);

    cout << "The server " << name << " has sent a country list to Main Server" << endl;

    while (true) {
        string contry = udp_server.recv();
        int uid = stoi(udp_server.recv());

        if (!cg.count(contry) || !cg[contry].count(uid)) {
            string msg = "User" + to_string(uid) + " not found";
            udp_server_main.send(msg);
            cout << "The server " << name << " has sent\"" + msg + "\" to Main Server" << endl;
        } else {
            int recommend = recommendation(cg[contry], uid);
            if (recommend == -1) {
                udp_server_main.send("None");
            } else {
                udp_server_main.send(to_string(recommend));
            }
            cout << "The server " << name << " is searching possible friends for User" << uid << endl;
            cout << "Here are the results: User" << recommend << endl;
        }
    }
}

