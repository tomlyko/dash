#ifndef BOLA_ALGORITHM_H
#define BOLA_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

class BolaAlgo : public AdaptationAlgorithm
{
public:
  BolaAlgo (  const videoData &videoData,
                      const playbackData & playbackData,
                      const bufferData & bufferData,
                      const throughputData & throughput);

  algorithmReply GetNextRep ( const int64_t segmentCounter, int64_t clientId);

private:

  const int64_t m_highestRepIndex;
  int64_t m_lastRepIndex;
  
  const int BOLA_STATE_INIT = -1;
  const int BOLA_STATE_STARTUP = 0;
  const int BOLA_STATE_STEADY = 1;
  int state = -1;
  const int MINIMUM_BUFFER_S = 1;
  const int MINIMUM_BUFFER_PER_BITRATE_LEVEL_S = 1;
  const int STABLE_BUFFER = 2;
  
  double Vp = 0;
  double gp = 0;
  
  double utilities [10];
  double bitrates [10];
  
  double AverageSegmentThroughput (int64_t currentSegment);
  void calculateBolaParameters();
  int getQualityFromBufferLevel(double bufferLevel);
  double maxBufferLevelForQuality(int quality);
  double minBufferLevelForQuality(int quality);
  int getQualityForBitrate(double bitrate);
  
  uint64_t segDuration;
};
} // namespace ns3
#endif /* BOLA_ALGORITHM_H */