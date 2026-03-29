#include "../include/Interface.hpp"
#include <exception>
#include <iostream>

int main()
{
    try {
        // Punto de entrada: delega toda la ejecucion a la interfaz grafica.
        Interface app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        // Captura de seguridad para errores no controlados.
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }
}
