#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <map>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

vector<string> do_parser(string q){
  vector<string> args;
  boost::split(args, q, boost::is_any_of("&"), boost::token_compress_on);
  vector<string> info;
  for(int i=0;i<args.size();i+=3){
    if(args[i][args[i].size()-1] == '=') break;
    info.push_back(args[i].substr(args[i].find("=")+1));
    info.push_back(args[i+1].substr(args[i+1].find("=")+1));
    info.push_back(args[i+2].substr(args[i+2].find("=")+1));
  }
  return info;
}

string WebPage(vector<string> endpoint){
  int i;
  string content = R""""(
    <!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Sample Console</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #212529;
      }
      pre {
        color: #cccccc;
      }
      b {
        color: #01b468;
      }
    </style>
  </head>
  <body>
    <table class="table table-dark table-bordered">
      <thead>
        <tr>)"""";
  for(i=0;i<(int)endpoint.size();i+=3) content += "<th scope=\"col\">" + endpoint[i] + ":" + endpoint[i+1] +"</th>";
  content += R""""(        
        </tr>
      </thead>
      <tbody>
        <tr>)"""";
  for(i=0;i<(int)endpoint.size()/3;i++) content += "<td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";
  content += R""""(
        </tr>
      </tbody>
    </table>
  </body>
</html>
  )"""";
  return content;
}

string do_panel(){
  string t = R""""(
<!DOCTYPE html>
<html lang="en">
  <head>
    <title>NP Project 3 Panel</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
      }
    </style>
  </head>
  <body class="bg-secondary pt-5">
    <form action="console.cgi" method="GET">
      <table class="table mx-auto bg-light" style="width: inherit">
        <thead class="thead-dark">
          <tr>
            <th scope="col">#</th>
            <th scope="col">Host</th>
            <th scope="col">Port</th>
            <th scope="col">Input File</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <th scope="row" class="align-middle">Session 1</th>
            <td>
              <div class="input-group">
                <select name="h0" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p0" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f0" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 2</th>
            <td>
              <div class="input-group">
                <select name="h1" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p1" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f1" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 3</th>
            <td>
              <div class="input-group">
                <select name="h2" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p2" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f2" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 4</th>
            <td>
              <div class="input-group">
                <select name="h3" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p3" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f3" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <th scope="row" class="align-middle">Session 5</th>
            <td>
              <div class="input-group">
                <select name="h4" class="custom-select">
                  <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p4" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f4" class="custom-select">
                <option></option>
                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
              </select>
            </td>
          </tr>
          <tr>
            <td colspan="3"></td>
            <td>
              <button type="submit" class="btn btn-info btn-block">Run</button>
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </body>
</html>
  )"""";
  return t;
}

class console
  : public std::enable_shared_from_this<console>
{
public:
  console(boost::asio::io_context& io_context,int i ,vector<string> v,shared_ptr<tcp::socket> browser)
    : io_context_(io_context),
      socket_(io_context),
      socket_browser(browser)
  {
    box = v;
    string file_name = v[i+2];
    memset(data_, '\0', sizeof(data_));
    file_stream.open("./test_case/" + file_name,ios::in | ios::binary);
    uid = int(i/3);
  }

  void start(int i)
  {
    do_resolve(i);
  }

private:    
  void do_resolve(int i){
    tcp::resolver resolver(io_context_);
    tcp::resolver::query query(box[i], box[i+1]); // 用來解析Domain Name的
    endpoint = resolver.resolve(query);
    do_connect(endpoint);
  }

  void do_connect(tcp::resolver::results_type::iterator it){
    auto self(shared_from_this());
    socket_.async_connect(*it, [this, self](boost::system::error_code ec){
        if(!ec)  
        {
          do_read();
        }
    });
  }

  string html_escape(string s){
    boost::replace_all(s, "\n", "&NewLine;");
    boost::replace_all(s, "<", "&lt;");
    boost::replace_all(s, ">", "&gt;");
    boost::replace_all(s, "\'", "&apos;");
    boost::replace_all(s, "\"", "&quot;");
    boost::replace_all(s, "\r", ""); // Linux換行為\r
    return s;
  }

  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            string s = html_escape(data_);
            output = "<script>document.getElementById('s" + to_string(uid) + "').innerHTML += '" + s + "';</script>";
            do_writeLine();
            memset(data_, '\0', sizeof(data_));
            if(s.find("%") == string::npos) do_read();
            else do_write();
          }
        });
  }

  void do_writeLine()
  {
    data = "HTTP/1.1 200 OK\r\n\r\n" + output;
    auto self(shared_from_this());
    boost::asio::async_write(*socket_browser, boost::asio::buffer(data, data.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
          }
        });  // 写回数据时，没有直接调用 socket.write_some，因为它不能保证一次写完所有数据，但是 boost::asio::write 可以
  }

  void do_write()
  {
    string s,line;
    getline(file_stream,line);
    line += "\n";
    s = html_escape(line);
    output = "<script>document.getElementById('s" + to_string(uid) + "').innerHTML += '" + s + "';</script>";
    do_writeLine();
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(line, line.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            do_read();
          }
        });
  }

  ifstream file_stream;
  int uid;
  string output,data;
  boost::asio::io_context& io_context_;
  vector<string> box;
  tcp::resolver::results_type endpoint;
  tcp::socket socket_;
  shared_ptr<tcp::socket> socket_browser;
  enum { max_length = 1024 };
  char data_[max_length];
};

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
            cout << "Socket : " << socket_.native_handle() << endl;
            do_parse();
            do_fork();
            memset(data_, '\0', sizeof(data_));
          }
        });
  }

  void do_write()
  {
    data = "HTTP/1.1 200 OK\r\n\r\n" + output;
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data, data.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            // cout << data << endl << endl;
            data.clear();
            output.clear();
          }
        });  // 写回数据时，没有直接调用 socket.write_some，因为它不能保证一次写完所有数据，但是 boost::asio::write 可以
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
    // for(map<string, string>::iterator it = env.begin(); it != env.end(); it++) cout << it->first << ":" << it->second << endl;
    // cout << endl;
  }

  void do_fork(){
    string file = (env["REQUEST_URI"].find("?") != string::npos ? env["REQUEST_URI"].substr(0, env["REQUEST_URI"].find("?")) : env["REQUEST_URI"]);
    if(!file.compare("/panel.cgi")){
      output = do_panel();
      do_write();
    }
    if(!file.compare("/console.cgi")){
      vector<string> endpoint = do_parser(env["QUERY_STRING"]);
      output = WebPage(endpoint);
      do_write();
      try{
        for(int i=0;i<(int)endpoint.size();i+=3){
          boost::asio::io_context io_context_;
          shared_ptr<tcp::socket> sp(&socket_);
          make_shared<console>(io_context_,i, endpoint,sp)->start(i);
          io_context_.run();
        }
      }
      catch (std::exception& e)
      {
        std::cerr << "Exception: " << e.what() << "\n";
      }
      cout << "check point1." << endl << endl;
    }
  }

  string output,data;
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
          cout << "check point2." << endl << endl;
          do_accept();
          cout << "check point3." << endl << endl;
        });
  }

  tcp::acceptor acceptor_; // tcp::acceptor 也是一种 I/O 对象，用来接收 TCP 连接，连接端口由 tcp::endpoint 指定
};

int main(int argc, char* argv[])
{
  // for(int i=0;i<NSIG;i++) signal(i,SIG_IGN);
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context; // io_context表示program到OS的I/O服务的链接
    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  return 0;
}