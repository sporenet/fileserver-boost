#include "client.hpp"
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>


TcpClient::TcpClient(boost::asio::io_service& ioService, const std::string& _userName,
        const std::string& server, const std::string& port)
    : userName(_userName), resolver(ioService), socket(ioService) {
        boost::asio::ip::tcp::resolver::query query(server, port);
        resolver.async_resolve(query, boost::bind(&TcpClient::handleResolve, this,
                    boost::asio::placeholders::error, boost::asio::placeholders::iterator));
}

void TcpClient::handleResolve(const boost::system::error_code& error,
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

void TcpClient::handleConnect(const boost::system::error_code& error,
        boost::asio::ip::tcp::resolver::iterator myIterator) {
    if (!error) {
        userNameRequest();
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

void TcpClient::fileSendRequest(const std::string& fileName) {
    upFile.open(fileName.c_str(), std::ios_base::binary | std::ios_base::ate);
    if (!upFile) {
        std::cout << "Failed to open " << fileName << std::endl;
        return;
    }

    size_t fileSize = upFile.tellg();
    upFile.seekg(0);

    std::ostream requestStream(&request);
    requestStream << "u\n" << fileName << "\n" << fileSize << "\n\n";
//    std::cout << "Request size: " << request.size()
//        << "bytes" << std::endl;

    bytesReadTotal = 0;

    std::cout << fileName << " is being uploaded... " << std::flush;

    async_write(socket, request,
            boost::bind(&TcpClient::handleFileSend, this,
                boost::asio::placeholders::error));
}

void TcpClient::fileRecvRequest(const std::string& fileName) {
    std::ostream requestStream(&request);
    requestStream << "d\n" << fileName << "\n\n";
//    std::cout << "Request size: " << request.size()
//        << "bytes" << std::endl;

    downFile.open(fileName.c_str(), std::ios_base::binary);

    if (!downFile) {
        std::cerr << "Failed to open " << fileName << std::endl;
        return;
    }

    std::cout << fileName << " is being downloaded... " << std::flush;

    async_write(socket, request,
            boost::bind(&TcpClient::handleFileRecvAckSub, this,
                boost::asio::placeholders::error));
}

void TcpClient::handleFileSend(const boost::system::error_code& error) {
    if (!error) {
        upFile.read(buf.c_array(), (std::streamsize)buf.size());

        std::streamsize bytesRead = upFile.gcount();
        bytesReadTotal += bytesRead;

        if (bytesRead < 0) {
            std::cerr << "File read error" << std::endl;
            upFile.close();
            return;
        } else if (bytesRead == 0) {
            upFile.close();
            std::cout << "Done" << std::endl;
            requestToServer();
            return;
        }

//        std::cout << "Send " << bytesRead << "bytes, total "
//            << bytesReadTotal << "bytes" << std::endl;

        async_write(socket, boost::asio::buffer(buf.c_array(), bytesRead),
                boost::asio::transfer_exactly(bytesRead),
                boost::bind(&TcpClient::handleFileSend, this,
                    boost::asio::placeholders::error));
    } else {
        std::cerr << "Error: " << error.message() << std::endl;
    }
}

void TcpClient::handleFileRecvAckSub(const boost::system::error_code& error) {
    if (!error) {
        async_read_until(socket, ack, "\n\n",
                boost::bind(&TcpClient::handleFileRecvAck, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    } else {
        std::cerr << "Error: " << error.message() << std::endl;
    }
}

void TcpClient::handleFileRecvAck(const boost::system::error_code& error,
        const std::size_t bytesTransferred) {
    if (!error) {
        std::istream ackStream(&ack);
//        std::cout << "Ack size: " << ack.size() << "bytes" << std::endl;

        std::size_t fileSize;
        ackStream >> fileSize;
        ackStream.read(buf.c_array(), 2);

        std::streamsize bytesRead = 0;

        // ack stream의 잔여 바이트를 파일에 씀
        // async_read_until의 동작때문
        do {
            ackStream.read(buf.c_array(), (std::streamsize)buf.size());
            bytesRead += ackStream.gcount();
//            std::cout << "Writes " << ackStream.gcount() << "bytes, total "
//                << bytesRead << "bytes" << std::endl;
            downFile.write(buf.c_array(), ackStream.gcount());
        } while (ackStream.gcount() > 0);

        std::size_t remainBytes = fileSize - bytesRead;

        if (remainBytes == 0) {
            // 다 읽음
            downFile.close();
            return requestToServer();
        } else if (remainBytes >= buf.size()) {
            // 읽어야 될 남은 양이 buf.size()보다 크거나 같음
            async_read(socket, boost::asio::buffer(buf.c_array(), buf.size()),
                    boost::bind(&TcpClient::handleFileRecv, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, fileSize));
        } else {
            // 읽어야 될 남은 양이 buf.size()보다 작음
            async_read(socket, boost::asio::buffer(buf.c_array(), remainBytes),
                    boost::bind(&TcpClient::handleFileRecv, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, fileSize));
        }
    } else {
        std::cerr << "Error: " << error.message() << std::endl;
    }
}

void TcpClient::handleFileRecv(const boost::system::error_code& error,
        const std::size_t bytesTransferred, const std::size_t fileSize) {
    if (!error) {
        if (bytesTransferred >= 0) {
            downFile.write(buf.c_array(), (std::streamsize)bytesTransferred);
//            std::cout << "Writes " << bytesTransferred << "bytes, total "
//                << downFile.tellp() << "bytes" << std::endl;

            if (downFile.tellp() >= (std::streamsize)fileSize) {
                downFile.close();
                std::cout << "Done" << std::endl;
                return requestToServer();
            }
        }

        std::size_t remainBytes = fileSize - downFile.tellp();

        if (remainBytes >= buf.size()) {
            // 읽어야 될 남은 양이 buf.size()보다 큼
            async_read(socket, boost::asio::buffer(buf.c_array(), buf.size()),
                    boost::bind(&TcpClient::handleFileRecv, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, fileSize));
        } else {
            // 읽어야 될 남은 양이 buf.size()보다 작음
            async_read(socket, boost::asio::buffer(buf.c_array(), remainBytes),
                    boost::bind(&TcpClient::handleFileRecv, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred, fileSize));
        }
    } else {
        std::cerr << "Error: " << error.message() << std::endl;
    }
}

void TcpClient::userNameRequest() {
    std::ostream requestStream(&request);
    requestStream << userName << "\n\n";
//    std::cout << "Request size: " << request.size() << "bytes" << std::endl;

    async_write(socket, request,
            boost::bind(&TcpClient::requestToServer, this));
}



void TcpClient::requestToServer() {
    std::cout << ">> ";

    std::string operation;
    std::string fileName;

    std::cin >> operation;

    // Quit or Exit
    if (operation == "quit" or operation == "exit") {
        std::cout << "Goodbye~!" << std::endl;
        exit(0);
    }

    // List
    if (operation == "list" or operation == "ls") {
        // Not Implemented
        return requestToServer();
    }

    // Upload
    if (operation == "upload" or operation == "up") {
        std::cin >> fileName;
        return fileSendRequest(fileName);
    }

    // Download
    if (operation == "download" or operation == "down") {
        std::cin >> fileName;
        return fileRecvRequest(fileName);
    }

    // Wrong Operation
    std::cout << "Wrong operation, please try again" << std::endl;
    requestToServer();
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ip port#" << std::endl;
        return 0;
    }

    std::string userName;

    std::cout << "Username: ";
    std::cin >> userName;

    boost::asio::io_service ioService;
    TcpClient client(ioService, userName, argv[1], argv[2]);
    ioService.run();

    return 0;
}

