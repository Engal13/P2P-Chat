#ifndef EncryptionEngine_HPP
#define EncryptionEngine_HPP

#include <string>
#include <vector>

struct ParLlaves 
{
    std::string llavePublica;
    std::string llavePrivada;
};

class EncryptionEngine 
{
public:
    // Prepara entorno criptografico (libsodium).
    EncryptionEngine();
    ~EncryptionEngine();

    // Cifra mensaje con llave simetrica.
    std::vector<unsigned char> encriptado(const std::string& textoPlano, const std::string& llave);
    
    // Descifra mensaje y valida integridad.
    std::string descifrar(const std::vector<unsigned char>& textoCifrado, const std::string& llave);

    // Genera par de llaves para intercambio.
    ParLlaves generarParLlaves();

    // Deriva llave compartida desde privado/publico.
    std::string CombinarLlaves(const std::string& P1_llavePrivada, const std::string& P2_LlavePublica);
    

};  

#endif
