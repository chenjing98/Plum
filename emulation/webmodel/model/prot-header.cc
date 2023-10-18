#include "prot-header.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("WebAppProtHeader");

    TypeId WebAppProtHeader::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::WebAppProtHeader")
                                .SetParent<Header>()
                                .SetGroupName("videoconf")
                                .AddConstructor<WebAppProtHeader>();
        return tid;
    };

    TypeId WebAppProtHeader::GetInstanceTypeId(void) const
    {
        return GetTypeId();
    };

    WebAppProtHeader::WebAppProtHeader()
        : m_frame_id(0),
          m_packet_id(0),
          m_dl_bitrate_reduce_factor(1e4){};

    WebAppProtHeader::WebAppProtHeader(uint16_t frame_id, uint16_t packet_id)
        : m_frame_id(frame_id),
          m_packet_id(packet_id)
    {
        m_dl_bitrate_reduce_factor = 1e4;
    };

    WebAppProtHeader::~WebAppProtHeader(){};

    void
    WebAppProtHeader::Serialize(Buffer::Iterator start) const
    {
        start.WriteHtonU16(m_frame_id);
        start.WriteHtonU16(m_packet_id);
        start.WriteHtonU32(m_dl_bitrate_reduce_factor);
        start.WriteHtonU32(m_payload_size);
    };

    uint32_t
    WebAppProtHeader::Deserialize(Buffer::Iterator start)
    {
        m_frame_id = start.ReadNtohU16();
        m_packet_id = start.ReadNtohU16();
        m_dl_bitrate_reduce_factor = start.ReadNtohU32();
        m_payload_size = start.ReadNtohU32();
        return GetSerializedSize();
    };

    uint32_t
    WebAppProtHeader::GetSerializedSize(void) const
    {
        return sizeof(m_frame_id) + sizeof(m_packet_id) + sizeof(m_dl_bitrate_reduce_factor) + sizeof(m_payload_size);
    };

    void
    WebAppProtHeader::Print(std::ostream &os) const
    {
        os << "FrameId= " << m_frame_id << " PacketId= " << m_packet_id << " TargetFrameSize= " << m_dl_bitrate_reduce_factor << " PayloadSize= " << m_payload_size;
    };

    void
    WebAppProtHeader::SetFrameId(uint16_t frame_id)
    {
        m_frame_id = frame_id;
    };

    void
    WebAppProtHeader::SetPacketId(uint16_t packet_id)
    {
        m_packet_id = packet_id;
    };

    void
    WebAppProtHeader::SetDlRedcFactor(uint32_t new_factor)
    {
        m_dl_bitrate_reduce_factor = new_factor;
    };

    void
    WebAppProtHeader::SetPayloadSize(uint32_t payload_size)
    {
        m_payload_size = payload_size;
    };

    uint16_t
    WebAppProtHeader::GetFrameId(void)
    {
        return m_frame_id;
    };

    uint16_t
    WebAppProtHeader::GetPacketId(void)
    {
        return m_packet_id;
    };

    uint32_t
    WebAppProtHeader::GetDlRedcFactor(void)
    {
        return m_dl_bitrate_reduce_factor;
    };

    uint32_t
    WebAppProtHeader::GetPayloadSize(void)
    {
        return m_payload_size;
    };

    void
    WebAppProtHeader::Reset()
    {
        m_frame_id = 0;
        m_packet_id = 0;
        m_dl_bitrate_reduce_factor = 1e4;
        m_payload_size = 0;
    };

    // WebAppProtHeaderInfo

    TypeId
    WebAppProtHeaderInfo::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::WebAppProtHeaderInfo")
                                .SetParent<Object>();
        return tid;
    };

    WebAppProtHeaderInfo::WebAppProtHeaderInfo(){};

    WebAppProtHeaderInfo::~WebAppProtHeaderInfo(){};

    WebAppProtHeaderInfo::WebAppProtHeaderInfo(uint16_t frame_id, uint16_t packet_id)
    {
        m_frame_id = frame_id;
        m_packet_id = packet_id;
        m_payload_size = 0;
    };

    void
    WebAppProtHeaderInfo::SetPayloadSize(uint32_t payload_size)
    {
        m_payload_size = payload_size;
    };

    uint16_t
    WebAppProtHeaderInfo::GetFrameId(void)
    {
        return m_frame_id;
    };

    uint16_t
    WebAppProtHeaderInfo::GetPacketId(void)
    {
        return m_packet_id;
    };

    uint32_t
    WebAppProtHeaderInfo::GetPayloadSize(void)
    {
        return m_payload_size;
    };

} // namespace ns3