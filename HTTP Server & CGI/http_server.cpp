#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <map>
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            do_parse();
            do_fork();
          }
        });
  }

  void do_write()
  {
    string data = data_;
    data = "HTTP/1.1 200 OK\r\n" + data;
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data, data.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            int STDOUT = dup(1);
            dup2(socket_.native_handle(),1);
            string data = data_;
            string s = "Content-type: text/html \r\n\r\n <!DOCTYPE html><html><head><title>The Test.</title></head><body>" + data + " </body></html>";
            cout << s;
            dup2(STDOUT,1);
          }
        }); 
  }

  void do_parse(){
    vector<string> data_line;
    vector<string> split_line;
    boost::split(data_line, data_, boost::is_any_of("\n"));
    boost::split(split_line, data_line[0], boost::is_any_of(" "));
    env["REQUEST_METHOD"] = split_line[0];
    env["REQUEST_URI"] = split_line[1];
    env["SERVER_PROTOCOL"] = split_line[2];
    env["QUERY_STRING"] = (split_line[1].find("?") != string::npos)?split_line[1].substr(split_line[1].find("?")+1):"";
    split_line.clear();
    
    boost::split(split_line, data_line[1], boost::is_any_of(" "), boost::token_compress_on);
    env["HTTP_HOST"] = split_line[1];
    env["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
    env["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
    env["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
    env["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());
  }

  void do_fork(){
    int pid = fork();
    if(!pid){
      string data = "HTTP/1.1 200 OK\r\n";
      auto self(shared_from_this());
      boost::asio::async_write(socket_, boost::asio::buffer(data, data.length()),
          [this, self](boost::system::error_code ec, std::size_t /*length*/)
          {
            if (!ec)
            {
              for(map<string, string>::iterator it = env.begin(); it != env.end(); it++) setenv((it->first).c_str(), (it->second).c_str(), 1);
              int STDOUT = dup(1);
              dup2(socket_.native_handle(),1);
              string file = "." + (env["REQUEST_URI"].find("?") != string::npos ? env["REQUEST_URI"].substr(0, env["REQUEST_URI"].find("?")) : env["REQUEST_URI"]);
              char *arg[] = {strdup(file.c_str()), NULL};
              execvp(file.c_str(), arg);
              dup2(STDOUT,1);
              close(STDOUT);
            }
          });
    }
    else if(pid>0) socket_.close();
  }

  tcp::socket socket_;
  map<string,string> env;
  enum { max_length = 1024 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port) // 所有IO對象都依賴io_context
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
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
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_; // tcp::acceptor 也是一种 I/O 对象，用来接收 TCP 连接，连接端口由 tcp::endpoint 指定
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

    boost::asio::io_context io_context; // io_context表示program到OS的I / O服务的链接
    signal(SIGCHLD, sig_handler);
    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}