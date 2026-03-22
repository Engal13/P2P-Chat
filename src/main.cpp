#include "../include/EncryptionEngine.hpp"
#include "../include/Interface.hpp"
#include "../include/NetworkManager.hpp"
#include "../include/StorageManager.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <sodium.h>

using namespace std;

int main() 
{

    NetworkManager red;
  
    EncryptionEngine cripto;

    ParLlaves misLlaves = cripto.generarParLlaves();  

    string LlaveMaestra;

    cout << "1. Host " << endl;
    cout << "2. Cliente " << endl;
    
    int opcion;
    cin >> opcion;


    //Intercambio de llaves para crear la llave maestra y conexion al host o iniciar conexion al host
    try 
    {
        if (opcion == 1) 
        {
            red.IniciarServidor(5000);

            vector<unsigned char> miPub(misLlaves.llavePublica.begin(), misLlaves.llavePublica.end());

            red.EnviarLlave(miPub);

            vector<unsigned char> P2_Pub = red.RecibirLlave(crypto_box_PUBLICKEYBYTES);
            string P2_Llave(P2_Pub.begin(), P2_Pub.end());

            LlaveMaestra = cripto.CombinarLlaves(misLlaves.llavePrivada, P2_Llave);

            cout << "Llave maestra generada" << endl;
    
        }
        else if (opcion == 2) 
        {
            cout << "Ingresa la IP (Escribe 127.0.0.1 para probar contigo mismo): ";
            string ip;
            cin >> ip;
            red.Conectar(ip, 5000);    
            
            vector<unsigned char> P2_Pub = red.RecibirLlave(crypto_box_PUBLICKEYBYTES);
            string P2_Llave(P2_Pub.begin(), P2_Pub.end());

            vector<unsigned char> miPub(misLlaves.llavePublica.begin(), misLlaves.llavePublica.end());

            red.EnviarLlave(miPub);

            LlaveMaestra = cripto.CombinarLlaves(misLlaves.llavePrivada, P2_Llave);
            cout << "Llave maestra generada" << endl;

        }
        else 
        {
            cout << "Opción inválida." << endl;
        }
    }
    catch (const exception& e) 
    {
        cerr << "Error crítico en la red: " << e.what() << endl;
    }

    //loop para ejecutar el chat p2p usando un hilo para permitir que puedan mandar mensajes sin esperar por el otro

     red.ejectuarLoop([&cripto, LlaveMaestra](vector<unsigned char> basuraRecibida) 
    {
        
        cout << "\nMensaje encriptado recibido: ";
        for(unsigned char b : basuraRecibida) 
        {
            cout << hex << setw(2) << setfill('0') << (int)b << " ";
        }
        cout << dec << endl;
        
        try 
        {
            
            string mensajeLimpio = cripto.descifrar(basuraRecibida, LlaveMaestra);
            cout << "Mensaje descifrado: " << mensajeLimpio << "\n> " << flush;
        }
        catch (const exception& e) 
        {
            cerr << "[Alerta de Seguridad] Mensaje corrupto o manipulado: " << e.what() << endl;
        }
    });
    cout << "\n=== CHAT ===" << endl;
    cout << "> ";
    
    
    cin.ignore(10000, '\n');
    
    while(true) 
    {
        string miMensaje;
        getline(cin, miMensaje);
        
        if (miMensaje == "salir") break;
        if (miMensaje.empty()) continue;
        
        vector<unsigned char> basuraEnviada = cripto.encriptado(miMensaje, LlaveMaestra);
        
        cout << "Mensaje encriptado enviado: ";
        for(unsigned char b : basuraEnviada) 
        {
            cout << hex << setw(2) << setfill('0') << (int)b << " ";
        }
        cout << dec << "\n> ";
        
        red.EnviarMensaje(basuraEnviada);
    }

    return 0;
}
