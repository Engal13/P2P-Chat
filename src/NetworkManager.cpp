#include "../include/NetworkManager.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <thread>


using namespace std;
using namespace asio;
using namespace asio::ip;

NetworkManager::NetworkManager() {
  receptor = nullptr;
  socket = nullptr;
}

NetworkManager::~NetworkManager() {
  if(receptor) delete receptor;
  if(socket) delete socket;
}

void NetworkManager::IniciarServidor(int port) 
{
   cout << "[Servidor Central] Iniciando en el puerto: " << port << endl;

  receptor = new tcp::acceptor(ioContext, tcp::endpoint(tcp::v4(), port));
  
  while(true)
  {
    auto nuevoCliente = std::make_shared<tcp::socket>(ioContext);

    receptor->accept(*nuevoCliente);

    uint32_t asignado_id = 0;
    {
      std::lock_guard<std::mutex>lock(clientesMutex);
      asignado_id = proximo_id_cliente++;
      sesionesActivas[asignado_id] = nuevoCliente;
    }

    std::thread HiloVigilante(&NetworkManager::ManejarCliente, this, nuevoCliente, asignado_id);

    HiloVigilante.detach();

  }

}

void NetworkManager::ManejarCliente(std::shared_ptr<tcp::socket> clienteVigilado, uint32_t my_id) 
{

  try 
  {
    while (true) 
    {
     
      PacketHeader header;
      asio::read(*clienteVigilado, asio::buffer(&header, sizeof(PacketHeader)));

      // 2. Dependiendo de lo que diga el sobre, preparamos la caja para la
      // basura (payload)
      vector<unsigned char> payload(header.payload_size);
      asio::read(*clienteVigilado, asio::buffer(payload));

      cout << "[Router Central] Paquete recibido de ID " << my_id
           << " dirigido a Destino ID " << header.target_id << endl;

      std::lock_guard<std::mutex> lock(clientesMutex);

      // 3. NUEVO: Si el destino es 0 (El Servidor), lo tratamos como un Broadcast Publico
      // Esto es vital para que un cliente nuevo le grite su Llave Publica y su Nombre a los demas
      if (header.target_id == 0) 
      {
          header.target_id = my_id; // El servidor firma el sobre con tu ID real de remitente
          for (auto& par : sesionesActivas) 
          {
              if (par.first != my_id) // No te lo reenvies a ti mismo
              {
                  asio::write(*(par.second), asio::buffer(&header, sizeof(PacketHeader)));
                  asio::write(*(par.second), asio::buffer(payload));
              }
          }
          cout << "   -> [Broadcast] Paquete replicado a " << (sesionesActivas.size() - 1) << " clientes." << endl;
          continue; // Listo, a esperar el siguiente paquete
      }

      // 4. Si el destino NO es 0, es un mensaje privado dirigido
      auto it = sesionesActivas.find(header.target_id);
      if (it != sesionesActivas.end()) 
      {
        // ¡Lo encontramos! Pasamos el paquete directamente a su cable
        auto destinatario = it->second;

        header.target_id = my_id;

        asio::write(*destinatario, asio::buffer(&header, sizeof(PacketHeader)));
        asio::write(*destinatario, asio::buffer(payload));
        cout << "   -> Redirigido exitosamente a ID " << it->first << endl;
      } else {
        cout << "   -> Error: El destinatario ID " << header.target_id
             << " no esta conectado." << endl;
      }
    }
  } 
  catch (const std::exception &e) 
  {
    cout << "[Router Central] Cliente ID " << my_id
         << " desconectado. Causa: " << e.what() << endl;

    std::lock_guard<std::mutex> lock(clientesMutex);
    sesionesActivas.erase(my_id);
  }
}

void NetworkManager::Conectar(const std::string &ip, int port) 
{
  std::cout << "[Red] Conectando al peer: " << ip << ":" << port << std::endl;
  
  socket = new tcp::socket(ioContext);

  try
  {
    tcp::endpoint destino(asio::ip::make_address(ip), port);

    socket->connect(destino);
    cout << "Conectado exitosamente" << endl;
  }
  catch(const exception& e)
  {
    cerr << "Error de conexion " << e.what() << endl;
}
}


void NetworkManager::EnviarMensaje(uint32_t target_id, PacketType type, const vector<unsigned char>& payload) 
{
    PacketHeader header;
    header.target_id = target_id;
    header.type = type;
    header.payload_size = payload.size();

    asio::write(*socket, asio::buffer(&header, sizeof(PacketHeader)));
   
    asio::write(*socket, asio::buffer(payload));
}

NetworkManager::PaqueteRecibido NetworkManager::RecibirMensaje()
{
    PaqueteRecibido paquete;
    
    asio::read(*socket, asio::buffer(&paquete.header, sizeof(PacketHeader)));
   
    paquete.payload.resize(paquete.header.payload_size);
    asio::read(*socket, asio::buffer(paquete.payload));

    return paquete;
}

void NetworkManager::ejectuarLoop(std::function<void(PaqueteRecibido)> alRecibir) 
{
  // Creamos el hilo fantasma del cliente
  std::thread hiloRecepcion([this, alRecibir]() 
  {
    try 
    {
      while (true) 
      {
        // Se queda bloqueado aqui hasta que llegue una carta del Cartero Ciego
        PaqueteRecibido paquete = RecibirMensaje();

        // Se la entregamos a nuestro main.cpp para que lea el remitente y
        // procese el chat
        alRecibir(paquete);
      }
    } 
    catch (const std::exception &e) 
    {
      cerr << "\n[Red] Se corto la conexion con el servidor. Causa: "
           << e.what() << endl;
    }
  });

  hiloRecepcion.detach();
}



