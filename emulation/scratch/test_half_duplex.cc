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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastEmulation");

enum LOG_LEVEL
{
    ERROR,
    DEBUG,
    LOGIC
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
    double_t minBitrateKbps = 4.0;

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

    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    //  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

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
        clientNodes.Get(id)->AddApplication(vcaClientApp);

        Simulator::Schedule(Seconds(simulationDuration), &VcaClient::StopEncodeFrame, vcaClientApp);
        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration + 4));
    }

    Ipv4Address serverUlAddr = ulIpIfaces[0].GetAddress(1); // sfuCenter
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
        ulP2p[0].EnablePcapAll("sfu-p2p");
        dlP2p[0].EnablePcapAll("sfu-p2p");
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