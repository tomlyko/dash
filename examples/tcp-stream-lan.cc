/* 
  New example script which accepts additional parameters and connects the clients to a server over a physical link.
*/

#include "ns3/point-to-point-helper.h"
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <ns3/buildings-module.h>
#include "ns3/building-position-allocator.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include "ns3/csma-module.h"
#include <fstream>

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStreamExampleLan");

queue<double> trafficShaping;

void ChangeBandwidth()
{
  string newBandwidth = to_string(trafficShaping.front()) + "Kbps";
  trafficShaping.pop();
  //cout << newBandwidth << "\n";
  Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue(newBandwidth) );
}

int
main (int argc, char *argv[])
{
  uint64_t segmentDuration;
  // The simulation id is used to distinguish log file results from potentially multiple consequent simulation runs.
  uint32_t simulationId;
  uint32_t numberOfClients;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;
  std::string linkRate = "500Kbps";
  std::string delay = "5ms";
  int buffer = -1;
  int playbackStart = -1;
  int chunk = 0;
  int cmaf = 0; //0: ABRs are dealing with chunks, 1: ABRs are dealing with segments, 2: ABRs optimized, 3: worst case scenario
  string tracePath;
  int segmentsBehindLive = 1;
  double streamJoinOffset = 0;
  int logLevel = 0; //0: All, 1: Only playback and stalls, 2: Only QoE metrics: Avg Quality Lvl, Quality S.D., Rebuffer Ratio and Rebuffer Frequency

  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH.\n");
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds OR the duration of a chunk if chunks are active", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes OR chunk sizes if chunks are active", segmentSizeFilePath);
  cmd.AddValue ("linkRate", "The bitrate of the link connecting the clients to the server (e.g. 500kbps)", linkRate);
  cmd.AddValue ("delay", "The delay of the link connecting the clients to the server (e.g. 5ms)", delay);
  cmd.AddValue ("buffer", "The initial buffer size as number of segments (eg 5)", buffer);
  cmd.AddValue ("playbackStart", "The number of segments/chunks to be fetched before playback starts (default -1: 1 complete DASH/CMAF segment).", playbackStart);
  cmd.AddValue ("trace", "The relative path (from ns-3.x directory) to the network trace file", tracePath);
  cmd.AddValue ("chunk", "Number of chunks in a segment, 0 if no chunks, set chunk duration in segmentDuration and chunk sizes in segmentSizeFile", chunk);
  cmd.AddValue ("cmaf", "CMAF version: 0: ABRs are dealing with chunks, 1: ABRs are dealing with segments, 2: ABRs optimized, 3: worst case scenario", cmaf);
  cmd.AddValue ("liveDelay", "Number of full DASH/CMAF segments behind live.", segmentsBehindLive);
  cmd.AddValue ("joinOffset", "Offset time to DASH/CMAF segment generation (s). eg 0.5s: the client will join the stream at 0.5s after a segment was generated", streamJoinOffset);
  cmd.AddValue ("logLevel", "Logging level: 0: All, 1: Only playback and stalls, 2: Only QoE metrics: Avg Quality Lvl, Quality S.D., Rebuffer Ratio and Rebuffer Frequency", logLevel);
  cmd.Parse (argc, argv);


  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

  // create folders for logs
  const char * mylogsDir = dashLogDirectory.c_str();
  mkdir (mylogsDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  std::string temp = dashLogDirectory + "/SimID_" + ToString (simulationId); 
  const char * dir = temp.c_str();
  mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
 
  // log parameters
  if(logLevel == 0) {
    std::ofstream parLog;
    std::string pars = temp + "/" + "parameters.txt";
    parLog.open (pars.c_str());  
    parLog << ToString(simulationId) << "," << ToString(numberOfClients) << "," << ToString(segmentDuration) << "," << ToString(buffer) << "," << ToString(adaptationAlgo) << "," << ToString(linkRate) << "," << ToString(delay) << "\n";
    parLog.flush ();
  }
    
  NS_LOG_INFO("Create nodes.");
    
  /* Create Nodes */	
  NodeContainer p2pNodes;
  p2pNodes.Create (2);
  
  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (numberOfClients);	
    
  NS_LOG_INFO("Create p2p between nodes.");

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (linkRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);    


  NS_LOG_INFO("Install the internet stack on the nodes.");    

  InternetStackHelper stack;
  stack.Install (p2pNodes.Get (0));
  stack.Install (csmaNodes);

  NS_LOG_INFO("Assign IP Addresses.");
    
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);
  
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);
	

  /* Determin client nodes for object creation with client helper class */
  std::vector <std::pair <Ptr<Node>, std::string> > clients;
  for (NodeContainer::Iterator i = csmaNodes.Begin () + 1; i != csmaNodes.End (); ++i)
  {
    std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
    clients.push_back (client);
  }

  /* Install TCP Receiver on the access point */
  TcpStreamServerHelper serverHelper (80);
  ApplicationContainer serverApp = serverHelper.Install (p2pNodes.Get (0));
  serverApp.Start (Seconds (0));
    
  /* Install TCP/UDP Transmitter on the station */
  TcpStreamClientHelper clientHelper (p2pInterfaces.GetAddress(0), 80);
  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
  if(playbackStart > 0) {
    clientHelper.SetAttribute ("PlaybackStart", UintegerValue (playbackStart));
  }
  clientHelper.SetAttribute ("Chunk", UintegerValue (chunk));
  clientHelper.SetAttribute ("Cmaf", UintegerValue (cmaf));
  clientHelper.SetAttribute ("LogLevel", UintegerValue (logLevel));
  ApplicationContainer clientApps = clientHelper.Install (clients);
  for (uint i = 0; i < clientApps.GetN (); i++)
  {
    double startTime = 0;
    if(buffer > -1) {
      // Old option, soon to be removed
      startTime = (double)buffer;
    } else {
      // Using liveDelay and joinOffset parameters
      double segDuration = (segmentDuration/1000000);
      if(chunk > 0) segDuration = ((segmentDuration*chunk)/1000000);
      startTime = (segmentsBehindLive*segDuration)+(streamJoinOffset);
    }
    clientApps.Get (i)->SetStartTime (Seconds (startTime));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Run Simulation.");
  NS_LOG_INFO ("Sim ID: " << simulationId << " Clients: " << numberOfClients);
	
  /* Read traffic shaping file, populate the queue and schedule the bandwidth changes */
  std::ifstream infile(tracePath);
  double a, b;
  while (infile >> a >> b)
  {
    //cout << a << " " << b << "\n";
    trafficShaping.push(b);
    Simulator::Schedule (Seconds(a) , &ChangeBandwidth);
  }
	  
  Simulator::Stop (Seconds(400));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

}
