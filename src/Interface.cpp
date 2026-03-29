#include "../include/Interface.hpp"
#include "../include/EncryptionEngine.hpp"
#include "../include/KeyManager.hpp"
#include "../include/NetworkManager.hpp"
#include "../include/StorageManager.hpp"
#include "../lib/imgui/imgui.h"
#include "../lib/imgui/imgui_impl_glfw.h"
#include "../lib/imgui/imgui_impl_opengl3.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sodium.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
struct LoadedTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
};

struct ToastItem {
    std::string text;
    std::chrono::steady_clock::time_point expiresAt;
};

// Genera un ID determinista a partir del nombre para reconstruir contactos offline.
uint32_t idEstablePorNombre(const std::string& nombre)
{
    uint32_t hash = 2166136261u;
    for (unsigned char c : nombre) {
        hash ^= c;
        hash *= 16777619u;
    }
    if (hash == 0u) hash = 1u;
    return hash;
}

// Utilidad para validar prefijos en cadenas.
bool iniciaCon(const std::string& s, const std::string& prefijo)
{
    return s.rfind(prefijo, 0) == 0;
}

// Deriva una llave de 32 bytes desde una contrasena de usuario.
std::string derivarLlaveDeContrasena(const std::string& passwordHumana)
{
    unsigned char hashDe32Bytes[crypto_secretbox_KEYBYTES];
    crypto_generichash(hashDe32Bytes, sizeof(hashDe32Bytes),
        reinterpret_cast<const unsigned char*>(passwordHumana.data()), passwordHumana.length(),
        nullptr, 0);
    return std::string(reinterpret_cast<char*>(hashDe32Bytes), sizeof(hashDe32Bytes));
}

// Agrega una entrada al log evitando duplicado consecutivo.
void agregarLog(std::vector<std::string>& logs, const std::string& msg)
{
    if (!logs.empty() && logs.back() == msg) return;
    logs.push_back(msg);
    if (logs.size() > 500) {
        logs.erase(logs.begin(), logs.begin() + 200);
    }
}

// Carga una imagen PNG/JPG a textura OpenGL para ImGui.
bool cargarTexturaDesdeArchivo(const char* filename, LoadedTexture& outTexture)
{
    int imageWidth = 0;
    int imageHeight = 0;
    unsigned char* imageData = stbi_load(filename, &imageWidth, &imageHeight, nullptr, 4);
    if (imageData == nullptr) return false;

    GLuint imageTexture = 0;
    glGenTextures(1, &imageTexture);
    glBindTexture(GL_TEXTURE_2D, imageTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
    stbi_image_free(imageData);

    outTexture.id = imageTexture;
    outTexture.width = imageWidth;
    outTexture.height = imageHeight;
    outTexture.valid = true;
    return true;
}
} // namespace

Interface::Interface() {}
Interface::~Interface() {}

