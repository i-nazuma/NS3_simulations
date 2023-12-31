/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Jaume Nin <jaume.nin@cttc.cat>
 *          Manuel Requena <manuel.requena@cttc.es>
 */

#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It also starts another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE("LenaSimpleEpc");

int
main(int argc, char* argv[])
{
    uint16_t numNodePairs = 2;
    Time simTime = Seconds(5);
    double distance = 60.0;
    Time interPacketInterval = MilliSeconds(1);
    bool useCa = false;
    bool disableDl = false;
    bool disableUl = false;
    bool disablePl = false;

    // Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodePairs", "Number of eNodeBs + UE pairs", numNodePairs);
    cmd.AddValue("simTime", "Total duration of the simulation", simTime);
    cmd.AddValue("distance", "Distance between eNBs [m]", distance);
    cmd.AddValue("interPacketInterval", "Inter packet interval", interPacketInterval);
    cmd.AddValue("useCa", "Whether to use carrier aggregation.", useCa);
    cmd.AddValue("disableDl", "Disable downlink data flows", disableDl);
    cmd.AddValue("disableUl", "Disable uplink data flows", disableUl);
    cmd.AddValue("disablePl", "Disable data flows between peer UEs", disablePl);
    cmd.Parse(argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    // parse again, so you can override default values from the command line
    cmd.Parse(argc, argv);

    if (useCa)
    {
        Config::SetDefault("ns3::LteHelper::UseCa", BooleanValue(useCa));
        Config::SetDefault("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue(2));
        Config::SetDefault("ns3::LteHelper::EnbComponentCarrierManager",
                           StringValue("ns3::RrComponentCarrierManager"));
    }

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a single RemoteHost
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(numNodePairs);
    ueNodes.Create(numNodePairs);

    // Install Mobility Model
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < numNodePairs; i++)
    {
        positionAlloc->Add(Vector(distance * i, 0, 0));
    }
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(2000.0),
                                  "MinY",
                                  DoubleValue(2000.0),
                                  "DeltaX",
                                  DoubleValue(3000.0),
                                  "DeltaY",
                                  DoubleValue(3000.0),
                                  "GridWidth",
                                  UintegerValue(30),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);
    int xVal = 20;
    for (int i = 0; i < numNodePairs; i++)
    {
        ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(xVal, 800, 0));
        ueNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(40, 0, 0));
        xVal += 30;
    }
    xVal += 80;
    for (int i = 0; i < numNodePairs; i++)
    {
        enbNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(xVal, 800, 0));
        enbNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(45, 0, 0));
        xVal += 30;
    }

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < numNodePairs; i++)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
    }

    // Install and start applications on UEs and remote host
    uint16_t dlPort = 1100;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        if (!disableDl)
        {
            PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                                InetSocketAddress(Ipv4Address::GetAny(), dlPort));
            serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));

            UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
            dlClient.SetAttribute("Interval", TimeValue(interPacketInterval));
            dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
            clientApps.Add(dlClient.Install(remoteHost));
        }

        if (!disableUl)
        {
            ++ulPort;
            PacketSinkHelper ulPacketSinkHelper("ns3::UdpSocketFactory",
                                                InetSocketAddress(Ipv4Address::GetAny(), ulPort));
            serverApps.Add(ulPacketSinkHelper.Install(remoteHost));

            UdpClientHelper ulClient(remoteHostAddr, ulPort);
            ulClient.SetAttribute("Interval", TimeValue(interPacketInterval));
            ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));
            clientApps.Add(ulClient.Install(ueNodes.Get(u)));
        }

        if (!disablePl && numNodePairs > 1)
        {
            ++otherPort;
            PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                              InetSocketAddress(Ipv4Address::GetAny(), otherPort));
            serverApps.Add(packetSinkHelper.Install(ueNodes.Get(u)));

            UdpClientHelper client(ueIpIface.GetAddress(u), otherPort);
            client.SetAttribute("Interval", TimeValue(interPacketInterval));
            client.SetAttribute("MaxPackets", UintegerValue(1000000));
            clientApps.Add(client.Install(ueNodes.Get((u + 1) % numNodePairs)));
        }
    }

    serverApps.Start(MilliSeconds(500));
    clientApps.Start(MilliSeconds(500));
    lteHelper->EnableTraces();
    // Uncomment to enable PCAP tracing
    // p2ph.EnablePcapAll("lena-simple-epc");

    // the animation is written here for the later discussed result
    std::string animFile = "4G-Animation.xml";
    ////change node color
    AnimationInterface pAnim(animFile);

    Simulator::Stop(simTime);
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Run();
    // raw data of the network performance metrics
    flowMonitor->SerializeToXmlFile("stat.xml", true, true);

    Simulator::Destroy();
    flowMonitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    uint64_t txPacketsum = 0;
    uint64_t rxPacketsum = 0;
    uint64_t DropPacketsum = 0;
    double Jitter = 0;
    double Delaysum = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        if (true) // Only data packets
        {
            txPacketsum += i->second.txPackets;
            rxPacketsum += i->second.rxPackets;
            // LostPacketsum += i->second.lostPackets;
            // DropPacketsum += i->second.packetsDropped.size();
            Delaysum += i->second.delaySum.GetMilliSeconds();
            Jitter += i->second.jitterSum.GetMilliSeconds();
        }
    }
    DropPacketsum = txPacketsum - rxPacketsum;

    std::cout << "\n**************** Simulation Stats ***********************";
    std::cout << "\nAll Packets  Transmitted: " << txPacketsum;
    std::cout << "\nAll Packets Received    :  " << rxPacketsum;
    std::cout << "\nAll Drop Packets        :  " << DropPacketsum;
    std::cout << "\nDelay in milliseconds   :  " << Delaysum / txPacketsum;
    std::cout << "\nJitter in milliseconds   :  " << Jitter / txPacketsum;
    std::cout << "\nPackets Delivery Ratio  : " << (double(rxPacketsum * 100) / txPacketsum) << "%";
    std::cout << "\nPackets Lost Ratio      : " << (double(DropPacketsum * 100) / txPacketsum)
              << "%" << std::endl;
    return 0;
}
