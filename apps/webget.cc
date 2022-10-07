#include "socket.hh"
#include "util.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

void get_URL(const string &host, const string &path) {
    Address address(host, "http");
    TCPSocket client;

    client.connect(address);

    //对url发送request
    string req = "GET " + path + " HTTP/1.0\r\n" + "Host: " + host +":80\r\n\r\n";
    // 合成请求报文
    send(client.fd_num(), req.c_str(), req.size(), 0);
    //输出sends back的字符
    while (!client.eof()) {
        string str = client.read();
        cout << str;
    }

    client.close();
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
