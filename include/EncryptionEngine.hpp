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
    EncryptionEngine();
    ~EncryptionEngine();

    std::vector<unsigned char> encriptado(const std::string& textoPlano, const std::string& llave);
    
    std::string descifrar(const std::vector<unsigned char>& textoCifrado, const std::string& llave);

    ParLlaves generarParLlaves();

    std::string CombinarLlaves(const std::string& P1_llavePrivada, const std::string& P2_LlavePublica);
    

};  

#endif
