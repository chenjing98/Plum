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
#include "ns3/webmodel-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastEmulation");

enum LOG_LEVEL
{
  ERROR,
  DEBUG,
  LOGIC
};

void showPosition(Ptr<Node> node, double deltaTime)
{
  uint32_t nodeId = node->GetId();
  Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel>();
  Vector3D pos = mobModel->GetPosition();
  Vector3D speed = mobModel->GetVelocity();
  std::cout << "At " << Simulator::Now().GetSeconds() << " node " << nodeId << ": Position("
            << pos.x << ", " << pos.y << ", " << pos.z << ");   Speed(" << speed.x << ", "
            << speed.y << ", " << speed.z << ")" << std::endl;

  Simulator::Schedule(Seconds(deltaTime), &showPosition, node, deltaTime);
};

std::pair<WifiStandard, WifiPhyBand>
ConvertStringToStandardAndBand(std::string version)
{
  WifiStandard standard = WIFI_STANDARD_80211a;
  WifiPhyBand band = WIFI_PHY_BAND_5GHZ;
  if (version == "80211a")
  {
    standard = WIFI_STANDARD_80211a;
    band = WIFI_PHY_BAND_5GHZ;
  }
  else if (version == "80211b")
  {
    standard = WIFI_STANDARD_80211b;
    band = WIFI_PHY_BAND_2_4GHZ;
  }
  else if (version == "80211g")
  {
    standard = WIFI_STANDARD_80211g;
    band = WIFI_PHY_BAND_2_4GHZ;
  }
  else if (version == "80211p")
  {
    standard = WIFI_STANDARD_80211p;
    band = WIFI_PHY_BAND_5GHZ;
  }
  else if (version == "80211n_2_4GHZ")
  {
    standard = WIFI_STANDARD_80211n;
    band = WIFI_PHY_BAND_2_4GHZ;
  }
  else if (version == "80211n_5GHZ")
  {
    standard = WIFI_STANDARD_80211n;
    band = WIFI_PHY_BAND_5GHZ;
  }
  else if (version == "80211ac")
  {
    standard = WIFI_STANDARD_80211ac;
    band = WIFI_PHY_BAND_5GHZ;
  }
  else if (version == "80211ax_2_4GHZ")
  {
    standard = WIFI_STANDARD_80211ax;
    band = WIFI_PHY_BAND_2_4GHZ;
  }
  else if (version == "80211ax_5GHZ")
  {
    standard = WIFI_STANDARD_80211ax;
    band = WIFI_PHY_BAND_5GHZ;
  }
  return {standard, band};
}

