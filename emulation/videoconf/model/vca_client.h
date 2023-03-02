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

namespace ns3
{

    class VcaClient : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaClient();
        ~VcaClient();

        void SetLocalAddress(Ipv4Address local);
        void SetPeerAddress(Address peer);
        void SetLocalUlPort(uint16_t port);
        void SetLocalDlPort(uint16_t port);

        Ptr<Socket> GetSocketUl(void);
        Ptr<Socket> GetSocketDl(void);

        void SetFps(uint8_t fps);
        void SetBitrate(uint32_t bitrate);

        void SetNodeId(uint32_t node_id);

        static const uint32_t payloadSize = 1448;

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

        void OutputStatistics();

        uint32_t m_node_id;

        Ptr<Socket> m_socket_ul;
        Ptr<Socket> m_socket_dl;

        std::list<Ptr<Socket>> m_socketList_dl;

        bool m_connected_ul;

        TypeId m_tid;

        Ipv4Address m_local;
        Address m_peer;

        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;

        uint8_t m_fps;
        uint32_t m_bitrate; // in kbps

        EventId m_enc_event;

        std::deque<Ptr<Packet>> m_send_buffer;

    }; // class VcaClient

}; // namespace ns3

#endif