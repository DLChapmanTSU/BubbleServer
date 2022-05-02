#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <cmath>
#include <ctime>
#include <iostream>
#include <sstream>
#include <chrono>
#include <memory>
#include <list>
#include <cstring>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <functional>

#define BUBBLE_SIZE 20
#define CANNON_H  60
#define CANNON_W  20
#define MAX_ANGLE 80
#define MIN_ANGLE 280
#define VELOCITY  7
#define WINDOW_H  600
#define WINDOW_W  1200
#define M_PI 3.14159265359f
#define PORT 555

sf::IpAddress _p1Address;
sf::IpAddress _p2Address;
unsigned short _p1Port;
unsigned short _p2Port;
sf::TcpSocket _connector;
sf::TcpSocket _connector2;

class Reciever;
class Accepter;
//class Queue;

enum class UserInput{
    E_SHOOT_PRESSED,
    E_SHOOT_RELEASED,
    E_LEFT_PRESSED,
    E_LEFT_RELEASED,
    E_RIGHT_PRESSED,
    E_RIGHT_RELEASED
};

struct ClientData{
    std::string c_name;
    int c_points;
    u_int8_t c_input;
    std::string c_message;
    float c_rotation;
};

sf::Packet& operator >>(sf::Packet& packet, ClientData& player)
{
    return packet >> player.c_name >> player.c_points >> player.c_input >> player.c_message >> player.c_rotation;
}

sf::Packet& operator <<(sf::Packet& packet, const ClientData& player)
{
    return packet << player.c_name << player.c_points << player.c_input << player.c_message << player.c_rotation;
}

struct ConnectionData{
    int c_seed;
    bool c_isPlayerOne;
};

sf::Packet& operator >>(sf::Packet& packet, ConnectionData& c)
{
    return packet >> c.c_seed >> c.c_isPlayerOne;
}

sf::Packet& operator <<(sf::Packet& packet, const ConnectionData& c)
{
    return packet << c.c_seed << c.c_isPlayerOne;
}

//Queue class
//Cannot just use std::queue as we we require locks to prevent overlapping
template <typename T>
class Queue
{
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
public:
    T Pop(){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this]{return !m_queue.empty();});
        auto val = m_queue.front();
        m_queue.pop();
        return val;
    };

    void Pop(T& i){
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty() == true){
            return;
        }

        i = m_queue.front();
        m_queue.pop();
    };

    void Push(const T& i){
        std::unique_lock<std::mutex> lock(m_mutex);
        std::cout << "Pushed" << std::endl;
        m_queue.push(i);
        m_cond.notify_one();
        std::cout << m_queue.size() << std::endl;
    };

    Queue() = default;
    Queue(const Queue&) = delete;
    Queue operator=(const Queue&) = delete;
};

template <typename T>
class List{
private:
    std::list<T> m_list;
    std::mutex m_mutex;
public:
    void ForEach(std::function<void(T)> f){
        std::cout << "Looping through " << m_list.size() << " items" << std::endl;
        std::unique_lock<std::mutex> lock(m_mutex);
        std::for_each(m_list.begin(), m_list.end(), f);
    };

    void Push(const T& i){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_list.push_back(i);
        std::cout << m_list.size() << std::endl;
    }

    List() = default;
    List(const List&) = delete;
    List operator=(const List&) = delete;
};

class Reciever{
private:
    std::shared_ptr<sf::TcpSocket> r_socket;
    Queue<std::pair<ClientData, bool>>& r_queue;
public:
    Reciever(std::shared_ptr<sf::TcpSocket>& s, Queue<std::pair<ClientData, bool>>& q);
    void ReceiverLoop();
};

Reciever::Reciever(std::shared_ptr<sf::TcpSocket>& s, Queue<std::pair<ClientData, bool>>& q) : r_socket(s), r_queue(q){
    //r_socket = s;
    //r_queue = q;
}

