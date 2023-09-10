#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-module.h>
#include <ns3/applications-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/netanim-module.h>
#include <ns3/mobility-module.h>
#include "qos-controller.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/trace-helper.h"
#include <ns3/ipv4-flow-classifier.h>

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint16_t clients = 32;
  uint16_t servers = 8;
  uint16_t simTime = 15;
  bool verbose = false;
  bool trace = false;

  // Configure command line parameters
  CommandLine cmd;
  cmd.AddValue ("clients", "Number of client nodes", clients);
  cmd.AddValue ("servers", "Number of server nodes", servers);
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      OFSwitch13Helper::EnableDatapathLogs ();
      LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
      LogComponentEnable ("QosController", LOG_LEVEL_ALL);
    }

  // Configure dedicated connections between controller and switches
  Config::SetDefault ("ns3::OFSwitch13Helper::ChannelType", EnumValue (OFSwitch13Helper::DEDICATEDCSMA));

  // Increase TCP MSS for larger packets
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1400));

  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Discard the first MAC address ("00:00:00:00:00:01") which will be used by
  // the border switch in association with the first IP address ("10.1.1.1")
  // for the Internet service.
  Mac48Address::Allocate ();

  // Create nodes for servers, switches, controllers and clients
  NodeContainer serverNodes, switchNodes, controllerNodes, clientNodes;
  serverNodes.Create (servers);
  switchNodes.Create (6);
  controllerNodes.Create (2);
  clientNodes.Create (clients);

  // Setting node positions for NetAnim support
  Ptr<ListPositionAllocator> listPosAllocator;
  listPosAllocator = CreateObject<ListPositionAllocator> ();
  listPosAllocator->Add (Vector (  0,  0, 0));  // Server 1
  listPosAllocator->Add (Vector (  0, 150, 0));  // Server 2
  listPosAllocator->Add (Vector (  0, 300, 0)); // Server 3
  listPosAllocator->Add (Vector (  0, 450, 0)); // Server 4
  listPosAllocator->Add (Vector (  0, 600, 0)); // Server 5
  listPosAllocator->Add (Vector (  0, 750, 0)); // Server 6
  listPosAllocator->Add (Vector (  0, 900, 0)); // Server 7
  listPosAllocator->Add (Vector (  0, 1050, 0)); // Server 8

  listPosAllocator->Add (Vector (500, 500, 0));  // Border switch
  listPosAllocator->Add (Vector (1000, 500, 0));  // Aggregation switch
  listPosAllocator->Add (Vector (1300, 300, 0));  // Client switch
  listPosAllocator->Add (Vector (1300, 500, 0));  // Client switch
  listPosAllocator->Add (Vector (1300, 700, 0));  // Client switch
  listPosAllocator->Add (Vector (1300, 900, 0));  // Client switch 
  listPosAllocator->Add (Vector (750, 75, 0));  // QoS controller
  listPosAllocator->Add (Vector (1300, 75, 0));  // Learning controller
  for (size_t i = 0; i < clients; i++)
    {
      listPosAllocator->Add (Vector (2000, 50 * i, 0)); // Clients
    }

  MobilityHelper mobilityHelper;
  mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityHelper.SetPositionAllocator (listPosAllocator);
  mobilityHelper.Install (NodeContainer (serverNodes, switchNodes, controllerNodes, clientNodes));

  // Create device containers
  NetDeviceContainer serverDevices, clientDevices;
  NetDeviceContainer switch0Ports, switch1Ports, switch2Ports, switch3Ports, switch4Ports, switch5Ports;
  NetDeviceContainer link;

  // Create two 100Mbps connections between border and aggregation switches
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (0), switchNodes.Get (1)));
  switch0Ports.Add (link.Get (0));
  switch1Ports.Add (link.Get (1));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (0), switchNodes.Get (1)));
  switch0Ports.Add (link.Get (0));
  switch1Ports.Add (link.Get (1));

  // Configure the CsmaHelper for 10Mbps connections
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));

  // Connect aggregation switch to client switch
  link = csmaHelper.Install (NodeContainer (switchNodes.Get (1), switchNodes.Get (2)));
  switch1Ports.Add (link.Get (0));
  switch2Ports.Add (link.Get (1));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (1), switchNodes.Get (3)));
  switch1Ports.Add (link.Get (0));
  switch3Ports.Add (link.Get (1));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (1), switchNodes.Get (4)));
  switch1Ports.Add (link.Get (0));
  switch4Ports.Add (link.Get (1));

  link = csmaHelper.Install (NodeContainer (switchNodes.Get (1), switchNodes.Get (5)));
  switch1Ports.Add (link.Get (0));
  switch5Ports.Add (link.Get (1));
  

  // Connect servers to border switch
  for (size_t i = 0; i < servers; i++)
   {
      link = csmaHelper.Install (NodeContainer (serverNodes.Get (i), switchNodes.Get (0)));
      serverDevices.Add (link.Get (0));
      switch0Ports.Add (link.Get (1));
   }

  // Connect client nodes to client switch
  for (size_t i = 0; i < 8; i++)
    {
      link = csmaHelper.Install (NodeContainer (clientNodes.Get (i), switchNodes.Get (2)));
      clientDevices.Add (link.Get (0));
      switch2Ports.Add (link.Get (1));
    }

  for (size_t j = 8; j < 16; j++)
    {
      link = csmaHelper.Install (NodeContainer (clientNodes.Get (j), switchNodes.Get (3)));
      clientDevices.Add (link.Get (0));
      switch3Ports.Add (link.Get (1));
    }

  for (size_t k = 16; k < 24; k++)
    {
      link = csmaHelper.Install (NodeContainer (clientNodes.Get (k), switchNodes.Get (4)));
      clientDevices.Add (link.Get (0));
      switch4Ports.Add (link.Get (1));
    }

  for (size_t l = 24; l < 32; l++)
    {
      link = csmaHelper.Install (NodeContainer (clientNodes.Get (l), switchNodes.Get (5)));
      clientDevices.Add (link.Get (0));
      switch5Ports.Add (link.Get (1));
    }
  
  

  // Configure OpenFlow QoS controller for border and aggregation switches
  // (#0 and #1) into controller node 0.
  Ptr<OFSwitch13InternalHelper> ofQosHelper =
    CreateObject<OFSwitch13InternalHelper> ();
  Ptr<QosController> qosCtrl = CreateObject<QosController> ();
  ofQosHelper->InstallController (controllerNodes.Get (0), qosCtrl);

  // Configure OpenFlow learning controller for client switch (#2) into controller node 1
  Ptr<OFSwitch13InternalHelper> ofLearningHelper = CreateObject<OFSwitch13InternalHelper> ();
  Ptr<OFSwitch13LearningController> learnCtrl = CreateObject<OFSwitch13LearningController> ();
  ofLearningHelper->InstallController (controllerNodes.Get (1), learnCtrl);

  // Install OpenFlow switches 0 and 1 with border controller
  OFSwitch13DeviceContainer ofSwitchDevices;
  ofSwitchDevices.Add (ofQosHelper->InstallSwitch (switchNodes.Get (0), switch0Ports));
  ofSwitchDevices.Add (ofQosHelper->InstallSwitch (switchNodes.Get (1), switch1Ports));
  ofQosHelper->CreateOpenFlowChannels ();

  // Install OpenFlow switches with learning controller
  ofSwitchDevices.Add (ofLearningHelper->InstallSwitch (switchNodes.Get (2), switch2Ports));
  ofSwitchDevices.Add (ofLearningHelper->InstallSwitch (switchNodes.Get (3), switch2Ports));
  ofSwitchDevices.Add (ofLearningHelper->InstallSwitch (switchNodes.Get (4), switch2Ports));
  ofSwitchDevices.Add (ofLearningHelper->InstallSwitch (switchNodes.Get (5), switch2Ports));
  ofLearningHelper->CreateOpenFlowChannels ();

  // Install the TCP/IP stack into hosts nodes
  InternetStackHelper internet;
  internet.Install (serverNodes);
  internet.Install (clientNodes);

  // Set IPv4 server and client addresses (discarding the first server address)
  Ipv4AddressHelper ipv4switches;
  Ipv4InterfaceContainer internetIpIfaces;
  ipv4switches.SetBase ("10.1.0.0", "255.255.0.0", "0.0.1.2");
  internetIpIfaces = ipv4switches.Assign (serverDevices);
  ipv4switches.SetBase ("10.1.0.0", "255.255.0.0", "0.0.2.1");
  internetIpIfaces = ipv4switches.Assign (clientDevices);

  // Configure applications for traffic generation. Client hosts send traffic
  // to server. The server IP address 10.1.1.1 is attended by the border
  // switch, which redirects the traffic to internal servers, equalizing the
  // number of connections to each server.
  Ipv4Address serverAddr ("10.1.1.1");

  // Installing a sink application at server nodes
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 5000));
  ApplicationContainer sinkApps = sinkHelper.Install (serverNodes);
  sinkApps.Start (Seconds (0));

  // Installing a sender application at client nodes
  BulkSendHelper senderHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverAddr, 5000));
  ApplicationContainer senderApps = senderHelper.Install (clientNodes);

  // Get random start times
  Ptr<UniformRandomVariable> rngStart = CreateObject<UniformRandomVariable> ();
  rngStart->SetAttribute ("Min", DoubleValue (0));
  rngStart->SetAttribute ("Max", DoubleValue (1));
  ApplicationContainer::Iterator appIt;
  for (appIt = senderApps.Begin (); appIt != senderApps.End (); ++appIt)
    {
      (*appIt)->SetStartTime (Seconds (rngStart->GetValue ()));
    }

  // Enable pcap traces and datapath stats
  if (trace)
    {
      ofLearningHelper->EnableOpenFlowPcap ("openflow");
      ofLearningHelper->EnableDatapathStats ("switch-stats");
      ofQosHelper->EnableOpenFlowPcap ("openflow");
      ofQosHelper->EnableDatapathStats ("switch-stats");
      csmaHelper.EnablePcap ("switch", switchNodes, true);
      csmaHelper.EnablePcap ("server", serverDevices);
      csmaHelper.EnablePcap ("client", clientDevices);
    }

  // Creating NetAnim output file
  AnimationInterface anim ("qosctrl-netanim.xml");
  anim.SetStartTime (Seconds (0));
  anim.SetStopTime (Seconds (4));

  // Set NetAnim node descriptions
  anim.UpdateNodeDescription (0, "Server 1");
  anim.UpdateNodeDescription (1, "Server 2");
  anim.UpdateNodeDescription (2, "Server 3");
  anim.UpdateNodeDescription (3, "Server 4");
  anim.UpdateNodeDescription (4, "Server 5");
  anim.UpdateNodeDescription (5, "Server 6");
  anim.UpdateNodeDescription (6, "Server 7");
  anim.UpdateNodeDescription (7, "Server 8");
  anim.UpdateNodeDescription (8, "Border switch");
  anim.UpdateNodeDescription (9, "Aggregation switch");
  anim.UpdateNodeDescription (10, "Client switch");
  anim.UpdateNodeDescription (11, "Client switch");
  anim.UpdateNodeDescription (12, "Client switch");
  anim.UpdateNodeDescription (13, "Client switch");
  anim.UpdateNodeDescription (14, "QoS controller");
  anim.UpdateNodeDescription (15, "Learning controller");
  for (size_t i = 0; i < clients; i++)
    {
      std::ostringstream desc;
      desc << "Client " << i;
      anim.UpdateNodeDescription (15 + i, desc.str ());
    }

  // Set NetAnim icon images and size
  char cwd [1024];
  if (getcwd (cwd, sizeof (cwd)) != NULL)
    {
      std::string path = std::string (cwd) +
        "ofswitch13-qos-controller/images";
      uint32_t serverImg = anim.AddResource (path + "server.png");
      uint32_t switchImg = anim.AddResource (path + "switch.png");
      uint32_t controllerImg = anim.AddResource (path + "controller.png");
      uint32_t clientImg = anim.AddResource (path + "client.png");

      anim.UpdateNodeImage (0, serverImg);
      anim.UpdateNodeImage (1, serverImg);
      anim.UpdateNodeImage (2, serverImg);
      anim.UpdateNodeImage (3, serverImg);
      anim.UpdateNodeImage (4, serverImg);
      anim.UpdateNodeImage (5, serverImg);
      anim.UpdateNodeImage (6, serverImg);
      anim.UpdateNodeImage (7, serverImg);
      anim.UpdateNodeImage (8, switchImg);
      anim.UpdateNodeImage (9, switchImg);
      anim.UpdateNodeImage (10, switchImg);
      anim.UpdateNodeImage (11, switchImg);
      anim.UpdateNodeImage (12, switchImg);
      anim.UpdateNodeImage (13, switchImg);
      anim.UpdateNodeImage (14, controllerImg);
      anim.UpdateNodeImage (15, controllerImg);
      for (size_t i = 0; i < clients; i++)
        {
          anim.UpdateNodeImage (i + 15, clientImg);
        }
      for (size_t i = 0; i < clients + 15U; i++)
        {
          anim.UpdateNodeSize (i, 10, 10);
        }
    }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
    	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          {
        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
        std::cout << "  Delay sum: " << i->second.delaySum << "\n";
        std::cout << "  Jitter sum: " << i->second.jitterSum << "\n";
        std::cout << "  Average Packet Loss: " << i->second.lostPackets / static_cast<double>(i->second.txPackets) << "\n";
        std::cout << "  Connection Latency: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " seconds" << "\n";
        std::cout << "  T_first_Tx: " << i->second.timeFirstTxPacket <<"\n";
        std::cout << "  T_first_Rx: " << i->second.timeFirstRxPacket <<"\n";
        std::cout << "  T_last_Tx: " << i->second.timeLastTxPacket << "\n";
        std::cout << "  T_last_Rx: " << i->second.timeLastRxPacket << "\n";
				std::cout << std::endl;
          }
      }

  Simulator::Destroy ();


  // Dump total of received bytes by sink applications
  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  Ptr<PacketSink> sink2 = DynamicCast<PacketSink> (sinkApps.Get (1));
  Ptr<PacketSink> sink3 = DynamicCast<PacketSink> (sinkApps.Get (2));
  Ptr<PacketSink> sink4 = DynamicCast<PacketSink> (sinkApps.Get (3));
  Ptr<PacketSink> sink5 = DynamicCast<PacketSink> (sinkApps.Get (4));
  Ptr<PacketSink> sink6 = DynamicCast<PacketSink> (sinkApps.Get (5));
  Ptr<PacketSink> sink7 = DynamicCast<PacketSink> (sinkApps.Get (6));
  Ptr<PacketSink> sink8 = DynamicCast<PacketSink> (sinkApps.Get (7));  
  std::cout << "Bytes received by server 1: " << sink1->GetTotalRx () << " ("
            << (8. * sink1->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 2: " << sink2->GetTotalRx () << " ("
            << (8. * sink2->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 3: " << sink3->GetTotalRx () << " ("
            << (8. * sink3->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 4: " << sink4->GetTotalRx () << " ("
            << (8. * sink4->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 5: " << sink5->GetTotalRx () << " ("
            << (8. * sink5->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 6: " << sink6->GetTotalRx () << " ("
            << (8. * sink6->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 7: " << sink7->GetTotalRx () << " ("
            << (8. * sink7->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
  std::cout << "Bytes received by server 8: " << sink8->GetTotalRx () << " ("
            << (8. * sink8->GetTotalRx ()) / 1000000 / simTime << " Mbps)"
            << std::endl;
}
