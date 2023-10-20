#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <string>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>



#define SOCKS_SOCKET 1
#define SERVICE_SOCKET 2

using boost::asio::ip::tcp;
using namespace std;

struct client_request{
    int vn;
    int cd;
    string src_ip;
    string src_port;
    string dst_ip;
    string dst_port;
    string cmd;
    string reply;
};

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_context& io_context,tcp::socket socket)
    :   io_context_(io_context),
        socket_server(io_context),
        resolver_(socket.get_executor()),
        socket_(std::move(socket))
    {
        memset(data_,'\0',sizeof(data_));
    }

    void start()
    {
        socket_.set_option(tcp::socket::reuse_address(true));
        do_read_request();
    }

private:
    void do_read_request(){
        memset(request,'\0',sizeof(request));
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(request, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    // for(int i=0;i<9;i++) cout << (int)request[i] << endl;
                    do_parse();
                    do_print_message();
                    do_resolve();
                }
            });
    }

    void do_resolve(){
        auto self(shared_from_this());
        resolver_.async_resolve(tcp::resolver::query({ cli_req.dst_ip,cli_req.dst_port }),
			[this, self](const boost::system::error_code& ec, tcp::resolver::iterator it)
			{
				if (!ec){
                    tcp::endpoint endpoint_ = *it;
                    if(cli_req.cd == 1) do_connect(it);
                    else if(cli_req.cd == 2) do_bind();
				}
			});
    }

    void do_bind(){
        memset(reply,'\0',sizeof(reply));
        if(cli_req.reply == "Accept"){
            reply[1] = 90;
            tcp::acceptor acceptor_(io_context_, tcp::endpoint(tcp::v4(), 0));
            acceptor_.listen();
            unsigned short dst_port = acceptor_.local_endpoint().port();
            reply[2] = (dst_port>>8) & 0X00FF;
            reply[3] = dst_port & 0X00FF;
            do_bind_reply_to_client();
            acceptor_.accept(socket_server);
            do_bind_reply_to_client();
            do_read(SOCKS_SOCKET | SERVICE_SOCKET);
        }
        else{
            reply[1] = 91;
            do_bind_reply_to_client();
            exit(EXIT_SUCCESS);
        }
    }

    void do_bind_reply_to_client(){
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(reply, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/){
                // cout << "Reply Fail." << endl;
                if(!ec){
                    // cout << "Reply Success." << endl;
                }
            });
    }

    void do_connect(tcp::resolver::iterator& it){
        auto self(shared_from_this()); 
		socket_server.async_connect(*it, 
			[this, self](const boost::system::error_code& ec)
			{
				if (!ec){
                    do_reply_client();
                }
            });
    }

    void do_reply_client(){
        memset(reply,'\0',sizeof(reply));
        if(cli_req.reply == "Accept") reply[1] = 90;
        else reply[1] = 91;
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(reply, 8),
            [this, self](boost::system::error_code ec, size_t /*length*/){
                if(!ec){
                    memset(reply, '\0', sizeof(reply));
                    if(reply[1] == 91) exit(EXIT_SUCCESS);
                    // do_read(SOCKS_SOCKET | SERVICE_SOCKET);
                    do_read_from_client();
                    do_read_from_server();
                }
            });
    }

    void do_read_from_server(){
        memset(buffer2, '\0', max_length);
        auto self(shared_from_this());
        socket_server.async_receive(boost::asio::buffer(buffer2),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    do_write_to_client(length);
                }
                else
                {
                    exit(EXIT_SUCCESS);
                }
            });
    }

    void do_write_to_client(size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(buffer2, length),
            [this, self](boost::system::error_code ec, std::size_t length_)
            {
                if (!ec){
                    do_read_from_server();
                }
                else
                {
                    exit(EXIT_SUCCESS);
                }
            });
    }

    void do_read_from_client(){
        memset(buffer1, '\0', max_length);
        auto self(shared_from_this());
        socket_.async_receive(boost::asio::buffer(buffer1),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    do_write_to_server(length);
                }
                else
                {
                    exit(EXIT_SUCCESS);
                }
            });
    }

    void do_write_to_server(size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_server, boost::asio::buffer(buffer1, length),
            [this, self](boost::system::error_code ec, std::size_t length_)
            {
                if (!ec){
                    do_read_from_client();
                }	
                else
                {
                    exit(EXIT_SUCCESS);
                }
            });
    }

    void do_read(int flag){
        auto self(shared_from_this());
        memset(buffer1, '\0', max_length);
        memset(buffer2, '\0', max_length);
        if(flag & SOCKS_SOCKET){
            socket_.async_receive(boost::asio::buffer(buffer1),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    if (!ec)
                    {
                        do_write(SOCKS_SOCKET, length);
                    }
                    else
                    {
                        exit(EXIT_SUCCESS);
                    }
                });
        }
        if(flag & SERVICE_SOCKET){
            socket_server.async_receive(boost::asio::buffer(buffer2),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    if (!ec)
                    {
                        do_write(SERVICE_SOCKET, length);
                    }
                    else
                    {
                        exit(EXIT_SUCCESS);
                    }
                });
        }
    }

    void do_write(int flag, size_t length){
        auto self(shared_from_this());
        switch(flag)
		{
            case SOCKS_SOCKET:
                boost::asio::async_write(socket_server, boost::asio::buffer(buffer1, length),
                    [this, self](boost::system::error_code ec, std::size_t length_)
                    {
                        if (!ec){
                            do_read(SOCKS_SOCKET);
                        }	
                        else
                        {
                            socket_.close(); 
                            socket_server.close();
                            return;
                        }
                    });
                break;
            case SERVICE_SOCKET:
                boost::asio::async_write(socket_, boost::asio::buffer(buffer2, length),
                    [this, self](boost::system::error_code ec, std::size_t length_)
                    {
                        if (!ec){
                            do_read(SERVICE_SOCKET);
                        }
                        else
                        {
                            socket_.close(); 
                            socket_server.close();
                            return;
                        }
                    });
                break;
		}
    }

    void do_parse(){
        cli_req.vn = (int)request[0];
        cli_req.cd = (int)request[1];
        if(cli_req.cd == 1) cli_req.cmd = "CONNECT";
        else if(cli_req.cd == 2) cli_req.cmd = "BIND";
        cli_req.src_ip = socket_.remote_endpoint().address().to_string();
        cli_req.src_port = to_string(socket_.remote_endpoint().port());
        cli_req.dst_port = to_string((uint)(request[2]<<8) | request[3]);
        if(!request[4] && !request[5] && !request[6] && request[7]){
            int i = 8;
            while(request[i++] != '\0'); // USERID
            i++; // NULL
            string name = "";
            while(request[i] != '\0') name += request[i++]; // Domain Name
            tcp::resolver sol(io_context_);
            tcp::endpoint domain = sol.resolve(name, cli_req.dst_port)->endpoint();
            cli_req.dst_ip = domain.address().to_string();
        }
        else{
            char ip[20] = "";
            sprintf(ip,"%d.%d.%d.%d",request[4],request[5],request[6],request[7]);
            cli_req.dst_ip = ip;
            // cout << "This is Destination IP : " + cli_req.dst_ip << endl;
        }
        if(do_firewall()) cli_req.reply = "Accept";
        else cli_req.reply = "Reject";
    }

    int do_firewall(){
        ifstream file("./socks.conf");
        string rule;
        while(getline(file,rule)){
            istringstream stream(rule);
            string s[3];
            int i=0;
            while(stream >> s[i++]);
            boost::replace_all(s[2], ".", "\\.");
            boost::replace_all(s[2], "*", "[0-9]{1,3}");
            // cout << "This is address : " + s[2] << endl;
            boost::regex exp(s[2]);
            if((s[1] == "c" && cli_req.cmd != "CONNECT") || (s[1] == "b" && cli_req.cmd != "BIND")) continue;
            if(boost::regex_match(cli_req.dst_ip, exp)) return 1;
        }
        return 0;
    }

    void do_print_message(){
        cout << "<S_IP>: " << cli_req.src_ip << endl;
        cout << "<S_PORT>: " << cli_req.src_port << endl;
        cout << "<D_IP>: " << cli_req.dst_ip << endl;
        cout << "<D_PORT>: " << cli_req.dst_port << endl;
        cout << "<Command>: " << cli_req.cmd << endl;
        cout << "<Reply>: " << cli_req.reply << endl << endl;
    }

    unsigned char reply[10];
    struct client_request cli_req; 
    tcp::resolver resolver_;
    tcp::socket socket_server;
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    enum { max_length = 1024 };
    unsigned char request[10];
    char data_[max_length];
    char buffer1[max_length];
	char buffer2[max_length];
};

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
        : io_context(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    io_context.notify_fork(boost::asio::io_context::fork_prepare);
                    int pid = fork();
                    if(pid){
                        io_context.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                    }
                    else{
                        io_context.notify_fork(boost::asio::io_context::fork_child); 
                        acceptor_.close();
                        make_shared<session>(io_context,std::move(socket))->start();
                    }
                    // make_shared<session>(io_context,std::move(socket))->start();
                }

                do_accept();
            });
    }

    boost::asio::io_context& io_context;
    tcp::acceptor acceptor_;
};

void sig_handler(int signo)
{
	int status;
	wait(&status);
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        signal(SIGCHLD, sig_handler);
        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

  return 0;
}