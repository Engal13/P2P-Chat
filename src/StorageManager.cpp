#include "../include/StorageManager.hpp"
#include <fstream>
#include <iostream>

StorageManager::StorageManager() {}

StorageManager::~StorageManager() {}

void StorageManager::saveToFile(const std::string &filename,
                                const std::string &data) {
  std::cout << "[Almacenamiento] Guardando archivo: " << filename << std::endl;
  std::ofstream out(filename);
  if (out.is_open()) {
    out << data;
    out.close();
  }
}

std::string StorageManager::loadFromFile(const std::string &filename) {
  std::cout << "[Almacenamiento] Cargando archivo: " << filename << std::endl;
  // Implementación Dummy
  return "";
}
