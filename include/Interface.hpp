#ifndef INTERFACE_HPP
#define INTERFACE_HPP

class Interface {
public:
  Interface();
  ~Interface();

  // Inicializar contexto de ImGui, GLFW/SDL, etc.
  void init();

  // Limpiar recursos
  void shutdown();

  // Renderizar un frame de la interfaz de usuario
  void renderFrame();
};

#endif
