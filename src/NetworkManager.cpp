#include "../include/NetworkManager.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
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
  cout << "[Red] Iniciando servidor en el puerto: " << port << endl;

  receptor = new tcp::acceptor(ioContext, tcp::endpoint(tcp::v4(), port));
  
  socket = new tcp::socket(ioContext);

  cout << "Esperando otras conexiones " << endl;

  receptor->accept(*socket);

  cout << "Conexion aceptada" << endl;

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
    // Creamos un hilo que vivirá en paralelo al programa principal
    std::thread hiloRecepcion([this, alRecibir]() 
    {
        try 
        {
            while (true) 
            {
                // Se queda pausado aquí hasta que llegue un mensaje
                vector<unsigned char> basuraRecibida = RecibirMensaje();
                
                // Cuando llega, se lo devuelve al programa principal usando la función
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

