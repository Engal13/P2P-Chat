#ifndef KEYMANAGER_HPP
#define KEYMANAGER_HPP

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <sodium.h>

// El "Contacto" o sala de chat con una persona especifica
struct SessionContext 
{
    uint32_t user_id;
    std::string username;
    
    // Libsodium RX (Receive) y TX (Transmit) para aislar cada chat
    unsigned char shared_rx[crypto_kx_SESSIONKEYBYTES]; 
    unsigned char shared_tx[crypto_kx_SESSIONKEYBYTES]; 
    
    bool is_established = false;
    
    std::vector<std::string> message_history;
};

class KeyManager 
{
public:
    std::map<uint32_t, std::shared_ptr<SessionContext>> active_sessions;
    
    unsigned char my_public_key[crypto_kx_PUBLICKEYBYTES];
    unsigned char my_private_key[crypto_kx_SECRETKEYBYTES];
    uint32_t my_id = 0; // Se seteara al registrarse en el servidor

    KeyManager();

    // Registra un cliente y deriva las llaves especificas TX/RX usando tu par de llaves
    bool registrar_contacto(uint32_t su_id, const std::string& su_nombre, const std::vector<unsigned char>& su_llave_publica);

    // Devuelve la llave TX (para encriptar lo que tu mandes) al EncryptionEngine
    std::string obtener_llave_tx(uint32_t su_id);

    // Devuelve la llave RX (Para desencriptar lo que te manden) al EncryptionEngine
    std::string obtener_llave_rx(uint32_t su_id);
};

#endif
