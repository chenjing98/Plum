#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <string.h>
#include <cstdlib>
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/videoconf-module.h"

#include <fstream>
#include <sstream>
#include <cstdlib>

#include "../../../callback.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastEmulation");

enum LOG_LEVEL
{
    ERROR,
    DEBUG,
    LOGIC
};

enum TRACE_MODE
{
    EVEN_SPLIT,
    UNEVEN_SPLIT
};

struct GlobalKnowledge
{
    double_t prevMaxAbw = 50;
    double_t newMaxAbw = 0;
    bool prevAmpleBwUserExist = 1;
    bool newAmpleBwUserExist = 0;
    uint32_t clientSetTraceCount = 0;
};

static GlobalKnowledge global_know;

struct TraceElem
{
    std::string trace;
    TRACE_MODE mode;
    Ptr<NetDevice> ul_snd_dev;
    Ptr<NetDevice> dl_snd_dev;
    uint16_t interval;    // in ms
    double_t simStopTime; // in s
    double_t maxAppBitrateMbps;
    double_t minAppBitrateMbps;
    uint32_t node_id;
    double_t ul_prop = 0.5;
    double_t prev_ul_bw = 0.0;
    double_t prev_dl_bw = 0.0;
    std::streampos curr_pos = std::ios::beg;
};