// Loop principal de interfaz: login, servidor, cliente y chat.
void Interface::run()
{
    if (sodium_init() < 0) {
        throw std::runtime_error("No se pudo inicializar libsodium");
    }

    if (!glfwInit()) {
        throw std::runtime_error("No se pudo inicializar GLFW");
    }

    const char* glslVersion = "#version 130";
    GLFWwindow* window = glfwCreateWindow(1700, 980, "P2P CHAT", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("No se pudo crear la ventana");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.45f, 0.70f, 0.90f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.60f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.38f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.15f, 0.50f, 0.75f, 0.75f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.62f, 0.90f, 0.90f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.10f, 0.45f, 0.70f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    EncryptionEngine motorCripto;
    StorageManager boveda;
    NetworkManager clienteRed;
    NetworkManager servidorLocal;
    KeyManager llavero;

    bool autenticado = false;
    bool conectado = false;
    bool modoRegistro = false;
    bool servidorIniciado = false;
    bool mostrarServidor = true;
    bool usarLocalhost = true;

    char usuarioBuf[64] = "";
    char contrasenaBuf[64] = "";
    char ipBuf[64] = "127.0.0.1";
    int puertoClienteUI = 6000;
    int puertoServerUI = 6000;
    char mensajeBuf[512] = "";
    char destinoBuf[16] = "1";

    std::string usuarioActual;
    std::string llaveBoveda;
    std::vector<std::string> logs;
    std::map<uint32_t, std::vector<std::string>> chatPorContacto;
    std::mutex uiMutex;
    std::mutex toastMutex;
    std::mutex llaveroMutex;
    std::map<uint32_t, std::string> contactosUI;
    std::map<std::string, uint32_t> usuarioAIdUI;
    std::map<uint32_t, bool> contactoActivoUI;
    std::vector<std::string> serverLogs;
    uint32_t contactoSeleccionado = 0;
    std::string estadoChat = "Sin conexion";
    auto ultimoAnuncio = std::chrono::steady_clock::now();
    bool enfocarUsuario = true;
    std::vector<ToastItem> toasts;
    bool autoscrollServerLog = true;
    bool autoRegistroActivo = false;
    int autoRegistroIntentos = 0;
    std::string errorLogin;
    std::string errorServidor;
    std::string errorConexion;
    std::string errorEnvio;
    LoadedTexture loginBackground;
    bool loginBgEnabled = false;

    agregarLog(logs, "Listo. Inicia sesion o registra usuario.");
    agregarLog(serverLogs, "Tab de servidor lista.");
    if (cargarTexturaDesdeArchivo("assets/login_bg.png", loginBackground) ||
        cargarTexturaDesdeArchivo("C:\\Users\\diego\\.cursor\\projects\\c-Users-diego-Desktop-Seguridad-P2P-Chat-main\\assets\\c__Users_diego_AppData_Roaming_Cursor_User_workspaceStorage_e411e318156aafb3bced90c726ba093b_images_Gemini_Generated_Image_ksy6xoksy6xoksy6-cbb0d28d-832b-4e31-a076-6c77e4e640e1.png", loginBackground)) {
        loginBgEnabled = true;
    }

    auto pushToast = [&](const std::string& text) {
        std::lock_guard<std::mutex> toastLock(toastMutex);
        toasts.push_back({text, std::chrono::steady_clock::now() + std::chrono::seconds(4)});
        if (toasts.size() > 5) {
            toasts.erase(toasts.begin());
        }
    };

    auto enviarRegistro = [&]() {
        std::vector<unsigned char> paqueteRegistro(usuarioActual.begin(), usuarioActual.end());
        paqueteRegistro.push_back('|');
        {
            std::lock_guard<std::mutex> keyLock(llaveroMutex);
            paqueteRegistro.insert(paqueteRegistro.end(), llavero.my_public_key,
                llavero.my_public_key + crypto_kx_PUBLICKEYBYTES);
        }
        clienteRed.EnviarMensaje(0, PacketType::RegisterClient, paqueteRegistro);
    };

    auto limpiarSesion = [&]() {
        if (conectado) {
            clienteRed.Desconectar();
            conectado = false;
        }
        autoRegistroActivo = false;
        autoRegistroIntentos = 0;
        {
            std::lock_guard<std::mutex> keyLock(llaveroMutex);
            llavero = KeyManager();
        }
        std::lock_guard<std::mutex> uiLock(uiMutex);
        contactosUI.clear();
        usuarioAIdUI.clear();
        contactoActivoUI.clear();
        chatPorContacto.clear();
        contactoSeleccionado = 0;
        estadoChat = "Sin conexion";
        errorConexion.clear();
        errorEnvio.clear();
        std::strncpy(destinoBuf, "1", sizeof(destinoBuf) - 1);
        destinoBuf[sizeof(destinoBuf) - 1] = '\0';
    };

    servidorLocal.SetServerLogger([&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(uiMutex);
        agregarLog(serverLogs, msg);
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::Begin("P2P CHAT", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (!autenticado) {
            if (loginBgEnabled && loginBackground.valid) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 bgMin = ImGui::GetWindowPos();
                const ImVec2 bgMax = ImVec2(bgMin.x + ImGui::GetWindowSize().x, bgMin.y + ImGui::GetWindowSize().y);
                drawList->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(loginBackground.id)), bgMin, bgMax);
                drawList->AddRectFilled(bgMin, bgMax, IM_COL32(8, 12, 20, 155));
            }

            const float panelW = 480.0f;
            const float panelH = 290.0f;
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float offX = (avail.x - panelW) * 0.5f;
            const float offY = (avail.y - panelH) * 0.5f;
            if (offX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);
            if (offY > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offY);
            ImGui::BeginChild("panel_login", ImVec2(panelW, panelH), true);
            ImGui::SetWindowFontScale(1.25f);
            ImGui::Text("Acceso seguro");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();
            if (enfocarUsuario) {
                ImGui::SetKeyboardFocusHere();
                enfocarUsuario = false;
            }
            ImGui::InputText("usuario", usuarioBuf, IM_ARRAYSIZE(usuarioBuf));
            ImGui::InputText("contrasena", contrasenaBuf, IM_ARRAYSIZE(contrasenaBuf), ImGuiInputTextFlags_Password);
            ImGui::Checkbox("registrar usuario nuevo", &modoRegistro);
            if (ImGui::Button("continuar", ImVec2(-1, 42))) {
                errorLogin.clear();
                const std::string usuario = usuarioBuf;
                const std::string pass = contrasenaBuf;
                if (usuario.empty() || pass.empty()) {
                    errorLogin = "Usuario y contrasena son obligatorios.";
                    agregarLog(logs, "Usuario y contrasena son obligatorios.");
                } else {
                    llaveBoveda = derivarLlaveDeContrasena(pass);
                    std::string usuarioCopia = usuario;
                    boveda.configurarBoveda(&motorCripto, llaveBoveda, usuarioCopia);
                    const std::string archivo = usuario + "_boveda.dat";
                    std::ifstream existe(archivo);
                    if (modoRegistro) {
                        if (existe.good()) {
                            errorLogin = "Ese usuario ya existe.";
                            agregarLog(logs, "Ese usuario ya existe.");
                        } else {
                            boveda.agregarMensaje("Sistema", "Bienvenido");
                            usuarioActual = usuario;
                            autenticado = true;
                            errorLogin.clear();
                            agregarLog(logs, "Cuenta creada. Sesion iniciada.");
                        }
                    } else {
                        if (!existe.good()) {
                            errorLogin = "Usuario no encontrado. Activa registro para crearlo.";
                            agregarLog(logs, "Usuario no encontrado. Activa registro para crearlo.");
                        } else {
                            try {
                                std::vector<unsigned char> data = boveda.loadFromFile(archivo);
                                std::string recuperado = motorCripto.descifrar(data, llaveBoveda);
                                boveda.deserializarHistorial(recuperado);
                                usuarioActual = usuario;
                                autenticado = true;
                                errorLogin.clear();
                                agregarLog(logs, "Sesion iniciada.");
                            } catch (const std::exception& e) {
                                errorLogin = std::string("Acceso denegado: ") + e.what();
                                agregarLog(logs, std::string("Acceso denegado: ") + e.what());
                            }
                        }
                    }

                    if (autenticado) {
                        contactosUI.clear();
                        usuarioAIdUI.clear();
                        contactoActivoUI.clear();
                        chatPorContacto.clear();

                        auto historial = boveda.obtenerHistorialChats();
                        for (const auto& par : historial) {
                            if (par.first == "__meta_contactos__" || par.first == "Sistema") continue;
                            uint32_t cid = idEstablePorNombre(par.first);
                            contactosUI[cid] = par.first;
                            usuarioAIdUI[par.first] = cid;
                            contactoActivoUI[cid] = false;
                            for (const auto& m : par.second) {
                                if (iniciaCon(m, "Yo: ")) {
                                    chatPorContacto[cid].push_back("[Yo -> " + par.first + "] " + m.substr(4));
                                } else {
                                    chatPorContacto[cid].push_back("[" + par.first + "] " + m);
                                }
                            }
                        }
                    }
                }
            }
            if (!errorLogin.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorLogin.c_str());
            }
            if (!loginBgEnabled) {
                ImGui::TextDisabled("Fondo no encontrado: usa assets/login_bg.png");
            }
            ImGui::EndChild();
        } else {
            ImGui::Text("Usuario activo: %s", usuarioActual.c_str());
            ImGui::Checkbox("mostrar tab servidor", &mostrarServidor);
            ImGui::Separator();

            if (ImGui::BeginTabBar("tabs_principales")) {
                if (mostrarServidor && ImGui::BeginTabItem("Servidor")) {
                    ImGui::Text("Servidor local");
                    ImGui::InputInt("puerto servidor", &puertoServerUI);
                    if (ImGui::Button("iniciar servidor local", ImVec2(220, 35)) && !servidorIniciado) {
                        errorServidor.clear();
                        if (puertoServerUI < 1 || puertoServerUI > 65535) {
                            errorServidor = "Puerto invalido para servidor.";
                            agregarLog(serverLogs, "Puerto invalido para servidor.");
                        } else {
                            servidorIniciado = true;
                            const int puertoServer = puertoServerUI;
                            std::thread([&servidorLocal, puertoServer]() {
                                servidorLocal.IniciarServidor(static_cast<int>(puertoServer));
                            }).detach();
                            agregarLog(serverLogs, "Servidor local levantado en puerto " + std::to_string(puertoServer));
                            pushToast("Servidor iniciado en puerto " + std::to_string(puertoServer));
                        }
                    }
                    ImGui::SameLine();
                    if (servidorIniciado) ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Activo");
                    ImGui::SameLine();
                    if (ImGui::Button("detener servidor", ImVec2(180, 35)) && servidorIniciado) {
                        servidorLocal.DetenerServidor();
                        servidorIniciado = false;
                        agregarLog(serverLogs, "Servidor apagado por usuario.");
                        pushToast("Servidor detenido");
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("autoscroll", &autoscrollServerLog);
                    if (!errorServidor.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorServidor.c_str());
                    }

                    ImGui::Separator();
                    ImGui::Text("Flujo de cifrado y ruteo");
                    ImGui::BeginChild("server_logs", ImVec2(0, 650), true);
                    {
                        std::lock_guard<std::mutex> lock(uiMutex);
                        for (const auto& l : serverLogs) ImGui::TextWrapped("%s", l.c_str());
                        if (autoscrollServerLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 30.0f) {
                            ImGui::SetScrollHereY(1.0f);
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Cliente / Chat")) {
                    if (conectado && autoRegistroActivo) {
                        auto ahora = std::chrono::steady_clock::now();
                        auto seg = std::chrono::duration_cast<std::chrono::seconds>(ahora - ultimoAnuncio).count();
                        if (seg >= 2 && autoRegistroIntentos > 0) {
                            enviarRegistro();
                            ultimoAnuncio = ahora;
                            --autoRegistroIntentos;
                            if (autoRegistroIntentos <= 0) {
                                autoRegistroActivo = false;
                            }
                        }
                    }

                    if (!conectado) {
                        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Paso 1: inicia o conecta al servidor");
                        ImGui::BulletText("Si no tienes servidor, abre tab Servidor y presiona iniciar servidor local.");
                        ImGui::BulletText("Puedes usar localhost o IP remota (ej: Hamachi).");
                        ImGui::BulletText("Cuando entren usuarios, selecciona contacto para chatear.");
                        ImGui::Separator();
                        ImGui::Text("Conexion cliente");
                        ImGui::Checkbox("usar localhost (127.0.0.1)", &usarLocalhost);
                        if (usarLocalhost) {
                            std::strncpy(ipBuf, "127.0.0.1", sizeof(ipBuf) - 1);
                            ipBuf[sizeof(ipBuf) - 1] = '\0';
                            ImGui::BeginDisabled();
                            ImGui::InputText("IP servidor", ipBuf, IM_ARRAYSIZE(ipBuf));
                            ImGui::EndDisabled();
                        } else {
                            ImGui::InputText("IP servidor", ipBuf, IM_ARRAYSIZE(ipBuf));
                        }
                        ImGui::InputInt("puerto", &puertoClienteUI);
                        if (ImGui::Button("conectar", ImVec2(180, 35))) {
                            errorConexion.clear();
                            std::string ipDestino = ipBuf;
                            if (ipDestino.empty()) {
                                errorConexion = "IP invalida.";
                                agregarLog(logs, "IP invalida.");
                            } else if (puertoClienteUI < 1 || puertoClienteUI > 65535) {
                                errorConexion = "Puerto invalido. Rango 1-65535.";
                                agregarLog(logs, "Puerto invalido. Rango 1-65535.");
                            } else if (clienteRed.Conectar(ipDestino, puertoClienteUI)) {
                                conectado = true;
                                agregarLog(logs, "Cliente conectado.");
                                estadoChat = "Conectado. Esperando contactos...";
                                ultimoAnuncio = std::chrono::steady_clock::now();
                                autoRegistroActivo = true;
                                autoRegistroIntentos = 4;
                                clienteRed.ejectuarLoop([&](NetworkManager::PaqueteRecibido paquete) {
                                    uint32_t suId = paquete.header.target_id;
                                    if (paquete.header.type == PacketType::RegisterClient) {
                                        std::string data(paquete.payload.begin(), paquete.payload.end());
                                        size_t sep = data.find('|');
                                        if (sep == std::string::npos) return;
                                        std::string suNombre = data.substr(0, sep);
                                        std::vector<unsigned char> suLlave(
                                            paquete.payload.begin() + static_cast<long long>(sep + 1),
                                            paquete.payload.end());
                                        std::lock_guard<std::mutex> keyLock(llaveroMutex);
                                        std::lock_guard<std::mutex> uiLock(uiMutex);
                                        auto itNombre = usuarioAIdUI.find(suNombre);
                                        if (itNombre != usuarioAIdUI.end() && itNombre->second != suId) {
                                            uint32_t viejo = itNombre->second;
                                            contactosUI.erase(viejo);
                                            llavero.active_sessions.erase(viejo);
                                            auto itChatViejo = chatPorContacto.find(viejo);
                                            if (itChatViejo != chatPorContacto.end()) {
                                                auto& chatNuevo = chatPorContacto[suId];
                                                chatNuevo.insert(chatNuevo.end(), itChatViejo->second.begin(), itChatViejo->second.end());
                                                chatPorContacto.erase(itChatViejo);
                                            }
                                            agregarLog(logs, "Reconexion detectada de " + suNombre +
                                                            ": reemplazo de ID " + std::to_string(viejo) +
                                                            " por ID " + std::to_string(suId));
                                        }
                                        bool contactoNuevo = (llavero.active_sessions.find(suId) == llavero.active_sessions.end());
                                        try {
                                            llavero.registrar_contacto(suId, suNombre, suLlave);
                                        } catch (...) {}
                                        contactosUI[suId] = suNombre;
                                        usuarioAIdUI[suNombre] = suId;
                                        contactoActivoUI[suId] = true;
                                        autoRegistroActivo = false;
                                        autoRegistroIntentos = 0;
                                        if (contactoSeleccionado == 0) {
                                            contactoSeleccionado = suId;
                                            std::snprintf(destinoBuf, sizeof(destinoBuf), "%u", suId);
                                        }
                                        if (suId != 0 && contactoNuevo) {
                                            std::vector<unsigned char> miResp(usuarioActual.begin(), usuarioActual.end());
                                            miResp.push_back('|');
                                            miResp.insert(miResp.end(), llavero.my_public_key,
                                                llavero.my_public_key + crypto_kx_PUBLICKEYBYTES);
                                            clienteRed.EnviarMensaje(suId, PacketType::RegisterClient, miResp);
                                        }
                                        agregarLog(logs, "Contacto disponible: " + suNombre + " (ID " + std::to_string(suId) + ")");
                                        estadoChat = "Tunel E2EE listo con contactos";
                                        pushToast("Contacto conectado: " + suNombre);
                                    } else if (paquete.header.type == PacketType::ChatMessage) {
                                        std::lock_guard<std::mutex> keyLock(llaveroMutex);
                                        std::lock_guard<std::mutex> uiLock(uiMutex);
                                        try {
                                            std::string llaveRx = llavero.obtener_llave_rx(suId);
                                            std::string msg = motorCripto.descifrar(paquete.payload, llaveRx);
                                            std::string nombre = contactosUI.count(suId) ? contactosUI[suId] : ("ID " + std::to_string(suId));
                                            chatPorContacto[suId].push_back("[" + nombre + "] " + msg);
                                            boveda.agregarMensaje(nombre, msg);
                                            agregarLog(logs, "Mensaje recibido cifrado (" + std::to_string(paquete.payload.size()) +
                                                            " bytes) y descifrado desde " + nombre);
                                        } catch (const std::exception& e) {
                                            agregarLog(logs, std::string("Error descifrado: ") + e.what());
                                        }
                                    } else if (paquete.header.type == PacketType::UserDisconnected) {
                                        std::lock_guard<std::mutex> keyLock(llaveroMutex);
                                        std::lock_guard<std::mutex> uiLock(uiMutex);
                                        uint32_t desconectadoId = suId;
                                        std::string nombre = "ID " + std::to_string(desconectadoId);
                                        auto it = contactosUI.find(desconectadoId);
                                        if (it != contactosUI.end()) nombre = it->second;
                                        if (!paquete.payload.empty()) {
                                            nombre = std::string(paquete.payload.begin(), paquete.payload.end());
                                        }
                                        llavero.active_sessions.erase(desconectadoId);
                                        contactoActivoUI[desconectadoId] = false;
                                        if (contactoSeleccionado == desconectadoId) {
                                            estadoChat = "El usuario seleccionado se desconecto";
                                        }
                                        agregarLog(logs, nombre + " se ha desconectado.");
                                        pushToast(nombre + " se desconecto");
                                    }
                                }, [&](const std::string& causa) {
                                    std::lock_guard<std::mutex> uiLock(uiMutex);
                                    conectado = false;
                                    autoRegistroActivo = false;
                                    autoRegistroIntentos = 0;
                                    for (auto& par : contactoActivoUI) par.second = false;
                                    estadoChat = "Desconectado del servidor";
                                    agregarLog(logs, "Conexion con servidor perdida: " + causa);
                                    pushToast("Servidor desconectado");
                                });
                                enviarRegistro();
                            } else {
                                errorConexion = "No se pudo conectar al servidor.";
                                agregarLog(logs, "No se pudo conectar al servidor.");
                            }
                        }
                        if (!errorConexion.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorConexion.c_str());
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("reenviar registro", ImVec2(180, 35))) {
                            if (conectado) {
                                enviarRegistro();
                                agregarLog(logs, "Registro reenviado al servidor.");
                            }
                        }
                    } else {
                        ImVec4 estadoColor = (contactoSeleccionado == 0) ? ImVec4(1.0f, 0.5f, 0.3f, 1.0f) : ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
                        ImGui::TextColored(estadoColor, "Estado: %s", estadoChat.c_str());
                        ImGui::Separator();
                        ImGui::Columns(2, nullptr, true);
                        ImGui::Text("Contactos (chat separado por usuario)");
                        ImGui::BeginDisabled();
                        ImGui::InputText("ID destino", destinoBuf, IM_ARRAYSIZE(destinoBuf));
                        ImGui::EndDisabled();
                        ImGui::BeginChild("contactos", ImVec2(0, 540), true);
                        {
                            std::lock_guard<std::mutex> lock(uiMutex);
                            if (contactosUI.empty()) {
                                ImGui::TextDisabled("Sin contactos");
                            } else {
                                for (const auto& par : contactosUI) {
                                    bool isSelected = (contactoSeleccionado == par.first);
                                    bool activo = contactoActivoUI.count(par.first) ? contactoActivoUI[par.first] : false;
                                    std::string estado = activo ? "online" : "offline";
                                    std::string label = "ID " + std::to_string(par.first) + " - " + par.second + " (" + estado + ")";
                                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                                        contactoSeleccionado = par.first;
                                        std::snprintf(destinoBuf, sizeof(destinoBuf), "%u", par.first);
                                        estadoChat = "Destino seleccionado: " + par.second;
                                    }
                                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                                        contactoSeleccionado = par.first;
                                        std::snprintf(destinoBuf, sizeof(destinoBuf), "%u", par.first);
                                        agregarLog(logs, "Destino seleccionado por doble click: " + par.second);
                                        estadoChat = "Destino seleccionado: " + par.second;
                                    }
                                }
                            }
                        }
                        ImGui::EndChild();

                        ImGui::NextColumn();
                        ImGui::SetWindowFontScale(1.25f);
                        ImGui::Text("Crear mensaje");
                        ImGui::SetWindowFontScale(1.0f);
                        if (contactoSeleccionado != 0) {
                            std::string nombreSel = contactosUI.count(contactoSeleccionado) ? contactosUI[contactoSeleccionado] : "Contacto";
                            ImGui::Text("Contacto: %s", nombreSel.c_str());
                        }
                        ImGui::BeginChild("chat", ImVec2(0, 500), true);
                        {
                            std::lock_guard<std::mutex> lock(uiMutex);
                            if (contactoSeleccionado == 0) {
                                ImGui::TextDisabled("Selecciona un contacto para ver su chat.");
                            } else {
                                auto itChat = chatPorContacto.find(contactoSeleccionado);
                                if (itChat == chatPorContacto.end() || itChat->second.empty()) {
                                    ImGui::TextDisabled("No hay mensajes con este contacto.");
                                } else {
                                    for (const auto& linea : itChat->second) ImGui::TextWrapped("%s", linea.c_str());
                                    ImGui::SetScrollHereY(1.0f);
                                }
                            }
                        }
                        ImGui::EndChild();
                        bool enterEnviar = ImGui::InputText("mensaje", mensajeBuf, IM_ARRAYSIZE(mensajeBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                        bool puedeEnviar = contactoSeleccionado != 0 && std::strlen(mensajeBuf) > 0;
                        if (contactoSeleccionado != 0 && std::strlen(destinoBuf) == 0) {
                            std::snprintf(destinoBuf, sizeof(destinoBuf), "%u", contactoSeleccionado);
                        }
                        ImGui::BeginDisabled(!puedeEnviar);
                        if (ImGui::Button("enviar", ImVec2(140, 35)) || (enterEnviar && puedeEnviar)) {
                            errorEnvio.clear();
                            int id = 0;
                            try { id = std::stoi(destinoBuf); } catch (...) { id = 0; }
                            const std::string mensaje = mensajeBuf;
                            if (id <= 0 || mensaje.empty()) {
                                errorEnvio = "Selecciona contacto y escribe mensaje.";
                                agregarLog(logs, "ID destino y mensaje son obligatorios.");
                            } else {
                                std::lock_guard<std::mutex> keyLock(llaveroMutex);
                                try {
                                    bool estaActivo = contactoActivoUI.count(static_cast<uint32_t>(id)) && contactoActivoUI[static_cast<uint32_t>(id)];
                                    if (!estaActivo) {
                                        throw std::runtime_error("El usuario esta desconectado, no puedes enviar mensaje");
                                    }
                                    if (llavero.active_sessions.find(static_cast<uint32_t>(id)) == llavero.active_sessions.end()) {
                                        throw std::runtime_error("Contacto offline o sin tunel E2EE");
                                    }
                                    std::string llaveTx = llavero.obtener_llave_tx(static_cast<uint32_t>(id));
                                    std::vector<unsigned char> cifrado = motorCripto.encriptado(mensaje, llaveTx);
                                    clienteRed.EnviarMensaje(static_cast<uint32_t>(id), PacketType::ChatMessage, cifrado);
                                    std::string nombre = "ID " + std::to_string(id);
                                    {
                                        std::lock_guard<std::mutex> lock(uiMutex);
                                        auto it = contactosUI.find(static_cast<uint32_t>(id));
                                        if (it != contactosUI.end()) nombre = it->second;
                                        chatPorContacto[static_cast<uint32_t>(id)].push_back("[Yo -> " + nombre + "] " + mensaje);
                                    }
                                    boveda.agregarMensaje(nombre, "Yo: " + mensaje);
                                    agregarLog(logs, "Mensaje cifrado y enviado a " + nombre +
                                                    " (" + std::to_string(cifrado.size()) + " bytes)");
                                    std::memset(mensajeBuf, 0, sizeof(mensajeBuf));
                                    estadoChat = "Mensaje enviado a " + nombre;
                                } catch (const std::exception& e) {
                                    errorEnvio = e.what();
                                    agregarLog(logs, std::string("No se pudo enviar: ") + e.what());
                                    estadoChat = "Error al enviar";
                                }
                            }
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        if (ImGui::Button("cerrar sesion", ImVec2(170, 35))) {
                            limpiarSesion();
                            autenticado = false;
                            usuarioActual.clear();
                            llaveBoveda.clear();
                            std::memset(contrasenaBuf, 0, sizeof(contrasenaBuf));
                            errorLogin.clear();
                            agregarLog(logs, "Sesion cerrada.");
                        }
                        if (!puedeEnviar) {
                            ImGui::TextDisabled("Tip: selecciona un contacto y escribe mensaje.");
                        }
                        if (!errorEnvio.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorEnvio.c_str());
                        }
                        ImGui::Columns(1);
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        if (ImGui::Button("salir")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui::End();

        auto now = std::chrono::steady_clock::now();
        std::vector<ToastItem> toastsSnapshot;
        {
            std::lock_guard<std::mutex> toastLock(toastMutex);
            toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
                [&](const ToastItem& t) { return t.expiresAt <= now; }), toasts.end());
            toastsSnapshot = toasts;
        }
        if (!toastsSnapshot.empty()) {
            const ImVec2 display = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::SetNextWindowPos(ImVec2(display.x - 30.0f, 30.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGuiWindowFlags toastFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                         ImGuiWindowFlags_NoNav;
            ImGui::Begin("toasts_overlay", nullptr, toastFlags);
            for (const auto& t : toastsSnapshot) {
                ImGui::TextWrapped("%s", t.text.c_str());
                ImGui::Separator();
            }
            ImGui::End();
        }

        ImGui::Render();

        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    clienteRed.Desconectar();
    servidorLocal.DetenerServidor();
    if (loginBackground.valid) {
        glDeleteTextures(1, &loginBackground.id);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
