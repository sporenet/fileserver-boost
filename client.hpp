#ifndef FILESERVER_CLIENT
#define FILESERVER_CLIENT

#include <fstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>


class TcpClient {
    private:
        boost::asio::ip::tcp::resolver resolver;
        boost::asio::ip::tcp::socket socket;

        boost::asio::streambuf request;
        boost::asio::streambuf ack;

        std::ifstream upFile;
        std::ofstream downFile;

        boost::array<char, 4096> buf;

        std::streamsize bytesReadTotal;

    public:
        TcpClient(boost::asio::io_service& ioService,
                const std::string& server, const std::string& port);

        void handleResolve(const boost::system::error_code& error,
                boost::asio::ip::tcp::resolver::iterator myIterator);

        void handleConnect(const boost::system::error_code& error,
                boost::asio::ip::tcp::resolver::iterator myIterator);

        void fileSendRequest(const std::string& fileName);

        void fileRecvRequest(const std::string& fileName);

        void handleFileSend(const boost::system::error_code& error);

        void handleFileRecvAckSub(const boost::system::error_code& error);

        void handleFileRecvAck(const boost::system::error_code& error,
                const std::size_t bytesTransferred);

        void handleFileRecv(const boost::system::error_code& error,
                const std::size_t bytesTransferred, const std::size_t fileSize);

        void requestToServer();
};

#endif
