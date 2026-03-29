#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <map>
#include "Protocol.hpp"
#include <string>
#include <asio.hpp>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include "Protocol.hpp"

class NetworkManager 
{
private:
    asio::io_context ioContext;
    asio::ip::tcp::acceptor* receptor;
    asio::ip::tcp::socket* socket;

    std::map<uint32_t, std::shared_ptr<asio::ip::tcp::socket>> sesionesActivas;
    std::map<std::string, uint32_t> usuarioAId;
    std::map<uint32_t, std::string> idAUsuario;
    uint32_t proximo_id_cliente = 1;

    std::mutex clientesMutex;
    std::function<void(const std::string&)> serverLogger;
    std::atomic<bool> serverRunning{false};

    void LogMensajeHex(const std::string& prefijo, const std::vector<unsigned char>& payload);
    void RegistrarNuevoUsuario(const std::vector<unsigned char>& payload, uint32_t& my_id, std::shared_ptr<asio::ip::tcp::socket> clienteVigilado);
    void ManejarMensajePrivado(uint32_t my_id, PacketHeader header, const std::vector<unsigned char>& payload);

public:
    // Inicializa estructura de sockets cliente/servidor.
    NetworkManager();
    ~NetworkManager();

    // Host: iniciar y detener servidor TCP.
    void IniciarServidor(int port);
    void DetenerServidor();
    bool ServidorActivo() const;

    // Callback para reflejar eventos del host en la UI.
    void SetServerLogger(std::function<void(const std::string&)> logger);
    // Devuelve true si la conexion fue exitosa, false si fallo
    bool Conectar(const std::string& ip, int port);
    void Desconectar();

    // Funciones adaptadas al protocolo E2EE
    void EnviarMensaje(uint32_t target_id, PacketType type, const std::vector<unsigned char>& payload);
    
    struct PaqueteRecibido {
        PacketHeader header;
        std::vector<unsigned char> payload;
    };
    PaqueteRecibido RecibirMensaje();

    void ManejarCliente(std::shared_ptr<asio::ip::tcp::socket> clienteVigilado, uint32_t my_id);
    void ManejarReconexion(const std::string& nombre_usuario, uint32_t& my_id, std::shared_ptr<asio::ip::tcp::socket> clienteVigilado);
    void Broadcast(uint32_t my_id, PacketHeader header, const std::vector<unsigned char>& payload);
    void ejectuarLoop(std::function<void(PaqueteRecibido)> alRecibir, std::function<void(const std::string&)> alDesconectar = nullptr);
};

#endif