static inline void SplitString(const std::string &s, std::vector<std::string> &v, const std::string &c)
{
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while (std::string::npos != pos2)
    {
        v.push_back(s.substr(pos1, pos2 - pos1));
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if (pos1 != s.length())
        v.push_back(s.substr(pos1));
}

void BandwidthTrace(TraceElem elem, uint32_t n_client)
{
    Ptr<PointToPointNetDevice> ulSndDev = StaticCast<PointToPointNetDevice, NetDevice>(elem.ul_snd_dev);
    Ptr<PointToPointNetDevice> dlSndDev = StaticCast<PointToPointNetDevice, NetDevice>(elem.dl_snd_dev);
    std::ifstream traceFile;

    std::string traceLine;
    std::vector<std::string> traceData;
    std::vector<std::string> bwValue;

    traceFile.open(elem.trace);
    traceFile.seekg(elem.curr_pos);
    if (elem.curr_pos == std::ios::beg && !traceFile.eof())
    {
        std::getline(traceFile, traceLine); // skip the first line
    }
    std::getline(traceFile, traceLine);
    elem.curr_pos = traceFile.tellg();
    if (traceLine.find(',') == std::string::npos)
    {
        traceFile.close();
        return;
    }

    // bwValue.clear();
    traceData.clear();
    SplitString(traceLine, traceData, ",");
    // SplitString(traceData[0], bwValue, "Mbps");

    double_t total_bw = std::stod(traceData[2]) * 1.5;
    double_t ul_bw = 300, dl_bw = 300;
    
    bool is_paused = m_paused.find(elem.node_id)==m_paused.end()?0:1;
    NS_LOG_UNCOND("[half-duplex] node_id = "<<(uint16_t)elem.node_id<<" is_paused = "<<is_paused);
    is_paused = 0;

    if (is_paused)
    {
        ul_bw = total_bw / 5;
        dl_bw = total_bw - ul_bw;
        NS_LOG_DEBUG("BwAlloc Node: " << (uint16_t)elem.node_id << " ul_bw: " << ul_bw << " dl_bw: " << dl_bw);
    }
    else if (elem.mode == EVEN_SPLIT)
    {
        ul_bw = total_bw / 2;
        dl_bw = total_bw / 2;
        NS_LOG_DEBUG("BwAlloc Node: " << (uint16_t)elem.node_id << " ul_bw: " << ul_bw << " dl_bw: " << dl_bw);
    }
    else
    {
        if (global_know.clientSetTraceCount >= n_client)
        {
            global_know.clientSetTraceCount = 0;
            global_know.prevAmpleBwUserExist = global_know.newAmpleBwUserExist;
            global_know.prevMaxAbw = std::max(n_client * elem.minAppBitrateMbps, global_know.newMaxAbw);
            global_know.newMaxAbw = 0;
            global_know.newAmpleBwUserExist = 0;
        }

        double_t min_recv_rate = (n_client - 1) * elem.minAppBitrateMbps;
        double_t app_limit_bw = n_client * elem.maxAppBitrateMbps;

        if (total_bw < min_recv_rate)
        {
            ul_bw = total_bw / 2;
            dl_bw = total_bw / 2;
        }
        else
        {
            if (global_know.prevAmpleBwUserExist)
            {
                if (total_bw < min_recv_rate + elem.maxAppBitrateMbps * 1.2)
                {
                    dl_bw = min_recv_rate;
                    ul_bw = total_bw - dl_bw;

                    NS_LOG_LOGIC("ul_bw: " << ul_bw << "Mbps, dl_bw: " << dl_bw << "Mbps line135");
                }
                else
                {
                    ul_bw = elem.maxAppBitrateMbps * 1.2;
                    dl_bw = total_bw - ul_bw;
                    NS_LOG_LOGIC("ul_bw: " << ul_bw << "Mbps, dl_bw: " << dl_bw << "Mbps line141");
                }
                // dl_bw = min_recv_rate;
                // ul_bw = total_bw - dl_bw;
            }
            else
            {
                double_t min_ul_bw_for_the_rest = (n_client - global_know.clientSetTraceCount - 1) * elem.minAppBitrateMbps;
                double_t fair_share_for_the_rest = global_know.prevMaxAbw / (n_client - global_know.clientSetTraceCount);
                ul_bw = std::min(std::max(elem.minAppBitrateMbps, std::min(global_know.prevMaxAbw - min_ul_bw_for_the_rest, fair_share_for_the_rest)), total_bw - min_recv_rate);
                dl_bw = total_bw - ul_bw;

                NS_LOG_LOGIC("ul_bw: " << ul_bw << "Mbps, dl_bw: " << dl_bw << "Mbps"
                                       << " prevmaxabw: " << global_know.prevMaxAbw << "Mbps"
                                       << " total_bw: " << total_bw << "Mbps"
                                       << " min ul bw for the rest: " << min_ul_bw_for_the_rest << "Mbps"
                                       << " fair share for the rest: " << fair_share_for_the_rest << "Mbps");

                global_know.prevMaxAbw = std::max(0.0, global_know.prevMaxAbw - ul_bw);
            }
        }

        // ul_bw = total_bw * elem.ul_prop;
        // dl_bw = total_bw * (1 - elem.ul_prop);
        NS_LOG_DEBUG("BwAlloc Node: " << (uint16_t)elem.node_id << " ul_bw: " << ul_bw << " dl_bw: " << dl_bw);

        elem.prev_ul_bw = ul_bw;
        elem.prev_dl_bw = dl_bw;

        // Update global Knowledge
        if (total_bw >= app_limit_bw)
        {
            global_know.newAmpleBwUserExist = 1;
        }
        global_know.newMaxAbw = std::max(global_know.newMaxAbw, total_bw);
        global_know.clientSetTraceCount++;
    }

    /* Set delay of n0-n1 as rtt/2 - 1, the delay of n1-n2 is 1ms */
    std::string ulBwStr = std::to_string(ul_bw) + "Mbps";
    std::string dlBwStr = std::to_string(dl_bw) + "Mbps";

    // Set bandwidth
    ulSndDev->SetAttribute("DataRate", StringValue(ulBwStr));
    dlSndDev->SetAttribute("DataRate", StringValue(dlBwStr));

    if (Simulator::Now() < Seconds(elem.simStopTime + 2))
    {
        if (!traceFile.eof())
        {
            traceFile.close();
            Simulator::Schedule(MilliSeconds(elem.interval), &BandwidthTrace, elem, n_client);
        }
        else
        {
            traceFile.close();
            elem.curr_pos = std::ios::beg; // start from the beginning again
            Simulator::Schedule(MilliSeconds(elem.interval), &BandwidthTrace, elem, n_client);
        }
    }
};

std::string GetRandomTraceFile(uint32_t max_trace_count)
{
    uint32_t trace_count = rand() % max_trace_count;
    uint32_t n_line = 0;
    std::string trace_file;
    std::fstream index_file;
    index_file.open("../../../scripts/tracefile_names", std::ios::in);
    while (getline(index_file, trace_file) && n_line < trace_count)
    {
        n_line++;
        if (index_file.eof())
        {
            break;
        }
    }

    return trace_file;
};

int main(int argc, char *argv[])
{

    std::string mode = "p2p";
    uint8_t logLevel = 0;
    double_t simulationDuration = 10.0; // in s
    uint32_t maxBitrateKbps = 10000;
    uint8_t policy = 0;
    uint32_t nClient = 1;
    bool printPosition = false;
    bool savePcap = false;
    bool vary_bw = false;
    uint8_t trace_mode = 0;
    double_t ul_prop = 0.5;
    double_t minBitrateKbps = 1000.0;
    uint16_t seed = 1;
    bool is_tack = false;
    uint32_t tack_max_count = 32;

    uint32_t MAX_TRACE_COUNT = 1115;

    // std::string Version = "80211n_5GHZ";

    CommandLine cmd(__FILE__);
    cmd.AddValue("mode", "p2p or sfu mode", mode);
    cmd.AddValue("logLevel", "Log level: 0 for error, 1 for debug, 2 for logic", logLevel);
    cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
    cmd.AddValue("maxBitrateKbps", "Max bitrate in kbps", maxBitrateKbps);
    cmd.AddValue("policy", "0 for vanilla, 1 for Yongyule", policy);
    cmd.AddValue("nClient", "Number of clients", nClient);
    cmd.AddValue("printPosition", "Print position of nodes", printPosition);
    cmd.AddValue("minBitrate", "Minimum tolerable bitrate in kbps", minBitrateKbps);
    cmd.AddValue("savePcap", "Save pcap file", savePcap);
    cmd.AddValue("varyBw", "Emulate in varying bandwidth or not", vary_bw);
    cmd.AddValue("traceMode", "0 for even split, 1 for uneven split", trace_mode);
    cmd.AddValue("ulProp", "Proportion of uplink bandwidth", ul_prop);
    cmd.AddValue("seed", "Random seed for trace selection", seed);
    cmd.AddValue("isTack", "Is TACK enabled", is_tack);
    cmd.AddValue("tackMaxCount", "Max TACK count", tack_max_count);

    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);
    std::srand(seed);

    // Config::SetDefault ("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue (QueueSize ("1p")));
    // Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (5 << 20)); // if (rwnd > 5M)，retransmission (RTO) will accumulate
    // Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (5 << 20)); // over 5M packets, causing packet metadata overflows.
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    // Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(200)));

    if (is_tack)
    {
        Config::SetDefault("ns3::TcpSocketBase::IsTack", BooleanValue(true));
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(tack_max_count));
    }
    else
    {
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    }

    // set log level
    if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::ERROR)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_ERROR);
        LogComponentEnable("VcaClient", LOG_LEVEL_ERROR);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_ERROR);
    }
    else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::DEBUG)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_DEBUG);
        LogComponentEnable("VcaClient", LOG_LEVEL_DEBUG);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_DEBUG);
    }
    else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::LOGIC)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_LOGIC);
        LogComponentEnable("VcaClient", LOG_LEVEL_LOGIC);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_LOGIC);
    }

    NS_LOG_DEBUG("[Scratch] SFU mode emulation started.");

    // Create nodes
    NodeContainer sfuCenter, clientNodes;
    clientNodes.Create(nClient);
    sfuCenter.Create(1);

    // Create backhaul links
    PointToPointHelper ulP2p[nClient], dlP2p[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
        ulP2p[i].SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        ulP2p[i].SetChannelAttribute("Delay", StringValue("10ms"));
        dlP2p[i].SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        dlP2p[i].SetChannelAttribute("Delay", StringValue("10ms"));
    }

    // Install NetDevices on backhaul links
    NetDeviceContainer ulDevices[nClient], dlDevices[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
        ulDevices[i] = ulP2p[i].Install(clientNodes.Get(i), sfuCenter.Get(0));
        dlDevices[i] = dlP2p[i].Install(clientNodes.Get(i), sfuCenter.Get(0));

        if (vary_bw)
        {
            std::string trace_dir = "../../../scripts/traces/";
            std::string trace_name = GetRandomTraceFile(MAX_TRACE_COUNT);
            // std::string trace_dir = "../../../scripts/";
            // std::string trace_name = "trace-debug.csv";
            std::string tracefile = trace_dir + trace_name;
            TraceElem elem = {tracefile, static_cast<TRACE_MODE>(trace_mode), ulDevices[i].Get(0), dlDevices[i].Get(1), 16, simulationDuration, (double_t)maxBitrateKbps / 1000., minBitrateKbps / 1000., i, ul_prop};
            BandwidthTrace(elem, nClient);
        }
    }

    InternetStackHelper stack;

    stack.Install(clientNodes);
    stack.Install(sfuCenter);

    // 给NetDevices分配IPv4地址
    Ipv4AddressHelper ipAddr;
    std::string ip;
    Ipv4InterfaceContainer ulIpIfaces[nClient], dlIpIfaces[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
        ip = "10.1." + std::to_string(i) + ".0";
        ipAddr.SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
        ulIpIfaces[i] = ipAddr.Assign(ulDevices[i]);

        ip = "10.2." + std::to_string(i) + ".0";
        ipAddr.SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
        dlIpIfaces[i] = ipAddr.Assign(dlDevices[i]);
    }

    // 给每个user(WifiSta)装上VcaClient，给Center装上VcaServer
    uint16_t client_ul = 80;
    uint16_t client_dl = 8080; // dl_port may increase in VcaServer, make sure it doesn't overlap with ul_port
    uint16_t client_peer = 80;

    std::list<Ipv4Address> serverUlAddrList;

    for (uint32_t id = 0; id < nClient; id++)
    {

        Ipv4Address clientUlAddr = ulIpIfaces[id].GetAddress(0);
        Ipv4Address clientDlAddr = dlIpIfaces[id].GetAddress(0);
        Ipv4Address serverUlAddr = ulIpIfaces[id].GetAddress(1);

        serverUlAddrList.push_back(serverUlAddr);

        // Ipv4Address serverDlAddr = dlIpIfaces[id].GetAddress(1);
        NS_LOG_DEBUG("SFU VCA Client NodeId " << clientNodes.Get(id)->GetId() << " Server NodeId " << sfuCenter.Get(0)->GetId());
        Ptr<VcaClient> vcaClientApp = CreateObject<VcaClient>();
        vcaClientApp->SetFps(20);
        vcaClientApp->SetLocalAddress(clientUlAddr, clientDlAddr);
        vcaClientApp->SetPeerAddress(std::vector<Ipv4Address>{serverUlAddr});
        vcaClientApp->SetLocalUlPort(client_ul);
        vcaClientApp->SetLocalDlPort(client_dl);
        vcaClientApp->SetPeerPort(client_peer);
        vcaClientApp->SetNodeId(clientNodes.Get(id)->GetId());
        vcaClientApp->SetNumNode(nClient);
        vcaClientApp->SetPolicy(static_cast<POLICY>(policy));
        vcaClientApp->SetMaxBitrate(maxBitrateKbps);
        vcaClientApp->SetMinBitrate(minBitrateKbps);
        // vcaClientApp->SetLogFile("../../../evaluation/results/trlogs/transient_rate_n" + std::to_string(nClient) + "_p" + std::to_string(policy) + "_i" + std::to_string(clientNodes.Get(id)->GetId()) + ".txt");
        clientNodes.Get(id)->AddApplication(vcaClientApp);

        Simulator::Schedule(Seconds(simulationDuration), &VcaClient::StopEncodeFrame, vcaClientApp);
        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration + 4));
    }

    Ptr<VcaServer> vcaServerApp = CreateObject<VcaServer>();
    // vcaServerApp->SetLocalAddress(serverUlAddr);
    vcaServerApp->SetLocalAddress(serverUlAddrList);
    vcaServerApp->SetLocalUlPort(client_peer);
    vcaServerApp->SetPeerDlPort(client_dl);
    vcaServerApp->SetLocalDlPort(client_dl);
    vcaServerApp->SetNodeId(sfuCenter.Get(0)->GetId());
    vcaServerApp->SetSeparateSocket();
    sfuCenter.Get(0)->AddApplication(vcaServerApp);
    vcaServerApp->SetStartTime(Seconds(0.0));
    vcaServerApp->SetStopTime(Seconds(simulationDuration + 2));

    if (savePcap)
    {
        AsciiTraceHelper ascii;
        ulP2p[0].EnablePcapAll("sfu-p2p");
        // dlP2p[0].EnablePcapAll("sfu-p2p");
        ulP2p[0].EnableAsciiAll(ascii.CreateFileStream("sfu-ul.tr"));
        // dlP2p[0].EnableAsciiAll(ascii.CreateFileStream("sfu-dl.tr"));
    }

    

    FlowMonitorHelper flowmonHelper;
    flowmonHelper.InstallAll();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationDuration + 5));
    Simulator::Run();
    flowmonHelper.SerializeToXmlFile("test-emulation.flowmon", true, true);
    Simulator::Destroy();
    return 0;
}