int main(int argc, char *argv[])
{

  uint8_t logLevel = 0;
  double_t simulationDuration = 10.0; // in s
  uint32_t maxBitrateKbps = 10000;
  uint8_t policy = 0;
  uint32_t nClient = 1;
  bool printPosition = false;
  bool savePcap = false;
  bool saveTransRate = false;
  double_t minBitrateKbps = 4.0;
  uint32_t kUlImprove = 3;
  double_t kDlYield = 0.5;
  uint32_t kLowUlThresh = 2e6;
  uint32_t kHighUlThresh = 5e6;
  bool is_tack = false;
  uint32_t tack_max_count = 32;

  // std::string Version = "80211n_5GHZ";

  CommandLine cmd(__FILE__);
  cmd.AddValue("logLevel", "Log level: 0 for error, 1 for debug, 2 for logic", logLevel);
  cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
  cmd.AddValue("maxBitrateKbps", "Max bitrate in kbps", maxBitrateKbps);
  cmd.AddValue("policy", "0 for vanilla, 1 for Yongyule", policy);
  cmd.AddValue("nClient", "Number of clients", nClient);
  cmd.AddValue("printPosition", "Print position of nodes", printPosition);
  cmd.AddValue("minBitrate", "Minimum tolerable bitrate in kbps", minBitrateKbps);
  cmd.AddValue("savePcap", "Save pcap file", savePcap);
  cmd.AddValue("ulImpv", "UL improvement param", kUlImprove);
  cmd.AddValue("dlYield", "DL yield param", kDlYield);
  cmd.AddValue("lowUlThresh", "Low UL threshold", kLowUlThresh);
  cmd.AddValue("highUlThresh", "High UL threshold", kHighUlThresh);
  cmd.AddValue("saveTransRate", "Save transmission rate", saveTransRate);
  cmd.AddValue("isTack", "Is TACK enabled", is_tack);
  cmd.AddValue("tackMaxCount", "Max TACK count", tack_max_count);

  cmd.Parse(argc, argv);
  Time::SetResolution(Time::NS);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));

  if (is_tack)
  {
    Config::SetDefault("ns3::TcpSocketBase::IsTack", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(tack_max_count));
  }

  // set log level
  if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::ERROR)
  {
    LogComponentEnable("WebServer", LOG_LEVEL_ERROR);
    LogComponentEnable("WebClient", LOG_LEVEL_ERROR);
    LogComponentEnable("MulticastEmulation", LOG_LEVEL_ERROR);
  }
  else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::DEBUG)
  {
    LogComponentEnable("WebServer", LOG_LEVEL_DEBUG);
    LogComponentEnable("WebClient", LOG_LEVEL_DEBUG);
    LogComponentEnable("MulticastEmulation", LOG_LEVEL_DEBUG);
  }
  else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::LOGIC)
  {
    LogComponentEnable("WebServer", LOG_LEVEL_LOGIC);
    LogComponentEnable("WebClient", LOG_LEVEL_LOGIC);
    LogComponentEnable("MulticastEmulation", LOG_LEVEL_LOGIC);
  }




  NS_LOG_UNCOND("[Scratch] HOLA! This is web.cc. Emulation started.");

  double_t showPositionDeltaTime = 1; // in s
  // nClient of WebClient, each in a different BSS
  NodeContainer p2pNodes, sfuCenter, wifiStaNodes[nClient], wifiApNode[nClient];
  uint8_t nWifi[nClient]; // number of stations in each BSS
  for (uint32_t i = 0; i < nClient; i++)
    nWifi[i] = 1;
  p2pNodes.Create(nClient + 1);
  for (uint8_t i = 0; i < nClient; i++)
  {
    wifiStaNodes[i].Create(nWifi[i]);
    wifiApNode[i] = p2pNodes.Get(i);
    if (printPosition)
    {
      Simulator::Schedule(Seconds(0.0), &showPosition, wifiStaNodes[i].Get(0), showPositionDeltaTime);
      Simulator::Schedule(Seconds(0.0), &showPosition, wifiApNode[i].Get(0), showPositionDeltaTime);
    }
  }
  sfuCenter = p2pNodes.Get(nClient);

  // 创建AP到Center之间的信道
  PointToPointHelper pointToPoint[nClient];
  for (uint32_t i = 0; i < nClient; i++)
  {
    pointToPoint[i].SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint[i].SetChannelAttribute("Delay", StringValue("10ms"));
  }

  // 在AP的p2p信道上安装NetDevice
  NetDeviceContainer backhaulDevices[nClient];
  for (uint32_t i = 0; i < nClient; i++)
    backhaulDevices[i] = pointToPoint[i].Install(wifiApNode[i].Get(0), sfuCenter.Get(0));

  // Wifi AP和stations之间建立channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  WifiMacHelper mac;
  Ssid ssid;
  WifiHelper wifi;
  NetDeviceContainer staDevices[nClient];
  NetDeviceContainer apDevices[nClient];

  // const auto &[Standard, Band] = ConvertStringToStandardAndBand(Version);
  wifi.SetStandard(WIFI_STANDARD_80211p);
  phy.Set("ChannelSettings", StringValue("{0, 10, BAND_5GHZ, 0}"));


  // Set different SSID (+ PHY channel) for each BSS(basic service set)
  for (uint32_t i = 0; i < nClient; i++)
  {

    std::string id = "ssid" + std::to_string(i + 1);
    ssid = Ssid(id);
    NS_LOG_DEBUG("[Scratch] Ssid= " << ssid);
    phy.SetChannel(channel.Create());

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices[i] = wifi.Install(phy, mac, wifiStaNodes[i]);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices[i] = wifi.Install(phy, mac, wifiApNode[i]);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX",
                                DoubleValue(0.0),
                                "MinY",
                                DoubleValue(0.0),
                                "DeltaX",
                                DoubleValue(100.0),
                                "DeltaY",
                                DoubleValue(1.0),
                                "GridWidth",
                                UintegerValue(nClient),
                                "LayoutType",
                                StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds",
                            RectangleValue(Rectangle(-30, 30, -30, 30)));

  mobility.Install(wifiStaNodes[0]);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  for (uint32_t i = 1; i < nClient; i++)
  {
    mobility.Install(wifiStaNodes[i]);
  }

  for (uint32_t i = 0; i < nClient; i++)
  {
    mobility.Install(wifiApNode[i]);
  }

  InternetStackHelper stack;
  for (uint32_t i = 0; i < nClient; i++)
  {
    stack.Install(wifiApNode[i]);
    stack.Install(wifiStaNodes[i]);
  }
  stack.Install(sfuCenter);

  // 给NetDevices分配IPv4地址
  Ipv4AddressHelper P2Paddress[nClient];
  Ipv4InterfaceContainer BackhaulIf[nClient];
  for (uint32_t i = 0; i < nClient; i++)
  {
    std::string ip = "10.1." + std::to_string(i + 1) + ".0";
    P2Paddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
    BackhaulIf[i] = P2Paddress[i].Assign(backhaulDevices[i]);
  }
  Ipv4AddressHelper Wifiaddress[nClient];
  Ipv4InterfaceContainer APinterfaces[nClient];
  Ipv4InterfaceContainer Stainterfaces[nClient];
  for (uint32_t i = 0; i < nClient; i++)
  {
    std::string ip = "10.1." + std::to_string(i + 1 + nClient) + ".0";
    Wifiaddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
    Stainterfaces[i] = Wifiaddress[i].Assign(staDevices[i]);
    APinterfaces[i] = Wifiaddress[i].Assign(apDevices[i]);
    NS_LOG_DEBUG("APinterfaces[" << i << "].getAddress(0) = " << APinterfaces[i].GetAddress(0));
  }

  // 把每个node的每个接口的ip地址打印出来
  for (uint32_t i = 0; i < nClient; i++)
  {
    Ptr<Ipv4> ippp = wifiApNode[i].Get(0)->GetObject<Ipv4>();
    int interfacenumber = ippp->GetNInterfaces();
    for (int k = 0; k < interfacenumber; k++)
    {
      Ipv4Address ipaddress = ippp->GetAddress(k, 0).GetLocal();
      NS_LOG_DEBUG("Node(" << i << ") Interface(" << k << ") IPAddress= " << ipaddress);
    }
  }

  // 给每个user(WifiSta)装上WebClient，给Center装上WebServer
  uint16_t client_ul = 80;
  uint16_t client_dl = 8080; // dl_port may increase in WebServer, make sure it doesn't overlap with ul_port
  uint16_t client_peer = 80;

  Ipv4Address serverAddr = BackhaulIf[0].GetAddress(1); // sfuCenter
  Ptr<WebServer> webServerApp = CreateObject<WebServer>();
  webServerApp->SetLocalAddress(serverAddr);
  webServerApp->SetLocalUlPort(client_peer);
  webServerApp->SetPeerDlPort(client_dl);
  webServerApp->SetLocalDlPort(client_dl);
  webServerApp->SetNodeId(sfuCenter.Get(0)->GetId());
  sfuCenter.Get(0)->AddApplication(webServerApp);
  webServerApp->SetStartTime(Seconds(0.0));
  webServerApp->SetStopTime(Seconds(simulationDuration + 2));

  for (uint32_t id = 0; id < nClient; id++)
  {

    for (uint8_t i = 0; i < nWifi[id]; i++)
    {
      Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
      Ipv4Address apAddr = BackhaulIf[id].GetAddress(i);

      Ipv4Address local;
      Ptr<Node> local_node;
      if (id == 0)
      {
        local = staAddr;
        local_node = wifiStaNodes[id].Get(i);
      }
      else
      {
        local = apAddr;
        local_node = wifiApNode[id].Get(i);
      }
      Ptr<Node> node = wifiStaNodes[id].Get(i);
      NS_LOG_DEBUG("SFU VCA NodeId " << local_node->GetId() << " " << sfuCenter.Get(0)->GetId());
      Ptr<WebClient> webClientApp = CreateObject<WebClient>();
      webClientApp->SetFps(20);
      webClientApp->SetLocalAddress(local);
      webClientApp->SetPeerAddress(std::vector<Ipv4Address>{serverAddr});
      webClientApp->SetLocalUlPort(client_ul);
      webClientApp->SetLocalDlPort(client_dl);
      webClientApp->SetPeerPort(client_peer);
      webClientApp->SetNodeId(local_node->GetId());
      webClientApp->SetNumNode(nClient);
      webClientApp->SetPolicy(static_cast<POLICY>(policy));
      webClientApp->SetUlDlParams(kUlImprove, kDlYield);
      webClientApp->SetUlThresh(kLowUlThresh, kHighUlThresh);
      webClientApp->SetMaxBitrate(maxBitrateKbps);
      webClientApp->SetMinBitrate(minBitrateKbps);
      // webClientApp->SetLogFile("../../../evaluation/results/transient_rate_debug_" + std::to_string(nClient) + "_node_" + std::to_string(local_node->GetId()) + ".txt");
      // wifiApNode[id].Get(0)->AddApplication(webClientApp);
      // wifiStaNodes[id].Get(i)->AddApplication(webClientApp);
      local_node->AddApplication(webClientApp);

      Simulator::Schedule(Seconds(simulationDuration), &WebClient::StopEncodeFrame, webClientApp);
      webClientApp->SetStartTime(Seconds(0.0));
      webClientApp->SetStopTime(Seconds(simulationDuration + 4));
    }
  }

  if (savePcap)
  {
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    pointToPoint[0].EnablePcapAll("sfu");
    phy.EnablePcap("sfu-ap", apDevices[0].Get(0));
    phy.EnablePcap("sfu-sta", staDevices[0].Get(0));
    phy.EnablePcap("sfu-ap", apDevices[2].Get(0));
    phy.EnablePcap("sfu-sta", staDevices[2].Get(0));
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