void Reciever::ReceiverLoop(){
    char buffer[256];
    sf::Packet packet;
    std::string name;
    int points;
    u_int8_t input;
    std::string message;
    float rotation;

    while (true){
        std::memset(buffer, 0, 256);
        size_t recieved;

        if (r_socket->receive(packet) != sf::Socket::Done){
            std::cout << "FATAL RECIEVER ERROR" << std::endl;
            return;
        }
        else{
            std::cout << "Reciever Loop Recieved " << buffer << std::endl;
        }

        if (packet >> name >> points >> input >> message >> rotation){
            //std::cout << "Reciever Loop Recieved:\n" << name << "\n" << points << "\n" << input << "\n" << message << std::endl;
        }

        bool isP1 = false;

        if (r_socket->getRemoteAddress() == _p1Address){
            std::cout << "Recieved from player one" << std::endl;
            isP1 = true;
        }
        else if (r_socket->getRemoteAddress() == _p2Address){
            std::cout << "Recieved from player two" << std::endl;
            isP1 = false;
        }

        ClientData d;
        d.c_name = name;
        d.c_points = points;
        d.c_input = input;
        d.c_message = message;
        d.c_rotation = rotation;
        

        std::pair<ClientData, bool> p;
        p.first = d;
        p.second = isP1;

        r_queue.Push(p);
    }

    //r_socket->disconnect();
    //r_socket->close();
}

class Accepter{
private:
    List<std::shared_ptr<sf::TcpSocket>>& a_socket;
    Queue<std::pair<ClientData, bool>>& a_queue;
public:
    Accepter(List<std::shared_ptr<sf::TcpSocket>>& s, Queue<std::pair<ClientData, bool>>& q);
    void operator()();
    //void AcceptLoop();
};

Accepter::Accepter(List<std::shared_ptr<sf::TcpSocket>>& s, Queue<std::pair<ClientData, bool>>& q) : a_socket(s), a_queue(q){ }

void Accepter::operator()(){
    sf::TcpListener listener;
    if (listener.listen(55561) != sf::Socket::Done){
        std::cout << "FATAL ACCEPTER ERROR" << std::endl;
        return;
    }

    while(true){
        std::shared_ptr<sf::TcpSocket> socket = std::make_shared<sf::TcpSocket>();
        if (listener.accept(*socket) != sf::Socket::Done){
            std::cout << "Could not accept" << std::endl;
            return;
        }
        else{
            std::cout << "Connection accepted" << std::endl;
            if (_connector.getRemoteAddress() == sf::IpAddress::None){
                std::cout << "Player 1 Connected" << std::endl;
                _p1Address = socket->getRemoteAddress();
                _p1Port = socket->getRemotePort();
                std::cout << _p1Address << std::endl;
                std::cout << _p1Port << std::endl;
                a_socket.Push(socket);
                std::shared_ptr<Reciever> r = std::make_shared<Reciever>(socket, a_queue);
                std::thread(&Reciever::ReceiverLoop, r).detach();
                _connector.connect(_p1Address, 55562);
            }
            else if (_connector2.getRemoteAddress() == sf::IpAddress::None){
                std::cout << "Player 2 Connected" << std::endl;
                _p2Address = socket->getRemoteAddress();
                _p2Port = socket->getRemotePort();
                std::cout << _p2Address << std::endl;
                std::cout << _p2Port << std::endl;
                a_socket.Push(socket);
                std::shared_ptr<Reciever> r = std::make_shared<Reciever>(socket, a_queue);
                std::thread(&Reciever::ReceiverLoop, r).detach();
                _connector2.connect(_p2Address, 55562);
            }
        }
    }

    listener.close();
}

//void Accepter::AcceptLoop(){
//    sf::TcpListener listener;
//    if (listener.listen(PORT) != sf::Socket::Done){
//        std::cout << "FATAL ERROR" << std::endl;
//    }

//    while(true){
//        std::shared_ptr<sf::TcpSocket> socket = std::make_shared<sf::TcpSocket>();
//        if (listener.accept(*socket) != sf::Socket::Done){
//            std::cout << "Could not accept" << std::endl;
//            return;
//        }
//        else{
//            a_socket.Push(socket);
//            std::shared_ptr<Reciever> r = std::make_shared<Reciever>(socket, a_queue);
//            std::thread(&Reciever::ReceiverLoop, r).detach();
//        }
//    }
//}


