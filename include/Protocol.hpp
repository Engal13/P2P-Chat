#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>

enum class PacketType : uint8_t {
    RegisterClient = 0x00,
    HandshakeKey = 0x01,
    ChatMessage = 0x02
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t target_id; // 0 = Servidor, >0 = ID de otro cliente (destinatario)
    PacketType type;
    uint32_t payload_size;
};
#pragma pack(pop)

#endif
