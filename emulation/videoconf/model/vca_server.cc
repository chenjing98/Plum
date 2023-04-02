#include "vca_server.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("VcaServer");

    TypeId VcaServer::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::VcaServer")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<VcaServer>();
        return tid;
    };

    VcaServer::VcaServer()
        : m_socket_ul(nullptr),
          m_socket_list_ul(),
          m_socket_list_dl(),
          m_socket_id_map(),
          m_socket_id(0),
          m_peer_list(),
          m_send_buffer_list(),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_fps(20){};

    VcaServer::~VcaServer(){};

    void
    VcaServer::DoDispose()
    {
        m_socket_list_dl.clear();
        m_socket_list_ul.clear();
        Application::DoDispose();
    };

    // Public helpers
    void
    VcaServer::SetLocalAddress(Ipv4Address local)
    {
        m_local = local;
    };

    void
    VcaServer::SetLocalUlPort(uint16_t port)
    {
        m_local_ul_port = port;
    };

    void
    VcaServer::SetLocalDlPort(uint16_t port)
    {
        m_local_dl_port = port;
    };

    void
    VcaServer::SetPeerDlPort(uint16_t port)
    {
        m_peer_dl_port = port;
    };

    void
    VcaServer::SetNodeId(uint32_t node_id)
    {
        m_node_id = node_id;
    };

    // Application Methods
    void
    VcaServer::StartApplication()
    {
        m_update_rate_event = Simulator::ScheduleNow(&VcaServer::UpdateRate, this);

        // Create the socket if not already
        if (!m_socket_ul)
        {
            m_socket_ul = Socket::CreateSocket(GetNode(), m_tid);
            if (m_socket_ul->Bind(InetSocketAddress{m_local, m_local_ul_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket_ul->Listen();
            m_socket_ul->ShutdownSend();

            m_socket_ul->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));
            m_socket_ul->SetRecvPktInfo(true);
            m_socket_ul->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&VcaServer::HandleAccept, this));
            m_socket_ul->SetCloseCallbacks(
                MakeCallback(&VcaServer::HandlePeerClose, this),
                MakeCallback(&VcaServer::HandlePeerError, this));
        }
    };

    void
    VcaServer::StopApplication()
    {
        if (m_update_rate_event.IsRunning())
        {
            Simulator::Cancel(m_update_rate_event);
        }
        while (!m_socket_list_ul.empty())
        { // these are accepted sockets, close them
            Ptr<Socket> acceptedSocket = m_socket_list_ul.front();
            m_socket_list_ul.pop_front();
            acceptedSocket->Close();
        }
        if (m_socket_ul)
        {
            m_socket_ul->Close();
            m_socket_ul->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
        if (!m_socket_list_dl.empty())
        {
            Ptr<Socket> connectedSocket = m_socket_list_dl.front();
            m_socket_list_dl.pop_front();
            connectedSocket->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t>());
            connectedSocket->Close();
        }
    };

    void
    VcaServer::UpdateRate()
    {
        // Update m_target_frame_size in a periodically invoked function
        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);
            Address peerAddress;
            Ptr<Socket> socket = *it;
            socket->GetPeerName(peerAddress);
            uint8_t socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
            if (ul_socket->GetTcb()->m_pacing)
            {
                uint64_t bitrate = ul_socket->GetTcb()->m_pacingRate.Get().GetBitRate();
                m_target_frame_size[socket_id] = bitrate / 8 / m_fps;
            }
            else
            {
                uint64_t bitrate = ul_socket->GetTcb()->m_cWnd * 8 / 40;
                m_target_frame_size[socket_id] = bitrate / 8 / m_fps;
            }
        }

        Time next_update_rate_time = MicroSeconds(1e6 / m_fps);
        m_update_rate_event = Simulator::Schedule(next_update_rate_time, &VcaServer::UpdateRate, this);
    };

    // private helpers

    void
    VcaServer::ConnectionSucceededDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Downlink connection succeeded");

        SendData(socket);
    };

    void
    VcaServer::ConnectionFailedDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Connection Failed");
    };

    void
    VcaServer::DataSendDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Downlink dataSend");
        SendData(socket);
    };

    void
    VcaServer::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandleRead");
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            if (packet->GetSize() == 0)
            { // EOF
                break;
            }

            Address peerAddress;
            socket->GetPeerName(peerAddress);
            uint8_t socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
            if (InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());

                ReceiveData(packet, socket_id);
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " SrcPort= " << Inet6SocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet, socket_id);
            }
        }
    };

    void
    VcaServer::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_LOGIC("[VcaServer][Node" << m_node_id << "] HandleAccept");
        socket->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));
        m_socket_list_ul.push_back(socket);

        Ipv4Address peer_ip = InetSocketAddress::ConvertFrom(from).GetIpv4();
        m_peer_list.push_back(peer_ip);

        // Create downlink socket as well
        Ptr<Socket> socket_dl = Socket::CreateSocket(GetNode(), m_tid);

        if (socket_dl->Bind(InetSocketAddress{m_local, m_local_dl_port}) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        socket_dl->Connect(InetSocketAddress{peer_ip, m_peer_dl_port}); // note here while setting client dl port number
        socket_dl->ShutdownRecv();
        socket_dl->SetConnectCallback(
            MakeCallback(&VcaServer::ConnectionSucceededDl, this),
            MakeCallback(&VcaServer::ConnectionFailedDl, this));
        socket_dl->SetSendCallback(
            MakeCallback(&VcaServer::DataSendDl, this));

        m_socket_list_dl.push_back(socket_dl);
        m_socket_id_map[peer_ip.Get()] = m_socket_id;
        m_socket_id += 1;

        m_local_dl_port += 1;

        m_send_buffer_list.push_back(std::deque<Ptr<Packet>>{});

        m_target_frame_size.push_back(1e7 / 8 / 20);
        m_frame_size_forwarded.push_back(0);
        m_prev_frame_id.push_back(0);
        
        m_dl_bitrate_reduce_factor.push_back(1.0);
    };

    void
    VcaServer::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandlePeerClose");
    };

    void
    VcaServer::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandlePeerError");
    };

    void
    VcaServer::SendData(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] SendData");
        Address peerAddress;
        socket->GetPeerName(peerAddress);
        uint8_t socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];

        if (!m_send_buffer_list[socket_id].empty())
        {
            Ptr<Packet> packet = m_send_buffer_list[socket_id].front();
            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_DEBUG("[VcaServer][Send][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << m_send_buffer_list[socket_id].size() - 1);

                m_send_buffer_list[socket_id].pop_front();
            }
            else
            {
                NS_LOG_DEBUG("[VcaServer] SendData failed");
            }
        }
    };

    void
    VcaServer::ReceiveData(Ptr<Packet> packet, uint8_t socket_id)
    {
        for (auto socket : m_socket_list_dl)
        {
            Address peerAddress;
            socket->GetPeerName(peerAddress);
            uint8_t other_socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
            if (other_socket_id == socket_id)
                continue;

            Ptr<Packet> packet_dl = TranscodeFrame(socket_id, packet);

            if (packet_dl == nullptr)
                continue;

            m_send_buffer_list[other_socket_id].push_back(packet_dl);
        }
    };

    Ptr<Packet>
    VcaServer::TranscodeFrame(uint8_t socket_id, Ptr<Packet> packet)
    {
        NS_LOG_DEBUG("[VcaServer] B4RmHeader PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)socket_id);

        VcaAppProtHeader app_header = VcaAppProtHeader();
        packet->RemoveHeader(app_header);

        uint16_t frame_id = app_header.GetFrameId();
        uint16_t pkt_id = app_header.GetPacketId();
        uint32_t dl_redc_factor = app_header.GetDlRedcFactor();
        uint32_t payload_size = app_header.GetPayloadSize();

        NS_LOG_DEBUG("[VcaServer][TranscodeFrame] FrameId= " << frame_id << " PktId= " << pkt_id << " SocketId= " << (uint16_t)socket_id << " PktSize= " << packet->GetSize() << " DlRedcFactor= " << dl_redc_factor << " PayloadSize= " << payload_size);

        if (frame_id == m_prev_frame_id[socket_id])
        {
            // packets of the same frame
            if (m_frame_size_forwarded[socket_id] < m_target_frame_size[socket_id] * m_dl_bitrate_reduce_factor[socket_id])
            {
                // have not reach the target transcode bitrate, forward the packet
                m_frame_size_forwarded[socket_id] += packet->GetSize();
                return packet;
            }
            else
            {
                // have reach the target transcode bitrate, drop the packet
                return nullptr;
            }
        }
        else if (frame_id > m_prev_frame_id[socket_id])
        {
            // packets of a new frame
            m_prev_frame_id[socket_id] = frame_id;
            m_frame_size_forwarded[socket_id] = packet->GetSize();
            return packet;
        }

        // packets of a previous frame, drop the packet
        return nullptr;
    };

}; // namespace ns3