#include "common.hpp"

const UDPSocket udp_server("servermain UDP server", SERVER_MAIN_UDP_PORT, SType::SERVER);
const UDPSocket udp_client_a("servermain UDP client A", SERVER_A_PORT, SType::CLIENT);
const UDPSocket udp_client_b("servermain UDP client B", SERVER_B_PORT, SType::CLIENT);
unordered_map<string, bool> contry_in_server_a;

void *tcp_thread(void *args) {
    const TCPSocket* sock = (const TCPSocket *)args;

    sock->recv();
    sock->send(sock->name);

    while (true) {
        string contry = sock->recv();
        string uid = sock->recv();

        cout << "The Main server has received the request on User " << uid
             << " in " << contry << " from " << (sock->name) << " using TCP over port " << SERVER_MAIN_TCP_PORT << endl;

        if (!contry_in_server_a.count(contry)) {
            cout << contry << " does not show up in server A&B" << endl;
            string msg = contry + ": not found";
            sock->send(msg);
            cout << "The Main Server has sent \"" << msg << "\"to " << (sock->name)
                 << "using TCP over port " << SERVER_MAIN_TCP_PORT << endl;
        } else {
            string from = "A";
            if (contry_in_server_a[contry]) {
                cout << contry << " shows up in server A" << endl;
                udp_client_a.send(contry);
                udp_client_a.send(uid);
            } else {
                from = "B";
                cout << contry << " shows up in server B" << endl;
                udp_client_b.send(contry);
                udp_client_b.send(uid);
            }

            cout << "The Main Server has sent request from User " << uid
                 << " to server " << from << " using UDP over port " << SERVER_MAIN_UDP_PORT << endl;

            string rec = udp_server.recv();
            if (rec.find("not found") != string::npos) {
                cout << "The Main server has received \"" << rec << "\" from server " << from << endl;
                sock->send(rec);
                cout << "The Main Server has sent error to client using TCP over " << SERVER_MAIN_TCP_PORT << endl;
            } else {
                cout << "The Main server has received searching result(s) of User " << uid << " from server " << from << endl;
                sock->send(rec);
            }
        }
    }

    return NULL;
}

void tcp_callback(const TCPSocket& tcp_client) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, tcp_thread, (void *)(&tcp_client))) {
        pexit("ERROR pthread_create failed");
    }
    pthread_join(tid, NULL);
}

void print_contries(const vector<string>& contries_a, const vector<string>& contries_b) {
    int la = (int)contries_a.size();
    int lb = (int)contries_b.size();
    cout.clear();
    cout << left << setw(20) << "Server A" << " | " << left << setw(20) << "Server B" << endl;
    for (int a = 0, b = 0; a < la || b < lb; ) {
        if (a < la) {
            cout << left << setw(20) << contries_a[a++];
        } else {
            cout << setw(20) << "";
        }
        cout << " | ";
        if (b < lb) {
            cout << left << setw(20) << contries_b[b++];
        } else {
            cout << setw(20) << "";
        }
        cout << endl;
    }
}

int main(int argc, char **argv) {
    cout << "The Main server is up and running." << endl;

    udp_client_a.send("hi");
    vector<string> contries_a = recv_countries(udp_server);

    cout << "The Main server has received the country list from server A using UDP over port " << SERVER_MAIN_UDP_PORT << endl;

    udp_client_b.send("hi");
    vector<string> contries_b = recv_countries(udp_server);

    cout << "The Main server has received the country list from server B using UDP over port " << SERVER_MAIN_UDP_PORT << endl;

    print_contries(contries_a, contries_b);

    for (auto& c: contries_a) contry_in_server_a[c] = true;
    for (auto& c: contries_b) contry_in_server_a[c] = false;

    TCPSocket tcp_server("servermain TCP server", SERVER_MAIN_TCP_PORT, SType::SERVER);
    tcp_server.run(tcp_callback);

    return 0;
}

