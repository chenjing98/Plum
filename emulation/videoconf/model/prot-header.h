#ifndef PROT_HEADER_H
#define PROT_HEADER_H

#include "ns3/header.h"

namespace ns3
{
    class VcaAppProtHeader : public Header
    {
    public:
        VcaAppProtHeader();
        VcaAppProtHeader(uint16_t frame_id, uint32_t packet_id);
        ~VcaAppProtHeader();
        static TypeId GetTypeId(void);
        TypeId GetInstanceTypeId(void) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator start) const;
        uint32_t Deserialize(Buffer::Iterator start);
        void Print(std::ostream &os) const;

        void SetFrameId(uint16_t frame_id);
        void SetPacketId(uint32_t packet_id);
        uint16_t GetFrameId(void);
        uint32_t GetPacketId(void);

    private:
        uint16_t m_frame_id;
        uint32_t m_packet_id;
    };

} // namespace ns3

#endif /* PROT_HEADER_H */