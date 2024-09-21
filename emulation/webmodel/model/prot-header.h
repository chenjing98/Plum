#ifndef PROT_HEADER_H
#define PROT_HEADER_H

#include "ns3/header.h"
#include "ns3/object.h"

namespace ns3
{
    class WebAppProtHeader : public Header
    {
    public:
        WebAppProtHeader();
        WebAppProtHeader(uint16_t frame_id, uint16_t packet_id);
        ~WebAppProtHeader();
        static TypeId GetTypeId(void);
        TypeId GetInstanceTypeId(void) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator start) const;
        uint32_t Deserialize(Buffer::Iterator start);
        void Print(std::ostream &os) const;

        void SetFrameId(uint16_t frame_id);
        void SetPacketId(uint16_t packet_id);
        void SetDlRedcFactor(uint32_t dlredcfactor);
        void SetPayloadSize(uint32_t payload_size);
        uint16_t GetFrameId(void);
        uint16_t GetPacketId(void);
        uint32_t GetDlRedcFactor(void);
        uint32_t GetPayloadSize(void);

        void Reset(void);

    private:
        uint16_t m_frame_id;
        uint16_t m_packet_id;
        uint32_t m_dl_bitrate_reduce_factor;
        uint32_t m_payload_size;
    };

    class WebAppProtHeaderInfo : public Object
    {
    public:
        static TypeId GetTypeId(void);
        WebAppProtHeaderInfo();
        ~WebAppProtHeaderInfo();
        WebAppProtHeaderInfo(uint16_t frame_id, uint16_t packet_id);

        void SetPayloadSize(uint32_t payload_size);
        uint16_t GetFrameId(void);
        uint16_t GetPacketId(void);
        uint32_t GetPayloadSize(void);

    private:
        uint16_t m_frame_id;
        uint16_t m_packet_id;
        uint32_t m_payload_size;
    };

} // namespace ns3

#endif /* PROT_HEADER_H */