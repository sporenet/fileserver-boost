#include "connection.hpp"
#include <dirent.h>
#include <boost/bind.hpp>

TcpConnection::TcpConnection(boost::asio::io_service& ioService)
    : mySocket(ioService) {}

    void TcpConnection::start() {
        std::cout << __FUNCTION__ << std::endl;
        async_read_until(mySocket, request, "\n\n",
                boost::bind(&TcpConnection::handleUserName,
                    shared_from_this(), boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    }

boost::asio::ip::tcp::socket& TcpConnection::socket() {
    return mySocket;
}

void TcpConnection::handleUserName(const boost::system::error_code& error,
        const std::size_t bytesTransferred) {
    if (error) {
        return handleError(__FUNCTION__, error);
    }

    std::cout << __FUNCTION__ << "(" << bytesTransferred << ")"
        << ", in_avail = " << request.in_avail()
        << ", size = " << request.size()
        << std::endl;

    std::istream requestStream(&request);

    requestStream >> this->userName;
    requestStream.read(buf.c_array(), 2);

    // User name을 가지는 폴더 없다면 만들기!
    mkdir(userName.c_str(), 0777);
    root = userName + "/";

    async_read_until(mySocket, request, "\n\n",
            boost::bind(&TcpConnection::handleRequest,
                shared_from_this(), boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}


void TcpConnection::handleRequest(const boost::system::error_code& error,
        const std::size_t bytesTransferred) {
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
    std::string fileName;
    std::size_t fileSize;

    requestStream >> operation;
    if (operation == "u") {
        requestStream >> fileName;
        requestStream >> fileSize;
        requestStream.read(buf.c_array(), 2);

        std::streamsize bytesRead = 0;

        //std::cout << fileName << " size is " << fileSize << std::endl;
        std::size_t pos = fileName.find_last_of('/');
        if (pos != std::string::npos)
            fileName = fileName.substr(pos + 1);
        std::cout << "Request for upload " << fileName << ": "
            << fileSize << "bytes" << std::endl;

        std::string filePath = root + fileName;
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

        std::size_t remainBytes = fileSize - bytesRead;

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
        requestStream >> fileName;
        requestStream.read(buf.c_array(), 2);

        std::string filePath = root + fileName;
        inFile.open(filePath.c_str(),
                std::ios_base::binary | std::ios_base::ate);

        if (!inFile) {
            std::cerr << "Error in " << __FUNCTION__ << ": failed to open file" << std::endl;
            return;
        }

        std::size_t fileSize = inFile.tellg();
        inFile.seekg(0);

        bytesReadTotal = 0;

        std::ostream ackStream(&ack);
        std::cout << "Request for download " << fileName << ": "
            << fileSize << "bytes" << std::endl;

        ackStream << fileSize << "\n\n";

        async_write(mySocket, ack,
                boost::bind(&TcpConnection::handleFileSend,
                    shared_from_this(), boost::asio::placeholders::error));
    } else if (operation == "l") {
        requestStream.read(buf.c_array(), 2);

        DIR *dir;
        struct dirent *ent;
        FILE *fp = NULL;
        std::size_t fileSize;
        int fileCount = 0;
        std::string fileName, filePath;

        if ((dir = opendir(root.c_str())) == NULL) {
            std::cerr << "opendir error" << std::endl;
            exit(1);
        }

        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 or strcmp(ent->d_name, "..") == 0)
                continue;
            fileCount++;
        }

        closedir(dir);

        if ((dir = opendir(root.c_str())) != NULL) {
            std::ostream ackStream(&ack);
            ackStream << fileCount << "\n";

            while ((ent = readdir(dir)) != NULL) {
                fileName = ent->d_name;
                filePath = root + fileName;

                fp = fopen(filePath.c_str(), "rb");
                fseek(fp, 0, SEEK_END);
                fileSize = ftell(fp);
                fclose(fp);

                if (strcmp(fileName.c_str(), ".") == 0 || strcmp(fileName.c_str(), "..") == 0)
                    continue;

                ackStream << fileName << "\n" << fileSize << "\n";
            }
            ackStream << "\n";
            closedir(dir);
        } else {
            std::cerr << "opendir error" << std::endl;
            exit(1);
        }

        async_write(mySocket, ack,
                boost::bind(&TcpConnection::handleList,
                    shared_from_this(), boost::asio::placeholders::error));
    }
}

void TcpConnection::handleFileSend(const boost::system::error_code& error) {
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

    async_write(mySocket,
            boost::asio::buffer(buf.c_array(), bytesRead),
            boost::asio::transfer_exactly(bytesRead),
            boost::bind(&TcpConnection::handleFileSend, shared_from_this(),
                boost::asio::placeholders::error));
}

void TcpConnection::handleFileRecv(const boost::system::error_code& error,
        std::size_t bytesTransferred, std::size_t fileSize) {
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

    std::size_t remainBytes = fileSize - outFile.tellp();

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

void TcpConnection::handleList(const boost::system::error_code& error) {
    if (error) {
        return handleError(__FUNCTION__, error);
    }

    async_read_until(mySocket, request, "\n\n",
            boost::bind(&TcpConnection::handleRequest,
                shared_from_this(), boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}

void TcpConnection::handleError(const std::string& functionName,
        const boost::system::error_code& error) {
    std::cerr << "Error in " << functionName << ": " << error << ": "
        << error.message() << std::endl;
}
