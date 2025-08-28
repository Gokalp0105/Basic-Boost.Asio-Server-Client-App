#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <boost/algorithm/string/predicate.hpp>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, std::set<std::shared_ptr<Session>>& sessions)
        : socket_(std::move(socket)), sessions_(sessions)
    {
    }

    void start()
    {
        sessions_.insert(shared_from_this());
        do_read();
    }

    void deliver(const std::string& msg)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(msg),
            [self](boost::system::error_code ec, std::size_t) {
                if (ec)
                {
                    std::cerr << "Deliver error: " << ec.message() << std::endl;
                }
            });
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, "\n",
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec)
                {
                    std::istream is(&buffer_);
                    std::string msg;
                    std::getline(is, msg);

                    if (boost::algorithm::starts_with(msg, "NAME::"))
                    {
                        name_ = msg.substr(6);
                        std::cout << "User connected: " << name_ << std::endl;
                    }
                    else if (boost::algorithm::starts_with(msg, "TXT::"))
                    {
                        std::string content = msg.substr(5);
                        std::string full_msg = "TXT::" + content + "\n";

                        // tüm diðer sessionlara gönder
                        for (auto& s : sessions_)
                        {
                            if (s.get() != this) // kendisine göndermiyoruz
                            {
                                s->deliver("TXT::" + name_ + ": " + content + "\n");
                            }
                        }

                        std::cout << name_ << ": " << content << std::endl;
                    }

                    do_read();
                }
                else
                {
                    sessions_.erase(self);
                    std::cerr << "Connection closed: " << name_ << std::endl;
                }
            });
    }

    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::set<std::shared_ptr<Session>>& sessions_;
    std::string name_;
};

class Server
{
public:
    Server(boost::asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec)
                {
                    std::make_shared<Session>(std::move(socket), sessions_)->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    std::set<std::shared_ptr<Session>> sessions_;
};

int main()
{
    try
    {
        boost::asio::io_context io;
        Server server(io, 1234);
        std::cout << "Server running on port 1234..." << std::endl;
        io.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}