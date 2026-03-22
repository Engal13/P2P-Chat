#ifndef STORAGEMANAGER_HPP
#define STORAGEMANAGER_HPP

#include <string>

class StorageManager {
public:
    StorageManager();
    ~StorageManager();

    // Guardar historial de chat u otros datos
    void saveToFile(const std::string& filename, const std::string& data);

    // Cargar historial
    std::string loadFromFile(const std::string& filename);
};

#endif
