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

    cout << "1. Iniciar Servidor Central (Router)" << endl;
    cout << "2. Entrar como Anfitrion de Chat" << endl;
    cout << "3. Entrar a Chat Existente (Invitado)" << endl;
    cout << "Seleccione: ";
    
    int opcion;
    cin >> opcion;

    if (opcion == 1) 
    {
        cout << "Puerto para alojar el Servidor Central: ";
        int port; cin >> port;
        red.IniciarServidor(port); 
        return 0; 
    }

    string LlaveMaestra;
    ParLlaves misLlaves = cripto.generarParLlaves();
    vector<unsigned char> miPublicaVector(misLlaves.llavePublica.begin(), misLlaves.llavePublica.end());

    try 
    {
        string ip; int port;
        cout << "IP del Servidor Central: "; cin >> ip;
        cout << "Puerto: "; cin >> port;
        red.Conectar(ip, port);

        if (opcion == 2) 
        {
            cout << "[Anfitrión] Esperando a que tu amigo se conecte y mande su llave..." << endl;
            vector<unsigned char> llaveAmigoVector = red.RecibirMensaje();
            cout << "[Anfitrión] ¡Llave recibida! Mandandole la tuya..." << endl;
            red.EnviarMensaje(miPublicaVector);

            string llaveAmigoString(llaveAmigoVector.begin(), llaveAmigoVector.end());
            LlaveMaestra = cripto.CombinarLlaves(misLlaves.llavePrivada, llaveAmigoString);
        }
        else if (opcion == 3) 
        {
            cout << "[Invitado] Mandando tu llave al anfitrion..." << endl;
            red.EnviarMensaje(miPublicaVector);
            cout << "[Invitado] Esperando la llave del anfitrion..." << endl;
            vector<unsigned char> llaveAmigoVector = red.RecibirMensaje();
            
            string llaveAmigoString(llaveAmigoVector.begin(), llaveAmigoVector.end());
            LlaveMaestra = cripto.CombinarLlaves(misLlaves.llavePrivada, llaveAmigoString);
        }
    } 
    catch (const exception& e) {
        cerr << "Error crítico de conexion: " << e.what() << endl; return 1;
    }

    cout << "\n[!] LLAVE MAESTRA DIFFIE-HELLMAN LISTA. INICIANDO CHAT." << endl;

    string miNombre;
    cout << "Ingresar nombre" << endl;
    cin >> miNombre;
    
    cin.ignore(10000, '\n');

    red.ejectuarLoop([&cripto, LlaveMaestra](vector<unsigned char> basuraRecibida) 
    {
        try 
        {
            string mensajeLimpio = cripto.descifrar(basuraRecibida, LlaveMaestra);
            cout << "\n: " << mensajeLimpio << "\n> " << flush;
        }
        catch (const exception& e) 
        {
            cerr << "\n[Error Desencriptando]: " << e.what() << "\n> " << flush;
        }
    });

    cin.ignore(10000, '\n');
    cout << "> ";
    
    while(true) 
    {
        string miMensaje;
        getline(cin, miMensaje);
        string mensajeEmpaquetado = miNombre + "|" + miMensaje;
        
        if (miMensaje == "salir") break;
        if (miMensaje.empty()) continue;
        
        vector<unsigned char> basuraEnviada = cripto.encriptado(mensajeEmpaquetado, LlaveMaestra);
        red.EnviarMensaje(basuraEnviada);
        cout << "> ";
    }

    return 0;
}
