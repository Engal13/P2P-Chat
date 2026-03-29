#include "KeyManager.hpp"

KeyManager::KeyManager() { crypto_kx_keypair(my_public_key, my_private_key); }

bool KeyManager::registrar_contacto(uint32_t su_id, const std::string& su_nombre, const std::vector<unsigned char>& su_llave_publica)
{
    if (active_sessions.find(su_id) != active_sessions.end()) return false; // Ya lo anotamos antes, evitar bucle infinito

    auto session = std::make_shared<SessionContext>();
    session->user_id = su_id;
    session->username = su_nombre;

    // Regla matematica de intercambio (determinismo P2P):
    // En lugar de usar IDs (que dependen del servidor), usamos la matematica pura.
    // El duenio de la llave publica mas "pequenia" asume el rol de Server. Esto previene colisiones 100%.
    int cmp = memcmp(my_public_key, su_llave_publica.data(), crypto_kx_PUBLICKEYBYTES);
    if (cmp < 0) 
    {
        if (crypto_kx_server_session_keys(session->shared_rx, session->shared_tx,my_public_key, my_private_key, su_llave_publica.data()) != 0) 
        {
            throw std::runtime_error("Fallo criptografico: No se pudo firmar como Server.");
        }
    } 
    else 
    {
        if (crypto_kx_client_session_keys(session->shared_rx, session->shared_tx,my_public_key, my_private_key, su_llave_publica.data()) != 0) 
        {
            throw std::runtime_error("Fallo criptografico: No se pudo firmar como Cliente.");
        }
    }

    session->is_established = true;
    active_sessions[su_id] = session;
    std::cout << "[Llaves] Tunel E2EE establecido con " << su_nombre << " (ID " << su_id << ")!" << std::endl;
    return true;
}

std::string KeyManager::obtener_llave_tx(uint32_t su_id) 
{
  if (active_sessions.find(su_id) == active_sessions.end())  throw std::runtime_error("No hay tunel con este ID.");
   
  return std::string(reinterpret_cast<char *>(active_sessions[su_id]->shared_tx), crypto_kx_SESSIONKEYBYTES);
}

// Devuelve la llave RX (Para desencriptar lo que te manden) al EncryptionEngine
std::string KeyManager::obtener_llave_rx(uint32_t su_id) 
{
  if (active_sessions.find(su_id) == active_sessions.end()) throw std::runtime_error("No hay tunel con este ID.");
  
  return std::string(reinterpret_cast<char *>(active_sessions[su_id]->shared_rx), crypto_kx_SESSIONKEYBYTES);
}
