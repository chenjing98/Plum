#ifndef PROT_HEADER_H
#define PROT_HEADER_H

#include "ns3/header.h"
#include "ns3/object.h"

namespace ns3
{
    enum POLICY
    {
        VANILLA,
        YONGYULE, // deprecated
        POLO,
        FIXED
    };

    enum RATE_CONTROL_STATE
    {
        NATRUAL,
        CONSTRAINED
    };

    uint16_t const VCA_APP_PROT_HEADER_LENGTH = 16 + 4;
    class VcaAppProtHeader : public Header
    {
    public:
        VcaAppProtHeader();
        VcaAppProtHeader(uint16_t frame_id, uint16_t packet_id);
        ~VcaAppProtHeader();
        static TypeId GetTypeId(void);
        TypeId GetInstanceTypeId(void) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator start) const;
        uint32_t Deserialize(Buffer::Iterator start);
        void Print(std::ostream &os) const;

        void SetFrameId(uint16_t frame_id);
        void SetPacketId(uint16_t packet_id);
        void SetUlTargetRate(uint32_t ul_target_rate);
        void SetPayloadSize(uint32_t payload_size);
        void SetSrcId(uint32_t src_id);
        void SetSendTime(uint32_t send_time);
        uint16_t GetFrameId(void);
        uint16_t GetPacketId(void);
        uint32_t GetUlTargetRate(void);
        uint32_t GetPayloadSize(void);
        uint32_t GetSrcId(void);
        uint32_t GetSendTime(void);

        void Reset(void);

    private:
        uint16_t m_frame_id;
        uint16_t m_packet_id;
        uint32_t m_payload_size;
        uint32_t m_src_id;
        uint32_t m_ul_target_rate_bps;
        uint32_t m_send_time;
    };

} // namespace ns3

#endif /* PROT_HEADER_H */