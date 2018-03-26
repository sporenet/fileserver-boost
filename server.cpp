#include "server.hpp"
#include <vector>
#include <boost/thread.hpp>
#include <boost/bind.hpp>


TcpServer::TcpServer(const unsigned short port, const std::size_t _threadPoolSize) :
    threadPoolSize(_threadPoolSize),
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
    std::vector<boost::shared_ptr<boost::thread> > threads;

    for (std::size_t i = 0; i < threadPoolSize; i++) {
        boost::shared_ptr<boost::thread> thread(new boost::thread(
                    boost::bind(&boost::asio::io_service::run, &ioService)));
        threads.push_back(thread);
    }

    for (std::size_t i = 0; i < threads.size(); i++) {
        threads[i]->join();
    }
}

void TcpServer::stop() {
    ioService.stop();
}


int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cout << "Usage: port# thread#" << std::endl;
            return 0;
        }

        const unsigned short port = atoi(argv[1]);
        const std::size_t threadPoolSize = atoi(argv[2]);

        std::cout << argv[0] << " listen on port " << port << std::endl;
        TcpServer myTcpServer(port, threadPoolSize);

        myTcpServer.run();
        myTcpServer.stop();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
