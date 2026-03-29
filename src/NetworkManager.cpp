#include "../include/NetworkManager.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <array>
#include <vector>
#include <functional>
#include <thread>


using namespace std;
using namespace asio;
using namespace asio::ip;

// Inicializa punteros de red en estado seguro.
NetworkManager::NetworkManager() {
  receptor = nullptr;
  socket = nullptr;
}

// Libera recursos de sockets creados dinamicamente.
NetworkManager::~NetworkManager() {
  if(receptor) delete receptor;
  if(socket) delete socket;
}

// Registra callback para reportar eventos del host/servidor.
void NetworkManager::SetServerLogger(std::function<void(const std::string&)> logger) {
  serverLogger = std::move(logger);
}

// Consulta rapida para saber si el servidor esta levantado.
bool NetworkManager::ServidorActivo() const {
  return serverRunning.load();
}

// Levanta el servidor TCP, acepta clientes y crea hilos por cliente.
void NetworkManager::IniciarServidor(int port) 
{
  if (serverRunning.load()) return;
  serverRunning.store(true);

  cout << "[Servidor Central] Iniciando en el puerto: " << port << endl;
  if (serverLogger) serverLogger("Servidor iniciado en puerto " + std::to_string(port));

  if (receptor) {
    delete receptor;
    receptor = nullptr;
  }

  receptor = new tcp::acceptor(ioContext, tcp::endpoint(tcp::v4(), port));
  
  while(serverRunning.load())
  {
    auto nuevoCliente = std::make_shared<tcp::socket>(ioContext);
    asio::error_code ec;
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    receptor->accept(*nuevoCliente, ec);
    if (ec) 
    {
      if (serverRunning.load() && serverLogger) 
      {
        serverLogger(std::string("Error al aceptar: ") + ec.message());
      }
      continue;
    }

    uint32_t asignado_id = 0;
    {
      std::lock_guard<std::mutex>lock(clientesMutex);
      asignado_id = proximo_id_cliente++;
      sesionesActivas[asignado_id] = nuevoCliente;
    }

    if (serverLogger) serverLogger("Cliente conectado con ID" + std::to_string(asignado_id));

    std::thread HiloVigilante(&NetworkManager::ManejarCliente, this, nuevoCliente, asignado_id);

    HiloVigilante.detach();
  }
  serverRunning.store(false);
}

// Detiene el servidor y cierra todas las conexiones activas.
void NetworkManager::DetenerServidor()
{
  if (!serverRunning.load()) return;

  serverRunning.store(false);

  try 
  {
    if (receptor) 
    {
      asio::error_code ec;
    
      ec = receptor->cancel(ec);
      if (ec) cout << "[Servidor] Error al cancelar receptor: " << ec.message() << endl;
      ec= receptor->close(ec);
      if (ec) cout << "[Servidor] Error al cerrar receptor: " << ec.message() << endl;
    }

  } catch (...) {}

  std::lock_guard<std::mutex> lock(clientesMutex);

  for (auto& par : sesionesActivas) {
    asio::error_code ec;
    ec = par.second->shutdown(tcp::socket::shutdown_both, ec);
    if (ec) cout << "Error en shutdown con cliente ID " << par.first << ": " << ec.message() << endl;
    
    ec = par.second->close(ec);
    if (ec) cout << "Error cerrando socket del cliente ID " << par.first << ": " << ec.message() << endl;
  }

  sesionesActivas.clear();
  usuarioAId.clear();
  idAUsuario.clear();
  if (serverLogger) serverLogger("Servidor detenido.");
}

void NetworkManager::ManejarReconexion(const string& nombre_usuario, uint32_t& my_id, std::shared_ptr<tcp::socket> clienteVigilado)
{
  auto itUsuario = usuarioAId.find(nombre_usuario);

  if (itUsuario != usuarioAId.end() && itUsuario->second != my_id) 
  {
    uint32_t idViejo = itUsuario->second;
    auto itSocketViejo = sesionesActivas.find(idViejo);

    if (itSocketViejo != sesionesActivas.end()) 
    {
      try 
      {
        itSocketViejo->second->shutdown(tcp::socket::shutdown_both);
        itSocketViejo->second->close();
      } catch (...) {}
      sesionesActivas.erase(itSocketViejo);
    }
    // Conservamos el ID historico del usuario para que sea estable.
    sesionesActivas.erase(my_id);
    sesionesActivas[idViejo] = clienteVigilado;
    idAUsuario.erase(my_id);
    my_id = idViejo;

    if (serverLogger) 
    {
      serverLogger("Reconexion detectada para usuario '" + nombre_usuario + "'. ID antiguo " + std::to_string(idViejo) +" reemplazado por ID " + std::to_string(my_id));
    }
  }
}

