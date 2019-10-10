/**

  Bola ABR (based on dash.js implementation)

**/

#include "bola.h"
#include <math.h>

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("BolaAlgo");
  NS_OBJECT_ENSURE_REGISTERED (BolaAlgo);

  BolaAlgo::BolaAlgo (const videoData &videoData, const playbackData & playbackData, const bufferData & bufferData, const throughputData & throughput) : AdaptationAlgorithm (videoData, playbackData, bufferData, throughput), m_highestRepIndex (videoData.averageBitrate.size () - 1) {
    NS_LOG_INFO (this);
    NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be >= 0");
  }

  algorithmReply BolaAlgo::GetNextRep ( const int64_t segmentCounter, int64_t clientId) {

    int64_t decisionCase = 0;
    int64_t delayDecision = 0;
    int64_t nextRepIndex = 0;
    int64_t bDelay = 0;
    const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();

    if(segmentCounter == 0) {  
	    segDuration = m_videoData.segmentDuration;
      m_lastRepIndex = nextRepIndex;
      algorithmReply answer;
      answer.nextRepIndex = nextRepIndex;
      answer.nextDownloadDelay = bDelay;
      answer.decisionTime = timeNow;
      answer.decisionCase = decisionCase;
      answer.delayDecisionCase = delayDecision;
	  answer.bandwidthEstimate = 0;
      return answer;
    }
   
    if(state == BOLA_STATE_INIT) {
      
      for(int i=0; i<=m_highestRepIndex; i++) {
        bitrates[i] = m_videoData.averageBitrate.at(i)/1000;
      }
      
      for(int i=0; i<=m_highestRepIndex; i++) {
        utilities[i] = log(bitrates[i]);
      }
      
      for(int i=0; i<=m_highestRepIndex; i++) {
        utilities[i] = utilities[i] - utilities[0] + 1;
      }

      calculateBolaParameters();

      state = BOLA_STATE_STARTUP;
      

    }

    double throughput = AverageSegmentThroughput(segmentCounter)/1000;
    
    if(state == BOLA_STATE_STARTUP) {

        int quality = getQualityForBitrate(throughput);

        nextRepIndex = quality;
        double bufferLevel = (m_bufferData.bufferLevelNew.back ()/ (double)1000000 - (timeNow - m_bufferData.timeNow.back())/ (double)1000000);

        if (bufferLevel >= (segDuration/1000000)) {
            state = BOLA_STATE_STEADY;
        }

    } else if(state == BOLA_STATE_STEADY) {

        double bufferLevel = (m_bufferData.bufferLevelNew.back ()/ (double)1000000 - (timeNow - m_bufferData.timeNow.back())/ (double)1000000);
        int quality = getQualityFromBufferLevel(bufferLevel);

        int qualityForThroughput = getQualityForBitrate(throughput);
      
        if (quality > m_lastRepIndex && quality > qualityForThroughput) {
            quality = std::max(qualityForThroughput, (int)m_lastRepIndex);
        }

        double delayS = std::max((double)0, (double)bufferLevel - maxBufferLevelForQuality(quality));

        nextRepIndex = quality;
        bDelay = delayS;

    }


    m_lastRepIndex = nextRepIndex;
    algorithmReply answer;
    answer.nextRepIndex = nextRepIndex;
    answer.nextDownloadDelay = bDelay;
    answer.decisionTime = timeNow;
    answer.decisionCase = decisionCase;
    answer.delayDecisionCase = delayDecision;
	answer.bandwidthEstimate = throughput/1000;
    return answer;
  }

  double BolaAlgo::AverageSegmentThroughput (int64_t currentSegment) {

    double lengthOfInterval;
    double sumThroughput = 0.0;
    double transmissionTime = 0.0;

    int count = 0;
    for(int index=currentSegment-1; index>=0; index--)
    {
      lengthOfInterval = (m_throughput.transmissionEnd.at (index) - m_throughput.transmissionStart.at (index))/(double)1000000;
      sumThroughput += m_throughput.bytesReceived.at(index);
      transmissionTime += lengthOfInterval;
      count++;
      if(count > 4) break;
    }

    return (sumThroughput*8 / (double)transmissionTime);

  }

  void BolaAlgo::calculateBolaParameters() {
      
    int highestUtilityIndex = 0;
    for(int i=0; i<=m_highestRepIndex; i++) {
      if(utilities[i] > utilities[highestUtilityIndex]) {
        highestUtilityIndex = i;
      }
    }

    double bufferTime = std::max(STABLE_BUFFER, MINIMUM_BUFFER_S + MINIMUM_BUFFER_PER_BITRATE_LEVEL_S * (int)(m_highestRepIndex+1));

    gp = (utilities[highestUtilityIndex] - 1) / (bufferTime / MINIMUM_BUFFER_S - 1);
    Vp = MINIMUM_BUFFER_S / gp;

  }
  
  int BolaAlgo::getQualityFromBufferLevel(double bufferLevel) {
    int quality = -1;
    double score = 0;
    for (int i = 0; i < (m_highestRepIndex+1); ++i) {
        double s = (Vp * (utilities[i] + gp) - bufferLevel) / bitrates[i];
        if (quality == -1 || s >= score) {
            score = s;
            quality = i;
        }
    }
    return quality;
  }

  double BolaAlgo::maxBufferLevelForQuality(int quality) {
    return Vp * (utilities[quality] + gp);
  }

  double BolaAlgo::minBufferLevelForQuality(int quality) {
    double qBitrate = bitrates[quality];
    double qUtility = utilities[quality];

    double min = 0;
    for (int i = quality - 1; i >= 0; --i) {
        if (utilities[i] < utilities[quality]) {
            double iBitrate = bitrates[i];
            double iUtility = utilities[i];

            double level = Vp * (gp + (qBitrate * iUtility - iBitrate * qUtility) / (qBitrate - iBitrate));
            min = std::max(min, level);
        }
    }
    return min;
  }
  
  int BolaAlgo::getQualityForBitrate(double bitrate) {
    int quality = m_highestRepIndex;
    for(int i=0; i<=m_highestRepIndex; i++) {
      if(bitrate <= bitrates[i]) {
        quality = i-1;
        break;
      }
    }
    if(std::isnan(bitrate)) { quality = 0; }
    return quality;
  }
  
} // namespace ns3
