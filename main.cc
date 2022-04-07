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
};

sf::Packet& operator >>(sf::Packet& packet, ClientData& player)
{
    return packet >> player.c_name >> player.c_points >> player.c_input >> player.c_message;
}

sf::Packet& operator <<(sf::Packet& packet, const ClientData& player)
{
    return packet << player.c_name << player.c_points << player.c_input << player.c_message;
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

        if (packet >> name >> points >> input >> message){
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

        std::pair<ClientData, bool> p;
        p.first = d;
        p.second = isP1;

        r_queue.Push(p);
    }
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





sf::IpAddress HandleUDPBroadcast(int s){
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

    if (socket.receive(data, 100, received, remoteIP, remotePort) != sf::Socket::Done){
        std::cout << "Failed to recieve" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Recieved: " << data << " from broadcast" << std::endl;
        //return 1;
    }

    //char newData[100] = s;

    if (senderSocket.send(&s, 100, remoteIP, 55572) != sf::Socket::Done){
        std::cout << "Could not broadcast" << std::endl;
        return sf::IpAddress::None;
    }
    else{
        std::cout << "Sent seed " << s << std::endl;
    }

    //socket.close();
    //senderSocket.close();

    return remoteIP;
}

int main(int argc, const char* argv[])
{
    std::srand(time(NULL));
    
    std::cout << "I am a server" << std::endl;
    Queue<std::pair<ClientData, bool>> queue;
    List<std::shared_ptr<sf::TcpSocket>> sockets;
    //Accepter a(sockets, queue);
    std::thread(Accepter(sockets, queue)).detach();
    //Setup a listener
    //sf::CircleShape c(4);
    //c.getLocalBounds();

    int seed = std::rand() % 99999;

    _p1Address = HandleUDPBroadcast(seed);
    _p2Address = HandleUDPBroadcast(seed);

    while (true){
        std::cout << "Looping started" << std::endl;
        std::pair<ClientData, bool> popped = queue.Pop();
        std::cout << "Popped" << std::endl;
        ClientData next = popped.first;
        std::cout << "Recieved: " << next.c_name << std::endl;

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