void NetworkManager::LogMensajeHex(const std::string& prefijo, const vector<unsigned char>& payload)
{
    if (!serverLogger) return;
    serverLogger(prefijo);
    std::ostringstream oss;
    oss << "[HEX Dump] ";
    for (size_t i = 0; i < std::min(payload.size(), (size_t)16); ++i) 
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)payload[i] << " ";
    }
    if (payload.size() > 16) oss << "...";
    serverLogger(oss.str());
}

void NetworkManager::Broadcast(uint32_t my_id, PacketHeader header, const vector<unsigned char>& payload)
{
    header.target_id = my_id;        
    std::array<asio::const_buffer, 2> paqueteFusionado = 
    {
    asio::buffer(&header, sizeof(PacketHeader)),
    asio::buffer(payload)
    };

    size_t enviosExitosos = 0;
    for (auto& par : sesionesActivas) 
    {
        if (par.first != my_id) 
        {
            (void)asio::write(*(par.second), paqueteFusionado);
            enviosExitosos++;
        }
    }
    cout << "   -> [Broadcast] Paquete replicado a " << enviosExitosos << " clientes." << endl;
    if (header.type == PacketType::ChatMessage) 
    {
        LogMensajeHex("Broadcast desde ID " + std::to_string(my_id) + ", bytes: " + std::to_string(payload.size()), payload);
    }
}

void NetworkManager::RegistrarNuevoUsuario(const vector<unsigned char>& payload, uint32_t& my_id, std::shared_ptr<tcp::socket> clienteVigilado)
{
    string registro(payload.begin(), payload.end());
    size_t sepRegistro = registro.find('|');

    if (sepRegistro != string::npos) 
    {
      string nombre_usuario = registro.substr(0, sepRegistro);
      ManejarReconexion(nombre_usuario, my_id, clienteVigilado);
     
      usuarioAId[nombre_usuario] = my_id;
      idAUsuario[my_id] = nombre_usuario;
      if (serverLogger) 
      {
        serverLogger("Registro de usuario '" + nombre_usuario + "' con ID " + std::to_string(my_id));
      }
    }
}

void NetworkManager::ManejarMensajePrivado(uint32_t my_id, PacketHeader header, const vector<unsigned char>& payload)
{
    auto it = sesionesActivas.find(header.target_id);
    if (it != sesionesActivas.end()) 
    {
      auto destinatario = it->second;
      header.target_id = my_id;

      std::array<asio::const_buffer, 2> paqueteFusionado = 
      {
          asio::buffer(&header, sizeof(PacketHeader)),
          asio::buffer(payload)
      };
      (void)asio::write(*destinatario, paqueteFusionado);

      cout << "   -> Redirigido exitosamente a ID " << it->first << endl;
      if (header.type == PacketType::ChatMessage) 
      {
        LogMensajeHex("Mensaje reenviado " + std::to_string(my_id) + " -> " + std::to_string(it->first) + " (" + std::to_string(payload.size()) + " bytes)", payload);
      }
    } 
    else 
    {
      cout << "   -> Error: El destinatario ID " << header.target_id << " no esta conectado." << endl;
      if (serverLogger) 
      {
        serverLogger("Error: destino ID " + std::to_string(header.target_id) + " no conectado");
      }
    }
}

