#include "../include/StorageManager.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

StorageManager::StorageManager() 
{
  cripto = nullptr;
}

StorageManager::~StorageManager() 
{
  if(cripto) delete  cripto;
}

void StorageManager::configurarBoveda(EncryptionEngine* motor, const std::string& llave, std::string& nombreUsuario) {
    cripto = motor;
    llaveBoveda = llave;
    archivoBoveda = nombreUsuario + "_boveda.dat";
}

void StorageManager::saveToFile(const std::string &filename, const std::vector<unsigned char> &data) {
  std::cout << "[Almacenamiento] Guardando archivo de boveda cifrada..." << std::endl;
  std::ofstream out(filename, std::ios::binary);
  if (out.is_open()) {
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();
  }
}

std::vector<unsigned char> StorageManager::loadFromFile(const std::string &filename) 
{
    std::cout << "[Almacenamiento] Cargando archivo: " << filename << std::endl;
    std::ifstream in(filename, std::ios::binary); // <-- ¡CRiTICO EN WINDOWS!
    
    if (!in.is_open()) return std::vector<unsigned char>();

    // Leer el archivo como datos crudos hasta el final
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(in), {});
    return buffer;
}


void StorageManager::agregarMensaje(const std::string& contacto, const std::string& mensaje) {
    std::lock_guard<std::mutex> lock(storageMutex);
    historialChats[contacto].push_back(mensaje);
    
    if (cripto == nullptr || llaveBoveda.empty()) return;

    // Guardamos automaticamente cifrado despues de recibir/enviar
    std::string dataPlanText = serializarHistorial();
    std::vector<unsigned char> dataCifrada = cripto->encriptado(dataPlanText, llaveBoveda);

    saveToFile(archivoBoveda, dataCifrada); 
}

std::string StorageManager::serializarHistorial() {
    std::ostringstream oss;
    for (const auto& par : historialChats) {
        // Marcamos el inicio de un contacto
        oss << "###" << par.first << "\n";
        for (const auto& msg : par.second) {
            oss << msg << "\n";
        }
    }
    return oss.str();
}

void StorageManager::deserializarHistorial(const std::string& data) {
    historialChats.clear();
    std::istringstream iss(data);
    std::string line;
    std::string contactoActual = "";

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line.substr(0, 3) == "###") {
            contactoActual = line.substr(3);
        } else if (!contactoActual.empty()) {
            historialChats[contactoActual].push_back(line);
        }
    }
}

