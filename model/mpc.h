#ifndef MPC_ALGORITHM_H
#define MPC_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

class MPCAlgo : public AdaptationAlgorithm
{
public:
  MPCAlgo (  const videoData &videoData,
                      const playbackData & playbackData,
                      const bufferData & bufferData,
                      const throughputData & throughput);

  algorithmReply GetNextRep ( const int64_t segmentCounter, int64_t clientId);

private:
  /**
   * \brief Average segment throughput during the time interval [t1, t2]
   */
  double AverageSegmentThroughput (int64_t currentSegment);

  const int64_t m_highestRepIndex;
  int64_t m_lastRepIndex;
  
  std::list<double>  past_errors;
  std::list<double>  past_bandwidth_ests;
  
  float REBUF_PENALTY = 7; //default: balanced
  float SMOOTH_PENALTY = 1;
  
  uint64_t segDuration;
  
};
} // namespace ns3
#endif /* MPC_ALGORITHM_H */