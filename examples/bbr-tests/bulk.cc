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
#define QUEUE_SIZE       1        // Packets
#define START_TIME       0.0       // Seconds
#define STOP_TIME        120.0       // Seconds
#define S_TO_R_BW        "1000Mbps" // Server to router
#define S_TO_R_DELAY     "10ms"
#define R_TO_R_BW        "20Mbps"  // Router to client (bttlneck)
#define R_TO_R_DELAY     "1ms"
#define PACKET_SIZE      1000      // Bytes.

// Uncomment one of the below.
#define TCP_PROTOCOL     "ns3::TcpCubic"
//#define TCP_PROTOCOL     "ns3::TcpNewReno"

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
  // Setup environment
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                     StringValue(TCP_PROTOCOL));

  Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (125));

  // Report parameters.
  NS_LOG_INFO("TCP protocol: " << TCP_PROTOCOL);
  NS_LOG_INFO("Server to Router Bwdth: " << S_TO_R_BW);
  NS_LOG_INFO("Server to Router Delay: " << S_TO_R_DELAY);
  NS_LOG_INFO("Router to Router Bwdth: " << R_TO_R_BW);
  NS_LOG_INFO("Router to Router Delay: " << R_TO_R_DELAY);
  NS_LOG_INFO("Packet size (bytes): " << PACKET_SIZE);
  
  // Set segment size (otherwise, ns-3 default is 536).
  Config::SetDefault("ns3::TcpSocket::SegmentSize",
                     UintegerValue(PACKET_SIZE)); 

  // Turn off delayed ack (so, acks every packet).
  // Note, BBR' still works without this.
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
   
  /////////////////////////////////////////
  // Create nodes.
  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes1;
  nodes1.Create(2);
  NodeContainer nodes2;
  nodes2.Create(2);
  NodeContainer nodes3;
  nodes3.Create(2);

  NodeContainer nodes;  // 0=router_left 1=router_right 2=bbr_source 3=bbr_sink 4=cubic_source 5=cubic_sink
  nodes.Add(nodes1);
  nodes.Add(nodes2);
  nodes.Add(nodes3);

  /////////////////////////////////////////
  // Create channels.
  NS_LOG_INFO("Creating channels.");
  NodeContainer r0_to_r1  = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n2_to_r0 = NodeContainer(nodes.Get(2), nodes.Get(0));
  NodeContainer r1_to_n3 = NodeContainer(nodes.Get(1), nodes.Get(3));
  NodeContainer n4_to_r0 = NodeContainer(nodes.Get(4), nodes.Get(0));
  NodeContainer r1_to_n5 = NodeContainer(nodes.Get(1), nodes.Get(5));

  /////////////////////////////////////////
  // Create links.
  NS_LOG_INFO("Creating links.");

  PointToPointHelper p2p;
  int mtu = 1500;

  // Router to Router.
  p2p.SetDeviceAttribute("DataRate", StringValue (R_TO_R_BW));
  p2p.SetChannelAttribute("Delay", StringValue (R_TO_R_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NS_LOG_INFO("Router queue size: "<< QUEUE_SIZE);
  p2p.SetQueue("ns3::DropTailQueue",
               "Mode", StringValue ("QUEUE_MODE_PACKETS"),
               "MaxPackets", UintegerValue(QUEUE_SIZE));
  NetDeviceContainer devices_r0_to_r1 = p2p.Install(r0_to_r1);

  // BBR Server to Router.
  p2p.SetDeviceAttribute("DataRate", StringValue (S_TO_R_BW));
  p2p.SetChannelAttribute("Delay", StringValue (S_TO_R_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NetDeviceContainer devices_n2_to_r0 = p2p.Install(n2_to_r0);
  NetDeviceContainer devices_r1_to_n3 = p2p.Install(r1_to_n3);
  NetDeviceContainer devices_n4_to_r0 = p2p.Install(n4_to_r0);
  NetDeviceContainer devices_r1_to_n5 = p2p.Install(r1_to_n5);

  /////////////////////////////////////////
  // Install Internet stack.
  NS_LOG_INFO("Installing Internet stack.");
  InternetStackHelper internet;
  internet.Install(nodes1);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    StringValue("ns3::TcpBbr"));
  internet.Install(nodes2);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    StringValue("ns3::TcpCubic"));
  internet.Install(nodes3);

  /////////////////////////////////////////
  // Add IP addresses.
  NS_LOG_INFO("Assigning IP Addresses.");

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(devices_r0_to_r1);
  
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i0 = ipv4.Assign(devices_n2_to_r0);

  ipv4.SetBase("191.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i3 = ipv4.Assign(devices_r1_to_n3);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i0 = ipv4.Assign(devices_n4_to_r0);

  ipv4.SetBase("191.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i5 = ipv4.Assign(devices_r1_to_n5);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  /////////////////////////////////////////
  // Create apps.
  NS_LOG_INFO("Creating applications.");
  NS_LOG_INFO("  Bulk send.");

  // Well-known port for server.
  uint16_t port = 911;  

  ApplicationContainer apps;

  // BBR Source (at node 2).
  BulkSendHelper bbr_source("ns3::TcpSocketFactory",
                        InetSocketAddress(i1i3.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  bbr_source.SetAttribute("MaxBytes", UintegerValue(0));
  bbr_source.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  apps = bbr_source.Install(nodes.Get(2));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));

  // BBR Sink (at node 3).
  PacketSinkHelper bbr_sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = bbr_sink.Install(nodes.Get(3));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));
  Ptr<PacketSink> p_sink_bbr = DynamicCast<PacketSink> (apps.Get(0)); // 4 stats

  // CUBIC Source (at node 4).
  BulkSendHelper cubic_source("ns3::TcpSocketFactory",
                        InetSocketAddress(i1i5.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  cubic_source.SetAttribute("MaxBytes", UintegerValue(0));
  cubic_source.SetAttribute("SendSize", UintegerValue(PACKET_SIZE));
  apps = cubic_source.Install(nodes.Get(4));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));

  // CUBIC Sink (at node 5).
  PacketSinkHelper cubic_sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = cubic_sink.Install(nodes.Get(5));
  apps.Start(Seconds(START_TIME));
  apps.Stop(Seconds(STOP_TIME));
  Ptr<PacketSink> p_sink_cubic = DynamicCast<PacketSink> (apps.Get(0)); // 4 stats

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
  NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(STOP_TIME));
  NS_LOG_INFO("Simulation time: [" << 
              START_TIME << "," <<
              STOP_TIME << "]");
  NS_LOG_INFO("---------------- Start -----------------------");
  Simulator::Run();
  NS_LOG_INFO("---------------- Stop ------------------------");

  /////////////////////////////////////////
  // Ouput stats.
  double tput;

  NS_LOG_INFO("----------------- BBR ------------------------");
  NS_LOG_INFO("Total bytes received: " << p_sink_bbr->GetTotalRx());
  tput = p_sink_bbr->GetTotalRx() / (STOP_TIME - START_TIME);
  tput *= 8;          // Convert to bits.
  tput /= 1000000.0;  // Convert to Mb/s
  NS_LOG_INFO("Throughput: " << tput << " Mb/s");
  
  NS_LOG_INFO("----------------- CUBIC ----------------------");
  NS_LOG_INFO("Total bytes received: " << p_sink_cubic->GetTotalRx());
  tput = p_sink_cubic->GetTotalRx() / (STOP_TIME - START_TIME);
  tput *= 8;          // Convert to bits.
  tput /= 1000000.0;  // Convert to Mb/s
  NS_LOG_INFO("Throughput: " << tput << " Mb/s");
  NS_LOG_INFO("Done.");

  // Done.
  Simulator::Destroy();
  return 0;
}
