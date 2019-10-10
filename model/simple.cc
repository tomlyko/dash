/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
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
 */

#include "simple.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SimpleAlgo");

NS_OBJECT_ENSURE_REGISTERED (SimpleAlgo);

SimpleAlgo::SimpleAlgo (  const videoData &videoData,
                                      const playbackData & playbackData,
                                      const bufferData & bufferData,
                                      const throughputData & throughput) :
  AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
  m_highestRepIndex (videoData.averageBitrate.size () - 1)
{
  NS_LOG_INFO (this);
  NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be >= 0");
}

algorithmReply
SimpleAlgo::GetNextRep ( const int64_t segmentCounter, int64_t clientId)
{
	if(segmentCounter == 0) { m_lastRepIndex=0; }

  int64_t decisionCase = 0;
  int64_t delayDecision = 0;
  int64_t nextRepIndex = 0;
  int64_t bDelay = 0;

  const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
	double nextHighestRepBitrate;
	if(m_lastRepIndex == m_highestRepIndex) {
		nextHighestRepBitrate = (m_videoData.averageBitrate.at (m_lastRepIndex));
	} else {
		nextHighestRepBitrate = (m_videoData.averageBitrate.at (m_lastRepIndex + 1));
	}
	double currentRepBitrate;
	currentRepBitrate = (m_videoData.averageBitrate.at (m_lastRepIndex));
  	if(segmentCounter > 4) {
  	  double averageSegmentThroughput = AverageSegmentThroughput(segmentCounter);
	  if (m_lastRepIndex < m_highestRepIndex && (nextHighestRepBitrate <= (1 * averageSegmentThroughput))) {
		nextRepIndex = m_lastRepIndex + 1;
		decisionCase = 2; //increase
	  } else if (m_lastRepIndex > 0 && (averageSegmentThroughput <= currentRepBitrate)) {
		nextRepIndex = m_lastRepIndex - 1;
		decisionCase = 3; //decrease
	  } else {
		nextRepIndex = m_lastRepIndex;
		decisionCase = 1; //stay the same
	  }
	} else {
		nextRepIndex = 0;
		decisionCase = 0; //start up
	}

  m_lastRepIndex = nextRepIndex;
  algorithmReply answer;
  answer.nextRepIndex = nextRepIndex;
  answer.nextDownloadDelay = bDelay;
  answer.decisionTime = timeNow;
  answer.decisionCase = decisionCase;
  answer.delayDecisionCase = delayDecision;
  return answer;
}

double
SimpleAlgo::AverageSegmentThroughput (int64_t currentSegment)
{

  double lengthOfInterval;
  double sumThroughput = 0.0;
  double transmissionTime = 0.0;
	
  for(int index=currentSegment-5; index<currentSegment; index++)
  {
      lengthOfInterval = (m_throughput.transmissionEnd.at (index) - m_throughput.transmissionStart.at (index))/(double)1000000;
	  sumThroughput += m_throughput.bytesReceived.at(index);
	  transmissionTime += lengthOfInterval;
  }
	
  return (sumThroughput*8 / (double)transmissionTime);

}
} // namespace ns3

