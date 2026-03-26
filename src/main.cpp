#include "../include/StorageManager.hpp"
#include "../include/EncryptionEngine.hpp"
#include "NetworkManager.hpp"
#include "KeyManager.hpp"
#include <iostream>
#include <sodium.h>
#include <fstream>

using namespace std;

// Función utilitaria para derivar la contraseña humana a una Llave de 32 Bytes
string derivarLlaveDeContrasena(const string& passwordHumana) 
{
    unsigned char hashDe32Bytes[crypto_secretbox_KEYBYTES];
    // Licuadora criptográfica de Libsodium (Genera siempre la misma llave para la misma contraseña)
    crypto_generichash(hashDe32Bytes, sizeof(hashDe32Bytes), 
                      (const unsigned char*)passwordHumana.data(), passwordHumana.length(), 
                      NULL, 0);
    
    return string(reinterpret_cast<char*>(hashDe32Bytes), sizeof(hashDe32Bytes));
}

int main() 
{
    if (sodium_init() < 0) return 1;

    EncryptionEngine motorCripto;
    StorageManager miBoveda;
    
    string miUsuario;
    cout << "Ingresa tu Nombre de Usuario: ";
    cin >> miUsuario;

    string miPassword;
    cout << "Ingresa tu Contraseña Maestra: ";
    cin >> miPassword;

    // 1. Transformamos la contraseña humana en la Llave Matemática Perfecta
    string llaveFuerte = derivarLlaveDeContrasena(miPassword);
    miBoveda.configurarBoveda(&motorCripto, llaveFuerte, miUsuario);

    // 2. Comprobamos si el disco duro ya tiene nuestra cuenta creada
    string miArchivoFisico = miUsuario + "_boveda.dat";
    ifstream archivoExiste(miArchivoFisico);
    
    if (!archivoExiste.good()) 
    {
        cout << "\n[*] No se encontro la cuenta de " << miUsuario << ". Creando cuenta nueva..." << endl;
        
        // Metemos un mensaje de bienvenida estandar y forzamos el guardado
        miBoveda.agregarMensaje("Sistema", "Bienvenido.");
        cout << "Cuenta creada exitosamente" << endl;
    } 
    else 
    {
        archivoExiste.close();
        // ------------- LOGIN (Uso consecutivo) -------------
        cout << "Cuenta encontrada. Recuperando informacion" << endl;
        
        vector<unsigned char> basuraRobada = miBoveda.loadFromFile(miArchivoFisico);
        
        try 
        {
            
            string textoRecuperado = motorCripto.descifrar(basuraRobada, llaveFuerte);
            
            cout << "\n[ACCESO CONCEDIDO] Login Exitoso. Identidad Verificada.\n" << endl;
            miBoveda.deserializarHistorial(textoRecuperado);
            
            cout << miBoveda.serializarHistorial() << endl;
            
        } 
        catch (const exception& e) 
        {
            // Si entramos aquí, la contraseña era equivocada.
            cout << "\n[ACCESO DENEGADO] ERROR DE CONTRASEÑA: " << e.what() << endl;
            cout << "Cerrando programa..." << endl;
            cin.ignore(10000, '\n'); 
            cin.get();
            return 1;
        }
    }
        // (AQUÍ ARRIBA VA TU CÓDIGO ACTUAL DEL STORAGE MANAGER Y EL LOGIN...)
    
    KeyManager miLlavero; 
    NetworkManager red;

    int opcion;
    cout << "\n1. Iniciar Servidor Central (Router Fantasma)" << endl;
    cout << "2. Conectar a la Red P2P" << endl;
    cout << "Seleccione: "; cin >> opcion;

    if (opcion == 1) 
    {
        cout << "Puerto: "; int port; cin >> port;
        red.IniciarServidor(port); 
        return 0; // El Servidor se queda en un bucle infinito atrapado ahí.
    }

    cout << "IP del Servidor: "; string ip; cin >> ip;
    cout << "Puerto: "; int port; cin >> port;
    
    red.Conectar(ip, port);

    vector<unsigned char> paqueteRegistro(miUsuario.begin(), miUsuario.end()); // Nombre
    paqueteRegistro.push_back('|'); 
    paqueteRegistro.insert(paqueteRegistro.end(), miLlavero.my_public_key, miLlavero.my_public_key + crypto_kx_PUBLICKEYBYTES);

    
    red.EnviarMensaje(0, PacketType::RegisterClient, paqueteRegistro);

    // 2. EL HILO ESCUCHA-TODO (Asíncrono)
    red.ejectuarLoop([&miLlavero, &motorCripto, &miBoveda, &red, miUsuario](NetworkManager::PaqueteRecibido paquete) 
    {
        uint32_t su_id = paquete.header.target_id; // Quien te lo mandó
        
        if (paquete.header.type == PacketType::RegisterClient) 
        {
            // Desentrañamos el Nombre y la Llave Pública de quien nos saludó
            string basuraString(paquete.payload.begin(), paquete.payload.end());
            size_t sep = basuraString.find('|');
            if (sep == string::npos) return;
            
            string su_nombre = basuraString.substr(0, sep);
            vector<unsigned char> su_llave_pub(paquete.payload.begin() + sep + 1, paquete.payload.end());
            
            // ¡Magia! El llavero hace el Diffie-Hellman al instante en memoria RAM.
            bool es_nuevo = miLlavero.registrar_contacto(su_id, su_nombre, su_llave_pub);

            // Si es alguien nuevo que acaba de entrar, le respondemos para que él también nos anote a nosotros
            if (es_nuevo && su_id != 0) 
            {
                vector<unsigned char> miRespuesta(miUsuario.begin(), miUsuario.end());
                miRespuesta.push_back('|'); 
                miRespuesta.insert(miRespuesta.end(), miLlavero.my_public_key, miLlavero.my_public_key + crypto_kx_PUBLICKEYBYTES);
                red.EnviarMensaje(su_id, PacketType::RegisterClient, miRespuesta);
            }
        }
        else if (paquete.header.type == PacketType::ChatMessage) 
        {
            try {
                // Sacamos nuestra llave RX específica para DESCIFRAR a esta persona exacta
                string llavePrivadaAislada = miLlavero.obtener_llave_rx(su_id);
                string mensajeLimpio = motorCripto.descifrar(paquete.payload, llavePrivadaAislada);
                
                string nombreContacto = miLlavero.active_sessions[su_id]->username;
                
                // Imprimimos el chat y lo guardamos a la bóveda
                cout << "\n[" << nombreContacto << "] dice: " << mensajeLimpio << "\n> " << flush;
                miBoveda.agregarMensaje(nombreContacto, mensajeLimpio);
            } 
            catch(const exception& e) {
                cout << "\n[Criptografía] Error Descifrando: " << e.what() << endl;
            }
        }
    });

    // 3. EL CLI DE COMANDOS (Hilo Principal)
    cin.ignore(10000, '\n');
    cout << "\n[*] SISTEMA LISTO." << endl;
    cout << "[*] Escribe '/list' para ver quien esta en linea." << endl;
    cout << "[*] Escribe '/msg <ID> <Mensaje>' para enviar chats." << endl;
    cout << "> ";
    
    while(true) 
    {
        string input;
        getline(cin, input);
        if (input == "salir") break;
        if (input.empty()) continue;

        if (input == "/list")
        {
            cout << "--- CONTACTOS E2EE ACTIVOS ---" << endl;
            if (miLlavero.active_sessions.empty()) cout << "(Ninguno conectado aún)" << endl;
            for (auto& par : miLlavero.active_sessions) {
                cout << "-> ID " << par.first << " : " << par.second->username << endl;
            }
            cout << "------------------------------" << endl;
            cout << "> ";
            continue;
        }

        if (input.substr(0, 5) == "/msg ") 
        {
            size_t esp = input.find(' ', 5);
            if (esp != string::npos) 
            {
                uint32_t id_pobre_victima = stoi(input.substr(5, esp - 5));
                string mensaje = input.substr(esp + 1);

                try 
                {
                    // Sacamos nuestra llave TX para ENCRIPTARLE solo a él
                    string llavePrivadaAislada = miLlavero.obtener_llave_tx(id_pobre_victima);
                    vector<unsigned char> bala = motorCripto.encriptado(mensaje, llavePrivadaAislada);

                    // El misil P2P fue enrutado y encriptado con éxito.
                    red.EnviarMensaje(id_pobre_victima, PacketType::ChatMessage, bala);
                    
                    cout << "[Enviado con exito a ID " << id_pobre_victima << "]" << endl;
                    
                    // Aseguramos nuestro disco duro
                    string contacto = miLlavero.active_sessions[id_pobre_victima]->username;
                    miBoveda.agregarMensaje(contacto, "Yo: " + mensaje);
                }
                catch (const exception& e) {
                    cerr << "! [" << e.what() << "]. (¿Seguro que ese ID existe?)" << endl;
                }
            }
        }
    }

    return 0;
}
