#include "common.hpp"

int main(int argc, char **argv) {
    TCPSocket tcp_client("tcp client", SERVER_MAIN_TCP_PORT, SType::CLIENT);

    tcp_client.send("hi");
    string cname = tcp_client.recv();
    cname[0] = 'C';

    cout << cname << " is up and running" << endl;

    string uid;
    string contry;

    while (true) {
        cout << "Please enter the User ID: ";
        cin >> uid;

        cout << "Please enter the Country Name: ";
        cin >> contry;

        tcp_client.send(contry);
        tcp_client.send(uid);

        cout << cname << " has sent User" << uid << " and " << contry << " to Main Server using TCP" << endl;

        string msg = tcp_client.recv();

        if (msg.find("not found") != string::npos) {
            cout << msg << endl;
        } else {
            cout << cname << " has received results from Main Server: "
                << msg << " is/are possible friend(s) of User" << uid << " in " << contry  << endl;
        }
    }

    return 0;
}

