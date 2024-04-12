#include <iostream>
#include <boost/asio.hpp>
#include <chrono>

using namespace boost::asio;
using namespace boost::asio::ip;

struct ClientData
{
    ip::tcp::socket socket;

    ClientData(ip::tcp::socket socket) : socket(std::move(socket)) {}
};

struct Color {
    int R;
    int G;
    int B;
};

std::list<ClientData> clientDataList;

std::mutex m;

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t signature;
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t imageSize;
    uint32_t xPixelsPerMeter;
    uint32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};
#pragma pack(pop)

Color getRandomColor() {
    Color randomColor;
    srand(time(nullptr));
    randomColor.R = rand() % 256;
    randomColor.G = rand() % 256;
    randomColor.B = rand() % 256;
    return randomColor;
}

uint8_t* generateBMPImage(int width, int height) {
    BMPHeader header;
    header.signature = 0x4D42;
    header.fileSize = sizeof(BMPHeader) + width * height * 3;
    header.reserved = 0;
    header.dataOffset = sizeof(BMPHeader);
    header.headerSize = 40;
    header.width = width;
    header.height = height;
    header.planes = 1;
    header.bpp = 24;
    header.compression = 0;
    header.imageSize = width * height * 3;
    header.xPixelsPerMeter = 0;
    header.yPixelsPerMeter = 0;
    header.colorsUsed = 0;
    header.colorsImportant = 0;

    uint8_t* imageData = new uint8_t[width * height * 3];

    Color color = getRandomColor();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = (y * width + x) * 3;
            imageData[index] = color.R;
            imageData[index + 1] = color.G;
            imageData[index + 2] = color.B;
        }
    }

    uint8_t* bmpData = new uint8_t[sizeof(BMPHeader) + width * height * 3];
    std::memcpy(bmpData, &header, sizeof(BMPHeader));
    std::memcpy(bmpData + sizeof(BMPHeader), imageData, width * height * 3);

    delete[] imageData;

    return bmpData;
}

void threadFunction()
{
    try
    {
        while (true)
        {
            m.lock();
            if (clientDataList.empty())
            {
                m.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }

            ClientData data = std::move(clientDataList.front());
            clientDataList.pop_front();
            m.unlock();

            boost::asio::streambuf request_buf;
            boost::asio::read_until(data.socket, request_buf, "\r\n\r\n");
            std::istream request_stream(&request_buf);
            std::string request;
            std::getline(request_stream, request);

            if (request.substr(0, 3) == "GET") {
                int width = 100;
                int height = 100;
                uint8_t* bmpData = generateBMPImage(width, height);

                std::ostringstream response_stream;
                response_stream << "HTTP/1.1 200 OK\r\n";
                response_stream << "Server: MySuperServer\r\n";
                response_stream << "Content-Type: image/bmp\r\n";
                response_stream << "Content-Length: " << sizeof(BMPHeader) + width * height * 3 << "\r\n";
                response_stream << "Connection: close\r\n\r\n";

                write(data.socket, buffer(response_stream.str()));

                write(data.socket, buffer(bmpData, sizeof(BMPHeader) + width * height * 3));

                delete[] bmpData;
            }

            data.socket.close();
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
}

std::array<std::thread, 4> threadArr;

int main()
{
    setlocale(LC_ALL, "Russian");
    try
    {
        io_context ctx;

        tcp::endpoint ep(address::from_string("0.0.0.0"), 10000);

        ip::tcp::acceptor acc(ctx, ep);

        threadArr[0] = std::thread(&threadFunction);
        threadArr[1] = std::thread(&threadFunction);
        threadArr[2] = std::thread(&threadFunction);
        threadArr[3] = std::thread(&threadFunction);

        while (true)
        {

            ip::tcp::socket socket = acc.accept();

            std::cout << "Someone connected" << std::endl;

            ClientData clientData{ std::move(socket) };

            clientDataList.insert(clientDataList.end(), std::move(clientData));
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Server error: " << e.what() << std::endl;
    }
}
