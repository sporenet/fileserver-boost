#ifndef FILESERVER_SERVER
#define FILESERVER_SERVER

#include "connection.hpp"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>


class TcpServer : private boost::noncopyable {
    typedef boost::shared_ptr<TcpConnection> ptrTcpConnection;

    private:
        boost::asio::io_service ioService;
        boost::asio::ip::tcp::acceptor acceptor;
        ptrTcpConnection newConnection;
        std::size_t threadPoolSize;

    public:
        TcpServer(const unsigned short port, const std::size_t _threadPoolSize);

        void handleAccept(const boost::system::error_code& error);

        void run();

        void stop();
};

#endif
