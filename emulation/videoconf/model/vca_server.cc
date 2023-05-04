#include "vca_server.h"
#include "../../localcallback.h"
std::set<uint32_t> m_paused[48];
std::map<uint8_t,ns3::Ipv4Address> socket_to_ip[48];
extern std::map<ns3::Ipv4Address,uint32_t> ip_to_node[48];

namespace ns3
{
    uint8_t DEBUG_SRC_SOCKET_ID = 3;
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
          dl_bitrate_reduce_factor(1.0),
          dl_rate_control_state(DL_RATE_CONTROL_STATE_NATRUAL),
          set_header(0),
          read_status(0),
          payload_size(0),
          half_header(nullptr),
          half_payload(nullptr),
          app_header(VcaAppProtHeader()){};

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
          m_separate_socket(0){};

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
    VcaServer::SetLastNid(uint32_t m_lastN_id)
    {
        lastN_id = m_lastN_id;
    };  
    void 
    VcaServer::SetClientNum(uint32_t nClient)
    {
        m_client_number = nClient;
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
    };

    void
    VcaServer::StopApplication()
    {
        double Tottime = Simulator::Now().GetSeconds();
        NS_LOG_UNCOND("[VcaClient][Result] Totwaste= "<<((totwaste/Tottime)/1e6)*8<<" ");//Mbps
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
                NS_LOG_DEBUG("[VcaServer] ccRate = " << bitrate / 1000000. << " framesize= " << client_info->cc_target_frame_size << " Rtt(ms) " << rtt_estimate*1000 << " Cwnd(bytes) " << dl_socketbase->GetTcb()->m_cWnd.Get() << " nowBuf " << curPendingBuf);
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
        NS_LOG_LOGIC("[VcaServer] HandleRead");
        Ptr<Packet> packet;
        Address peerAddress;
        socket->GetPeerName(peerAddress);
        uint8_t socket_id = m_ul_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];

        Ptr<ClientInfo> client_info = m_client_info_map[socket_id];
//        NS_LOG_UNCOND("socket_to_ip["<<(uint32_t)socket_id<<"] = "<<client_info->ul_addr);
        socket_to_ip[lastN_id][socket_id] = client_info->ul_addr;

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
                ReceiveData(client_info->half_payload, socket_id);
                client_info->read_status = 0;
                client_info->set_header = 0;
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
        client_info->dl_bitrate_reduce_factor = 1.0;
        client_info->dl_rate_control_state = DL_RATE_CONTROL_STATE_NATRUAL;
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
        uint32_t dl_redc_factor = client_info->app_header.GetDlRedcFactor();
        uint32_t payload_size = client_info->app_header.GetPayloadSize();

        /*
            Having decoded customized header and payload,
            we're going to maintain a lastNqueue based on m_node_id
        */
        uint8_t lastN_mode = 0;
        uint32_t lastN_number = m_client_number-1;
//        NS_LOG_UNCOND("lastN_number = "<<lastN_number<<"  nclient = "<<m_client_number);
        if(lastN_mode){
            uint32_t client_node_id = ip_to_node[lastN_id][socket_to_ip[lastN_id][socket_id]];
//            NS_LOG_UNCOND("client_node_id ::: socket_id="<<(uint32_t)socket_id<<"  ip="<<socket_to_ip[lastN_id][socket_id]<<" node_id="<<client_node_id);
//           NS_LOG_UNCOND("lastN[Server] insert "<<client_node_id);
            //Push: add lastest client_node_id
            lastN.push_back(client_node_id);
            if(in_queue[client_node_id]==0){//paused -> not paused
                if(m_paused[lastN_id].find(client_node_id) != m_paused[lastN_id].end()){
                    m_paused[lastN_id].erase(m_paused[lastN_id].find(client_node_id));
                    // NS_LOG_UNCOND("m_paused.erase("<<client_node_id<<")");
                }
            }
            in_queue[client_node_id] += 1;

            //Pop: maintain queue
            while(in_queue.size() > lastN_number){
                uint32_t old_node_id = lastN.front();
                // NS_LOG_UNCOND("lastN[Server] erase "<<old_node_id);
                lastN.pop_front();
                in_queue[old_node_id] -= 1;
                if(in_queue[old_node_id] == 0){//not paused -> paused
                    if(m_paused[lastN_id].find(old_node_id) == m_paused[lastN_id].end()){
                        m_paused[lastN_id].insert(old_node_id);
                        // NS_LOG_UNCOND("m_paused.insert("<<old_node_id<<")");
                    }
                    in_queue.erase(old_node_id);
                } 
            }
        }

