#ifndef FILESERVER_CONNECTION
#define FILESERVER_CONNECTION

#include <iostream>
#include <string>
#include <fstream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>


class TcpConnection : public boost::enable_shared_from_this <TcpConnection> {
    private:
        std::string userName;
        std::string root;

        boost::asio::streambuf request;
        boost::asio::streambuf ack;

        boost::asio::ip::tcp::socket mySocket;

        std::ofstream outFile;
        std::ifstream inFile;

        boost::array<char, 4096> buf;
        std::streamsize bytesReadTotal;

        void handleUserName(const boost::system::error_code& error,
                const std::size_t bytesTransferred);

        void handleRequest(const boost::system::error_code& error,
                const std::size_t bytesTransferred);

        void handleFileSend(const boost::system::error_code& error);

        void handleFileRecv(const boost::system::error_code& error,
                std::size_t bytesTransferred, std::size_t fileSize);

        void handleList(const boost::system::error_code& error);

        void handleError(const std::string& functionName,
                const boost::system::error_code& error);

    public:
        TcpConnection(boost::asio::io_service& ioService);

        void start();

        boost::asio::ip::tcp::socket& socket();
};

#endif
