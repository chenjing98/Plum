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

//改编自first.cc
//
// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//
 
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TestScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  std::string mode = "p2p";
  cmd.AddValue("mode","p2p or sfu mode",mode);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  


  
  if(mode == "p2p"){
    printf("Now it's p2p+WiFi!\n");
    //创建节点
    int n = 2;//AP的数量
    int nWifi[n];//每个AP管多少Nodes
    int num = (n-1)*n/2;//AP之间的信道数量
    NodeContainer p2pNodes,wifiStaNodes[n],wifiApNode[n];
    for(int i = 0; i < n; i++) 
      nWifi[i] = 1;
    p2pNodes.Create (n);
    for(int i = 0; i < n; i++){
      wifiStaNodes[i].Create (nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
    }

    //创建AP之间的信道
    PointToPointHelper pointToPoint[num];
    for(int i = 0; i < num; i ++){
      pointToPoint[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      pointToPoint[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    }

    //在AP的p2p信道上安装NetDevice
    NetDeviceContainer p2pDevices[num];
    int p = 0;
    int dev[100][100];
    for(int i = 0; i < n; i++)
      for(int j = i + 1; j < n; j++){
        p2pDevices[p] = pointToPoint[p].Install (p2pNodes.Get(i),p2pNodes.Get(j));
        dev[i][j] = p;
        dev[j][i] = p;
        p++;
      }
    // for(int i=0;i<n;i++)
    //   for(int j=0;j<n;j++)
    //     printf("dev[%d][%d]=%d\n",i,j,dev[i][j]);

    //抄一些WifiAP和WifiNodes之间建立联系的代码
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    NetDeviceContainer staDevices[n];
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    for(int i = 0; i < n; i++){
      staDevices[i] = wifi.Install(phy, mac, wifiStaNodes[i]);
    }

    NetDeviceContainer apDevices[n];
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    for(int i = 0; i < n; i++){
      apDevices[i] = wifi.Install(phy, mac, wifiApNode[i]);
    }


    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-50, 50, -50, 50)));
    for(int i = 0; i < n; i++){
      mobility.Install(wifiStaNodes[i]);
    }

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for(int i = 0; i < n; i++){
      mobility.Install(wifiApNode[i]);
    }

    printf("HOLAA!!!!!\n");

    InternetStackHelper stack;
    printf("HOLAA!!!!!\n");
    for(int i = 0; i < n; i ++){
      stack.Install (wifiApNode[i]);
    }
    printf("HOLAA!!!!!\n");
    for(int i = 0; i < n; i ++){
      stack.Install (wifiStaNodes[i]);
    }
    printf("HOLAA!!!!!\n");


    //给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[num];
    Ipv4InterfaceContainer interfaces[num];
    for(int i = 0; i < num; i++){
      std::string ip = "10.1." + std::to_string(i+1) + ".0";
      P2Paddress[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign (p2pDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[n];
    Ipv4InterfaceContainer APinterfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1+num) + ".0";
      std::cout<<"ip = "<<ip<<std::endl;
      Wifiaddress[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Wifiaddress[i].Assign (staDevices[i]);
      APinterfaces[i] = Wifiaddress[i].Assign (apDevices[i]);
      std::cout<<"APinterfaces["<<i<<"].getAddress(0) = "<<APinterfaces[i].GetAddress(0)<<std::endl;
    }


    //给AP两两之间安装CS
    int port_c = 9;
    for(int i = 0; i < n; i++){
      for(int j = 0; j < n; j++){
        if(i == j) continue;
        // Client -> i  Server -> j
        //printf("Client %d ;; Server %d   dev[%d][%d]=%d\n",i,j,i,j,dev[i][j]);
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (p2pNodes.Get(j));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        int op = (i<j)?0:1;
//        printf("op=%d\n",op);
        UdpEchoClientHelper echoClient (interfaces[dev[i][j]].GetAddress(1-op), port_c);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (p2pNodes.Get(i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }
    }

    //给每个AP内的点都安装Client，以AP作为Server
    for(int id = 0; id < n; id ++){
      for(int i = 0; i < nWifi[id]; i ++){
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (wifiApNode[id].Get(0));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpEchoClientHelper echoClient (APinterfaces[id].GetAddress(0), port_c);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (wifiStaNodes[id].Get(i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }
    }

    printf("Successfully Done!!!! [P2P MODE] \n");
  }







  else if(mode == "sfu"){
    printf("Now it's sfu!\n");
    //创建节点
    NodeContainer nodes;
    int n = 3;
    nodes.Create (n+1);
    //0,1,2,3,4,5...n-1都连到中间的选择转发单元n上

    //创建节点之间的信道
    PointToPointHelper pointToPoint[n];
    for(int i = 0; i < n; i ++){
      pointToPoint[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      pointToPoint[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    }

    //在信道上安装NetDevice
    NetDeviceContainer devices[n];
    for(int i = 0; i < n; i++)
        devices[i] = pointToPoint[i].Install (nodes.Get(i),nodes.Get(n));
    InternetStackHelper stack;
    stack.Install (nodes);

    //给NetDevices分配IPv4地址
    Ipv4AddressHelper address[n];
    Ipv4InterfaceContainer interfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1) + ".0";
      address[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
  //    address.SetBase("10.1.1.0","255.255.255.0");
      interfaces[i] = address[i].Assign (devices[i]);
    }

    //给所有点与选择转发单元安装CS
    int port_c = 9;
    for(int i = 0; i < n; i++){
      for(int op = 0; op <= 1; op++){
        // op = 0 :::: Client -> i  Server -> n
        // op = 1 :::: Client -> n  Server -> i
        int server = (op==0)?n:i;
        int client = (op==0)?i:n;
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (nodes.Get(server));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpEchoClientHelper echoClient (interfaces[i].GetAddress(1-op), port_c);//(i,n)->(0,1)
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (nodes.Get(client));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }
    }

    printf("Successfully Done [SFU MODE] !!!!\n");
  }


  
  
  double_t stopTime = 5.0; // in s
  FlowMonitorHelper flowmonHelper;
  flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run ();
  flowmonHelper.SerializeToXmlFile("test-emulation.flowmon", true, true);
  Simulator::Destroy ();
  return 0;
}
