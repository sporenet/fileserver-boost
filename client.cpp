#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>


class TcpClient {
    private:
        boost::asio::ip::tcp::resolver resolver;
        boost::asio::ip::tcp::socket socket;
        boost::array<char, 4096> buf;
        boost::asio::streambuf request;
        std::ifstream sourceFile;
        std::streamsize bytesReadTotal;

        void handleResolve(const boost::system::error_code& error,
                boost::asio::ip::tcp::resolver::iterator myIterator) {
            if (!error) {
                boost::asio::ip::tcp::endpoint endpoint = *myIterator;
                socket.async_connect(endpoint,
                        boost::bind(&TcpClient::handleConnect, this,
                            boost::asio::placeholders::error, ++myIterator));
            } else {
                std::cerr << "Error: " << error.message() << std::endl;
            }
        }

        void handleConnect(const boost::system::error_code& error,
                boost::asio::ip::tcp::resolver::iterator myIterator) {
            if (!error) {
                requestToServer();
            } else if (myIterator != boost::asio::ip::tcp::resolver::iterator()) {
                socket.close();
                boost::asio::ip::tcp::endpoint endpoint = *myIterator;
                socket.async_connect(endpoint,
                        boost::bind(&TcpClient::handleConnect, this,
                            boost::asio::placeholders::error, ++myIterator));
            } else {
                std::cerr << "Error: " << error.message() << std::endl;
            }
        }

        void requestToServer() {
            std::cout << ">> ";

            std::string operation;
            std::string fileName;

            std::cin >> operation;

            if (operation == "quit" or operation == "exit") {
                std::cout << "Goodbye~!" << std::endl;
                exit(0);
            }

            if (operation == "list" or operation == "ls") {
                // Not Implemented
                return requestToServer();
            }

            if (operation == "upload" or operation == "up") {
                std::cin >> fileName;
                return sendFileRequest(fileName);
            }

            if (operation == "download" or operation == "down") {
                std::cin >> fileName;
                return recvFileRequest(fileName);
            }

            // Wrong operation
            std::cout << "Wrong operation, please try again" << std::endl;
            requestToServer();
        }

        void sendFileRequest(std::string& fileName) {
            sourceFile.open(fileName.c_str(),
                    std::ios_base::binary | std::ios_base::ate);
            if (!sourceFile) {
                std::cout << "Failed to open " << fileName << std::endl;
                return;
            }

            size_t fileSize = sourceFile.tellg();
            sourceFile.seekg(0);

            std::ostream requestStream(&request);
            requestStream << "u\n" << fileName << "\n" << fileSize << "\n\n";
            std::cout << "Request size: " << request.size()
                << "bytes" << std::endl;

            bytesReadTotal = 0;

            boost::asio::async_write(socket, request,
                    boost::bind(&TcpClient::handleSendFile, this,
                        boost::asio::placeholders::error));
        }

        void recvFileRequest(std::string& fileName) {
/*            std::ostream requestStream(&request);
            requestStream << "d\n" << fileName << "\n\n";
            std::cout << "Request size: " << request.size()
                << "bytes" << std::endl;

            boost::asio::async_write(socket, request,
                    boost::bind(&TcpClient::handleWriteFile, this,
                        boost::asio::placeholders::error));

            // Not implemented
*/        }

        void handleSendFile(const boost::system::error_code& error) {
            if (!error) {
                sourceFile.read(buf.c_array(), (std::streamsize)buf.size());

                std::streamsize bytesRead = sourceFile.gcount();
                bytesReadTotal += bytesRead;

                if (bytesRead < 0) {
                    std::cerr << "File read error" << std::endl;
                    sourceFile.close();
                    return;
                } else if (bytesRead == 0) {
                    sourceFile.close();
                    requestToServer();
                    return;
                }

                std::cout << "Send " << bytesRead << "bytes, total "
                    << bytesReadTotal << "bytes" << std::endl;

                boost::asio::async_write(socket,
                        boost::asio::buffer(buf.c_array(), bytesRead),
                        boost::asio::transfer_exactly(bytesRead),
                        boost::bind(&TcpClient::handleSendFile, this,
                            boost::asio::placeholders::error));
            } else {
                std::cerr << "Error: " << error.message() << std::endl;
            }
        }

    public:
        TcpClient(boost::asio::io_service& ioService, const std::string& server,
                const std::string& port)
            : resolver(ioService), socket(ioService) {
                boost::asio::ip::tcp::resolver::query query(server, port);
                resolver.async_resolve(query, boost::bind(&TcpClient::handleResolve, this,
                            boost::asio::placeholders::error, boost::asio::placeholders::iterator));
            }
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cout << "Usage: ip port#" << std::endl;
            return 0;
        }

        boost::asio::io_service ioService;
        TcpClient client(ioService, argv[1], argv[2]);
        ioService.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

