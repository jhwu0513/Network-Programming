#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <sys/stat.h>
#include <string>
#include <fcntl.h>
#include <bits/stdc++.h>

using boost::asio::ip::tcp;
using namespace std;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(boost::asio::io_context& io_context,int i ,vector<string> v)
    : io_context_(io_context),
      socket_(io_context)
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
            cout << "<script>document.getElementById('s" << uid << "').innerHTML += '" << s << "';</script>" << flush;
            memset(data_, '\0', sizeof(data_));
            if(s.find("% ") == string::npos) do_read();
            else do_write();
          }
        });
  }

  void do_write()
  {
    string s;
    getline(file_stream,line);
    line += "\n";
    s = html_escape(line);
    cout << "<script>document.getElementById('s" << uid << "').innerHTML += '" << s << "';</script>";
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
  string line;
  boost::asio::io_context& io_context_;
  vector<string> box;
  tcp::resolver::results_type endpoint;
  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

vector<string> do_parse(string query){
  vector<string> v;
  vector<string> rs;
  boost::split(v, query, boost::is_any_of("&"), boost::token_compress_on);
  for(int i=0;i<(int)v.size() && (v[i][(int)v[i].length()-1] != '=');i+=3)
    for(int j=0;j<3;j++) rs.push_back(v[i+j].substr(v[i+j].find("=")+1));
  return rs;
}

void WebPage(vector<string> endpoint){
  cout << "Content-type: text/html\r\n\r\n" << flush;
  string content = "";
  content = content \
  + "<!DOCTYPE html>" 
  + "<html lang=\"en\">"
  + "  <head>"
  + "    <meta charset=\"UTF-8\" />"
  + "    <title>NP Project 3 Sample Console</title>"
  + "    <link"
  + "      rel=\"stylesheet\""
  + "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""
  + "    integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""
  + "      crossorigin=\"anonymous\""
  + "    />"
  + "    <link"
  + "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""
  + "      rel=\"stylesheet\""
  + "    />"
  + "   <link"
  + "      rel=\"icon\""
  + "      type=\"image/png\""
  + "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""
  + "    />"
  + "    <style>"
  + "      * {"
  + "        font-family: 'Source Code Pro', monospace;"
  + "       font-size: 1rem !important;"
  + "    }"
  + "     body {"
  + "       background-color: #212529;"
  + "     }"
  + "     pre {"
  + "       color: #cccccc;"
  + "     }"
  + "     b {"
  + "       color: #01b468;"
  + "     }"
  + "   </style>"
  + " </head>"
  + " <body>"
  + "   <table class=\"table table-dark table-bordered\">"
  + "     <thead>"
  + "       <tr>";
  int i;
  for(i=0;i<(int)endpoint.size();i+=3)
    content += "<th scope=\"col\">" + endpoint[i] + ":" + endpoint[i+1] +"</th>";
  content = content \
  + "     </tr>"
  + "     </thead>"
  + "     <tbody>"
  + "       <tr>";
  for(i=0;i<(int)endpoint.size()/3;i++)
    content += "<td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>";
  content = content \
  + "     </tr>"
  + "     </tbody>"
  + "   </table>"
  + " </body>"
  + "</html>";
  cout << content << flush;
}

int main(int argc, char* argv[])
{
  try
  {
    vector<string> endpoint = do_parse(getenv("QUERY_STRING"));
    WebPage(endpoint);
    boost::asio::io_context io_context;
    for(int i=0;i<(int)endpoint.size();i+=3) make_shared<session>(io_context,i, endpoint)->start(i);
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}