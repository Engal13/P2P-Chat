#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <memory>
#include <mutex>
#include <map>
#include "Protocol.hpp"
#include <string>
#include <asio.hpp>
#include <vector>
#include <map>
#include <functional>
#include "Protocol.hpp"

class NetworkManager 
{
private:
    asio::io_context ioContext;
    asio::ip::tcp::acceptor* receptor;
    asio::ip::tcp::socket* socket;

    
    std::map<uint32_t, std::shared_ptr<asio::ip::tcp::socket>> sesionesActivas;
    uint32_t proximo_id_cliente = 1;

    std::mutex clientesMutex;
public:

    NetworkManager();
    ~NetworkManager();

    void IniciarServidor(int port);
    void Conectar(const std::string& ip, int port);

    // Funciones adaptadas al protocolo E2EE
    void EnviarMensaje(uint32_t target_id, PacketType type, const std::vector<unsigned char>& payload);
    
    struct PaqueteRecibido {
        PacketHeader header;
        std::vector<unsigned char> payload;
    };
    PaqueteRecibido RecibirMensaje();

    void ManejarCliente(std::shared_ptr<asio::ip::tcp::socket> clienteVigilado, uint32_t my_id);

    void ejectuarLoop(std::function<void(PaqueteRecibido)> alRecibir);
};

#endif