        NS_LOG_LOGIC("[VcaServer][TranscodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " FrameId= " << frame_id << " PktId= " << pkt_id << " PktSize= " << packet->GetSize() << " SocketId= " << (uint16_t)socket_id << " DlRedcFactor= " << (double_t)dl_redc_factor / 10000. << " NumDegradedUsers= " << m_num_degraded_users);

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
                total_frame_size += payload_size + 12;
            }
        }

        m_total_packet_size += payload_size + 12;

        client_info->dl_bitrate_reduce_factor = (double_t)dl_redc_factor / 10000.0;

        // update dl rate control state
        if (client_info->dl_bitrate_reduce_factor < 1.0 && client_info->dl_rate_control_state == DL_RATE_CONTROL_STATE_NATRUAL && m_num_degraded_users < m_client_info_map.size() / 2)
        {
            client_info->dl_rate_control_state = DL_RATE_CONTROL_STATE_LIMIT;

            // store the capacity (about to enter app-limit phase where cc could not fully probe the capacity)
            client_info->capacity_frame_size = client_info->cc_target_frame_size;
            m_num_degraded_users += 1;

            NS_LOG_DEBUG("[VcaServer][DlRateControlStateLimit][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " DlRedcFactor= " << (double_t)dl_redc_factor / 10000.);
        }
        else if (client_info->dl_bitrate_reduce_factor == 1.0 && client_info->dl_rate_control_state == DL_RATE_CONTROL_STATE_LIMIT)
        {
            client_info->dl_rate_control_state = DL_RATE_CONTROL_STATE_NATRUAL;

            // restore the capacity before the controlled (limited bw) phase
            client_info->cc_target_frame_size = client_info->capacity_frame_size;
            m_num_degraded_users -= 1;

            NS_LOG_DEBUG("[VcaServer][DlRateControlStateNatural][Sock" << (uint16_t)socket_id << "] Time= " << Simulator::Now().GetMilliSeconds());
        }

        bool wasted_flag = 1;
        for (auto it = m_client_info_map.begin(); it != m_client_info_map.end(); it++)
        {

            // Address peerAddress;
            // socket_dl->GetPeerName(peerAddress);
            // uint8_t other_socket_id = m_socket_id_map[InetSocketAddress::ConvertFrom(peerAddress).GetIpv4().Get()];
            uint8_t other_socket_id = it->first;
            Ptr<ClientInfo> other_client_info = it->second;
            if (other_socket_id == socket_id)
                continue;

//             Ptr<Packet> packet_dl ;
//             std::string ip = "10.1." + std::to_string(5) + ".1";
//             Ipv4Address move_addr = Ipv4Address(ip.c_str());
// //            NS_LOG_UNCOND("addr: "<<client_info->ul_addr);
//             if (client_info->ul_addr == move_addr) packet_dl = TranscodeFrame(socket_id, other_socket_id, packet, frame_id);
//             else{
//                 packet_dl = Copy(packet);
//                 NS_LOG_UNCOND("skip transcode addr = "<<client_info->ul_addr);
//             }
            Ptr<Packet> packet_dl = TranscodeFrame(socket_id, other_socket_id, packet, frame_id);

            if (packet_dl == nullptr)
                continue;
            
            wasted_flag = 0;//if there is one sent successfully, then not wasted

            other_client_info->send_buffer.push_back(packet_dl);

            SendData(other_client_info->socket_dl);
        }

        if(wasted_flag) totwaste += packet->GetSize(); 
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
                NS_LOG_LOGIC("[VcaServer][SrcSock" << (uint16_t)src_socket_id << "][DstSock" << (uint16_t)dst_socket_id << "][FrameForward] TargetFrameSize= " << GetTargetFrameSize(dst_socket_id));

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
        if (client_info->dl_rate_control_state == DL_RATE_CONTROL_STATE_NATRUAL)
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
            // NS_LOG_UNCOND("ICARE socket_id="<<(uint32_t)socket_id<<" client_number = "<<(m_client_info_map.size() - 1)<<" cc_target_frame_size = "<<client_info->cc_target_frame_size<<" fair share = "<<fair_share);
            return std::max(fair_share, kMinFrameSizeBytes);
        }
        else if (client_info->dl_rate_control_state == DL_RATE_CONTROL_STATE_LIMIT)
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

}; // namespace ns3