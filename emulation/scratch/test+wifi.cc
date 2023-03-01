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
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[n];
    NetDeviceContainer apDevices[n];

    //每次的SSID、PHY要不同
    for(int i = 0; i < n; i++){
      std::string id = "ssid" + std::to_string(i+1);
//      std::cout<<id<<std::endl;
      ssid = Ssid(id);
      std::cout<<ssid<<std::endl;
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

    InternetStackHelper stack;
    for(int i = 0; i < n; i ++)
      stack.Install (wifiApNode[i]);
    for(int i = 0; i < n; i ++)
      stack.Install (wifiStaNodes[i]);


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
    Ipv4InterfaceContainer Stainterfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1+num) + ".0";
//      std::cout<<"ip = "<<ip<<std::endl;
      Wifiaddress[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Stainterfaces[i] = Wifiaddress[i].Assign (staDevices[i]);
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
    
    // //wifiStaNodes[0].Get(0) -> Client
    // //wifiStaNodes[1].Get(0) -> Server
    // UdpEchoServerHelper echoServer(port_c);
    // ApplicationContainer serverApps = echoServer.Install (wifiStaNodes[1].Get(0));
    // serverApps.Start (Seconds (1.0));
    // serverApps.Stop (Seconds (10.0));

    // std::cout<<Stainterfaces[1].GetAddress(0)<<std::endl;
    // UdpEchoClientHelper echoClient (Stainterfaces[1].GetAddress(0), port_c);
    // echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
    // echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    // echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    // ApplicationContainer clientApps = echoClient.Install (wifiStaNodes[0].Get(0));
    // clientApps.Start (Seconds (2.0));
    // clientApps.Stop (Seconds (10.0));
    // port_c++;

    printf("Successfully Done!!!! [P2P MODE] \n");

  }







  else if(mode == "sfu"){
    printf("Now it's sfu!\n");
    //创建节点
    int n = 2;//AP的数量
    int nWifi[n];//每个AP管多少Nodes
    NodeContainer p2pNodes,sfuCenter,wifiStaNodes[n],wifiApNode[n];
    for(int i = 0; i < n; i++) nWifi[i] = 1;
    p2pNodes.Create (n+1);
    for(int i = 0; i < n; i++){
      wifiStaNodes[i].Create (nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
    }
    sfuCenter = p2pNodes.Get(n);


    //创建AP到Center之间的信道
    PointToPointHelper pointToPoint[n];
    for(int i = 0; i < n; i ++){
      pointToPoint[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      pointToPoint[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    }

    //在AP的p2p信道上安装NetDevice
    NetDeviceContainer sfuDevices[n];
    for(int i = 0; i < n; i++)
      sfuDevices[i] = pointToPoint[i].Install (wifiApNode[i].Get(0),sfuCenter.Get(0));
    // for(int i=0;i<n;i++)
    //   for(int j=0;j<n;j++)
    //     printf("dev[%d][%d]=%d\n",i,j,dev[i][j]);

    //抄一些WifiAP和WifiNodes之间建立联系的代码
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[n];
    NetDeviceContainer apDevices[n];


    //每次的SSID、PHY要不同
    for(int i = 0; i < n; i++){
      std::string id = "ssid" + std::to_string(i+1);
//      std::cout<<id<<std::endl;
      ssid = Ssid(id);
      std::cout<<ssid<<std::endl;
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

    InternetStackHelper stack;
    for(int i = 0; i < n; i ++)
      stack.Install (wifiApNode[i]);
    for(int i = 0; i < n; i ++)
      stack.Install (wifiStaNodes[i]);
    stack.Install (sfuCenter);


    //给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[n];
    Ipv4InterfaceContainer interfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1) + ".0";
      P2Paddress[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign (sfuDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[n];
    Ipv4InterfaceContainer APinterfaces[n];
    Ipv4InterfaceContainer Stainterfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1+n) + ".0";
//      std::cout<<"ip = "<<ip<<std::endl;
      Wifiaddress[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Stainterfaces[i] = Wifiaddress[i].Assign (staDevices[i]);
      APinterfaces[i] = Wifiaddress[i].Assign (apDevices[i]);
      std::cout<<"APinterfaces["<<i<<"].getAddress(0) = "<<APinterfaces[i].GetAddress(0)<<std::endl;
    }

     //Debug：把每个node的每个接口的ip地址打印出来
    for(int i = 0; i < n; i ++){
      Ptr<Ipv4> ippp = wifiApNode[i].Get(0)->GetObject<Ipv4>();
      int interfacenumber = ippp->GetNInterfaces();
      for(int k = 0; k < interfacenumber; k++){
        Ipv4Address ipaddress = ippp->GetAddress(k,0).GetLocal();
        std::cout<<"Node("<<i<<"),interface("<<k<<")   it's IPAddress ="<<ipaddress<<std::endl;
      }
    }

    //给所有点与选择转发单元安装CS
    int port_c = 9;
    for(int i = 0; i < n; i++){
      //Client:AP -> Server:Center
      if(1) {
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (sfuCenter.Get(0));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpEchoClientHelper echoClient (interfaces[i].GetAddress(1), port_c);//(i,center)->(0,1)
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (wifiApNode[i].Get(0));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }

      //Client:Center -> Server:AP 
      if(1){
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (wifiApNode[i].Get(0));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpEchoClientHelper echoClient (interfaces[i].GetAddress(0), port_c);//(i,center)->(0,1)
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (sfuCenter.Get(0));
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

    printf("Successfully Done [SFU MODE] !!!!\n");
  }


  double_t stopTime = 5.0; // in s
  FlowMonitorHelper flowmonHelper;
  flowmonHelper.InstallAll();
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // int tracing = 1;
  // if (tracing) {
  //   phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  //   pointToPoint[0].EnablePcapAll("third");
  //   phy.EnablePcap("third", apDevices[0].Get(0));
  //   phy.EnablePcap("third", apDevices[1].Get(0));
  //   phy.EnablePcap("third-sta", staDevices[0].Get(0));
  // }

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run ();
  flowmonHelper.SerializeToXmlFile("test-emulation.flowmon", true, true);
  Simulator::Destroy ();
  return 0;
}