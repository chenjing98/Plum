#include "web_server.h"

namespace ns3
{
    uint8_t DEBUG_SRC_SOCKET_ID = 0;
    uint8_t DEBUG_DST_SOCKET_ID = 1;

    NS_LOG_COMPONENT_DEFINE("WebServer");

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
          dl_bitrate_reduce_factor(1.0),
          dl_rate_control_state(NATRUAL),
          set_header(0),
          read_status(0),
          payload_size(0),
          half_header(nullptr),
          half_payload(nullptr),
          app_header(WebAppProtHeader()){};

    ClientInfo::~ClientInfo(){};

    TypeId WebServer::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::WebServer")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<WebServer>();
        return tid;
    };

    WebServer::WebServer()
        : m_socket_ul(nullptr),
          m_ul_socket_id_map(),
          m_dl_socket_id_map(),
          m_socket_id(0),
          m_client_info_map(),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_fps(20),
          m_num_degraded_users(0),
          m_total_packet_size(0),
          m_separate_socket(0){};

    WebServer::~WebServer(){};

    void
    WebServer::DoDispose()
    {
        Application::DoDispose();
    };

    // Public helpers
    void
    WebServer::SetLocalAddress(Ipv4Address local)
    {
        m_local = local;
    };

    void
    WebServer::SetLocalAddress(std::list<Ipv4Address> local)
    {
        m_local_list = local;
    };

    void
    WebServer::SetLocalUlPort(uint16_t port)
    {
        m_local_ul_port = port;
    };

    void
    WebServer::SetLocalDlPort(uint16_t port)
    {
        m_local_dl_port = port;
    };

    void
    WebServer::SetPeerDlPort(uint16_t port)
    {
        m_peer_dl_port = port;
    };

    void
    WebServer::SetNodeId(uint32_t node_id)
    {
        m_node_id = node_id;
    };

    void
    WebServer::SetSeparateSocket()
    {
        m_separate_socket = 1;
    };

    // Application Methods
    void
    WebServer::StartApplication()
    {
        m_update_rate_event = Simulator::ScheduleNow(&WebServer::UpdateRate, this);

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

                m_socket_ul->SetRecvCallback(MakeCallback(&WebServer::HandleRead, this));
                m_socket_ul->SetRecvPktInfo(true);
                m_socket_ul->SetAcceptCallback(
                    MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                    MakeCallback(&WebServer::HandleAccept, this));
                m_socket_ul->SetCloseCallbacks(
                    MakeCallback(&WebServer::HandlePeerClose, this),
                    MakeCallback(&WebServer::HandlePeerError, this));
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

                NS_LOG_LOGIC("[WebServer] listening on " << ip << ":" << m_local_ul_port);

                socket_ul->SetRecvCallback(MakeCallback(&WebServer::HandleRead, this));
                socket_ul->SetRecvPktInfo(true);
                socket_ul->SetAcceptCallback(
                    MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                    MakeCallback(&WebServer::HandleAccept, this));
                socket_ul->SetCloseCallbacks(
                    MakeCallback(&WebServer::HandlePeerClose, this),
                    MakeCallback(&WebServer::HandlePeerError, this));
                m_socket_ul_list.push_back(socket_ul);
            }
        }
    };

    void
    WebServer::StopApplication()
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
    };

    void
    WebServer::UpdateRate()
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

                NS_LOG_DEBUG("[WebServer] Pacing rate = " << bitrate / 1000000. << " framesize= " << client_info->cc_target_frame_size << " Rtt(ms) " << (uint32_t)dl_socketbase->GetRtt()->GetEstimate().GetMilliSeconds() << " Cwnd(bytes) " << dl_socketbase->GetTcb()->m_cWnd.Get() << " nowBuf " << curPendingBuf << " bbr " << bbr);
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
                NS_LOG_DEBUG("[WebServer] ccRate = " << bitrate / 1000000. << " framesize= " << client_info->cc_target_frame_size << " Rtt(ms) " << rtt_estimate * 1000 << " Cwnd(bytes) " << dl_socketbase->GetTcb()->m_cWnd.Get() << " nowBuf " << curPendingBuf);
            }
        }

        Time next_update_rate_time = MicroSeconds(1e6 / m_fps);
        m_update_rate_event = Simulator::Schedule(next_update_rate_time, &WebServer::UpdateRate, this);
    };

    // private helpers

    void
    WebServer::ConnectionSucceededDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] Downlink connection succeeded");

        SendData(socket);
    };

    void
    WebServer::ConnectionFailedDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] Connection Failed");
    };

    void
    WebServer::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] HandleRead");
        Ptr<Packet> packet;
        Address peerAddress;
        socket->GetPeerName(peerAddress);
        uint8_t socket_id = m_ul_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];

        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];

        while (true)
        {
            // start to read header
            if (client_info->read_status == 0)
            {
                packet = socket->Recv(12, false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_header = packet;
                if (client_info->half_header->GetSize() < 12)
                    client_info->read_status = 1; // continue to read header;
                if (client_info->half_header->GetSize() == 12)
                    client_info->read_status = 2; // start to read payload;
            }
            // continue to read header
            if (client_info->read_status == 1)
            {
                packet = socket->Recv(12 - client_info->half_header->GetSize(), false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_header->AddAtEnd(packet);
                if (client_info->half_header->GetSize() == 12)
                    client_info->read_status = 2; // start to read payload;
            }
            // start to read payload
            if (client_info->read_status == 2)
            {

                if (client_info->set_header == 0)
                {
                    client_info->app_header.Reset();
                    client_info->half_header->RemoveHeader(client_info->app_header);
                    client_info->payload_size = client_info->app_header.GetPayloadSize();

                    // uint16_t frame_id = app_header[socket_id].GetFrameId();
                    // uint16_t pkt_id = app_header[socket_id].GetPacketId();
                    // uint32_t dl_redc_factor = app_header[socket_id].GetDlRedcFactor();

                    client_info->set_header = 1;
                    if (client_info->payload_size == 0)
                    {
                        // read again
                        client_info->read_status = 0;
                        client_info->set_header = 0;
                        return;
                    }
                }
                packet = socket->Recv(client_info->payload_size, false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_payload = packet;
                if (client_info->half_payload->GetSize() < client_info->payload_size)
                    client_info->read_status = 3; // continue to read payload;
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND;
            }
            // continue to read payload
            if (client_info->read_status == 3)
            {
                packet = socket->Recv(client_info->payload_size - client_info->half_payload->GetSize(), false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_payload->AddAtEnd(packet);
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND;
            }
            // Send packets only when header+payload is ready
            // status = 0  (1\ all empty then return    2\ all ready)
            if (client_info->read_status == 4)
            {

                uint8_t *buffer = new uint8_t[client_info->half_payload->GetSize()];               // 创建一个buffer，用于存储packet元素
                client_info->half_payload->CopyData(buffer, client_info->half_payload->GetSize()); // 将packet元素复制到buffer中
                // for (int i = 0; i < m_half_payload[socket_id]->GetSize(); i++){
                //     uint8_t element = buffer[i]; // 获取第i个元素
                // if(element != 0)
                //     NS_LOG_UNCOND("i = " <<i<<"  ele = "<<(uint32_t)element);
                // }

                ReceiveData(client_info->half_payload, socket_id);
                client_info->read_status = 0;
                client_info->set_header = 0;
            }
        }
    };

    void
    WebServer::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_DEBUG("[WebServer][Node" << m_node_id << "] HandleAccept ul socket " << socket);
        socket->SetRecvCallback(MakeCallback(&WebServer::HandleRead, this));

        Ptr<ClientInfo> client_info = CreateObject<ClientInfo>();
        client_info->socket_ul = socket;

        Ipv4Address ul_peer_ip = InetSocketAddress::ConvertFrom(from).GetIpv4();
        client_info->ul_addr = ul_peer_ip;

        Ipv4Address dl_peer_ip, dl_local_ip;

        if (m_separate_socket)
        {
            dl_peer_ip = Ipv4Address(GetDlAddr(ul_peer_ip.Get(), 0));
            dl_local_ip = Ipv4Address(GetDlAddr(ul_peer_ip.Get(), 1));

            NS_LOG_LOGIC("[WebServer] dl server ip " << dl_local_ip.Get() << " dl client ip " << dl_peer_ip.Get() << " ul client ip " << ul_peer_ip.Get());
        }
        else
        {
            dl_peer_ip = ul_peer_ip;
            dl_local_ip = m_local;
            NS_LOG_DEBUG("[WebServer] dl server ip " << dl_local_ip << " dl client ip " << dl_peer_ip << " ul client ip " << ul_peer_ip << " local_dl_port " << m_local_dl_port << " peer_dl_port " << m_peer_dl_port);
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
            MakeCallback(&WebServer::ConnectionSucceededDl, this),
            MakeCallback(&WebServer::ConnectionFailedDl, this));

        client_info->socket_dl = socket_dl;
        client_info->cc_target_frame_size = 1e7 / 8 / 20;
        client_info->dl_bitrate_reduce_factor = 1.0;
        client_info->dl_rate_control_state = NATRUAL;
        client_info->capacity_frame_size = 1e7;
        client_info->half_header = nullptr;
        client_info->half_payload = nullptr;

        m_ul_socket_id_map[ul_peer_ip.Get()] = m_socket_id;
        m_dl_socket_id_map[dl_peer_ip.Get()] = m_socket_id;
        m_client_info_map[m_socket_id] = client_info;

        m_socket_id += 1;
        m_local_dl_port += 1;
    };

    void
    WebServer::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] HandlePeerClose");
    };

    void
    WebServer::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] HandlePeerError");
    };

    void
    WebServer::SendData(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebServer] SendData");
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
                NS_LOG_LOGIC("[WebServer][Send][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << client_info->send_buffer.size() - 1);

                client_info->send_buffer.pop_front();
            }
            else
            {
                NS_LOG_LOGIC("[WebServer][Send][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " SendData failed");
            }
        }
    };

    void
    WebServer::ReceiveData(Ptr<Packet> packet, uint8_t socket_id)
    {
        NS_LOG_LOGIC("[WebServer] ReceiveData");
        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];
        uint16_t frame_id = client_info->app_header.GetFrameId();
        uint16_t pkt_id = client_info->app_header.GetPacketId();
        uint32_t dl_redc_factor = client_info->app_header.GetDlRedcFactor();
        uint32_t payload_size = client_info->app_header.GetPayloadSize();

        content_source_size += payload_size;

        NS_LOG_LOGIC("[WebServer][TranscodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " FrameId= " << frame_id << " PktId= " << pkt_id << " PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)socket_id << " DlRedcFactor= " << (double_t)dl_redc_factor / 10000. << " NumDegradedUsers= " << m_num_degraded_users);

        if (socket_id == DEBUG_SRC_SOCKET_ID)
        {
            if (std::floor(Simulator::Now().GetSeconds()) > last_time)
            {
                NS_LOG_LOGIC("[WebServer] ServerRate " << total_frame_size * 8 / 1000.0 << " kbps");
                total_frame_size = 0;
                last_time = std::floor(Simulator::Now().GetSeconds());
            }
            else if (std::floor(Simulator::Now().GetSeconds()) == last_time)
            {
                total_frame_size += payload_size + 12;
            }
        }

        m_total_packet_size += payload_size + 12;

        client_info->dl_bitrate_reduce_factor = (double_t)dl_redc_factor / 10000.0;

        // update dl rate control state
        if (client_info->dl_bitrate_reduce_factor < 1.0 && client_info->dl_rate_control_state == NATRUAL && m_num_degraded_users < m_client_info_map.size() / 2)
        {
            client_info->dl_rate_control_state = CONSTRAINED;

            // store the capacity (about to enter app-limit phase where cc could not fully probe the capacity)
            client_info->capacity_frame_size = client_info->cc_target_frame_size;
            m_num_degraded_users += 1;

            NS_LOG_DEBUG("[WebServer][DlRateControlStateLimit][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " DlRedcFactor= " << (double_t)dl_redc_factor / 10000.);
        }
        else if (client_info->dl_bitrate_reduce_factor == 1.0 && client_info->dl_rate_control_state == CONSTRAINED)
        {
            client_info->dl_rate_control_state = NATRUAL;

            // restore the capacity before the controlled (limited bw) phase
            client_info->cc_target_frame_size = client_info->capacity_frame_size;
            m_num_degraded_users -= 1;

            NS_LOG_DEBUG("[WebServer][DlRateControlStateNatural][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds());
        }

        // for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        // {

        //     // Address peerAddress;
        //     // socket_dl->GetPeerName(peerAddress);
        //     // uint8_t other_socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
        //     uint8_t other_socket_id = it->first;
        //     Ptr<ClientInfo> other_client_info = it->second;
        //     if (other_socket_id == socket_id)
        //         continue;

        //     Ptr<Packet> packet_dl = TranscodeFrame(socket_id, other_socket_id, packet, frame_id);

        //     if (packet_dl == nullptr)
        //         continue;

        //     //todo : update other_client_info -> lambda
        //     Add_Pkt_Header_Serv(packet_dl,other_client_info->lambda);

        //     other_client_info->send_buffer.push_back(packet_dl);

        //     SendData(other_client_info->socket_dl);
        // }

        if (content_source_size >= content_response_size)
        {
            Ptr<Packet> packet_dl = TranscodeFrame(socket_id, socket_id, packet, frame_id);
            if (packet_dl == nullptr)
                return;

            // todo1 : prepare packetdl(different from vca)
            // todo2 : update other_client_info -> lambda
            Add_Pkt_Header_Serv(packet_dl, client_info->lambda);
            client_info->send_buffer.push_back(packet_dl);
            SendData(client_info->socket_dl);

            content_source_size = 0;
        }
    };

    Ptr<Packet>
    WebServer::TranscodeFrame(uint8_t src_socket_id, uint8_t dst_socket_id, Ptr<Packet> packet, uint16_t frame_id)
    {

        // std::pair<uint8_t, uint8_t> socket_id_pair = std::pair<uint8_t, uint8_t>(src_socket_id, dst_socket_id);
        Ptr<ClientInfo> src_client_info = m_client_info_map[src_socket_id];

        auto it = src_client_info->prev_frame_id.find(dst_socket_id);
        if (it == src_client_info->prev_frame_id.end())
        {
            src_client_info->prev_frame_id[dst_socket_id] = 0;
            src_client_info->frame_size_forwarded[dst_socket_id] = 0;
        }

        NS_LOG_LOGIC("[WebServer] B4RmHeader PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)src_socket_id << " DstSocketId= " << (uint16_t)dst_socket_id << " FrameId= " << frame_id << " prevframeid " << src_client_info->prev_frame_id[dst_socket_id] << " FrameSizeForwarded= " << src_client_info->frame_size_forwarded[dst_socket_id]);

        if (frame_id == src_client_info->prev_frame_id[dst_socket_id])
        {
            // packets of the same frame
            if (src_client_info->frame_size_forwarded[dst_socket_id] < GetTargetFrameSize(dst_socket_id))
            {
                NS_LOG_LOGIC("[WebServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] Forward TargetFrameSize= " << GetTargetFrameSize(dst_socket_id) << " FrameSizeForwarded= " << src_client_info->frame_size_forwarded[dst_socket_id] << " PktSize= " << packet->GetSize());
                // have not reach the target transcode bitrate, forward the packet
                src_client_info->frame_size_forwarded[dst_socket_id] += packet->GetSize();

                if (src_socket_id == DEBUG_SRC_SOCKET_ID && dst_socket_id == DEBUG_DST_SOCKET_ID)
                    forwarded_frame_size += packet->GetSize();

                Ptr<Packet> packet_to_forward = Copy(packet);
                return packet_to_forward;
            }
            else
            {
                NS_LOG_LOGIC("[WebServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] TargetFrameSize= " << GetTargetFrameSize(dst_socket_id));

                if (src_socket_id == DEBUG_SRC_SOCKET_ID && dst_socket_id == DEBUG_DST_SOCKET_ID)
                    dropped_frame_size += packet->GetSize();

                // have reach the target transcode bitrate, drop the packet
                return nullptr;
            }
        }
        else if (frame_id > src_client_info->prev_frame_id[dst_socket_id])
        {
            NS_LOG_LOGIC("[WebServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] Start FrameId= " << frame_id);
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
    WebServer::GetTargetFrameSize(uint8_t socket_id)
    {
        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];
        if (client_info->dl_rate_control_state == NATRUAL)
        {
            uint32_t fair_share;
            // stick to original cc decisions
            if (m_client_info_map.size() > 1)
            {
                fair_share = client_info->cc_target_frame_size / (m_client_info_map.size() - 1);
            }
            else
            {
                fair_share = client_info->cc_target_frame_size;
            }
            // fair_share = std::round((double_t)fair_share * 1.25);
            return std::max(fair_share, kMinFrameSizeBytes);
        }
        else if (client_info->dl_rate_control_state == CONSTRAINED)
        {
            uint32_t fair_share, manual_dl_share;
            // limit the forwarding bitrate by capacity * reduce_factor
            if (m_client_info_map.size() > 1)
            {
                fair_share = client_info->cc_target_frame_size / (m_client_info_map.size() - 1);
                manual_dl_share = (uint32_t)std::ceil((double_t)client_info->capacity_frame_size * client_info->dl_bitrate_reduce_factor) / (m_client_info_map.size() - 1);
            }
            else
            {
                fair_share = client_info->cc_target_frame_size;
                manual_dl_share = (uint32_t)std::ceil((double_t)client_info->capacity_frame_size * client_info->dl_bitrate_reduce_factor);
            }
            // limit the forwarding bitrate by capacity * reduce_factor
            // fair_share = std::round((double_t)fair_share * 1.25);
            return std::max(std::min(fair_share, manual_dl_share), kMinFrameSizeBytes);
        }
        else
        {
            return 1e6;
        }
    };

    uint32_t WebServer::GetDlAddr(uint32_t ul_addr, int node)
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
    WebServer::Add_Pkt_Header_Serv(Ptr<Packet> packet, double lambda)
    {
        // reuse these uint16_t. (65536)
        // store integer part and fractional part respectively
        uint16_t m_frame_id = (int)lambda;
        uint16_t pkt_id_in_frame = ((int)(lambda * 10000)) % 10000;
        WebAppProtHeader app_header_info =
            WebAppProtHeader(m_frame_id, pkt_id_in_frame);

        uint32_t packet_size = packet->GetSize();
        app_header_info.SetPayloadSize(packet_size);
        packet->AddHeader(app_header_info);
    }

}; // namespace ns3