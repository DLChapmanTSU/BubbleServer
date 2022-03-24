#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
//#include <TcpSocket.hpp>

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

class Reciever;
class Accepter;
//class Queue;

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
        m_queue.push(i);
        m_cond.notify_one();
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
    Queue<std::string>& r_queue;
public:
    Reciever(std::shared_ptr<sf::TcpSocket>& s, Queue<std::string>& q);
    void ReceiverLoop();
};

Reciever::Reciever(std::shared_ptr<sf::TcpSocket>& s, Queue<std::string>& q) : r_socket(s), r_queue(q){
    //r_socket = s;
    //r_queue = q;
}

void Reciever::ReceiverLoop(){
    char buffer[256];
    while (true){
        std::memset(buffer, 0, 256);
        size_t recieved;

        if (r_socket->receive(buffer, 256, recieved) != sf::Socket::Done){
            std::cout << "FATAL RECIEVER ERROR" << std::endl;
            return;
        }
        else{
            std::cout << "Reciever Loop Recieved " << buffer << std::endl;
        }

        r_queue.Push(std::string(buffer));
    }
}

class Accepter{
private:
    List<std::shared_ptr<sf::TcpSocket>>& a_socket;
    Queue<std::string>& a_queue;
public:
    Accepter(List<std::shared_ptr<sf::TcpSocket>>& s, Queue<std::string>& q);
    void operator()();
    //void AcceptLoop();
};

Accepter::Accepter(List<std::shared_ptr<sf::TcpSocket>>& s, Queue<std::string>& q) : a_socket(s), a_queue(q){ }

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
            _p1Address = socket->getRemoteAddress();
            _p1Port = socket->getRemotePort();
            std::cout << _p1Address << std::endl;
            std::cout << _p1Port << std::endl;
            a_socket.Push(socket);
            std::shared_ptr<Reciever> r = std::make_shared<Reciever>(socket, a_queue);
            std::thread(&Reciever::ReceiverLoop, r).detach();
            _connector.connect(_p1Address, 55562);
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

int main(int argc, const char* argv[])
{
    std::cout << "I am a server" << std::endl;
    Queue<std::string> queue;
    List<std::shared_ptr<sf::TcpSocket>> sockets;
    //Accepter a(sockets, queue);
    std::thread(Accepter(sockets, queue)).detach();
    //Setup a listener
    //sf::CircleShape c(4);
    //c.getLocalBounds();

    sf::UdpSocket socket;
    sf::UdpSocket senderSocket;

// bind the socket to a port
    if (socket.bind(55571) != sf::Socket::Done)
    {
        return 1;
    }

    if (senderSocket.bind(55573) != sf::Socket::Done)
    {
        return 1;
    }

    char data[100];
    size_t received;
    sf::IpAddress remoteIP;
    unsigned short remotePort;

    if (socket.receive(data, 100, received, remoteIP, remotePort) != sf::Socket::Done){
        std::cout << "Failed to recieve" << std::endl;
        return 1;
    }
    else{
        std::cout << "Recieved: " << data << " from broadcast" << std::endl;
        //return 1;
    }

    char newData[100] = "Hello ther client. Here are the details of me";

    if (senderSocket.send(newData, 100, remoteIP, 55572) != sf::Socket::Done){
        std::cout << "Could not broadcast" << std::endl;
        return 1;
    }
    else{
        std::cout << "Sent" << std::endl;
    }

    _p1Address = remoteIP;

    while (true){
        std::string next = queue.Pop();
        std::cout << "Recieved: " << next << std::endl;
        auto send = [&] (std::shared_ptr<sf::TcpSocket> socket){
            //if (socket->connect("152.105.67.105", 55562)){
            //    std::cout << "Connected to client" << std::endl;
            //}

            
            
            if (_connector.send(next.c_str(), next.size() + 1) != sf::Socket::Done){
                std::cerr << "Failed to send data to client" << std::endl;
                return 1;
            }
            else{
                std::cout << "Sent: " << next << std::endl;
            }
            return 0;
        };

        sockets.ForEach(send);
    }

    return 0;
}