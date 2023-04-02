#ifndef VCA_CLIENT_H
#define VCA_CLIENT_H

#include "ns3/application.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/socket.h"
#include <deque>
#include <unordered_map>
#include <algorithm>

#include "prot-header.h"

enum POLICY
{
    VANILLA,
    YONGYULE
};

namespace ns3
{

    class VcaClient : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaClient();
        ~VcaClient();

        void SetLocalAddress(Ipv4Address local);
        void SetPeerAddress(std::vector<Ipv4Address> peer_list);
        void SetLocalUlPort(uint16_t port);
        void SetLocalDlPort(uint16_t port);
        void SetPeerPort(uint16_t port);

        Ptr<Socket> GetSocketDl(void);

        void SetFps(uint8_t fps);
        void SetBitrate(uint32_t bitrate);
        void SetMaxBitrate(uint32_t bitrate);

        void SetNodeId(uint32_t node_id);

        void SetPolicy(POLICY policy);

        static const uint32_t payloadSize = 524; // internet TCP MTU = 576B, - 20B(IP header) - 20B(TCP header) - 12B(VCA header)

    protected:
        void DoDispose(void);

    private:
        void StartApplication(void);
        void StopApplication(void);

        void ConnectionSucceededUl(Ptr<Socket> socket);
        void ConnectionFailedUl(Ptr<Socket> socket);
        void DataSendUl(Ptr<Socket> socket);

        void HandleRead(Ptr<Socket> socket);
        void HandleAccept(Ptr<Socket> socket, const Address &from);
        void HandlePeerClose(Ptr<Socket> socket);
        void HandlePeerError(Ptr<Socket> socket);

        void SendData(Ptr<Socket> socket);
        void ReceiveData(Ptr<Packet> packet);

        void EncodeFrame();
        void UpdateEncodeBitrate();

        void DecideBottleneckPosition();
        float DecideDlParam();

        void OutputStatistics();

        uint32_t m_node_id;

        Ptr<Socket> m_socket_dl;

        std::list<Ptr<Socket>> m_socket_list_ul;
        std::list<Ptr<Socket>> m_socket_list_dl;
        std::unordered_map<Ptr<Socket>, uint8_t> m_socket_id_map_ul;
        uint8_t m_socket_id_ul;
        std::unordered_map<Ptr<Socket>, uint8_t> m_socket_id_map_dl;
        uint8_t m_socket_id_dl;
        std::vector<Ipv4Address> m_peer_list;

        TypeId m_tid;

        Ipv4Address m_local;

        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;
        uint16_t m_peer_ul_port;

        uint8_t m_fps;
        uint32_t m_bitrate;     // in kbps
        uint32_t m_max_bitrate; // in kbps
        uint16_t m_frame_id;

        EventId m_enc_event;
        std::vector<uint64_t> m_cc_rate;

        uint32_t m_total_packet_bit;
        std::vector<uint32_t> m_min_packet_bit;
        std::vector<uint32_t> m_send_persec;
        std::vector<uint32_t> m_recv_persec;

        std::vector<std::deque<Ptr<Packet>>> m_send_buffer_list;

        bool m_is_my_wifi_access_bottleneck;

        POLICY m_policy;

        uint32_t m_target_dl_bitrate_redc_factor;

    }; // class VcaClient

}; // namespace ns3

#endif