#include <iostream>
#include <string>
#include <fstream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>


class TcpConnection : public boost::enable_shared_from_this <TcpConnection> {
    private:
        boost::asio::streambuf request;
        boost::asio::streambuf ack;
        boost::asio::ip::tcp::socket mySocket;
        boost::array<char, 4096> buf;
        std::ofstream outFile;
        std::ifstream inFile;
        std::streamsize bytesReadTotal;

        void handleRequest(const boost::system::error_code& error,
                const size_t bytesTransferred) {
            if (error) {
                // asio.misc:2 End of file -> client connection이 끊어짐
                return handleError(__FUNCTION__, error);
            }

            std::cout << __FUNCTION__ << "(" << bytesTransferred << ")"
                << ", in_avail = " << request.in_avail()
                << ", size = " << request.size()
                << std::endl;

            std::istream requestStream(&request);
            std::string operation;
            std::string filePath;
            size_t fileSize;

            requestStream >> operation;
            if (operation == "u") {
                requestStream >> filePath;
                requestStream >> fileSize;
                requestStream.read(buf.c_array(), 2);

                std::streamsize bytesRead = 0;

                //std::cout << filePath << " size is " << fileSize << std::endl;
                size_t pos = filePath.find_last_of('\\');
                if (pos != std::string::npos)
                    filePath = filePath.substr(pos + 1);
                std::cout << "Request for upload " << filePath << ": "
                    << fileSize << "bytes" << std::endl;

                outFile.open(filePath.c_str(), std::ios_base::binary);

                // request stream의 잔여 바이트를 파일에 씀
                // async_read_until의 동작때문
                do {
                    requestStream.read(buf.c_array(), (std::streamsize)buf.size());
                    bytesRead += requestStream.gcount();
                    std::cout << __FUNCTION__ << " writes " <<
                        requestStream.gcount() << "bytes, total " << bytesRead
                        << "bytes" << std::endl;
                    outFile.write(buf.c_array(), requestStream.gcount());
                } while (requestStream.gcount() > 0);

                size_t remainBytes = fileSize - bytesRead;

                if (remainBytes == 0) {
                    // 다 읽음
                    outFile.close();
                    async_read_until(mySocket, request, "\n\n",
                            boost::bind(&TcpConnection::handleRequest,
                                shared_from_this(), boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
                } else if (remainBytes >= buf.size()) {
                    // 읽어야 될 남은 양이 buf.size()보다 크거나 같음
                    async_read(mySocket, boost::asio::buffer(buf.c_array(), buf.size()),
                            boost::bind(&TcpConnection::handleFileRecv,
                                shared_from_this(), boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred, fileSize));
                } else {
                    // 읽어야 될 남은 양이 buf.size()보다 작음
                    async_read(mySocket, boost::asio::buffer(buf.c_array(), remainBytes),
                            boost::bind(&TcpConnection::handleFileRecv,
                                shared_from_this(), boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred, fileSize));
                }
            } else if (operation == "d") {
                requestStream >> filePath;
                requestStream.read(buf.c_array(), 2);

                inFile.open(filePath.c_str(),
                        std::ios_base::binary | std::ios_base::ate);

                if (!inFile) {
                    std::cerr << "Error in " << __FUNCTION__ << ": failed to open file" << std::endl;
                    return;
                }

                size_t fileSize = inFile.tellg();
                inFile.seekg(0);

                bytesReadTotal = 0;

                std::ostream ackStream(&ack);
                std::cout << "Request for download " << filePath << ": "
                    << fileSize << "bytes" << std::endl;

                ackStream << fileSize << "\n\n";

                boost::asio::async_write(mySocket, ack,
                        boost::bind(&TcpConnection::handleFileSend,
                            shared_from_this(), boost::asio::placeholders::error));
            } else if (operation == "l") {
                // Not implemented
            }
        }

        void handleFileSend(const boost::system::error_code& error) {
            if (error) {
                return handleError(__FUNCTION__, error);
            }
            inFile.read(buf.c_array(), (std::streamsize)buf.size());

            std::streamsize bytesRead = inFile.gcount();
            bytesReadTotal += bytesRead;

            if (bytesRead < 0) {
                std::cerr << "File read error" << std::endl;
                inFile.close();
                return;
            } else if (bytesRead == 0) {
                inFile.close();
                async_read_until(mySocket, request, "\n\n",
                        boost::bind(&TcpConnection::handleRequest,
                            shared_from_this(), boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
                return;
            }

            std::cout << __FUNCTION__ << " reads " << bytesRead << "bytes, total "
                << bytesReadTotal << "bytes" << std::endl;

            boost::asio::async_write(mySocket,
                    boost::asio::buffer(buf.c_array(), bytesRead),
                    boost::asio::transfer_exactly(bytesRead),
                    boost::bind(&TcpConnection::handleFileSend, shared_from_this(),
                        boost::asio::placeholders::error));
        }
        void handleFileRecv(const boost::system::error_code& error,
                std::size_t bytesTransferred, size_t fileSize) {
            if (error) {
                return handleError(__FUNCTION__, error);
            }

            if (bytesTransferred >= 0) {
                outFile.write(buf.c_array(), (std::streamsize) bytesTransferred);
                std::cout << __FUNCTION__ << " writes " << bytesTransferred
                    << "bytes, total " << outFile.tellp() << "bytes" << std::endl;

                if (outFile.tellp() >= (std::streamsize)fileSize) {
                    outFile.close();
                    async_read_until(mySocket, request, "\n\n",
                            boost::bind(&TcpConnection::handleRequest,
                                shared_from_this(), boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
                    return;
                }
            }

            size_t remainBytes = fileSize - outFile.tellp();

            if (remainBytes >= buf.size()) {
                // 읽어야 될 남은 양이 buf.size()보다 큼
                async_read(mySocket, boost::asio::buffer(buf.c_array(), buf.size()),
                        boost::bind(&TcpConnection::handleFileRecv,
                            shared_from_this(), boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred, fileSize));
            } else {
                // 읽어야 될 남은 양이 buf.size()보다 작음
                async_read(mySocket, boost::asio::buffer(buf.c_array(), remainBytes),
                        boost::bind(&TcpConnection::handleFileRecv,
                            shared_from_this(), boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred, fileSize));
            }
        }

        void handleError(const std::string& functionName,
                const boost::system::error_code& error) {
            std::cerr << "Error in " << functionName << ": " << error << ": "
                << error.message() << std::endl;
        }

    public:
        TcpConnection(boost::asio::io_service& io_service)
            : mySocket(io_service) {}

        void start() {
            std::cout << __FUNCTION__ << std::endl;
            async_read_until(mySocket, request, "\n\n",
                    boost::bind(&TcpConnection::handleRequest,
                        shared_from_this(), boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
        }

        boost::asio::ip::tcp::socket& socket() {
            return mySocket;
        }
};

class TcpServer : private boost::noncopyable {
    private:
        boost::asio::io_service ioService;
        boost::asio::ip::tcp::acceptor acceptor;

    public:
        typedef boost::shared_ptr <TcpConnection> ptrTcpConnection;

        TcpServer(unsigned short port) : acceptor(ioService,
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), true) {
            ptrTcpConnection newConnection(new TcpConnection(ioService));
            acceptor.async_accept(newConnection->socket(),
                    boost::bind(&TcpServer::handleAccept, this, newConnection,
                        boost::asio::placeholders::error));
            ioService.run();
        }

        void handleAccept(ptrTcpConnection currentConnection,
                const boost::system::error_code& error) {
            std::cout << __FUNCTION__ << " " << error << ", " <<
                error.message() << std::endl;
            if (!error) {
                currentConnection->start();
            }
        }

        ~TcpServer()
        {
            ioService.stop();
        }
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cout << "Usage: ip" << std::endl;
            return 0;
        }

        int port = atoi(argv[1]);

        std::cout << argv[0] << " listen on port " << port << std::endl;
        TcpServer *myTcpServer = new TcpServer(port);
        delete myTcpServer;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
