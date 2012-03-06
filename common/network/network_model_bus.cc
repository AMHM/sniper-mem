#include "core_manager.h"
#include "simulator.h"
#include "network.h"
#include "network_model_bus.h"
#include "memory_manager_base.h"
#include "stats.h"
#include "log.h"
#include "dvfs_manager.h"
#include "config.hpp"

NetworkModelBusGlobal* NetworkModelBus::_bus_global[NUM_STATIC_NETWORKS] = { NULL };

NetworkModelBusGlobal::NetworkModelBusGlobal(String name)
   : _bandwidth(8 * Sim()->getCfg()->getFloat("network/bus/bandwidth")) /* = 8 * GB/s / Gcycles/s = bits / cycle, round down (implicit: float to int conversion) */
   #ifndef BUS_USE_QUEUE_MODEL
   , _contention_model(name, 0)
   #endif
   , _num_packets(0)
   , _num_packets_delayed(0)
   , _num_bytes(0)
   , _time_used(SubsecondTime::Zero())
   , _total_delay(SubsecondTime::Zero())
{
   #ifdef BUS_USE_QUEUE_MODEL
   _queue_model = QueueModel::create(Sim()->getCfg()->getString("network/bus/queue_model/type", "history_list"), 10 * SubsecondTime::NS());
   #endif
   /* 8 * GB/s / Gcycles/s = bits / cycle, round down (implicit: float to int conversion) */
   registerStatsMetric(name, 0, "num-packets", &_num_packets);
   registerStatsMetric(name, 0, "num-packets-delayed", &_num_packets_delayed);
   registerStatsMetric(name, 0, "num-bytes", &_num_bytes);
   registerStatsMetric(name, 0, "time-used", &_time_used);
   registerStatsMetric(name, 0, "total-delay", &_total_delay);
}

NetworkModelBusGlobal::~NetworkModelBusGlobal()
{
   #ifdef BUS_USE_QUEUE_MODEL
   delete _queue_model;
   #endif
}

/* Model bus utilization. In: packet start time and size. Out: packet out time */
SubsecondTime
NetworkModelBusGlobal::useBus(SubsecondTime t_start, UInt32 length)
{
   SubsecondTime t_delay = _bandwidth.getLatency(length * 8);
   #ifdef BUS_USE_QUEUE_MODEL
   SubsecondTime t_queue = _queue_model->computeQueueDelay(t_start, t_delay);
   #else
   SubsecondTime t_complete = _contention_model.getCompletionTime(t_start, t_delay);
   SubsecondTime t_queue = t_complete - t_start - t_delay;
   #endif
   _time_used += t_delay;
   _total_delay += t_queue;
   if (t_queue > SubsecondTime::Zero())
      _num_packets_delayed ++;
   return t_start + t_queue + t_delay;
}

NetworkModelBus::NetworkModelBus(Network *net, EStaticNetwork net_type)
   : NetworkModel(net)
   , _enabled(false)
   , _mcp_detour(Sim()->getConfig()->getProcessNumForCore(Sim()->getConfig()->getMCPCoreNum()) != Sim()->getConfig()->getCurrentProcessNum())
   , _ignore_local(Sim()->getCfg()->getBool("network/bus/ignore_local_traffic", true))
{
   if (!_bus_global[net_type]) {
      String name = String("network.")+EStaticNetworkStrings[net_type]+".bus";
      _bus_global[net_type] = new NetworkModelBusGlobal(name);
   }
   _bus = _bus_global[net_type];
}

void
NetworkModelBus::routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops)
{
   if (!_mcp_detour || getNetwork()->getCore()->getId() == Config::getSingleton()->getMCPCoreNum()) {
      /* On MCP: account for time, send message to destination */

      SubsecondTime t_recv;
      if (accountPacket(pkt)) {
         ScopedLock sl(_bus->_lock);
         _bus->_num_packets ++;
         _bus->_num_bytes += getNetwork()->getModeledLength(pkt);
         t_recv = _bus->useBus(pkt.time, pkt.length);
      } else
         t_recv = pkt.time;

      if (pkt.receiver == NetPacket::BROADCAST)
      {
         UInt32 total_cores = Config::getSingleton()->getTotalCores();

         for (SInt32 i = 0; i < (SInt32) total_cores; i++)
         {
            Hop h;
            h.final_dest = i;
            h.next_dest = i;
            h.time = t_recv;

            nextHops.push_back(h);
         }
      }
      else
      {
         Hop h;
         h.final_dest = pkt.receiver;
         h.next_dest = pkt.receiver;
         h.time = t_recv;

         nextHops.push_back(h);
      }
   }
   else
   {
      /* On all other cores: send message to MCP core for timing simulation */
      Hop h;
      h.final_dest = pkt.receiver;
      h.next_dest = Config::getSingleton()->getMCPCoreNum();
      h.time = pkt.time;

      nextHops.push_back(h);
   }
}

void
NetworkModelBus::processReceivedPacket(NetPacket &pkt)
{
}

bool
NetworkModelBus::accountPacket(const NetPacket &pkt)
{
   core_id_t requester = INVALID_CORE_ID;

   if ((pkt.type == SHARED_MEM_1) || (pkt.type == SHARED_MEM_2))
      requester = getNetwork()->getCore()->getMemoryManager()->getShmemRequester(pkt.data);
   else // Other Packet types
      requester = pkt.sender;

   LOG_ASSERT_ERROR((requester >= 0) && (requester < (core_id_t) Config::getSingleton()->getTotalCores()),
         "requester(%i)", requester);

   if (  !_enabled
         || (_ignore_local && pkt.sender == pkt.receiver)
            // Data to/from MCP: admin traffic, don't account
         || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores())
         || (pkt.sender >= (core_id_t) Config::getSingleton()->getApplicationCores())
         || (pkt.receiver >= (core_id_t) Config::getSingleton()->getApplicationCores())
      )
      return false;
   else
      return true;
}

void NetworkModelBus::outputSummary(std::ostream &out)
{
   if (getNetwork()->getCore()->getId() == Config::getSingleton()->getMCPCoreNum()) {
      out << "    num packets received: " << _bus->_num_packets << std::endl;
      out << "    num bytes received: " << _bus->_num_bytes << std::endl;
      if (_bus->_num_packets == 0)
         out << "    average delay: inf" << std::endl;
      else
         out << "    average delay: " << (_bus->_total_delay / _bus->_num_packets) << std::endl;
   } else {
      out << "    num packets received: " << "-->" << std::endl;
      out << "    num bytes received: " << "-->" << std::endl;
      out << "    average delay: " << "-->" << std::endl;
   }
}
