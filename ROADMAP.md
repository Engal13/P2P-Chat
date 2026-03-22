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

- [ ] Integrar el backend de GLFW + Dear ImGui.
- [ ] Crear la ventana principal con input de texto y botón "Enviar".
- [ ] Conectar los eventos de UI con la capa criptográfica y de red.
- [ ] Añadir sección de "Traffic Monitor" para visualizar los bytes crudos (hexadecimales) enviados y recibidos.

- [ ] Diseñar el sistema de guardado y carga de mensajes locales.
- [ ] Actualizar `StorageManager` para gestionar lectura/escritura en disco.
- [ ] Encriptar el archivo del historial local para proteger los logs en reposo.

## Fase 6: Expansión a Chat Grupal (Opcional / Hardcore)
- [ ] Modificar `NetworkManager` para dejar el `acceptor` en un bucle continuo y aceptar a múltiples clientes.
- [ ] Mapear de cada cliente (Socket) a su respectiva `LlaveMaestra` (Diffie-Hellman) individual.
- [ ] Permitir que el Host reciba mensajes cifrados de un nodo y los retransmita (forwarding) cifrándolos de nuevo con las llaves de los demás destinatarios.
