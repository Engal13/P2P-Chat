#ifndef INTERFACE_HPP
#define INTERFACE_HPP

class Interface {
public:
  // Inicializa y ejecuta la aplicacion de escritorio.
  Interface();
  ~Interface();

  // Inicia el loop principal de UI y red.
  void run();
};

#endif