struct Room{
    bool r_p1Ready{ false };
    bool r_p2Ready{ false };
};


sf::IpAddress HandleUDPBroadcast(int s1, int s2, bool p){
    sf::UdpSocket socket;
    sf::UdpSocket senderSocket;

    // bind the socket to a port
    if (socket.bind(55571) != sf::Socket::Done)
    {
        return sf::IpAddress::None;
    }

    if (senderSocket.bind(55573) != sf::Socket::Done)
    {
        return sf::IpAddress::None;
    }

    char data[100];
    size_t received;
    sf::IpAddress remoteIP;
    unsigned short remotePort;

    std::cout << "Waiting for udp broadcast" << std::endl;

    if (socket.receive(data, 100, received, remoteIP, remotePort) != sf::Socket::Done){
        std::cout << "Failed to recieve" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Recieved: " << data << " from broadcast at " << remoteIP << " :: " << remotePort << std::endl;
        //return 1;
    }

    sf::Packet packet;
    ConnectionData cData;
    cData.c_isPlayerOne = p;
    cData.c_seed = s1;

    packet << cData;

    int newData = s1;
    int newData2 = s2;

    if (senderSocket.send(&newData, 100, remoteIP, 55572) != sf::Socket::Done){
        std::cout << "Could not broadcast" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Sent seed " << s1 << std::endl;
    }

    if (senderSocket.send(&newData2, 100, remoteIP, 55572) != sf::Socket::Done){
        std::cout << "Could not broadcast" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Sent seed " << s2 << std::endl;
    }

    bool playerNumberData = p;

    if (senderSocket.send(&playerNumberData, 100, remoteIP, 55572) != sf::Socket::Done){
        std::cout << "Could not broadcast" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Sent seed Is P1: " << p << std::endl;
    }


    //socket.close();
    //senderSocket.close();

    return remoteIP;
}

