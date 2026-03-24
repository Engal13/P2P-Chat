#include "../include/NetworkManager.hpp"
#include <cstddef>
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

     cout << "[Servidor Central] ¡Nuevo usuario conectado desde: " << nuevoCliente->remote_endpoint().address().to_string() << "!" << endl;
    {
      std::lock_guard<std::mutex>lock(clientesMutex);

      clientesConectados.push_back(nuevoCliente);
    }

    std::thread HiloVigilante(&NetworkManager::ManejarCliente, this, nuevoCliente);

    HiloVigilante.detach();

  }

}

void NetworkManager::ManejarCliente(std::shared_ptr<tcp::socket> clienteVigilado) 
{
    cout << "[Hilo] Iniciando vigilancia del nuevo cliente." << endl;

    try 
    {
    while(true)
    {
      uint32_t tamano = 0;
      asio::read(*clienteVigilado, asio::buffer(&tamano, sizeof(tamano)));

      vector<unsigned char> basuraRecibida(tamano);

      asio::read(*clienteVigilado, asio::buffer(basuraRecibida));

      cout << "[Cartero Ciego] Recibi un bulto de basura ininteligible de un usuario. Rebotando..." << endl;

      for(unsigned char b : basuraRecibida)
      {
        cout << hex << setw(2) << setfill('0') << int(b) << " ";
      }
      cout << dec << endl;

      std::lock_guard<std::mutex>lock(clientesMutex);

      for(auto& otroCliente : clientesConectados)
      {
        if(otroCliente != clienteVigilado)
        {
           cout << "[Cartero Ciego] Rebotando a IP: " << otroCliente->remote_endpoint().address().to_string() << endl;
          asio::write(*otroCliente, asio::buffer(&tamano, sizeof(tamano)));
          asio::write(*otroCliente, asio::buffer(basuraRecibida));

        }
      }

    }
    } catch (const std::exception& e) 
    {
      cout << "[Cartero Ciego] Usuario desconectado. Causa: " << e.what() << endl;
      std::lock_guard<std::mutex>lock(clientesMutex);
      for(auto it = clientesConectados.begin(); it != clientesConectados.end();)
      {
        if(*it == clienteVigilado)
        {
          it = clientesConectados.erase(it);
          break;
        }
        else 
        {
          it++;
        }
      }
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

void NetworkManager::EnviarLlave(const vector<unsigned char>& datos)
{
  asio::write(*socket, asio::buffer(datos));
}

vector<unsigned char> NetworkManager::RecibirLlave(size_t cantidad)
{
  vector<unsigned char> BufferEntrada(cantidad);

  asio::read(*socket, asio::buffer(BufferEntrada));

  return BufferEntrada;
}

void NetworkManager::EnviarMensaje(const vector<unsigned char>& datosCifrados) 
{
    
    uint32_t tamano = datosCifrados.size();
    asio::write(*socket, asio::buffer(&tamano, sizeof(tamano)));
   
    asio::write(*socket, asio::buffer(datosCifrados));
}

vector<unsigned char> NetworkManager::RecibirMensaje() 
{
    
    uint32_t tamano = 0;
    asio::read(*socket, asio::buffer(&tamano, sizeof(tamano)));
   
    vector<unsigned char> bufferRecepcion(tamano);

    asio::read(*socket, asio::buffer(bufferRecepcion));

    return bufferRecepcion;
}


void NetworkManager::ejectuarLoop(std::function<void(vector<unsigned char>)> alRecibir) 
{
    std::thread hiloRecepcion([this, alRecibir]() 
    {
        try 
        {
            while (true) 
            {
                
                vector<unsigned char> basuraRecibida = RecibirMensaje();
                
                alRecibir(basuraRecibida);
            }
        }
        catch (const exception& e) 
        {
            cerr << "\n[Red] Se corto la conexión" << endl;
        }
    });

    hiloRecepcion.detach();
}

