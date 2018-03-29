#include "server.hpp"
#include <vector>
#include <boost/thread.hpp>
#include <boost/bind.hpp>


TcpServer::TcpServer(const unsigned short port) :
    acceptor(ioService, boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(), port), true),
    newConnection(new TcpConnection(ioService)) {
        acceptor.async_accept(newConnection->socket(),
                boost::bind(&TcpServer::handleAccept, this,
                    boost::asio::placeholders::error));
}

void TcpServer::handleAccept(const boost::system::error_code& error) {
    std::cout << __FUNCTION__ << " " << error << ", " << error.message() << std::endl;
    if (!error) {
        newConnection->start();
        newConnection.reset(new TcpConnection(ioService));
        acceptor.async_accept(newConnection->socket(),
                boost::bind(&TcpServer::handleAccept, this,
                    boost::asio::placeholders::error));
    }
}

void TcpServer::run() {
    ioService.run();
}

void TcpServer::stop() {
    ioService.stop();
}


int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cout << "Usage: port#" << std::endl;
            return 0;
        }

        const unsigned short port = atoi(argv[1]);

        std::cout << argv[0] << " listen on port " << port << std::endl;
        TcpServer myTcpServer(port);

        myTcpServer.run();
        myTcpServer.stop();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
