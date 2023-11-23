#include "vca_server.h"

namespace ns3
{
    uint8_t DEBUG_SRC_SOCKET_ID = 0;
    uint8_t DEBUG_DST_SOCKET_ID = 1;

    NS_LOG_COMPONENT_DEFINE("VcaServer");

    TypeId ClientInfo::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::ClientInfo")
                                .SetParent<Object>()
                                .SetGroupName("videoconf")
                                .AddConstructor<ClientInfo>();
        return tid;
    };

    ClientInfo::ClientInfo()
        : cc_target_frame_size(1e7 / 8 / 20),
          capacity_frame_size(1e7),
          dl_target_rate(0.0),
          dl_rate_control_state(NATRUAL),
          set_header(0),
          read_status(0),
          payload_size(0),
          half_header(nullptr),
          half_payload(nullptr),
          app_header(VcaAppProtHeader()),
          ul_rate(100),
          dl_rate(100),
          ul_target_rate(0.0){};

    ClientInfo::~ClientInfo(){};

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
          m_ul_socket_id_map(),
          m_dl_socket_id_map(),
          m_socket_id(0),
          m_client_info_map(),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_fps(20),
          m_num_degraded_users(0),
          m_total_packet_size(0),
          m_separate_socket(0),
          m_opt_params(),
          m_policy(VANILLA),
          m_dl_percentage(0){};

    VcaServer::~VcaServer(){};

    void
    VcaServer::DoDispose()
    {
        Application::DoDispose();
    };

    // Public helpers
    void
    VcaServer::SetLocalAddress(Ipv4Address local)
    {
        m_local = local;
    };

    void
    VcaServer::SetLocalAddress(std::list<Ipv4Address> local)
    {
        m_local_list = local;
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

    void
    VcaServer::SetSeparateSocket()
    {
        m_separate_socket = 1;
    };

    void
    VcaServer::SetNumNode(uint8_t num_node)
    {
        m_num_node = num_node;
    };

    void
    VcaServer::SetRho(double_t rho)
    {
        m_opt_params.rho = rho;
    };

    void
    VcaServer::SetQoEType(QOE_TYPE qoe_type)
    {
        m_opt_params.qoe_type = qoe_type;
    };

    void
    VcaServer::SetMaxThroughput(double_t max_throughput_kbps)
    {
        m_opt_params.max_bitrate_kbps = max_throughput_kbps;
    };

    void
    VcaServer::SetPolicy(POLICY policy)
    {
        m_policy = policy;
    };

    void
    VcaServer::SetDlpercentage(double percentage)
    {
        m_dl_percentage = percentage;
    };

    // Application Methods
    void
    VcaServer::StartApplication()
    {
        m_update_rate_event = Simulator::ScheduleNow(&VcaServer::UpdateRate, this);

        // Create the socket if not already
        if (!m_separate_socket)
        {
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
        }
        else
        {
            for (auto ip : m_local_list)
            {
                Ptr<Socket> socket_ul = Socket::CreateSocket(GetNode(), m_tid);
                if (socket_ul->Bind(InetSocketAddress{ip, m_local_ul_port}) == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                socket_ul->Listen();
                socket_ul->ShutdownSend();

                NS_LOG_LOGIC("[VcaServer] listening on " << ip << ":" << m_local_ul_port);

                socket_ul->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));
                socket_ul->SetRecvPktInfo(true);
                socket_ul->SetAcceptCallback(
                    MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                    MakeCallback(&VcaServer::HandleAccept, this));
                socket_ul->SetCloseCallbacks(
                    MakeCallback(&VcaServer::HandlePeerClose, this),
                    MakeCallback(&VcaServer::HandlePeerError, this));
                m_socket_ul_list.push_back(socket_ul);
            }
        }

        if (m_policy == POLO || m_policy == FIXED)
        {
            m_py_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (m_py_socket == -1)
            {
                NS_FATAL_ERROR("Failed to create socket");
            }
            struct sockaddr_in sock_addr;
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_port = htons(SOLVER_SOCKET_PORT + (uint16_t)m_num_node);
            sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            while (connect(m_py_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1)
            {
                NS_LOG_DEBUG("[VcaServer] Connecting to the python server to connect");
            }

            // change triggering time
            Simulator::Schedule(Seconds(5), &VcaServer::UpdateCapacities, this);
        }
    };

    void
    VcaServer::StopApplication()
    {
        // NS_LOG_UNCOND("Average thoughput (all clients) = "<<1.0*m_total_packet_size/Simulator::Now().GetSeconds());
        if (m_update_rate_event.IsRunning())
        {
            Simulator::Cancel(m_update_rate_event);
        }
        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            Ptr<ClientInfo> client_info = it->second;
            // these are accepted/connected sockets, close them
            client_info->socket_ul->Close();
            client_info->socket_dl->Close();
        }
        for (auto socket_ul : m_socket_ul_list)
        {
            socket_ul->Close();
        }
        if (m_socket_ul)
        {
            m_socket_ul->Close();
            m_socket_ul->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }

        // Send RST and close the socket with python server
        m_opt_params.rst = 1;
        send(m_py_socket, &m_opt_params, sizeof(m_opt_params), 0);
        shutdown(m_py_socket, 2);
        close(m_py_socket);
        int t = 1;
        setsockopt(m_py_socket, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int));
    };

    void
    VcaServer::UpdateRate()
    {
        // Update m_cc_target_frame_size in a periodically invoked function
        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            Ptr<ClientInfo> client_info = it->second;
            Ptr<Socket> socket_dl = client_info->socket_dl;
            Ptr<TcpSocketBase> dl_socketbase = DynamicCast<TcpSocketBase, Socket>(socket_dl);
            Address peerAddress;

            socket_dl->GetPeerName(peerAddress);
            // uint8_t socket_id = m_ul_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
            if (dl_socketbase->GetTcb()->m_pacing)
            {
                uint64_t bitrate = 1.1 * dl_socketbase->GetTcb()->m_pacingRate.Get().GetBitRate();

                // bitrate = std::min(bitrate, (uint64_t)15000000);

                Ptr<TcpBbr> bbr = DynamicCast<TcpBbr, TcpCongestionOps>(dl_socketbase->GetCongCtrl());

                if (bbr)
                {
                    double gain = 1.0;
                    if (bbr->GetBbrState() == 0)
                    { // startup
                        gain = 2.89;
                    }
                    if (bbr->GetBbrState() == 2)
                    { // probebw
                        gain = std::max(0.75, bbr->GetPacingGain());
                    }
                    // bitrate = 1.1 * bbr->m_maxBwFilter.GetBest().GetBitRate();
                    bitrate /= gain;
                }

                client_info->cc_target_frame_size = bitrate / 8 / m_fps;

                uint32_t sentsize = dl_socketbase->GetTxBuffer()->GetSentSize(); //
                uint32_t txbufferavailable = dl_socketbase->GetTxAvailable();
                UintegerValue val;
                socket_dl->GetAttribute("SndBufSize", val);
                uint32_t curPendingBuf = (uint32_t)val.Get() - sentsize - txbufferavailable;

                NS_LOG_DEBUG("[VcaServer] Pacing rate = " << bitrate / 1000000. << " framesize= " << client_info->cc_target_frame_size << " Rtt(ms) " << (uint32_t)dl_socketbase->GetRtt()->GetEstimate().GetMilliSeconds() << " Cwnd(bytes) " << dl_socketbase->GetTcb()->m_cWnd.Get() << " nowBuf " << curPendingBuf << " bbr " << bbr);
            }
            else
            {
                double_t rtt_estimate = dl_socketbase->GetRtt()->GetEstimate().GetSeconds();
                if (rtt_estimate == 0)
                {
                    rtt_estimate = 0.02;
                }
                uint64_t bitrate = dl_socketbase->GetTcb()->m_cWnd * 8 / rtt_estimate;
                client_info->cc_target_frame_size = bitrate / 8 / m_fps;

                uint32_t sentsize = dl_socketbase->GetTxBuffer()->GetSentSize(); //
                uint32_t txbufferavailable = dl_socketbase->GetTxAvailable();
                UintegerValue val;
                socket_dl->GetAttribute("SndBufSize", val);
                uint32_t curPendingBuf = (uint32_t)val.Get() - sentsize - txbufferavailable;
                NS_LOG_DEBUG("[VcaServer] ccRate = " << bitrate / 1000000. << " framesize= " << client_info->cc_target_frame_size << " Rtt(ms) " << rtt_estimate * 1000 << " Cwnd(bytes) " << dl_socketbase->GetTcb()->m_cWnd.Get() << " nowBuf " << curPendingBuf);
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
    VcaServer::HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> rx_data;
        Address peerAddress;
        socket->GetPeerName(peerAddress);
        uint8_t socket_id = m_ul_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];

        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];

        while (true)
        {
            // start to read header
            if (client_info->read_status == 0)
            {
                rx_data = socket->Recv(VCA_APP_PROT_HEADER_LENGTH, false);
                if (rx_data == NULL)
                    return;
                if (rx_data->GetSize() == 0)
                    return;
                client_info->half_header = rx_data;
                if (client_info->half_header->GetSize() < VCA_APP_PROT_HEADER_LENGTH)
                    client_info->read_status = 1; // continue to read header;
                if (client_info->half_header->GetSize() == VCA_APP_PROT_HEADER_LENGTH)
                    client_info->read_status = 2; // start to read payload;
            }
            // continue to read header
            if (client_info->read_status == 1)
            {
                rx_data = socket->Recv(VCA_APP_PROT_HEADER_LENGTH - client_info->half_header->GetSize(), false);
                if (rx_data == NULL)
                    return;
                if (rx_data->GetSize() == 0)
                    return;
                client_info->half_header->AddAtEnd(rx_data);
                if (client_info->half_header->GetSize() == VCA_APP_PROT_HEADER_LENGTH)
                    client_info->read_status = 2; // start to read payload;
            }
            // start to read payload
            if (client_info->read_status == 2)
            {
                if (client_info->set_header == 0)
                {
                    client_info->half_header->RemoveHeader(client_info->app_header);
                    client_info->payload_size = client_info->app_header.GetPayloadSize();

                    client_info->set_header = 1;
                    if (client_info->payload_size == 0)
                    {
                        // read again
                        client_info->read_status = 0;
                        client_info->set_header = 0;
                        return;
                    }
                }
                rx_data = socket->Recv(client_info->payload_size, false);
                if (rx_data == NULL)
                    return;
                if (rx_data->GetSize() == 0)
                    return;
                client_info->half_payload = rx_data;
                if (client_info->half_payload->GetSize() < client_info->payload_size)
                    client_info->read_status = 3; // continue to read payload
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND
            }
            // continue to read payload
            if (client_info->read_status == 3)
            {
                rx_data = socket->Recv(client_info->payload_size - client_info->half_payload->GetSize(), false);
                if (rx_data == NULL)
                    return;
                if (rx_data->GetSize() == 0)
                    return;
                client_info->half_payload->AddAtEnd(rx_data);
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND;
            }
            // Send packets only when header+payload is ready
            // status = 0  (1\ all empty then return    2\ all ready)
            if (client_info->read_status == 4)
            {
                ReceiveData(client_info->half_payload, socket_id);
                client_info->read_status = 0;
                client_info->set_header = 0;
                client_info->half_payload->RemoveAtEnd(client_info->payload_size);
                client_info->app_header.Reset();
            }
        }
    };

    void
    VcaServer::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_DEBUG("[VcaServer][Node" << m_node_id << "] HandleAccept ul socket " << socket);
        socket->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));

        Ptr<ClientInfo> client_info = CreateObject<ClientInfo>();
        client_info->socket_ul = socket;

        Ipv4Address ul_peer_ip = InetSocketAddress::ConvertFrom(from).GetIpv4();
        client_info->ul_addr = ul_peer_ip;

        Ipv4Address dl_peer_ip, dl_local_ip;

        if (m_separate_socket)
        {
            dl_peer_ip = Ipv4Address(GetDlAddr(ul_peer_ip.Get(), 0));
            dl_local_ip = Ipv4Address(GetDlAddr(ul_peer_ip.Get(), 1));

            NS_LOG_LOGIC("[VcaServer] dl server ip " << dl_local_ip.Get() << " dl client ip " << dl_peer_ip.Get() << " ul client ip " << ul_peer_ip.Get());
        }
        else
        {
            dl_peer_ip = ul_peer_ip;
            dl_local_ip = m_local;
            NS_LOG_DEBUG("[VcaServer] dl server ip " << dl_local_ip << " dl client ip " << dl_peer_ip << " ul client ip " << ul_peer_ip << " local_dl_port " << m_local_dl_port << " peer_dl_port " << m_peer_dl_port);
        }

        // Create downlink socket as well
        Ptr<Socket> socket_dl = Socket::CreateSocket(GetNode(), m_tid);

        if (socket_dl->Bind(InetSocketAddress{dl_local_ip, m_local_dl_port}) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        socket_dl->Connect(InetSocketAddress{dl_peer_ip, m_peer_dl_port}); // note here while setting client dl port number
        socket_dl->ShutdownRecv();
        socket_dl->SetConnectCallback(
            MakeCallback(&VcaServer::ConnectionSucceededDl, this),
            MakeCallback(&VcaServer::ConnectionFailedDl, this));

        client_info->socket_dl = socket_dl;
        client_info->cc_target_frame_size = 1e7 / 8 / 20;
        client_info->dl_target_rate = 1.0;
        client_info->dl_rate_control_state = NATRUAL;
        client_info->capacity_frame_size = 1e7;
        client_info->half_header = nullptr;
        client_info->half_payload = nullptr;
        client_info->ul_target_rate = 0.0;

        m_ul_socket_id_map[ul_peer_ip.Get()] = m_socket_id;
        m_dl_socket_id_map[dl_peer_ip.Get()] = m_socket_id;
        m_client_info_map[m_socket_id] = client_info;

        m_socket_id += 1;
        m_local_dl_port += 1;
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
        uint8_t socket_id = m_dl_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];

        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];

        if (!client_info->send_buffer.empty())
        {
            Ptr<Packet> packet = client_info->send_buffer.front();
            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_LOGIC("[VcaServer][Send][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << client_info->send_buffer.size() - 1);

                client_info->send_buffer.pop_front();
            }
            else
            {
                NS_LOG_LOGIC("[VcaServer][Send][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " SendData failed");
            }
        }
    };

    void
    VcaServer::ReceiveData(Ptr<Packet> packet, uint8_t socket_id)
    {
        NS_LOG_LOGIC("[VcaServer] ReceiveData");
        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];
        uint16_t frame_id = client_info->app_header.GetFrameId();
        uint16_t pkt_id = client_info->app_header.GetPacketId();
        uint32_t payload_size = client_info->app_header.GetPayloadSize();
        uint32_t src_id = client_info->app_header.GetSrcId();

        NS_LOG_LOGIC("[VcaServer][TranscodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " FrameId= " << frame_id << " PktId= " << pkt_id << " PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)socket_id << " SrcId= " << src_id << " NumDegradedUsers= " << m_num_degraded_users);

        if (socket_id == DEBUG_SRC_SOCKET_ID)
        {
            if (std::floor(Simulator::Now().GetSeconds()) > last_time)
            {
                NS_LOG_LOGIC("[VcaServer] ServerRate " << total_frame_size * 8 / 1000.0 << " kbps");
                total_frame_size = 0;
                last_time = std::floor(Simulator::Now().GetSeconds());
            }
            else if (std::floor(Simulator::Now().GetSeconds()) == last_time)
            {
                total_frame_size += payload_size + VCA_APP_PROT_HEADER_LENGTH;
            }
        }

        m_total_packet_size += payload_size + VCA_APP_PROT_HEADER_LENGTH;

        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            uint8_t other_socket_id = it->first;
            Ptr<ClientInfo> other_client_info = it->second;
            if (other_socket_id == socket_id)
                continue;

            Ptr<Packet> packet_dl = TranscodeFrame(socket_id, other_socket_id, packet, frame_id);

            if (packet_dl == nullptr)
                continue;

            // update other_client_info -> lambda
            VcaAppProtHeader app_header(frame_id, pkt_id);
            app_header.SetSrcId(src_id);
            app_header.SetPayloadSize(payload_size);
            app_header.SetUlTargetRate((uint32_t)(other_client_info->ul_target_rate * 1000.0));
            packet_dl->AddHeader(app_header);

            other_client_info->send_buffer.push_back(packet_dl);

            SendData(other_client_info->socket_dl);
        }
    };

    Ptr<Packet>
    VcaServer::TranscodeFrame(uint8_t src_socket_id, uint8_t dst_socket_id, Ptr<Packet> packet, uint16_t frame_id)
    {

        // std::pair<uint8_t, uint8_t> socket_id_pair = std::pair<uint8_t, uint8_t>(src_socket_id, dst_socket_id);
        Ptr<ClientInfo> src_client_info = m_client_info_map[src_socket_id];

        auto it = src_client_info->prev_frame_id.find(dst_socket_id);
        if (it == src_client_info->prev_frame_id.end())
        {
            src_client_info->prev_frame_id[dst_socket_id] = 0;
            src_client_info->frame_size_forwarded[dst_socket_id] = 0;
        }

        NS_LOG_LOGIC("[VcaServer] B4RmHeader PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)src_socket_id << " DstSocketId= " << (uint16_t)dst_socket_id << " FrameId= " << frame_id << " prevframeid " << src_client_info->prev_frame_id[dst_socket_id] << " FrameSizeForwarded= " << src_client_info->frame_size_forwarded[dst_socket_id]);

        if (frame_id == src_client_info->prev_frame_id[dst_socket_id])
        {
            // packets of the same frame
            if (src_client_info->frame_size_forwarded[dst_socket_id] < GetTargetFrameSize(dst_socket_id))
            {
                NS_LOG_LOGIC("[VcaServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] Forward TargetFrameSize= " << GetTargetFrameSize(dst_socket_id) << " FrameSizeForwarded= " << src_client_info->frame_size_forwarded[dst_socket_id] << " PktSize= " << packet->GetSize());
                // have not reach the target transcode bitrate, forward the packet
                src_client_info->frame_size_forwarded[dst_socket_id] += packet->GetSize();

                if (src_socket_id == DEBUG_SRC_SOCKET_ID && dst_socket_id == DEBUG_DST_SOCKET_ID)
                    forwarded_frame_size += packet->GetSize();

                Ptr<Packet> packet_to_forward = Copy(packet);
                return packet_to_forward;
            }
            else
            {
                NS_LOG_DEBUG("[VcaServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] TargetFrameSize= " << GetTargetFrameSize(dst_socket_id));

                if (src_socket_id == DEBUG_SRC_SOCKET_ID && dst_socket_id == DEBUG_DST_SOCKET_ID)
                    dropped_frame_size += packet->GetSize();

                // have reach the target transcode bitrate, drop the packet
                return nullptr;
            }
        }
        else if (frame_id > src_client_info->prev_frame_id[dst_socket_id])
        {
            NS_LOG_LOGIC("[VcaServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] Start FrameId= " << frame_id);
            // packets of a new frame, simply forward it
            src_client_info->prev_frame_id[dst_socket_id] = frame_id;
            src_client_info->frame_size_forwarded[dst_socket_id] = packet->GetSize();

            // if(src_socket_id == DEBUG_SRC_SOCKET_ID && dst_socket_id == DEBUG_DST_SOCKET_ID)
            // {
            //     std::ofstream ofs("./debug-server.log", std::ios_base::app);
            //     ofs << "FrameId= " << frame_id - 1 << " Forwarded " << forwarded_frame_size << " Dropped " << dropped_frame_size << std::endl;
            //     forwarded_frame_size = packet->GetSize();
            //     dropped_frame_size = 0;
            //     ofs.close();
            // }

            Ptr<Packet> packet_to_forward = Copy(packet);
            return packet_to_forward;
        }

        // packets of a previous frame, drop the packet
        return nullptr;
    };

    uint32_t
    VcaServer::GetTargetFrameSize(uint8_t socket_id)
    {
        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];

        uint32_t manual_dl_share;

        if (m_policy == POLO || m_policy == FIXED)
        {
            if (client_info->dl_rate_control_state == NATRUAL)
            {
                return 1e6;
            }
            else if (client_info->dl_rate_control_state == CONSTRAINED)
            {
                manual_dl_share = (uint32_t)std::ceil(client_info->dl_target_rate * 1000 / 8 / m_fps);
                // fair_share = std::round((double_t)fair_share * 1.25);
                return std::max(manual_dl_share, kMinFrameSizeBytes);
            }
        }
        else if (m_policy == YONGYULE)
        {
            if (client_info->dl_rate_control_state == NATRUAL)
            {
                return std::max(GetFrameSizeFairShare(client_info->cc_target_frame_size), kMinFrameSizeBytes);
            }
            else if (client_info->dl_rate_control_state == CONSTRAINED)
            {

                // limit the forwarding bitrate by capacity * reduce_factor
                if (m_client_info_map.size() > 1)
                {

                    manual_dl_share = (uint32_t)std::ceil((double_t)client_info->capacity_frame_size * client_info->dl_target_rate) / (m_client_info_map.size() - 1);
                }
                else
                {

                    manual_dl_share = (uint32_t)std::ceil((double_t)client_info->capacity_frame_size * client_info->dl_target_rate);
                }

                return std::max(std::min(GetFrameSizeFairShare(client_info->cc_target_frame_size), manual_dl_share), kMinFrameSizeBytes);
            }
        }

        return 1e6;
        // return GetFrameSizeFairShare(client_info->cc_target_frame_size);
    };

    uint32_t VcaServer::GetFrameSizeFairShare(uint32_t cc_target_framesize)
    {
        if (m_client_info_map.size() > 1)
        {
            return cc_target_framesize / (m_client_info_map.size() - 1);
        }
        else
        {
            return cc_target_framesize;
        }
    };

    uint32_t VcaServer::GetDlAddr(uint32_t ul_addr, int node)
    {
        NS_ASSERT_MSG(node == 0 || node == 1, "node should be 0 or 1");
        // param node: 0: client, 1: server
        uint32_t first8b = (ul_addr >> 24) & 0xff;
        uint32_t second8b = (ul_addr >> 16) & 0xff;
        uint32_t third8b = (ul_addr >> 8) & 0xff;
        uint32_t fourth8b = ul_addr & 0xff;

        uint32_t dl_addr;

        if (node == 0)
        {
            dl_addr = (first8b << 24) | ((second8b + 1) << 16) | (third8b << 8) | (fourth8b);
        }
        else
        {
            dl_addr = (first8b << 24) | ((second8b + 1) << 16) | (third8b << 8) | (fourth8b + 1);
        }

        // NS_LOG_UNCOND("Input addr " << ul_addr << " Output addr " << dl_addr << " first8b " << first8b << " second8b " << second8b << " third8b " << third8b << " fourth8b " << fourth8b);
        return dl_addr;
    };

    void
    VcaServer::OptimizeAllocation()
    {
        NS_LOG_LOGIC("[VcaServer] OptimizeAllocation");
        // define the communication format
        // params for solver: n, capacities
        // params for solver: [rho, max_bitrate, qoe_type, qoe_func_alpha, qoe_func_beta, num_view, method, init_bw, plot]

        m_opt_params.num_users = m_num_node;
        NS_ASSERT_MSG(m_opt_params.num_users == m_client_info_map.size(), "num_users should be equal to the number of clients");

        send(m_py_socket, &m_opt_params, sizeof(m_opt_params), 0);
        recv(m_py_socket, m_opt_alloc, sizeof(double_t) * m_opt_params.num_users, 0);

        // NS_LOG_DEBUG(m_opt_alloc[0] << " " << m_opt_alloc[1] << " " << m_opt_alloc[2]);
        if (!CheckOptResultsValidity())
        {
            // back to original cc decisions

            for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
            {
                Ptr<ClientInfo> client_info = it->second;
                client_info->ul_target_rate = 0.0;
                client_info->dl_rate_control_state = NATRUAL;
            }
            return;
        }

        double_t ul_alloc, dl_alloc;
        // send back to clients
        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            Ptr<ClientInfo> client_info = it->second;

            if(m_policy == POLO)
                dl_alloc = m_opt_alloc[it->first];
            else if(m_policy == FIXED)
                dl_alloc = m_opt_params.capacities_kbps[it->first] * m_dl_percentage;
            else dl_alloc = 0; //will not occur. just for full compiling
            ul_alloc = m_opt_params.capacities_kbps[it->first] - dl_alloc;

            client_info->ul_target_rate = ul_alloc;
            client_info->dl_target_rate = dl_alloc;

            NS_LOG_DEBUG("[VcaServer] Opti Client " << (uint16_t)it->first << " ul_target_rate " << client_info->ul_target_rate << " dl_target_rate " << client_info->dl_target_rate << " capacity " << m_opt_params.capacities_kbps[it->first] << " ul_rate " << client_info->ul_rate << " dl_rate " << client_info->dl_rate);

            // update dl rate control state
            if (client_info->dl_target_rate < client_info->dl_rate && client_info->dl_rate_control_state == NATRUAL)
            {
                client_info->dl_rate_control_state = CONSTRAINED;

                // store the capacity (about to enter app-limit phase where cc could not fully probe the capacity)
                NS_LOG_DEBUG("[VcaServer] Enter dl rate control state CONSTRAINED");
                client_info->cc_target_frame_size = client_info->dl_rate * 1000 / 8 / m_fps;
                client_info->capacity_frame_size = client_info->cc_target_frame_size;
                m_num_degraded_users += 1;
            }
            else if (client_info->dl_target_rate >= client_info->dl_rate && client_info->dl_rate_control_state == CONSTRAINED)
            {
                NS_LOG_DEBUG("[VcaServer] Enter dl rate control state NATURAL");
                client_info->dl_rate_control_state = NATRUAL;

                // restore the capacity before the controlled (limited bw) phase
                client_info->cc_target_frame_size = client_info->capacity_frame_size;
                m_num_degraded_users -= 1;
            }

            // update ul rate control state
            if (client_info->ul_target_rate > client_info->ul_rate)
            {
                client_info->ul_target_rate = 0.0; // let the client grow by its own CCA
            }
        }
    };

    void
    VcaServer::UpdateCapacities()
    {
        // realize updating the capacities

        double change_rate = 0;
        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            Ptr<ClientInfo> client_info = it->second;
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(client_info->socket_ul);
            Ptr<TcpSocketBase> dl_socket = DynamicCast<TcpSocketBase, Socket>(client_info->socket_dl);
            uint64_t ul_bitrate = ul_socket->GetTcb()->m_pacingRate.Get().GetBitRate();
            uint64_t dl_bitrate = dl_socket->GetTcb()->m_pacingRate.Get().GetBitRate(); // bps

            client_info->ul_rate = ul_bitrate / 1000.0;
            client_info->dl_rate = dl_bitrate / 1000.0;

            double_t new_bitrate = (ul_bitrate + dl_bitrate) / 1000.0; // kbps
            double_t old_bitrate = m_opt_params.capacities_kbps[it->first];
            m_opt_params.capacities_kbps[it->first] = new_bitrate;
            change_rate = std::max(change_rate, abs(new_bitrate - old_bitrate) / old_bitrate);
        }
        double rearrange_threshold = 0.2; // 暂时采用变化率>0.2作为判断标准，写定后可以写到.h中
        if (change_rate > rearrange_threshold)
            Simulator::ScheduleNow(&VcaServer::OptimizeAllocation, this);

        Simulator::Schedule(MilliSeconds(500), &VcaServer::UpdateCapacities, this);
    };

    bool
    VcaServer::CheckOptResultsValidity()
    {
        // check if the optimization results are valid

        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {
            if (m_opt_alloc[it->first] <= 2000 || m_opt_params.capacities_kbps[it->first] - m_opt_alloc[it->first] <= 1000)
            {
                return false;
            }
        }

        return true;
    };

}; // namespace ns3