//           
// Network topology
//
//       n0 ------------ (n1/router) -------------- n2
//            10.1.1.x                192.168.1.x
//       10.1.1.1    10.1.1.2   192.16.1.1     192.168.1.2
//
// - Flow from n0 to n2 using BulkSendApplication.
//
// - Tracing of queues and packet receptions to file "*.tr" and
//   "*.pcap" when tracing is turned on.
//

// System includes.
#include <string>
#include <fstream>

// NS3 includes.
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// Constants.
#define ENABLE_PCAP      false     // Set to "true" to enable pcap
#define ENABLE_TRACE     false     // Set to "true" to enable trace
#define BIG_QUEUE        2000      // Packets
#define QUEUE_SIZE       1         // Packets
#define START_TIME       0.0       // Seconds
#define STOP_TIME        120.0       // Seconds
#define S_TO_R_BW        "1000Mbps" // Server to router
#define S_TO_R_DELAY     "10ms"
#define R_TO_R_BW        "20Mbps"  // Router to client (bttlneck)
#define R_TO_R_DELAY     "1ms"
#define PACKET_SIZE      1000      // Bytes.

// Uncomment one of the below.

enum TCP_PROTOCOL { BBR, CUBIC };

typedef struct {
  TCP_PROTOCOL prot;
  int rtt;  
} flow_info;

// For logging. 

NS_LOG_COMPONENT_DEFINE ("main");

