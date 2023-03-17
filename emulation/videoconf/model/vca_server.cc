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
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")){};

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
            connectedSocket->Close();
        }
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

            if (InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());

                ReceiveData(packet, m_socket_id_map[socket]);
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " SrcPort= " << Inet6SocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet, m_socket_id_map[socket]);
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
        m_socket_id_map[socket_dl] = m_socket_id;
        m_socket_id += 1;

        m_local_dl_port += 1;

        m_send_buffer_list.push_back(std::deque<Ptr<Packet>>{});
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
        uint8_t socket_id = m_socket_id_map[socket];

        NS_LOG_DEBUG("sock id " << (uint16_t)socket_id << " send buffer size " << m_send_buffer_list[socket_id].size());

        if (!m_send_buffer_list[socket_id].empty())
        {
            Ptr<Packet> packet = m_send_buffer_list[socket_id].front();
            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_DEBUG("[VcaServer][Send] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << m_send_buffer_list[socket_id].size() - 1);

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
            uint8_t other_socket_id = m_socket_id_map[socket];

            if (other_socket_id == socket_id)
                continue;

            Ptr<Packet> packet_dl = TranscodeFrame(socket_id, packet);

            if (packet_dl == nullptr)
                continue;

            m_send_buffer_list[other_socket_id].push_back(packet_dl);
            // SendData(socket);
        }
    };

    Ptr<Packet>
    VcaServer::TranscodeFrame(uint8_t socket_id, Ptr<Packet> packet)
    {
        VcaAppProtHeader app_header = VcaAppProtHeader();
        packet->RemoveHeader(app_header);

        uint16_t frame_id = app_header.GetFrameId();
        uint32_t pkt_id = app_header.GetPacketId();

        NS_LOG_DEBUG("[VcaServer][TranscodeFrame] FrameId= " << frame_id << " PktId= " << pkt_id << " SocketId= " << (uint16_t)socket_id << " PktSize= " << packet->GetSize());
    };

}; // namespace ns3