int main(int argc, const char* argv[])
{
    std::srand(time(NULL));
    
    std::cout << "I am a server" << std::endl;
    std::cout << sf::IpAddress::getLocalAddress() << std::endl;
    Queue<std::pair<ClientData, bool>> queue;
    List<std::shared_ptr<sf::TcpSocket>> sockets;
    //Accepter a(sockets, queue);
    std::thread(Accepter(sockets, queue)).detach();
    //Setup a listener
    //sf::CircleShape c(4);
    //c.getLocalBounds();

    
    int seed1 = (std::rand() % 64) + 1;
    int seed2 = (std::rand() % 64) + 1;
    std::srand(seed1);

    for (size_t i = 0; i < 5; i++)
    {
        std::cout << std::rand() % 5 << std::endl;
    }
    

    _p1Address = HandleUDPBroadcast(seed1, seed2, true);
    _p2Address = HandleUDPBroadcast(seed1, seed2, false);

    

    bool bothConnected{ false };
    bool gameStarted{ false };
    Room gameRoom;


    while (true){

        if (bothConnected == false){
            if (_connector.getRemoteAddress() != sf::IpAddress::None && _connector2.getRemoteAddress() != sf::IpAddress::None){
                //All UDP pings in the above function are blocking
                //Send out a TCP ping to each player to tell them to start the simulation
                //The recievers on the client side should be blocking
                ClientData startData;
                startData.c_input = 10;
                startData.c_message = "Lobby Full";
                startData.c_name = "Server";
                startData.c_points = 0;

                sf::Packet startPacket;
                startPacket << startData;

                std::cout << "Sending start to: " << _connector.getRemoteAddress() << std::endl;
                std::cout << "Should connect to: " << _p1Address << std::endl;
                if (_connector.send(startPacket) != sf::Socket::Done){
                    std::cerr << "Failed to send data to client" << std::endl;
                    return 1;
                }
                else{
                    std::cout << "Sent: Start Notification" << std::endl;
                    std::cout << "Player One" << std::endl;
                }

                std::cout << "Sending start to: " << _connector2.getRemoteAddress() << std::endl;
                std::cout << "Should connect to: " << _p2Address << std::endl;

                //_connector2.connect(_p2Address, 55562);

                if (_connector2.send(startPacket) != sf::Socket::Done){
                    std::cerr << "Failed to send data to client" << std::endl;
                    return 1;
                }
                else{
                    std::cout << "Sent: Start Notification" << std::endl;
                    std::cout << "Player Two" << std::endl;
                }

                bothConnected = true;
            }
            continue;
        }

        if (gameRoom.r_p1Ready == true && gameRoom.r_p2Ready == true && gameStarted == false){
            ClientData readyData;
            readyData.c_message = "Start";
            readyData.c_input = 0;
            readyData.c_name = "Server";
            readyData.c_points = 0;
            readyData.c_rotation = 0;
            sf::Packet readyPacket;
            readyPacket << readyData;
            gameStarted = true;
            std::cout << "Game Starting" << std::endl;

            if (_connector.send(readyPacket) != sf::Socket::Done){
                std::cerr << "Failed to send data to client" << std::endl;
                return 1;
            }
            else{
                std::cout << "Sent: " << readyData.c_message << std::endl;
                std::cout << "Player One" << std::endl;
            }

            if (_connector2.send(readyPacket) != sf::Socket::Done){
                std::cerr << "Failed to send data to client" << std::endl;
                return 1;
            }
            else{
                std::cout << "Sent: " << readyData.c_message << std::endl;
                std::cout << "Player One" << std::endl;
            }

            continue;
        }

        std::cout << "Looping started" << std::endl;
        std::pair<ClientData, bool> popped = queue.Pop();
        std::cout << "Popped" << std::endl;
        ClientData next = popped.first;
        std::cout << "Recieved: " << next.c_name << std::endl;

        

        if (gameRoom.r_p1Ready == false || gameRoom.r_p2Ready == false){
            std::cout << "Waiting for ready up" << std::endl;
            if (popped.second == true){
                if (next.c_message == "Ready"){
                    gameRoom.r_p1Ready = true;
                    std::cout << "p1 ready" << std::endl;
                }
            }
            else{
                if (next.c_message == "Ready"){
                    gameRoom.r_p2Ready = true;
                    std::cout << "p2 ready" << std::endl;
                }
            }

            std::cout << "Continue" << std::endl;

            continue;
        }

        sf::Packet packet;
        packet << next;

        std::cout << "Sending message from " << next.c_name << std::endl;

        if (popped.second == true){
            if (_connector2.send(packet) != sf::Socket::Done){
                std::cerr << "Failed to send data to client" << std::endl;
                return 1;
            }
            else{
                std::cout << "Sent: " << next.c_name << std::endl;
                std::cout << "Player Two" << std::endl;
            }
        }
        else{
            if (_connector.send(packet) != sf::Socket::Done){
                std::cerr << "Failed to send data to client" << std::endl;
                return 1;
            }
            else{
                std::cout << "Sent: " << next.c_name << std::endl;
                std::cout << "Player One" << std::endl;
            }
        }

        /*
        auto send = [&] (std::shared_ptr<sf::TcpSocket> socket){
            //if (socket->connect("152.105.67.105", 55562)){
            //    std::cout << "Connected to client" << std::endl;
            //}

            sf::Packet packet;
            packet << next;

            std::cout << "Sending message from " << next.c_name << std::endl;

            if (popped.second == true){
                if (_connector2.send(packet) != sf::Socket::Done){
                    std::cerr << "Failed to send data to client" << std::endl;
                    return 1;
                }
                else{
                    std::cout << "Sent: " << next.c_name << std::endl;
                    std::cout << "Player Two" << std::endl;
                }
            }
            else{
                if (_connector.send(packet) != sf::Socket::Done){
                    std::cerr << "Failed to send data to client" << std::endl;
                    return 1;
                }
                else{
                    std::cout << "Sent: " << next.c_name << std::endl;
                    std::cout << "Player One" << std::endl;
                }
            }
            
            return 0;
        };
        */
        

        //sockets.ForEach(send);
    }

    return 0;
}