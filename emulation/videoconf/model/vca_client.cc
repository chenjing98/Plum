#include "vca_client.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("VcaClient");

    TypeId VcaClient::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::VcaClient")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<VcaClient>();
        return tid;
    };

    VcaClient::VcaClient()
        : m_socket_ul(nullptr),
          m_socket_dl(nullptr),
          m_socketList_dl(),
          m_connected_ul(false),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_local_ul_port(0),
          m_fps(0),
          m_bitrate(0),
          m_enc_event(),
          m_send_buffer(){};

    VcaClient::~VcaClient(){};

    void VcaClient::DoDispose()
    {
        Application::DoDispose();
    };

    // Public helpers
    Ptr<Socket> VcaClient::GetSocketUl(void)
    {
        return m_socket_ul;
    };

    Ptr<Socket> VcaClient::GetSocketDl(void)
    {
        return m_socket_dl;
    };

    void VcaClient::SetFps(uint8_t fps)
    {
        m_fps = fps;
    };

    void VcaClient::SetBitrate(uint32_t bitrate)
    {
        m_bitrate = bitrate;
    };

    void VcaClient::SetLocalAddress(Ipv4Address local)
    {
        m_local = local;
    };

    void VcaClient::SetPeerAddress(Address peer)
    {
        m_peer = peer;
    };

    void VcaClient::SetLocalUlPort(uint16_t port)
    {
        m_local_ul_port = port;
    };

    void VcaClient::SetLocalDlPort(uint16_t port)
    {
        m_local_dl_port = port;
    };

    void VcaClient::SetNodeId(uint32_t node_id)
    {
        m_node_id = node_id;
    };

    // Application Methods
    void VcaClient::StartApplication()
    {
        m_enc_event = Simulator::ScheduleNow(&VcaClient::EncodeFrame, this);

        // Create the socket if not already
        if (!m_socket_ul)
        {
            m_socket_ul = Socket::CreateSocket(GetNode(), m_tid);

            if (m_socket_ul->Bind(InetSocketAddress{m_local, m_local_ul_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }

            m_socket_ul->Connect(m_peer);
            m_socket_ul->ShutdownRecv();
            m_socket_ul->SetConnectCallback(
                MakeCallback(&VcaClient::ConnectionSucceededUl, this),
                MakeCallback(&VcaClient::ConnectionFailedUl, this));
            m_socket_ul->SetSendCallback(
                MakeCallback(&VcaClient::DataSendUl, this));
        }

        if (!m_socket_dl)
        {
            m_socket_dl = Socket::CreateSocket(GetNode(), m_tid);
            if (m_socket_dl->Bind(InetSocketAddress{m_local, m_local_dl_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket_dl->Listen();
            m_socket_dl->ShutdownSend();

            m_socket_dl->SetRecvCallback(MakeCallback(&VcaClient::HandleRead, this));
            m_socket_dl->SetRecvPktInfo(true);
            m_socket_dl->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&VcaClient::HandleAccept, this));
            m_socket_dl->SetCloseCallbacks(
                MakeCallback(&VcaClient::HandlePeerClose, this),
                MakeCallback(&VcaClient::HandlePeerError, this));
        }

        if (m_connected_ul)
        {
            // send data
            SendData(m_socket_ul);
        }
    };

    void VcaClient::StopApplication()
    {
        OutputStatistics();
        if (m_enc_event.IsRunning())
        {
            Simulator::Cancel(m_enc_event);
        }

        while (!m_socketList_dl.empty())
        { // these are accepted sockets, close them
            Ptr<Socket> acceptedSocket = m_socketList_dl.front();
            m_socketList_dl.pop_front();
            acceptedSocket->Close();
        }
        if (m_socket_dl)
        {
            m_socket_dl->Close();
            m_socket_dl->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
        if (m_socket_ul)
        {
            m_socket_ul->Close();
            m_connected_ul = false;
        }
    };

    // Private helpers
    void VcaClient::ConnectionSucceededUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] Uplink connection succeeded");
        m_connected_ul = true;

        SendData(socket);
    };

    void VcaClient::ConnectionFailedUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient] Connection Failed");
    };

    void VcaClient::DataSendUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] Uplink datasend");
        if (m_connected_ul)
        {
            SendData(socket);
        }
    };

    void VcaClient::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandleRead");
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            if (packet->GetSize() == 0)
            { // EOF
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][ReceivePkt] PktSize(B)= 0");
                break;
            }

            if (InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet);
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " SrcPort= " << Inet6SocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet);
            }
        }
    };

    void
    VcaClient::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandleAccept");
        socket->SetRecvCallback(MakeCallback(&VcaClient::HandleRead, this));
        m_socketList_dl.push_back(socket);
    };

    void
    VcaClient::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandlePeerClose");
    };

    void
    VcaClient::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandlePeerError");
    };

    // TX RX Logics
    void
    VcaClient::SendData(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] SendData");
        if (!m_send_buffer.empty())
        {
            Ptr<Packet> packet = m_send_buffer.front();
            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][Send] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << m_send_buffer.size() - 1);

                m_send_buffer.pop_front();
            }
            else
            {
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] SendData failed");
            }
        }
    };

    void
    VcaClient::ReceiveData(Ptr<Packet> packet)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] ReceiveData");
    };

    void
    VcaClient::EncodeFrame()
    {
        NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][EncodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " Bitrate=" << m_bitrate);

        // Calculate packets in the frame
        uint32_t frame_size = m_bitrate * 1000 / 8 / m_fps;
        // uint16_t num_pkt_in_frame = frame_size / payloadSize + (frame_size % payloadSize != 0);

        // Produce packets
        for (uint32_t data_ptr = 0; data_ptr < frame_size; data_ptr += payloadSize)
        {
            Ptr<Packet> packet = Create<Packet>(std::min(payloadSize, frame_size - data_ptr));
            m_send_buffer.push_back(packet);
        }

        // Schedule next frame's encoding
        Time next_enc_frame = MicroSeconds(1e6 / m_fps);
        m_enc_event = Simulator::Schedule(next_enc_frame, &VcaClient::EncodeFrame, this);
    };

    void
    VcaClient::OutputStatistics()
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] OutputStatistics");
    };

}; // namespace ns3