/////////////////////////////////////////////////
int main (int argc, char *argv[]) {

  /////////////////////////////////////////
  // Turn on logging for this script.
  // Note: for BBR', other components that may be
  // of interest include "TcpBbr" and "BbrState".
  LogComponentEnable("main", LOG_LEVEL_INFO);

  /////////////////////////////////////////
  // Read input
  std::string flows = "BCBCBC";
  std::vector<int> rtts { 20, 50, 80 };

  int packetSize = 1000;
  double bdp = 2;
  int bandwidth = 20;
  int maxRtt = *std::max_element(rtts.begin(), rtts.end());

  CommandLine cmd;
  cmd.AddValue("flows", "Flow combinations of BBR and Cubic", flows);
  cmd.AddValue("bdp", "BDP", bdp);
  cmd.AddValue("bandwidth", "Bandwidth", bandwidth);
  cmd.Parse(argc, argv);

  int queueSizeBytes = bdp * bandwidth * maxRtt * 1000 / 8;
  int queueSizePackets = queueSizeBytes / packetSize;
  
  int nSender = flows.size();
  std::vector<flow_info> flow_infos(nSender);
  for (int i = 0; i < nSender; i++) {
    flow_infos[i] = flow_info { flows[i] == 'B' ? BBR : CUBIC, rtts[i / (nSender / rtts.size())] };
  }

  /////////////////////////////////////////
  // Setup environment
  Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (queueSizePackets));

  // Report parameters.
  NS_LOG_INFO("Flow: " << flows);
  NS_LOG_INFO("Server to Router Bwdth: " << S_TO_R_BW);
  NS_LOG_INFO("Server to Router Delay: " << S_TO_R_DELAY);
  NS_LOG_INFO("Router to Router Bwdth: " << bandwidth << "Mbps");
  NS_LOG_INFO("Packet size (bytes): " << packetSize);
  NS_LOG_INFO("Queue Size (packets): " << queueSizePackets);
  
  // Set segment size (otherwise, ns-3 default is 536).
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize)); 

  // Turn off delayed ack (so, acks every packet).
  // Note, BBR' still works without this.
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
   
  /////////////////////////////////////////
  // Create nodes.
  // NS_LOG_INFO("Creating nodes.");

  NodeContainer routerNodes; 
  routerNodes.Create(2);
  NodeContainer senderNodes;
  senderNodes.Create(nSender);
  NodeContainer receiverNodes;
  receiverNodes.Create(nSender);

  /////////////////////////////////////////
  // Create channels.
  // NS_LOG_INFO("Creating channels.");

  /////////////////////////////////////////
  // Create links.
  // NS_LOG_INFO("Creating links.");

  PointToPointHelper p2p;
  int mtu = 2000;

  // Router to Router.
  NodeContainer r0_to_r1  = NodeContainer(routerNodes.Get(0), routerNodes.Get(1));
  std::string dataRate = std::to_string(bandwidth) + "Mbps";
  p2p.SetDeviceAttribute("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute("Delay", StringValue (R_TO_R_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  // NS_LOG_INFO("Router queue size: "<< QUEUE_SIZE);
  p2p.SetQueue("ns3::DropTailQueue",
               "Mode", StringValue ("QUEUE_MODE_PACKETS"),
               "MaxPackets", UintegerValue(1));
  NetDeviceContainer routerDevices = p2p.Install(r0_to_r1);

  NetDeviceContainer senderDevices, router0Devices, router1Devices, receiverDevices;

  // BBR Server to Router.
  for (int i = 0; i < nSender; i++) {
    std::string delay = std::to_string(flow_infos[i].rtt / 2) + "ms";
    p2p.SetDeviceAttribute("DataRate", StringValue (S_TO_R_BW));
    p2p.SetChannelAttribute("Delay", StringValue (delay));
    p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
    NodeContainer s_to_r0 = NodeContainer(senderNodes.Get(i), routerNodes.Get(0));
    NodeContainer r1_to_c = NodeContainer(routerNodes.Get(1), receiverNodes.Get(i));
    NetDeviceContainer leftDevices = p2p.Install(s_to_r0);
    senderDevices.Add(leftDevices.Get(0));
    router0Devices.Add(leftDevices.Get(1));
    NetDeviceContainer rightDevices = p2p.Install(r1_to_c);
    router1Devices.Add(rightDevices.Get(0));
    receiverDevices.Add(rightDevices.Get(1));
  }

  /////////////////////////////////////////
  // Install Internet stack.
  // NS_LOG_INFO("Installing Internet stack.");
  InternetStackHelper internet;
  internet.Install(routerNodes);

  for (int i = 0; i < nSender; i++) {
    switch (flow_infos[i].prot)
    {
    case BBR:
      Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
      break;
    case CUBIC:
      Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
      break;
    default:
      break;
    }
    internet.Install(senderNodes.Get(i));
    internet.Install(receiverNodes.Get(i));
  }

  /////////////////////////////////////////
  // Add IP addresses.
  // NS_LOG_INFO("Assigning IP Addresses.");

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(routerDevices);
  Ipv4InterfaceContainer iSenders, iRouter0, iRouter1, iReceivers;

  for (int i = 0; i < nSender; i++) {
    std::string addr = "10.1." + std::to_string(i) + ".0";
    ipv4.SetBase(addr.c_str(), "255.255.255.0");
    NetDeviceContainer leftDevices(senderDevices.Get(i), router0Devices.Get(i));
    Ipv4InterfaceContainer iLeft = ipv4.Assign(leftDevices);
    iSenders.Add(iLeft.Get(0));
    iRouter0.Add(iLeft.Get(1));

    addr = "192.168." + std::to_string(i) + ".0";
    ipv4.SetBase(addr.c_str(), "255.255.255.0"); 
    NetDeviceContainer rightDevices(router1Devices.Get(i), receiverDevices.Get(i));
    Ipv4InterfaceContainer iRight = ipv4.Assign(rightDevices);
    iRouter1.Add(iRight.Get(0));
    iReceivers.Add(iRight.Get(1));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  /////////////////////////////////////////
  // Create apps.
  // NS_LOG_INFO("Creating applications.");
  // NS_LOG_INFO("  Bulk send.");

  // Well-known port for server.
  uint16_t port = 911;  

  std::vector<Ptr<PacketSink>> sinks(nSender);

  for (int i = 0; i < nSender; i++) {
    ApplicationContainer apps;
  
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(iReceivers.GetAddress(i), port));
    // Set the amount of data to send in bytes (0 for unlimited).
    source.SetAttribute("MaxBytes", UintegerValue(0));
    source.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
    apps = source.Install(senderNodes.Get(i));
    apps.Start(Seconds(START_TIME));
    apps.Stop(Seconds(STOP_TIME));

    // Packet Sink at Receiver
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(receiverNodes.Get(i));
    apps.Start(Seconds(START_TIME));
    apps.Stop(Seconds(STOP_TIME));
    sinks[i] = DynamicCast<PacketSink> (apps.Get(0)); // 4 stats
  }

  /////////////////////////////////////////
  // Setup tracing (as appropriate).
  if (ENABLE_TRACE) {
    NS_LOG_INFO("Enabling trace files.");
    AsciiTraceHelper ath;
    p2p.EnableAsciiAll(ath.CreateFileStream("trace.tr"));
  }  
  if (ENABLE_PCAP) {
    NS_LOG_INFO("Enabling pcap files.");
    p2p.EnablePcapAll("shark", true);
  }

  /////////////////////////////////////////
  // Run simulation.
  // NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(STOP_TIME));
  NS_LOG_INFO("Simulation time: [" << 
              START_TIME << "," <<
              STOP_TIME << "]");
  // NS_LOG_INFO("---------------- Start -----------------------");
  Simulator::Run();
  // NS_LOG_INFO("---------------- Stop ------------------------");

  /////////////////////////////////////////
  // Ouput stats.

  std::ofstream outputFile;
  outputFile.open(flows + "_" + std::to_string(bdp));

  double tput;

  for (int i = 0; i < nSender; i++) {
    NS_LOG_INFO("----------------- Flow " << i << ": " << flow_infos[i].prot << ", RTT " << flow_infos[i].rtt << "ms ------------------------");
    NS_LOG_INFO("Total bytes received: " << sinks[i]->GetTotalRx());
    tput = sinks[i]->GetTotalRx() / (STOP_TIME - START_TIME);
    tput *= 8;          // Convert to bits.
    tput /= 1000000.0;  // Convert to Mb/s
    NS_LOG_INFO("Throughput: " << tput << " Mb/s");
    outputFile << tput << " ";
  }
  outputFile.close();

  // Done.
  Simulator::Destroy();
  return 0;
}
