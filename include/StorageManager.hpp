#ifndef STORAGEMANAGER_HPP
#define STORAGEMANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "../include/EncryptionEngine.hpp"

class StorageManager {
private:
    std::string archivoBoveda;
    std::map<std::string, std::vector<std::string>> historialChats;
    std::mutex storageMutex;
    std::string llaveBoveda;
    EncryptionEngine* cripto;

public:
    StorageManager();
    ~StorageManager();

    void configurarBoveda(EncryptionEngine* motor, const std::string& llave, std::string& nombreUsuario);

    // Guardar historial de chat u otros datos
    void saveToFile(const std::string& filename, const std::vector<unsigned char>& data);

    // Cargar historial
    std::vector<unsigned char> loadFromFile(const std::string& filename);

    // Manejo de mensajes de chat
    void agregarMensaje(const std::string& contacto, const std::string& mensaje);
    std::string serializarHistorial();
    void deserializarHistorial(const std::string& data);
};

#endif