// Loop por cliente: recibe paquetes y los reenvia segun destino.
void NetworkManager::ManejarCliente(std::shared_ptr<tcp::socket> clienteVigilado, uint32_t my_id) 
{
  try 
  {
    while (true) 
    {
      PacketHeader header;
      asio::read(*clienteVigilado, asio::buffer(&header, sizeof(PacketHeader)));

      // 2. Dependiendo de lo que diga el sobre, preparamos la caja para la basura (payload)
      vector<unsigned char> payload(header.payload_size);
      asio::read(*clienteVigilado, asio::buffer(payload));

      cout << "[Router Central] Paquete recibido de ID " << my_id << " dirigido a Destino ID " << header.target_id << endl;

      std::lock_guard<std::mutex> lock(clientesMutex);

      if (header.type == PacketType::RegisterClient) 
      {
        RegistrarNuevoUsuario(payload, my_id, clienteVigilado);
      }
      // 3. Si el destino es 0 (El Servidor), lo tratamos como un Broadcast Publico
      if (header.target_id == 0) 
      {
          Broadcast(my_id, header, payload);
          continue; 
      }
      // 4. Si el destino NO es 0, es un mensaje privado dirigido
      ManejarMensajePrivado(my_id, header, payload);
    }
  } 
  catch (const std::exception &e) 
  {
    cout << "[Router Central] Cliente: " << my_id << " desconectado. Causa: " << e.what() << endl;
    std::lock_guard<std::mutex> lock(clientesMutex);
    string usuarioQueSale;
    auto itNombre = idAUsuario.find(my_id);
    if (itNombre != idAUsuario.end())
    {
      usuarioQueSale = itNombre->second;
      idAUsuario.erase(itNombre);
    }
    sesionesActivas.erase(my_id);
    if (serverLogger) serverLogger("Cliente desconectado: " + std::to_string(my_id));
    PacketHeader aviso;
    aviso.target_id = my_id;
    aviso.type = PacketType::UserDisconnected;
    std::vector<unsigned char> payload(usuarioQueSale.begin(), usuarioQueSale.end());
    aviso.payload_size = static_cast<uint32_t>(payload.size());
    Broadcast(my_id, aviso, payload);
  }
} 

// Conecta el cliente a un servidor remoto por IP/puerto.
bool NetworkManager::Conectar(const std::string &ip, int port) 
{
  std::cout << "[Red] Conectando al peer: " << ip << ":" << port << std::endl;
  
  socket = new tcp::socket(ioContext);

  try
  {
    tcp::resolver resolver(ioContext);
    auto endpoints = resolver.resolve(ip, std::to_string(port));

    asio::connect(*socket, endpoints);
    cout << "[Red] Conectado exitosamente al servidor P2P!" << endl;
    return true;
  }
  catch(const exception& e)
  {
    cerr << "\n[CRITICO] Error de conexion: " << e.what() << endl;
    cerr << "Asegurate de que la IP o URL este bien escrita y el Host este encendido." << endl;
    // No abortamos el programa entero: dejamos que la capa superior decida si reintentar
    if (socket) {
      delete socket;
      socket = nullptr;
    }
    return false;
  }
}

// Cierra de forma segura la conexion cliente actual.
void NetworkManager::Desconectar()
{
  try {
    if (socket) 
    {
      socket->shutdown(tcp::socket::shutdown_both);
      socket->close();
      delete socket;
      socket = nullptr;
    }
  } catch (...) {
    if (socket) 
    {
      delete socket;
      socket = nullptr;
    }
  }
}


// Envia un paquete ya serializado (header + payload).
void NetworkManager::EnviarMensaje(uint32_t target_id, PacketType type, const vector<unsigned char>& payload) 
{
    PacketHeader header;
    header.target_id = target_id;
    header.type = type;
    header.payload_size = payload.size();

    std::array<asio::const_buffer, 2> paqueteFusionado = {
        asio::buffer(&header, sizeof(PacketHeader)),
        asio::buffer(payload)
    };

    (void)asio::write(*socket, paqueteFusionado);
}

// Lee un paquete completo desde el socket.
NetworkManager::PaqueteRecibido NetworkManager::RecibirMensaje()
{
    PaqueteRecibido paquete;
    
    asio::read(*socket, asio::buffer(&paquete.header, sizeof(PacketHeader)));
   
    paquete.payload.resize(paquete.header.payload_size);
    asio::read(*socket, asio::buffer(paquete.payload));

    return paquete;
}

// Inicia hilo de recepcion para procesar paquetes entrantes.
void NetworkManager::ejectuarLoop(std::function<void(PaqueteRecibido)> alRecibir,
                                  std::function<void(const std::string&)> alDesconectar) 
{
  // Creamos el hilo fantasma del cliente
  std::thread hiloRecepcion([this, alRecibir, alDesconectar]() 
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
      if (alDesconectar) 
      {
        alDesconectar(e.what());
      }
    }
  });

  hiloRecepcion.detach();
}



