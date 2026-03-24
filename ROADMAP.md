# Hoja de Ruta del Proyecto: P2P Chat 

> **Nota:** Este documento sirve para sincronizar nuestro progreso. Las primeras dos partes del proyecto ya estan funcionando en local. El siguiente paso es enfocarnos en la Red.

- [x] Creación de la clase `EncryptionEngine`.
- [x] Implementación de la encriptación XChaCha20-Poly1305 con libsodium.
- [x] Prueba de consola funcional (cifrado/descifrado de strings puros).
- [x] Validación hexadecimal terminada.

## Fase 3: La Comunicación (Capa de Red)
*(Completado y funcional)*
- [x] Implementar arquitectura Cliente/Servidor usando la librería Asio (`asio_inc`).
- [x] Configurar el `NetworkManager` para manejar sockets TCP locales/remotos.
- [x] Establecer una conexión básica enviando texto plano entre dos terminales.
- [x] **Extra de Seguridad (E2EE):** Generar `ParLlaves` e intercambiar las Llaves Públicas por el socket para derivar la contraseña maestra.
- [x] Integrar el `EncryptionEngine` para cifrar y enviar el texto con la contraseña maestra acordada y descifrarlo en el nodo receptor.

## Fase 4: Migración a Servidor Central E2EE (El "Cartero Ciego")
*(Siguiente paso a implementar por el equipo)*

**1. Refactorización del NetworkManager (Multihilos para el Servidor)**
- [ ] Modificar el Servidor para que acepte múltiples conexiones continuas usando un bucle `while(true)` y almacene a los clientes en un arreglo dinámico (`std::vector<std::shared_ptr<socket>>`).
- [ ] Abrir un hilo (`std::thread`) dedicado para escuchar a cada cliente individual de forma asíncrona.

**2. Lógica de Enrutamiento (Blind Forwarding)**
- [ ] Crear la lógica de retransmisión en el Servidor.
- [ ] Garantizar que el Servidor NUNCA importe ni utilice la clase `EncryptionEngine` (aislamiento criptográfico).
- [ ] Cuando el Servidor reciba basura binaria de un Cliente, iterará por toda su lista de sockets enviando una copia a los demás clientes conectados.

**3. Intercambio de Llaves Mediado (Diffie-Hellman)**
- [ ] Los Clientes (A y B) generarán su `ParLlaves` (Pública/Privada) localmente al arrancar el programa.
- [ ] Los Clientes enviarán su Llave Pública al Servidor Central, y este la retransmitirá (Forwarding) al destinatario.
- [ ] Los Clientes cruzarán las llaves públicas recibidas para derivar la **Llave Maestra Secreta** idéntica en ambas máquinas.

**4. Demostración en Consola (Prueba 27001)**
- [ ] Modificar el `main.cpp` para ejecutar 3 pestañas: 1 Servidor Central (solo imprime la basura que mueve) y 2 Clientes (que imprimen el chat limpio).

## Fase 5: El Rostro (Interfaz con Dear ImGui)
- [ ] Integrar el backend de GLFW + Dear ImGui.

- [ ] Diseñar el sistema de guardado y carga de mensajes locales.
- [ ] Actualizar `StorageManager` para gestionar lectura/escritura en disco.
- [ ] Encriptar el archivo del historial local para proteger los logs en reposo.


