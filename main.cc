#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <cmath>
#include <ctime>
#include <iostream>
#include <sstream>
#include <chrono>
#include <memory>

#define BUBBLE_SIZE 20
#define CANNON_H  60
#define CANNON_W  20
#define MAX_ANGLE 80
#define MIN_ANGLE 280
#define VELOCITY  7
#define WINDOW_H  600
#define WINDOW_W  1200
#define M_PI 3.14159265359f

class Reciever{
private:
    std::shared_ptr<sf::TcpSocket> r_socket;
    std::list<std::string>& m_queue;
public:
    void ReceiverLoop();
};

void Reciever::ReceiverLoop(){
    char buffer[256];
    while (true){
        std::memset(buffer, 0, 256);
        size_t recieved;

        if (r_socket->recieve(buffer, 256, recieved) != sf::Socket::Done){
            std::cout << "FATAL ERROR" << std::endl;
            return;
        }
        else{
            std::cout << "Recieved " << buffer << std::endl;
        }
    }
}

int main(int argc, const char* argv[])
{
    std::cout << "I am a server UwU" << std::endl;
    std::list<std::string> queue;
    //Setup a listener

    return 0;
}