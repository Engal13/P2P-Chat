#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <cstddef>
#include <string>
#include <asio.hpp>
#include <vector>

class NetworkManager 
{
private:
    asio::io_context ioContext;
    asio::ip::tcp::acceptor* receptor;
    asio::ip::tcp::socket* socket;
public:

    NetworkManager();
    ~NetworkManager();

    void IniciarServidor(int port);

    void Conectar(const std::string& ip, int port);

    void EnviarLlave(const std::vector<unsigned char>& datos);

    std::vector<unsigned char> RecibirLlave(size_t cantidad);

    void EnviarMensaje(const std::vector<unsigned char>& datosCifrados);

    std::vector<unsigned char> RecibirMensaje();

    void ejectuarLoop(std::function<void(std::vector<unsigned char>)> alRecibir);
};

#endif
