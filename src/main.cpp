#include "../include/StorageManager.hpp"
#include "../include/EncryptionEngine.hpp"
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

    cout << "=======================================" << endl;
    cout << "      BOVEDA P2P - MODO TERMINAL       " << endl;
    cout << "=======================================" << endl;
    
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
        // ------------- REGISTRO (Primer uso) -------------
        cout << "\n[*] No se encontro la boveda de " << miUsuario << ". Creando cuenta nueva..." << endl;
        
        // Metemos un mensaje de bienvenida estandar y forzamos el guardado
        miBoveda.agregarMensaje("Sistema", "Bienvenido a tu nueva Boveda P2P.");
        cout << "[Exito] Boveda cifrada creada exitosamente." << endl;
    } 
    else 
    {
        archivoExiste.close();
        // ------------- LOGIN (Uso consecutivo) -------------
        cout << "\n[*] Boveda detectada. Intentando desencriptar..." << endl;
        
        vector<unsigned char> basuraRobada = miBoveda.loadFromFile(miArchivoFisico);
        
        try 
        {
            // EL MOMENTO DE LA VERDAD 
            // Si la contraseña era mala, llaveFuerte será distinta a la original
            // Y el motorCripto explotará lanzando un error (MAC Authentication Failed).
            string textoRecuperado = motorCripto.descifrar(basuraRobada, llaveFuerte);
            
            cout << "\n[ACCESO CONCEDIDO] Login Exitoso. Identidad Verificada.\n" << endl;
            miBoveda.deserializarHistorial(textoRecuperado);
            
            cout << "=== TUS CHATS RECUPERADOS ===" << endl;
            cout << miBoveda.serializarHistorial() << endl;
            
        } 
        catch (const exception& e) 
        {
            // Si entramos aquí, la contraseña era equivocada.
            cout << "\n[ACCESO DENEGADO] ERROR DE CONTRASEÑA: " << e.what() << endl;
            cout << "La Boveda permanece sellada. Cerrando programa..." << endl;
            cin.ignore(10000, '\n'); 
            cin.get();
            return 1;
        }
    }

    cout << "\nPresiona ENTER para salir..." << flush;
    
    // Limpiamos la basura invisible del "Enter" anterior
    cin.ignore(10000, '\n'); 
    
    // El programa se congela aquí hasta que presiones una tecla
    cin.get(); 

    return 0;
}
