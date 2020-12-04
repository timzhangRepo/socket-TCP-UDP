Name:Tianchen Zhang	
Student ID:4347909915

For this assignment, I have completed all the functionalities required.

Besides the required file servermain.cc, serverA/B.cc, client.cc, I have created one additional file called "common.hpp".
It is basically a utility library, which is used among all cc files.
More specific, common.hpp has:
1. socket wrapper class: Socket, UDPSocket, TCPSocket, it all has `send` and `recv` method
2. data file
2.1 read_graph
2.2 send_countries
2.3 recv_countries
3. recommend calculation: `recommendation`

For the message over the net, it transport like:
* send
1. first send length
2. send content
* recv
1. first recv length
2. read length bytes and decode to string
 
All the code is written by myself.

