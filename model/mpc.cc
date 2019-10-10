/**

MPC ABR (based on penseive implementation)

**/

#include "mpc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MPCAlgo");
NS_OBJECT_ENSURE_REGISTERED (MPCAlgo);

MPCAlgo::MPCAlgo (const videoData &videoData, const playbackData & playbackData, const bufferData & bufferData, const throughputData & throughput) : AdaptationAlgorithm (videoData, playbackData, bufferData, throughput), m_highestRepIndex (videoData.averageBitrate.size () - 1)
{
  NS_LOG_INFO (this);
  NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be >= 0");
}

algorithmReply MPCAlgo::GetNextRep ( const int64_t segmentCounter, int64_t clientId)
{
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
	
	// Calculate error
	double curr_error = 0;
	if ( past_bandwidth_ests.size() > 0 ) {
		double lastEstimate = past_bandwidth_ests.front();
		double lastBandwidth = (m_throughput.bytesReceived.at(segmentCounter-1)*8 / ((m_throughput.transmissionEnd.at (segmentCounter-1) - m_throughput.transmissionStart.at (segmentCounter-1))/(double)1000000));
		curr_error = abs((lastEstimate - lastBandwidth) / (double)lastBandwidth);
	}
	past_errors.push_front(curr_error);
	
	// pick bitrate according to MPC           
	// first get harmonic mean of last 5 bandwidths
	// compute throughput estimation
	std::vector<double> thrptEstimationTmp;
	for (unsigned sd = m_playbackData.playbackIndex.size (); sd-- > 0; ) {
		if (m_throughput.bytesReceived.at (sd) == 0) {
			continue;
		} else {
			thrptEstimationTmp.push_back ((8.0 * m_throughput.bytesReceived.at (sd)) / ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0)));
		}
		if (thrptEstimationTmp.size () == 5) {
			break;
		}
	}

	double harmonicMeanDenominator = 0;
	for (uint i = 0; i < thrptEstimationTmp.size (); i++) {
		harmonicMeanDenominator += 1 / (thrptEstimationTmp.at (i));
	}
	double harmonic_bandwidth = thrptEstimationTmp.size () / harmonicMeanDenominator;				

	// future bandwidth prediction
	// divide by 1 + max of last 5 (or up to 5) errors
	double max_error = 0;
	int count = 0;
	std::list <double> :: iterator it;
	for(it = past_errors.begin(); it != past_errors.end(); ++it) {
		if(*it > max_error) {
			max_error = *it; 
		}
		count++;
		if(count > 4) { break; }
	}
	double future_bandwidth = harmonic_bandwidth/(1+max_error);
	past_bandwidth_ests.push_front(harmonic_bandwidth);

	double max_reward = -100000000;
	double start_buffer = (m_bufferData.bufferLevelNew.back ()/ (double)1000000 - (timeNow - m_bufferData.timeNow.back())/ (double)1000000);
	nextRepIndex = (int)m_lastRepIndex;
	int possibleCombos = (int)std::pow(5.0, m_highestRepIndex+1);
	int combos[possibleCombos+1][5];
	count = 0;
	for(int i=0; i<=m_highestRepIndex; i++) {
		for(int j=0; j<=m_highestRepIndex; j++) {
			for(int k=0; k<=m_highestRepIndex; k++) {
				for(int l=0; l<=m_highestRepIndex; l++) {
					for(int m=0; m<=m_highestRepIndex; m++) {
						combos[count][0] = i;
						combos[count][1] = j;
						combos[count][2] = k;
						combos[count][3] = l;
						combos[count][4] = m;
						count++;
					}
				}
			}
		}
	}
	
	for(int i=0; i<possibleCombos; i++) {
		double curr_rebuffer_time = 0;
		double a = (double)segDuration/1000000;
		double curr_buffer = start_buffer-a;
		double bitrate_sum = 0;
		double smoothness_diffs = 0;
		int last_quality = (int)m_lastRepIndex;
		for(int j=0; j<5; j++) {
			int chunk_quality = combos[i][j];
			double download_time = (m_videoData.averageBitrate.at(chunk_quality) * a) / future_bandwidth;
			if ( curr_buffer < download_time ) {
				curr_rebuffer_time += (download_time - curr_buffer);
				curr_buffer = 0;
			} else {
				curr_buffer -= download_time;
			}
			curr_buffer += a;
			bitrate_sum += m_videoData.averageBitrate.at(chunk_quality)/1000;
			smoothness_diffs += abs((m_videoData.averageBitrate.at(chunk_quality)/1000) - (m_videoData.averageBitrate.at(last_quality)/1000));
			last_quality = chunk_quality;
		}
				
		double reward = (bitrate_sum/1000) - (REBUF_PENALTY*curr_rebuffer_time) - (SMOOTH_PENALTY*smoothness_diffs/1000);
		

		if ( reward >= max_reward ) {
			max_reward = reward;
			nextRepIndex = combos[i][0];
		}
		
	}

	m_lastRepIndex = nextRepIndex;
	algorithmReply answer;
	answer.nextRepIndex = nextRepIndex;
	answer.nextDownloadDelay = bDelay;
	answer.decisionTime = timeNow;
	answer.decisionCase = decisionCase;
	answer.delayDecisionCase = delayDecision;
	answer.bandwidthEstimate = future_bandwidth/(double)1000000;
	return answer;
}

} // namespace ns3
