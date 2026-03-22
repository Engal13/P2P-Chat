#include "../include/EncryptionEngine.hpp"
#include <cstddef>
#include <iostream>
#include <sodium.h>
#include <stdexcept>
#include <vector>

using namespace std;

ParLlaves EncryptionEngine::generarParLlaves()
{
  unsigned char llavePublica[crypto_box_PUBLICKEYBYTES];
  unsigned char llavePrivada[crypto_box_SECRETKEYBYTES];

  crypto_kx_keypair(llavePublica, llavePrivada);

  ParLlaves par;

  par.llavePublica = string(reinterpret_cast<const char*>(llavePublica), sizeof(llavePublica));
  par.llavePrivada = string(reinterpret_cast<const char*>(llavePrivada), sizeof(llavePrivada));

  return par;
}

EncryptionEngine::EncryptionEngine() 
{
  if (sodium_init() < 0) 
  {
    throw std::runtime_error("Error al inicializar");
  }
}

EncryptionEngine::~EncryptionEngine() {}
//Encriptado y descincriptado
vector<unsigned char> EncryptionEngine::encriptado(const std::string &textoPlano, const std::string &llave) 
{
  std::cout << "Encriptando mensaje..." << std::endl;

  unsigned char HashedKey[crypto_secretbox_KEYBYTES];
  crypto_generichash(HashedKey, sizeof(HashedKey), (const unsigned char*)llave.data(), llave.length(), NULL, 0);

  unsigned char nonce[crypto_secretbox_NONCEBYTES];
  randombytes_buf(nonce, sizeof(nonce));

  vector<unsigned char> textoCifrado(textoPlano.length() + crypto_secretbox_MACBYTES);

  if(crypto_secretbox_easy(textoCifrado.data(), (const unsigned char*) textoPlano.data(), textoPlano.length(), nonce, HashedKey) != 0)
  {
    throw runtime_error("Error al encriptar el mensaje");
  }
  
  vector<unsigned char> MensajeFinal;

  MensajeFinal.insert(MensajeFinal.end(), nonce, nonce + sizeof(nonce));
  
  MensajeFinal.insert(MensajeFinal.end(), textoCifrado.begin(), textoCifrado.end());

  return MensajeFinal;

}

string EncryptionEngine::descifrar(const vector<unsigned char> &textoCifrado, const string &llave) 
{
  if(textoCifrado.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES)
  {
    throw runtime_error("el mensaje cifrado es demasiado corto o esta corrupto");
  }

  unsigned char HashedKey[crypto_secretbox_KEYBYTES];
  crypto_generichash(HashedKey, sizeof(HashedKey), (const unsigned char *)llave.data(), llave.length(), NULL, 0);

  const unsigned char* nonce = textoCifrado.data();

  const unsigned char* basuraBinaria = textoCifrado.data() + crypto_secretbox_NONCEBYTES;

  size_t tamañoBasura = textoCifrado.size() - crypto_secretbox_NONCEBYTES;

  vector<unsigned char> textoLimpio(tamañoBasura - crypto_secretbox_MACBYTES);

  if(crypto_secretbox_open_easy(textoLimpio.data(), basuraBinaria, tamañoBasura, nonce, HashedKey) != 0)
  {
    throw runtime_error("Fallo al descifrar: Mensaje corrupto o llave incorrecta");
  }

  return string(textoLimpio.begin(), textoLimpio.end());
}

string EncryptionEngine::CombinarLlaves(const string& P1_llavePrivada, const string& P2_LlavePublica)
{
  unsigned char llaveCompartida[crypto_scalarmult_BYTES];

  if(crypto_scalarmult(llaveCompartida, (const unsigned char*)P1_llavePrivada.data(), (const unsigned char*)P2_LlavePublica.data()) != 0)
  {
    throw runtime_error("Error al derivar la llave");
  }

  unsigned char llaveCompartidaFinal[crypto_secretbox_KEYBYTES];
  crypto_generichash(llaveCompartidaFinal, sizeof(llaveCompartidaFinal), llaveCompartida, sizeof(llaveCompartida), NULL, 0);

  return string(reinterpret_cast<const char*>(llaveCompartidaFinal), sizeof(llaveCompartidaFinal));

}
