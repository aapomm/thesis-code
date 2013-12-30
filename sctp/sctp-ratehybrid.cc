#ifndef lint
static const char rcsid[] =
"@(#) $Header: /cvsroot/nsnam/ns-2/sctp/sctp-ratehybrid.cc,v 1.5 2013/12/10 05:51:27 aaron_manaloto Exp $ (UD/PEL)";
#endif

#include "ip.h"
#include "sctp-ratehybrid.h"
#include "flags.h"
#include "formula.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define	MIN(x,y)	(((x)<(y))?(x):(y))
#define	MAX(x,y)	(((x)>(y))?(x):(y))

static class SctpRateHybridClass : public TclClass 
{ 
public:
  SctpRateHybridClass() : TclClass("Agent/SCTP/Ratehybrid") {}
  TclObject* create(int, const char*const*) 
  {
    return (new SctpRateHybrid());
  }
} classSctpRateHybrid;

SendTimer::SendTimer(SctpRateHybrid *agent) : TimerHandler(){
	agent_ = agent;
}

SctpRateHybrid::SctpRateHybrid() : SctpAgent()
{
	bind("packetSize_", &size_);
	bind("rate_", &rate_);
	bind("df_", &df_);
	bind("tcp_tick_", &tcp_tick_);
	bind("true_loss_rate_", &true_loss_rate_);
	bind("srtt_init_", &srtt_init_);
	bind("rttvar_init_", &rttvar_init_);
	bind("rtxcur_init_", &rtxcur_init_);
	bind("rttvar_exp_", &rttvar_exp_);
	bind("ssmult_", &ssmult_);
	bind("T_SRTT_BITS", &T_SRTT_BITS);
	bind("T_RTTVAR_BITS", &T_RTTVAR_BITS);
	bind("bval_", &bval_);
	bind_bool("conservative_", &conservative_);
	bind("ca_", &ca_);
	bind("minrto_", &minrto_);
	bind("maxHeavyRounds_", &maxHeavyRounds_);
	bind("scmult_", &scmult_);
//	bind("standard_", &standard_);
	bind("fsize_", &fsize_);
	bind("overhead_", &overhead_);
	bind_bool("slow_increase_", &slow_increase_);
	bind_bool("idleFix_", &idleFix_);

	// printf("isHEADEMPTY? %d\n", pktQ_.spHead == NULL);
	// printf("isTAILEMPTY? %d\n", pktQ_.spTail == NULL);
	// printf("length? %d\n", pktQ_.uiLength);

 	total = 0;

 	//test initialization
	// seqno_=0;				
 	// rate_ = InitRate_;
 	rate_ = 100000.0;
	oldrate_ = rate_;  
	rate_change_ = SLOW_START;
	UrgentFlag = 1;
	rtt_=0;	 
	sqrtrtt_=1;
	rttcur_=1;
	tzero_ = 0;
	last_change_=0;	
	maxrate_ = 0; 
	ss_maxrate_ = 0;
	// ndatapack_=0;
	// ndatabytes_ = 0;
	true_loss_rate_ = 0;
	// active_ = 1; 
	round_id = 0;
	heavyrounds_ = 0;
	t_srtt_ = int(srtt_init_/tcp_tick_) << T_SRTT_BITS;
	t_rttvar_ = int(rttvar_init_/tcp_tick_) << T_RTTVAR_BITS;
	t_rtxcur_ = rtxcur_init_;
	rcvrate = 0 ;
	// all_idle_ = 0;

	first_pkt_rcvd = 0 ;
	delta_ = 0;
	sendBufferIndex = 0;

	timer_send = new SendTimer(this);

	// printf("size_: %d, rate_: %lf, initrate_: %lf\n", size_, rate_, size_/rate_);
	size_ = 1460;
	timer_send->sched(size_/rate_);
	sendData_ = 0;
	firstSend = 0;
	noRtt = true;

	p_ = -1;
}

SctpRateHybrid::~SctpRateHybrid(){
	cancelTimer();

	delete timer_send;
}

void SctpRateHybrid::delay_bind_init_all()
{
  SctpAgent::delay_bind_init_all();
}

int SctpRateHybrid::delay_bind_dispatch(const char *cpVarName, 
					  const char *cpLocalName, 
					  TclObject *opTracer)
{
  return SctpAgent::delay_bind_dispatch(cpVarName, cpLocalName, opTracer);
}

void SctpRateHybrid::SendMuch()
{
  u_char *ucpOutData = new u_char[uiMaxPayloadSize];
  int iOutDataSize = 0;
  double dCurrTime = Scheduler::instance().clock();

  u_int uiChunkSize = 0;
  sendData_ = 1;
  while((spNewTxDest->iOutstandingBytes < spNewTxDest->iCwnd) &&
	(eDataSource == DATA_SOURCE_INFINITE || sAppLayerBuffer.uiLength != 0))
    {
    	// printf("iobytes: %d < %d\n", spNewTxDest->iOutstandingBytes, spNewTxDest->iCwnd);
    	// if(spNewTxDest->iOutstandingBytes == 65024 && spNewTxDest->iCwnd == 66060){
    	// 	break;
    	// }
      uiChunkSize = GetNextDataChunkSize(); 
      if(uiChunkSize <= uiPeerRwnd)
	{
	  if(eUseMaxBurst == MAX_BURST_USAGE_ON)
	    if( (eApplyMaxBurst == TRUE) && (uiBurstLength++ >= MAX_BURST) )
	      {
		/* we've reached Max.Burst limit, so jump out of loop
		 */
		eApplyMaxBurst = FALSE;// reset before jumping out of loop
		break;
	      }

	  /* PN: 5/2007. Simulating Sendwindow */ 
	  if (uiInitialSwnd)
	  {
	    if (uiChunkSize > uiAvailSwnd) 
	    {
	      break; // from loop
	    }
	  }

	  memset(ucpOutData, 0, uiMaxPayloadSize); // reset
	  iOutDataSize = BundleControlChunks(ucpOutData);
 		iOutDataSize += GenMultipleDataChunks(ucpOutData+iOutDataSize, 0);
	  	SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
	}
      else if(TotalOutstanding() == 0)  // section 6.1.A
	{
	  /* probe for a change in peer's rwnd
	   */
	  memset(ucpOutData, 0, uiMaxPayloadSize);
	  iOutDataSize = BundleControlChunks(ucpOutData);
	  iOutDataSize += GenOneDataChunk(ucpOutData+iOutDataSize);
	  SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
	}
      else
	{
		break;
	}
	// getchar();
    }



  if(iOutDataSize > 0)  // did we send anything??
    {
      spNewTxDest->opCwndDegradeTimer->resched(spNewTxDest->dRto);
      if(uiHeartbeatInterval != 0)
	{
	  spNewTxDest->dIdleSince = dCurrTime;
	}
    }

  /* this reset MUST happen at the end of the function, because the burst 
   * measurement is carried over from RtxMarkedChunks() if it was called.
   */
  uiBurstLength = 0;
  sendData_ = 0;
  delete [] ucpOutData;
}

void SctpRateHybrid::SendPacket(u_char *ucpData, int iDataSize, SctpDest_S *spDest)
{
  Node_S *spNewNode = NULL;
  Packet *opPacket = NULL;
  PacketData *opPacketData = NULL;

  SetSource(spDest); // set src addr, port, target based on "routing table"
  SetDestination(spDest); // set dest addr & port 

  opPacket = allocpkt();
  opPacketData = new PacketData(iDataSize);
  memcpy(opPacketData->data(), ucpData, iDataSize);
  opPacket->setdata(opPacketData);
  hdr_cmn::access(opPacket)->size() = iDataSize + SCTP_HDR_SIZE+uiIpHeaderSize;

  hdr_sctp *sctph = hdr_sctp::access(opPacket);
  sctph->NumChunks() = uiNumChunks;
  sctph->SctpTrace() = new SctpTrace_S[uiNumChunks];
  memcpy(sctph->SctpTrace(), spSctpTrace, 
	 (uiNumChunks * sizeof(SctpTrace_S)) );

  uiNumChunks = 0; // reset the counter

// printf("time: %lf\n", Scheduler::instance().clock());
  // if(askPerm()){
  // if(timer_send->status() == TIMER_IDLE){
  // 	timer_send->sched(snd_rate);
  // }

	  if(dRouteCalcDelay == 0){
	  		addToList(opPacket);
	      // send(opPacket, 0);
	      // timer_send->resched(snd_rate);
	  }
	  else{
	  	if(spDest->eRouteCached == TRUE){
		  spDest->opRouteCacheFlushTimer->resched(dRouteCacheLifetime);
		  addToList(opPacket);
		  // send(opPacket, 0);
	  	//   timer_send->resched(snd_rate);
		}
	    else{
		  spNewNode = new Node_S;
		  spNewNode->eType = NODE_TYPE_PACKET_BUFFER;
		  spNewNode->vpData = opPacket;
		  InsertNode(&spDest->sBufferedPackets,spDest->sBufferedPackets.spTail,
			     spNewNode, NULL);

		  if(spDest->opRouteCalcDelayTimer->status() != TIMER_PENDING)
		    spDest->opRouteCalcDelayTimer->sched(dRouteCalcDelay);
		}
	  }
	// }
	// else{
	// 	Packet::free(opPacket);
	// 	total += 1;
	// 	printf("total: %d\n", total);
	// }
	  // printf("send!\n");
	  // getchar();
}

bool SctpRateHybrid::askPerm(){
	return (timer_send->status()!=TIMER_PENDING);
}

void SctpRateHybrid::cancelTimer(){
	timer_send->force_cancel();
}

void SendTimer::expire(Event*){
	agent_->nextpkt();
}

void SctpRateHybrid::addToList(Packet *p){
	Node_S *spNewNode= new Node_S;
	spNewNode->vpData = p;

	if(pktQ_.spTail == NULL)
		pktQ_.spHead = spNewNode;
	else
		pktQ_.spTail->spNext = spNewNode;

	spNewNode->spPrev = pktQ_.spTail;
	spNewNode->spNext = NULL;

	pktQ_.spTail = spNewNode;

	pktQ_.uiLength++;

  // printf("add! length: %d time: %lf\n", pktQ_.uiLength, Scheduler::instance().clock());
}

void SctpRateHybrid::recv(Packet *opInPkt, Handler*){
  	if(eState == SCTP_STATE_UNINITIALIZED)
    	Reset(); 

  	hdr_ip *spIpHdr = hdr_ip::access(opInPkt);
	PacketData *opInPacketData = (PacketData *) opInPkt->userdata();
	u_char *ucpInData = opInPacketData->data();
	u_char *ucpCurrInChunk = ucpInData;
	int iRemainingDataLen = opInPacketData->size();

  	u_char *ucpOutData = new u_char[uiMaxPayloadSize];
	u_char *ucpCurrOutData = ucpOutData;

 	int iOutDataSize = 0; 

	memset(ucpOutData, 0, uiMaxPayloadSize);
 	memset(spSctpTrace, 0,
	 (uiMaxPayloadSize / sizeof(SctpChunkHdr_S)) * sizeof(SctpTrace_S) );

 	spReplyDest = GetReplyDestination(spIpHdr);

 	eStartOfPacket = TRUE;

 	if (noRtt) {
 		rtt_ = Scheduler::instance().clock() - firstSend;
		printf("%lf\n", rtt_);
 		//noRtt = false;
 	}

  if(hdr_sctp::access(opInPkt)->NumChunks() > 0)
  {
    do{
      iOutDataSize += ProcessChunk(ucpCurrInChunk, &ucpCurrOutData);
      NextChunk(&ucpCurrInChunk, &iRemainingDataLen);
    }
    while(ucpCurrInChunk != NULL);
  }

 	if(iOutDataSize > 0) {
    	SendPacket(ucpOutData, iOutDataSize, spReplyDest);
    }

 	if(eSackChunkNeeded == TRUE){
    	memset(ucpOutData, 0, uiMaxPayloadSize);
    	iOutDataSize = BundleControlChunks(ucpOutData);
    	if (eUseNonRenegSacks == TRUE) {
			iOutDataSize += GenChunk(SCTP_CHUNK_NRSACK, ucpOutData+iOutDataSize);
		}
     	else {
	  		iOutDataSize += GenChunk(SCTP_CHUNK_SACK, ucpOutData+iOutDataSize);
		}

     	SendPacket(ucpOutData, iOutDataSize, spReplyDest);

    	if (eUseNonRenegSacks == TRUE) {
	  		//DEBUG FUNCTION
		}
     	else {
	  		//DEBUG FUNCTION
		}

     	eSackChunkNeeded = FALSE;  // reset AFTER sent (o/w breaks dependencies)
	}

	if(eForwardTsnNeeded == TRUE) {
     	memset(ucpOutData, 0, uiMaxPayloadSize);
     	iOutDataSize = BundleControlChunks(ucpOutData);
     	iOutDataSize += GenChunk(SCTP_CHUNK_FORWARD_TSN,ucpOutData+iOutDataSize);
     	SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
     	eForwardTsnNeeded = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

 	if(eSendNewDataChunks == TRUE && eMarkedChunksPending == FALSE) {
     	SendMuch();       // Send new data till our cwnd is full!
     	eSendNewDataChunks = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

	// print loss intervals
	/*for (int count = 0; count < 8; count++)
	{
		printf("%d ", lossIntervals[count]);
	}
	printf("\n");*/
	//FREE PACKET
 	delete hdr_sctp::access(opInPkt)->SctpTrace();
 	hdr_sctp::access(opInPkt)->SctpTrace() = NULL;
 	Packet::free(opInPkt);
 	opInPkt = NULL;
 	delete [] ucpOutData;
}

void SctpRateHybrid::TfrcUpdate(u_char *ucpInChunk, double p){
    /* 
   		TFRC INTEGRATION
    */
  double now = Scheduler::instance().clock();
  double flost = 0;
  if (ucpInChunk != NULL && p < 0) {
		//Extract SCTP headers for TFRC calculation
		SctpTfrcAckChunk_S *nck = (SctpTfrcAckChunk_S *) ucpInChunk;
		//double ts = nck->timestamp_echo;
		double ts = nck->timestamp_echo + nck->timestamp_offset;
		double rate_since_last_report = nck->rate_since_last_report;
		// printf("tfrc rslr: %lf\n", rate_since_last_report);
		// double NumFeedback_ = nck->NumFeedback_;
		flost = nck->flost; 
		int losses = nck->losses;
		true_loss_rate_ = nck->true_loss;

		round_id ++;

		if (round_id > 1 && rate_since_last_report > 0) {
			/* compute the max rate for slow-start as two times rcv rate */ 
			ss_maxrate_ = 2*rate_since_last_report*size_;
			// printf("ratehybrid ss_maxrate_: %lf\n", ss_maxrate_);
			if (conservative_) { 
				if (losses >= 1) {
					/* there was a loss in the most recent RTT */
					if (debug_) printf("time: %5.2f losses: %d rate %5.2f\n", 
					  now, losses, rate_since_last_report);
					maxrate_ = rate_since_last_report*size_;
				} else { 
					/* there was no loss in the most recent RTT */
					maxrate_ = scmult_*rate_since_last_report*size_;
				}
				// if (debug_) printf("time: %5.2f losses: %d rate %5.2f maxrate: %5.2f\n", now, losses, rate_since_last_report, maxrate_);
			} else 
				maxrate_ = 2*rate_since_last_report*size_;
		} else {
			ss_maxrate_ = 0;
			maxrate_ = 0; 
		}

		/* update the round trip time */
		// if(ts == 0){
		// 	ts = now - 0.04;
		// }
		// printf("ratehybrid ts: %lf\n", ts);
		// getchar();
		update_rtt (ts, now);
  }
  else if (ucpInChunk == NULL && p > 0) {
  	flost = p;
  	p = -1;
  }


	rcvrate = p_to_b(flost, rtt_, tzero_, fsize_, bval_);
  if(rate_ == 0.00) rate_ = 1.00;
	
	/* if we are in slow start and we just saw a loss */
	/* then come out of slow start */
	if (first_pkt_rcvd == 0) {
		first_pkt_rcvd = 1 ; 
		slowstart();
		nextpkt();
	}
	else {
		if (rate_change_ == SLOW_START) {
			if (flost > 0) {
				rate_change_ = OUT_OF_SLOW_START;
				oldrate_ = rate_ = rcvrate;
			}
			else {
				slowstart();
				nextpkt();
			}
		}
		else {
			if (rcvrate>rate_)	{
				increase_rate(flost);
			}
			else 
				decrease_rate ();		
		}
	}
	// bool printStatus_ = true;
	if (printStatus_) {
		printf("time: %5.2f rate: %5.2f\n", now, rate_);
		double packetrate = rate_ * rtt_ / uiMaxPayloadSize;
		printf("time: %5.2f packetrate: %5.2f\n", now, packetrate);
		double maxrate = maxrate_ * rtt_ / uiMaxPayloadSize;
		printf("time: %5.2f maxrate: %5.2f\n", now, maxrate);
	}
}

void SctpRateHybrid::update_rtt(double tao, double now){

	t_rtt_ = int((now-tao) /tcp_tick_ + 0.5);
	if (t_rtt_==0) t_rtt_=1;
	if (t_srtt_ != 0) {
		register short rtt_delta;
		rtt_delta = t_rtt_ - (t_srtt_ >> T_SRTT_BITS);    
		if ((t_srtt_ += rtt_delta) <= 0)    
			t_srtt_ = 1;
		if (rtt_delta < 0)
			rtt_delta = -rtt_delta;
	  	rtt_delta -= (t_rttvar_ >> T_RTTVAR_BITS);
	  	if ((t_rttvar_ += rtt_delta) <= 0)  
			t_rttvar_ = 1;
	} else {
		t_srtt_ = t_rtt_ << T_SRTT_BITS;		
		t_rttvar_ = t_rtt_ << (T_RTTVAR_BITS-1);	
	}
	t_rtxcur_ = (((t_rttvar_ << (rttvar_exp_ + (T_SRTT_BITS - T_RTTVAR_BITS))) + t_srtt_)  >> T_SRTT_BITS ) * tcp_tick_;
	tzero_=t_rtxcur_;
 	if (tzero_ < minrto_) 
  		tzero_ = minrto_;

	/* fine grained RTT estimate for use in the equation */
	if (rtt_ > 0) {
		rtt_ = df_*rtt_ + ((1-df_)*(now - tao));
		sqrtrtt_ = df_*sqrtrtt_ + ((1-df_)*sqrt(now - tao));
	} else {
		rtt_ = now - tao;
		sqrtrtt_ = sqrt(now - tao);
	}
	// printf("ratehybrid rttcur: %lf = %lf - %lf\n", rttcur_, now, tao);
	rttcur_ = now - tao;
}
void SctpRateHybrid::decrease_rate (){ 
	double now = Scheduler::instance().clock(); 
	rate_ = rcvrate;
	double maximumrate = (maxrate_>size_/rtt_)?maxrate_:size_/rtt_ ;

	// Allow sending rate to be greater than maximumrate
	//   (which is by default twice the receiving rate)
	//   for at most maxHeavyRounds_ rounds.
	if (rate_ > maximumrate)
		heavyrounds_++;
	else
		heavyrounds_ = 0;
	if (heavyrounds_ > maxHeavyRounds_) {
		rate_ = (rate_ > maximumrate)?maximumrate:rate_ ;
	}

	rate_change_ = RATE_DECREASE;
	last_change_ = now;
	
	// if (printStatus_) {
	// 	double rate = rate_ * rtt_ / size_;
	// 	printf("Decrease: now: %5.2f rate: %5.2f rtt: %5.2f\n", now, rate, rtt_);
	// }
}

void SctpRateHybrid::increase_rate(double p){

    double now = Scheduler::instance().clock();
    double maximumrate;

	double mult = (now-last_change_)/rtt_ ;
	if (mult > 2) mult = 2 ;

	rate_ = rate_ + (size_/rtt_)*mult ;
	if (datalimited_ || lastlimited_ > now - 1.5*rtt_) {
		// Modified by Sally on 3/10/2006
		// If the sender has been datalimited, rate should be
		//   at least the initial rate, when increasing rate.
		double init_rate = initial_rate()*size_/rtt_;
	   	maximumrate = (maxrate_>init_rate)?maxrate_:init_rate ;
        } else {
	   	maximumrate = (maxrate_>size_/rtt_)?maxrate_:size_/rtt_ ;
	}
	maximumrate = (maximumrate>rcvrate)?rcvrate:maximumrate;
	rate_ = (rate_ > maximumrate)?maximumrate:rate_ ;
	
    rate_change_ = CONG_AVOID;  
    last_change_ = now;
	heavyrounds_ = 0;
}

double SctpRateHybrid::initial_rate(){
	//defualt value of rate_init_option is 2
	//so, return(rfc3390(size_))

	//default value of size_==0?
	//so
	return (rfc3390(uiMaxPayloadSize));
}

double SctpRateHybrid::rfc3390(int size)
{
        if (size <= 1095) {
                return (4.0);
        } else if (size < 2190) {
                return (3.0);
        } else {
                return (2.0);
        }
}

void SctpRateHybrid::reduce_rate_on_no_feedback()
{
	// double now = Scheduler::instance().clock();
	// Assumption: Datalimited and/or all_idle_
	// and use RFC 3390
	//rtt_ = spDest->dSrtt;
	if(rate_change_ != SLOW_START)
		rate_change_ = RATE_DECREASE;
  if (rate_ > 2.0 * rfc3390(uiMaxPayloadSize) * uiMaxPayloadSize/rtt_ ) {
          rate_*=0.5;
  } else if ( rate_ > rfc3390(uiMaxPayloadSize) * uiMaxPayloadSize/rtt_ ) {
          rate_ = rfc3390(uiMaxPayloadSize) * uiMaxPayloadSize/rtt_;
  }	
}

void SctpRateHybrid::nextpkt(){
	double next = -1;
	double xrate = -1;
	// printf("sendtimer expire!\n");

	//DEQUEUE	
	if(pktQ_.spHead == NULL){
		timer_send->resched(size_/rate_);
		// printf("sendtimer expired! time: %lf\n", Scheduler::instance().clock());
		// getchar();
		return;
	}

	Node_S *node = pktQ_.spHead;

	if(node->spNext != NULL){ 
		pktQ_.spHead = node->spNext; 
	} 
	else {
		pktQ_.spHead = NULL;
		pktQ_.spTail = NULL;
	}

	SctpSendBufferNode_S *spCurrNodeData = NULL;

	// assign timestamp
	spCurrNodeData = (SctpSendBufferNode_S *) node->vpData;
	spCurrNodeData->dTxTimestamp = spCurrNodeData->dTxTimestamp != 0 ? Scheduler::instance().clock() : 0;

	if (eState == SCTP_STATE_ESTABLISHED) {
		/* Get TFRC chunk */
		Packet *pktToSend = (Packet *) node->vpData;
		PacketData *pktToSendData = (PacketData*) pktToSend->userdata(); 
		SctpTfrcChunk_S *firstChunk = (SctpTfrcChunk_S *) pktToSendData->data(); // TFRC chunk is always the 1st chunk

		if(noRtt){
			rtt_ = 0.209112;
			noRtt = false;
		}
		else
		{
			rtt_ = spReplyDest->dSrtt;
		}

		//printf("rtt_: %lf\n", rtt_);
		/* Add TFRC details */	
		firstChunk->timestamp = Scheduler::instance().clock();
		//printf("ts: %lf\n", firstChunk->timestamp);
		firstChunk->rtt = rtt_;
		firstChunk->seqno = ++seqno_;
		firstChunk->tzero=tzero_;
		firstChunk->rate=rate_;
		firstChunk->psize=size_;
		firstChunk->fsize=fsize_;
		firstChunk->UrgentFlag=UrgentFlag;
		firstChunk->round_id=round_id;
	}

	send((Packet *)(node->vpData),0);
	// If slow_increase_ is set, then during slow start, we increase rate
	// slowly - by amount delta per packet 
	// printf("ratehybrid values: %d, %d, %d, %lf, %lf\n", slow_increase_, round_id, rate_change_, oldrate_, rate_);
	if (slow_increase_ && round_id > 2 && (rate_change_ == SLOW_START) 
		       && (oldrate_+SMALLFLOAT< rate_)) {
		oldrate_ = oldrate_ + delta_;
		xrate = oldrate_;
		// printf("ratehybrid xrate: %lf\n", xrate);
	} else {
		if (ca_) {
			if (debug_) printf("SQRT: now: %5.2f factor: %5.2f\n", Scheduler::instance().clock(), sqrtrtt_/sqrt(rttcur_));
			// printf("ratehybrid sqrtrtt: %lf, %lf, %lf\n", rate_, sqrtrtt_, rttcur_);
			// getchar();
			xrate = rate_ * sqrtrtt_/sqrt(rttcur_);
		} else
			xrate = rate_;
	}
	// printf("ratehybrid xrate: %lf\n", xrate);
	// getchar();
	if (xrate > SMALLFLOAT) {
		next = uiMaxPayloadSize/xrate;
		//
		// randomize between next*(1 +/- woverhead_) 
		//
		// printf("ratehybrid next: %d, %lf, %lf\n", uiMaxPayloadSize, xrate, next);
		next = next*(2*overhead_*Random::uniform()-overhead_+1);
		if (next > SMALLFLOAT)
			timer_send->resched(next);
        else 
			timer_send->resched(SMALLFLOAT);
	}
	else{
		timer_send->resched(uiMaxPayloadSize/rate_);
	}

	// delete (Packet *) node->vpData;
	node->vpData = NULL;
	delete node;

	pktQ_.uiLength--;
	// printf("length: %d time: %lf\n", pktQ_.uiLength, Scheduler::instance().clock());
}

void SctpRateHybrid::slowstart () 
{
	double now = Scheduler::instance().clock(); 
	double initrate = initial_rate()*size_/rtt_;
	// printf("%5.2f, %d, %lf\n", initial_rate(), size_, rtt_);
	// If slow_increase_ is set to true, delta is used so that 
	//  the rate increases slowly to new value over an RTT. 
	if (debug_) printf("SlowStart: round_id: %d rate: %7.2f ss_maxrate_: %5.2f\n", round_id, rate_, ss_maxrate_);
	if (round_id <=1 || (round_id == 2 && initial_rate() > 1)) {
		// We don't have a good rate report yet, so keep to  
		//   the initial rate.
		// printf("SlowStart: Keep to the initial rate, which is %7.2f\n", initrate);				     
		oldrate_ = rate_;
		if (rate_ < initrate) rate_ = initrate;
		delta_ = (rate_ - oldrate_)/(rate_*rtt_/size_);
		last_change_=now;
	} else if (ss_maxrate_ > 0) {
		// printf("SlowStart: Maxrate > 0\n");
		// printf("SS_MAXRATE %lf?\n", ss_maxrate_);
		if (idleFix_ && (datalimited_ || lastlimited_ > now - 1.5*rtt_)
			     && ss_maxrate_ < initrate) {
			// Datalimited recently, and maxrate is small.
			// Don't be limited by maxrate to less that initrate.
			oldrate_ = rate_;
			if (rate_ < initrate) rate_ = initrate;
			delta_ = (rate_ - oldrate_)/(rate_*rtt_/size_);
			last_change_=now;
		} else if (rate_ < ss_maxrate_ && 
		                    now - last_change_ > rtt_) {
			// Not limited by maxrate, and time to increase.
			// Multiply the rate by ssmult_, if maxrate allows.
			oldrate_ = rate_;
			if (ssmult_*rate_ > ss_maxrate_) 
				rate_ = ss_maxrate_;
			else rate_ = ssmult_*rate_;
			if (rate_ < size_/rtt_) 
				rate_ = size_/rtt_; 
			delta_ = (rate_ - oldrate_)/(rate_*rtt_/size_);
			last_change_=now;
		} else if (rate_ > ss_maxrate_) {
			// Limited by maxrate.  
			rate_ = oldrate_ = ss_maxrate_/2.0;
			delta_ = 0;
			last_change_=now;
		} 
	} else {
		// If we get here, ss_maxrate <= 0, so the receive rate is 0.
		// We should go back to a very small sending rate!!!
		oldrate_ = rate_;
		rate_ = size_/rtt_; 
		delta_ = 0;
        last_change_=now;
	}
	// printf("ratehybrid rate: %lf\n", rate_);
	if (debug_) printf("SlowStart: now: %5.2f rate: %5.2f delta: %5.2f\n", now, rate_, delta_);
	if (printStatus_) {
		double rate = rate_ * rtt_ / size_;
	  	printf("SlowStart: now: %5.2f rate: %5.2f ss_maxrate: %5.2f delta: %5.2f\n", now, rate, ss_maxrate_, delta_);
	}
	// printf("ratehybrid rate: %lf\n", rate_);
	// getchar();
}

void SctpRateHybrid::sendmsg(int iNumBytes, const char *cpFlags)
{
  /* Let's make sure that a Reset() is called, because it isn't always
   * called explicitly with the "reset" command. For example, wireless
   * nodes don't automatically "reset" their agents, but wired nodes do. 
   */
  if(eState == SCTP_STATE_UNINITIALIZED)
    Reset();

  u_char *ucpOutData = new u_char[uiMaxPayloadSize];
  int iOutDataSize = 0;
  AppData_S *spAppData = (AppData_S *) cpFlags;
  Node_S *spNewNode = NULL;
  int iMsgSize = 0;
  u_int uiMaxFragSize = uiMaxDataSize - sizeof(SctpDataChunkHdr_S);

  if(iNumBytes == -1) 
    eDataSource = DATA_SOURCE_INFINITE;    // Send infinite data
  else 
    eDataSource = DATA_SOURCE_APPLICATION; // Send data passed from app
      
  if(eDataSource == DATA_SOURCE_APPLICATION) 
    {
      if(spAppData != NULL)
	{
	  /* This is an SCTP-aware app!! Anything the app passes down
	   * overrides what we bound from TCL.
	   */
	  spNewNode = new Node_S;
	  uiNumOutStreams = spAppData->usNumStreams;
	  uiNumUnrelStreams = spAppData->usNumUnreliable;
	  spNewNode->eType = NODE_TYPE_APP_LAYER_BUFFER;
	  spNewNode->vpData = spAppData;
	  InsertNode(&sAppLayerBuffer, sAppLayerBuffer.spTail, spNewNode,NULL);
	}
      else
	{
	  /* This is NOT an SCTP-aware app!! We rely on TCL-bound variables.
	   */
	  uiNumOutStreams = 1; // non-sctp-aware apps only use 1 stream
	  uiNumUnrelStreams = (uiNumUnrelStreams > 0) ? 1 : 0;

	  /* To support legacy applications and uses such as "ftp send
	   * 12000", we "fragment" the message. _HOWEVER_, this is not
	   * REAL SCTP fragmentation!! We do not maintain the same SSN or
	   * use the B/E bits. Think of this block of code as a shim which
	   * breaks up the message into useable pieces for SCTP. 
	   */
	  for(iMsgSize = iNumBytes; 
	      iMsgSize > 0; 
	      iMsgSize -= MIN(iMsgSize, (int) uiMaxFragSize) )
	    {
	      spNewNode = new Node_S;
	      spNewNode->eType = NODE_TYPE_APP_LAYER_BUFFER;
	      spAppData = new AppData_S;
	      spAppData->usNumStreams = uiNumOutStreams;
	      spAppData->usNumUnreliable = uiNumUnrelStreams;
	      spAppData->usStreamId = 0;  
	      spAppData->usReliability = uiReliability;
	      spAppData->eUnordered = eUnordered;
	      spAppData->uiNumBytes = MIN(iMsgSize, (int) uiMaxFragSize);
	      spNewNode->vpData = spAppData;
	      InsertNode(&sAppLayerBuffer, sAppLayerBuffer.spTail, 
			 spNewNode, NULL);
	    }
	}      

      if(uiNumOutStreams > MAX_NUM_STREAMS)
	{
	  fprintf(stderr, "%s number of streams (%d) > max (%d)\n",
		  "SCTP ERROR:",
		  uiNumOutStreams, MAX_NUM_STREAMS);
	  exit(-1);
	}
      else if(uiNumUnrelStreams > uiNumOutStreams)
	{
	  fprintf(stderr,"%s number of unreliable streams (%d) > total (%d)\n",
		  "SCTP ERROR:",
		  uiNumUnrelStreams, uiNumOutStreams);
	  exit(-1);
	}

      if(spAppData->uiNumBytes + sizeof(SctpDataChunkHdr_S) 
	 > MAX_DATA_CHUNK_SIZE)
	{
	  fprintf(stderr, "SCTP ERROR: message size (%d) too big\n",
		  spAppData->uiNumBytes);
	  fprintf(stderr, "%s data chunk size (%lu) > max (%d)\n",
		  "SCTP ERROR:",
		  spAppData->uiNumBytes + 
		    (unsigned long) sizeof(SctpDataChunkHdr_S), 
		  MAX_DATA_CHUNK_SIZE);
	  exit(-1);
	}
      else if(spAppData->uiNumBytes + sizeof(SctpDataChunkHdr_S)
	      > uiMaxDataSize)
	{
	  fprintf(stderr, "SCTP ERROR: message size (%d) too big\n",
		  spAppData->uiNumBytes);
	  fprintf(stderr, 
		  "%s data chunk size (%lu) + SCTP/IP header(%d) > MTU (%d)\n",
		  "SCTP ERROR:",
		  spAppData->uiNumBytes + 
		    (unsigned long) sizeof(SctpDataChunkHdr_S),
		  SCTP_HDR_SIZE + uiIpHeaderSize, uiMtu);
	  fprintf(stderr, "           %s\n",
		  "...chunk fragmentation is not yet supported!");
	  exit(-1);
	}
    }

  switch(eState)
    {
    case SCTP_STATE_CLOSED:

      /* This must be done especially since some of the legacy apps use their
       * own packet type (don't ask me why). We need our packet type to be
       * sctp so that our tracing output comes out correctly for scripts, etc
       */
      set_pkttype(PT_SCTP); 
      firstSend = Scheduler::instance().clock();
      iOutDataSize = GenChunk(SCTP_CHUNK_INIT, ucpOutData);
      opT1InitTimer->resched(spPrimaryDest->dRto);
      eState = SCTP_STATE_COOKIE_WAIT;
      // printf("DITO KA KAYA?\n");
      SendPacket(ucpOutData, iOutDataSize, spPrimaryDest);
      break;
      
    case SCTP_STATE_ESTABLISHED:
      if(eDataSource == DATA_SOURCE_APPLICATION) 
	{ 
	  /* NE: 10/12/2007 Check if all the destinations are confirmed (new
	     data can be sent) or there are any marked chunks. */
	  if(eSendNewDataChunks == TRUE && eMarkedChunksPending == FALSE) 
	    {
	      SendMuch();
        // printf("SENDMSG\n");
	      eSendNewDataChunks = FALSE;
	    }
	}
      else if(eDataSource == DATA_SOURCE_INFINITE)
	{
	  fprintf(stderr, "[sendmsg] ERROR: unexpected state... %s\n",
		  "sendmsg called more than once for infinite data");
	  exit(-1);
	}
      break;
      
    default:  
      /* If we are here, we assume the application is trying to send data
       * before the 4-way handshake has completed. ...so buffering the
       * data is ok, but DON'T send it yet!!  
       */
      break;
    }

  delete [] ucpOutData;
}

/* This function bundles control chunks with data chunks. We copy the timestamp
 * chunk into the outgoing packet and return the size of the timestamp chunk.
 */
int SctpRateHybrid::BundleControlChunks(u_char *ucpOutData)
{
	if(eState == SCTP_STATE_ESTABLISHED)
	{
		SctpTfrcChunk_S *spTfrcChunk = (SctpTfrcChunk_S *) ucpOutData;

		spTfrcChunk->sHdr.ucType = SCTP_CHUNK_TFRC;
		spTfrcChunk->sHdr.usLength = sizeof(SctpTfrcChunk_S);

		/* assign dummy values */
		spTfrcChunk->seqno = 0;
		spTfrcChunk->timestamp = 0;
		spTfrcChunk->rtt = 0;

		return spTfrcChunk->sHdr.usLength;
	}
	return 0;
}

void SctpRateHybrid::ProcessOptionChunk(u_char *ucpInChunk)
{
	if(eState == SCTP_STATE_ESTABLISHED)
	{
		switch( ((SctpChunkHdr_S *) ucpInChunk)->ucType)
		{
			case SCTP_CHUNK_TFRC_ACK:
				TfrcUpdate(ucpInChunk, -1);	
				break;
		}
	}
}

void SctpRateHybrid::FastRtx()
{
  Node_S *spCurrDestNode = NULL;
  SctpDest_S *spCurrDestData = NULL;
  Node_S *spCurrBuffNode = NULL;
  SctpSendBufferNode_S *spCurrBuffData = NULL;

  /* be sure we clear all the eCcApplied flags before using them!
   */
  for(spCurrDestNode = sDestList.spHead;
      spCurrDestNode != NULL;
      spCurrDestNode = spCurrDestNode->spNext)
    {
      spCurrDestData = (SctpDest_S *) spCurrDestNode->vpData;
      spCurrDestData->eCcApplied = FALSE;
    }
  
  /* go thru chunks marked for rtx and cut back their ssthresh, cwnd, and
   * partial bytes acked. make sure we only apply congestion control once
   * per destination and once per window (ie, round-trip).
   */
  for(spCurrBuffNode = sSendBuffer.spHead;
      spCurrBuffNode != NULL;
      spCurrBuffNode = spCurrBuffNode->spNext)
    {
      spCurrBuffData = (SctpSendBufferNode_S *) spCurrBuffNode->vpData;

      /* If the chunk has been either marked for rtx or advanced ack, we want
       * to apply congestion control (assuming we didn't already).
       *
       * Why do we do it for advanced ack chunks? Well they were advanced ack'd
       * because they were lost. The ONLY reason we are not fast rtxing them is
       * because the chunk has run out of retransmissions (u-sctp). So we need
       * to still account for the fact they were lost... so apply congestion
       * control!
       */
      if( (spCurrBuffData->eMarkedForRtx != NO_RTX ||
	  spCurrBuffData->eAdvancedAcked == TRUE) &&
	 spCurrBuffData->spDest->eCcApplied == FALSE &&
	 spCurrBuffData->spChunk->uiTsn > uiRecover)
	 
	{ 
	  /* Lukasz Budzisz : 03/09/2006
	     Section 7.2.3 of rfc4960 
	   */
    // printf("ASDFASDFASDFASDF\n");
	  spCurrBuffData->spDest->iSsthresh 
	    = MAX(spCurrBuffData->spDest->iCwnd/2, 4 * (int) uiMaxDataSize);
      // = MAX(spCurrBuffData->spDest->iCwnd*(1/4), 4 * (int) uiMaxDataSize);
	  spCurrBuffData->spDest->iCwnd = spCurrBuffData->spDest->iSsthresh;

		// when SCTP detects a loss in the FastRtx() function, it's a new loss event

		// compute p!
		double I_tot0 = 0;
		double I_tot1 = 0;
		double W_tot = 0;
		int k = 0;
		while (lossIntervals[k++] != 0) {
			printf("lossIntervals[%d] = %d\n", k, lossIntervals[k]);
			lossIntervals[0] -= lossIntervals[k];
		}
		shift_array (lossIntervals, 9, 0); 
		for(int i = 0; i <= k - 1; i++){
			I_tot0 += ((double) lossIntervals[i] * weights[i]);
			W_tot += weights[i];
		}
		for(int i = 1; i <= k; i++){
			I_tot1 += ((double) lossIntervals[i] * weights[i - 1]);
		}
		double I_tot = MAX(I_tot0, I_tot1);
		double I_mean = I_tot / W_tot;
		printf("p: %lf\n", 1 / I_mean);
		p_ = 1 / I_mean;

		TfrcUpdate(NULL, p_);

	  spCurrBuffData->spDest->iPartialBytesAcked = 0; //reset
	  tiCwnd++; // trigger changes for trace to pick up
	  spCurrBuffData->spDest->eCcApplied = TRUE;

	  /* Cancel any pending RTT measurement on this
	   * destination. Stephan Baucke (2004-04-27) suggested this
	   * action as a fix for the following simple scenario:
	   *
	   * - Host A sends packets 1, 2 and 3 to host B, and choses 3 for
	   *   an RTT measurement
	   *
	   * - Host B receives all packets correctly and sends ACK1, ACK2,
	   *   and ACK3.
	   *
	   * - ACK2 and ACK3 are lost on the return path
	   *
	   * - Eventually a timeout fires for packet 2, and A retransmits 2
	   *
	   * - Upon receipt of 2, B sends a cumulative ACK3 (since it has
	   *   received 2 & 3 before)
	   *
	   * - Since packet 3 has never been retransmitted, the SCTP code
	   *   actually accepts the ACK for an RTT measurement, although it
	   *   was sent in reply to the retransmission of 2, which results
	   *   in a much too high RTT estimate. Since this case tends to
	   *   happen in case of longer link interruptions, the error is
	   *   often amplified by subsequent timer backoffs.
	   */
	  spCurrBuffData->spDest->eRtoPending = FALSE; 

	  /* Set the recover variable to avoid multiple cwnd cuts for losses
	   * in the same window (ie, round-trip).
	   */
	  uiRecover = GetHighestOutstandingTsn();
	}
    }

  /* possible that no chunks are pending retransmission since they could be 
   * advanced ack'd 
   */
  if(eMarkedChunksPending == TRUE)  
    RtxMarkedChunks(RTX_LIMIT_ONE_PACKET);
}

void SctpRateHybrid::ProcessSackChunk(u_char *ucpSackChunk)
{

  SctpSackChunk_S *spSackChunk = (SctpSackChunk_S *) ucpSackChunk;

  Boolean_E eFastRtxNeeded = FALSE;
  Boolean_E eNewCumAck = FALSE;
  Node_S *spCurrDestNode = NULL;
  SctpDest_S *spCurrDestNodeData = NULL;
  u_int uiTotalOutstanding = 0;
  int i = 0;

  /* make sure we clear all the iNumNewlyAckedBytes before using them!
   */
  for(spCurrDestNode = sDestList.spHead;
      spCurrDestNode != NULL;
      spCurrDestNode = spCurrDestNode->spNext)
    {
      spCurrDestNodeData = (SctpDest_S *) spCurrDestNode->vpData;
      spCurrDestNodeData->iNumNewlyAckedBytes = 0;
      spCurrDestNodeData->spFirstOutstanding = NULL;
    }

  if(spSackChunk->uiCumAck < uiCumAckPoint) 
    {
      /* this cumAck's a previously cumAck'd tsn (ie, it's out of order!)
       * ...so ignore!
       */
      return;
    }
  else if(spSackChunk->uiCumAck > uiCumAckPoint)
    {
      eNewCumAck = TRUE; // incomding SACK's cum ack advances the cum ack point
      SendBufferDequeueUpTo(spSackChunk->uiCumAck);
      uiCumAckPoint = spSackChunk->uiCumAck; // Advance the cumAck pointer
			lossIntervals[0] = uiCumAckPoint;
    }

  if(spSackChunk->usNumGapAckBlocks != 0) // are there any gaps??
    {
      eFastRtxNeeded = ProcessGapAckBlocks(ucpSackChunk, eNewCumAck);
    } 

  for(spCurrDestNode = sDestList.spHead;
      spCurrDestNode != NULL;
      spCurrDestNode = spCurrDestNode->spNext)
    {
      spCurrDestNodeData = (SctpDest_S *) spCurrDestNode->vpData;

      /* Only adjust cwnd if:
       *    1. sack advanced the cum ack point 
       *    2. this destination has newly acked bytes
       *    3. the cum ack is at or beyond the recovery window
       *
       * Also, we MUST adjust our congestion window BEFORE we update the
       * number of outstanding bytes to reflect the newly acked bytes in
       * received SACK.
       */
      if(eNewCumAck == TRUE &&
	 spCurrDestNodeData->iNumNewlyAckedBytes > 0 &&
	 spSackChunk->uiCumAck >= uiRecover)
	{
	  AdjustCwnd(spCurrDestNodeData);
	}

      /* The number of outstanding bytes is reduced by how many bytes this 
       * sack acknowledges.
       */
      if(spCurrDestNodeData->iNumNewlyAckedBytes <=
	 spCurrDestNodeData->iOutstandingBytes)
	{
	  spCurrDestNodeData->iOutstandingBytes 
	    -= spCurrDestNodeData->iNumNewlyAckedBytes;
	}
      else
	spCurrDestNodeData->iOutstandingBytes = 0;

      
      if(spCurrDestNodeData->iOutstandingBytes == 0)
	{
	  /* All outstanding data has been acked
	   */
	  spCurrDestNodeData->iPartialBytesAcked = 0;  // section 7.2.2

	  /* section 6.3.2.R2
	   */
	  if(spCurrDestNodeData->eRtxTimerIsRunning == TRUE)
	    {
	      StopT3RtxTimer(spCurrDestNodeData);
	    }
	}

      /* section 6.3.2.R3 - Restart timers for destinations that have
       * acknowledged their first outstanding (ie, no timer running) and
       * still have outstanding data in flight.  
       */
      if(spCurrDestNodeData->iOutstandingBytes > 0 &&
	 spCurrDestNodeData->eRtxTimerIsRunning == FALSE)
	{
	  StartT3RtxTimer(spCurrDestNodeData);
	}
    }


  AdvancePeerAckPoint();

  if(eFastRtxNeeded == TRUE)  // section 7.2.4
    FastRtx();

  /* Let's see if after process this sack, there are still any chunks
   * pending... If so, rtx all allowed by cwnd.
   */
  else if( (eMarkedChunksPending = AnyMarkedChunks()) == TRUE)
    {
      /* section 6.1.C) When the time comes for the sender to
       * transmit, before sending new DATA chunks, the sender MUST
       * first transmit any outstanding DATA chunks which are marked
       * for retransmission (limited by the current cwnd).  
       */
      RtxMarkedChunks(RTX_LIMIT_CWND);
    }

  /* (6.2.1.D.ii) Adjust PeerRwnd based on total oustanding bytes on all
   * destinations. We need to this adjustment after any
   * retransmissions. Otherwise the sender's view of the peer rwnd will be
   * off, because the number outstanding increases again once a marked
   * chunk gets retransmitted (when marked, outstanding is decreased).
   */
  uiTotalOutstanding = TotalOutstanding();
  if(uiTotalOutstanding <= spSackChunk->uiArwnd)
    uiPeerRwnd = (spSackChunk->uiArwnd  - uiTotalOutstanding);
  else
    uiPeerRwnd = 0;
  
}

// Shift array a[] up, starting with a[sz-2] -> a[sz-1].
void SctpRateHybrid::shift_array(int *a, int sz, int defval) 
{
	int i ;
	for (i = sz-2 ; i >= 0 ; i--) {
		a[i+1] = a[i] ;
	}
	a[0] = defval;
}
void SctpRateHybrid::shift_array(double *a, int sz, double defval) {
	int i ;
	for (i = sz-2 ; i >= 0 ; i--) {
		a[i+1] = a[i] ;
	}
	a[0] = defval;
}

/* Handles timeouts for both DATA chunks and HEARTBEAT chunks. The actions are
 * slightly different for each. Covers rfc sections 6.3.3 and 8.3.
 */
void SctpRateHybrid::Timeout(SctpChunkType_E eChunkType, SctpDest_S *spDest)
{
  double dCurrTime = Scheduler::instance().clock();

  if(eChunkType == SCTP_CHUNK_DATA)
    {
      spDest->eRtxTimerIsRunning = FALSE;
      
      /* Lukasz Budzisz : 03/09/2006 
       * Section 7.2.3 of rfc4960
       * NE: 4/29/2007 - This conditioal is used for some reason but for 
       * now we dont know why. 
       */
      if(spDest->iCwnd > 1 * (int) uiMaxDataSize)
	{
	  spDest->iSsthresh 
	    = MAX(spDest->iCwnd/2, 4 * (int) uiMaxDataSize);
	  spDest->iCwnd = 1*uiMaxDataSize;
	  spDest->iPartialBytesAcked = 0; // reset
	  tiCwnd++; // trigger changes for trace to pick up
	}

      spDest->opCwndDegradeTimer->force_cancel();

      /* Cancel any pending RTT measurement on this destination. Stephan
       * Baucke suggested (2004-04-27) this action as a fix for the
       * following simple scenario:
       *
       * - Host A sends packets 1, 2 and 3 to host B, and choses 3 for
       *   an RTT measurement
       *
       * - Host B receives all packets correctly and sends ACK1, ACK2,
       *   and ACK3.
       *
       * - ACK2 and ACK3 are lost on the return path
       *
       * - Eventually a timeout fires for packet 2, and A retransmits 2
       *
       * - Upon receipt of 2, B sends a cumulative ACK3 (since it has
       *   received 2 & 3 before)
       *
       * - Since packet 3 has never been retransmitted, the SCTP code
       *   actually accepts the ACK for an RTT measurement, although it
       *   was sent in reply to the retransmission of 2, which results
       *   in a much too high RTT estimate. Since this case tends to
       *   happen in case of longer link interruptions, the error is
       *   often amplified by subsequent timer backoffs.
       */
      spDest->eRtoPending = FALSE; // cancel any pending RTT measurement

			// TFRC no feedback expire
			reduce_rate_on_no_feedback();
    }

  spDest->dRto *= 2;    // back off the timer
  if(spDest->dRto > dMaxRto)
    spDest->dRto = dMaxRto;
  tdRto++;              // trigger changes for trace to pick up

  spDest->iTimeoutCount++;
  spDest->iErrorCount++; // @@@ window probe timeouts should not be counted

  if(spDest->eStatus == SCTP_DEST_STATUS_ACTIVE)
    {  
      iAssocErrorCount++;

      // Path.Max.Retrans exceeded?
      if(spDest->iErrorCount > (int) uiPathMaxRetrans) 
	{
	  spDest->eStatus = SCTP_DEST_STATUS_INACTIVE;
	  if(spDest == spNewTxDest)
	    {
	      spNewTxDest = GetNextDest(spDest);
	    }
	}
      if(iAssocErrorCount > (int) uiAssociationMaxRetrans)
	{
	  /* abruptly close the association!  (section 8.1)
	   */
	  Close();
	  return;
	}
    }

  // trace it!
  tiTimeoutCount++;
  tiErrorCount++;       

  if(spDest->iErrorCount > (int) uiChangePrimaryThresh &&
     spDest == spPrimaryDest)
    {
      spPrimaryDest = spNewTxDest;
    }

  if(eChunkType == SCTP_CHUNK_DATA)
    {
      TimeoutRtx(spDest);
      if(spDest->eStatus == SCTP_DEST_STATUS_INACTIVE && 
	 uiHeartbeatInterval!=0)
	SendHeartbeat(spDest);  // just marked inactive, so send HB immediately
    }
  else if(eChunkType == SCTP_CHUNK_HB)
    {
      if( (uiHeartbeatInterval != 0) ||
	  (spDest->eStatus == SCTP_DEST_STATUS_UNCONFIRMED))
	
	SendHeartbeat(spDest);
    }
}

/* Go thru the send buffer deleting all chunks which have a tsn <= the 
 * tsn parameter passed in. We assume the chunks in the rtx list are ordered by
 * their tsn value. In addtion, for each chunk deleted:
 *   1. we add the chunk length to # newly acked bytes and partial bytes acked
 *   2. we update round trip time if appropriate
 *   3. stop the timer if the chunk's destination timer is running
 */
void SctpRateHybrid::SendBufferDequeueUpTo(u_int uiTsn)
{
  Node_S *spDeleteNode = NULL;
  Node_S *spCurrNode = sSendBuffer.spHead;
  SctpSendBufferNode_S *spCurrNodeData = NULL;

  /* PN: 5/2007. Simulate send window */
  u_short usChunkSize = 0;
  char cpOutString[500];
  iAssocErrorCount = 0;

  while(spCurrNode != NULL &&
	((SctpSendBufferNode_S*)spCurrNode->vpData)->spChunk->uiTsn <= uiTsn)
    {
      spCurrNodeData = (SctpSendBufferNode_S *) spCurrNode->vpData;

      /* Only count this chunk as newly acked and towards partial bytes
       * acked if it hasn't been gap acked or marked as ack'd due to rtx
       * limit.  
       */
      if((spCurrNodeData->eGapAcked == FALSE) &&
	 (spCurrNodeData->eAdvancedAcked == FALSE) )
	{
	  uiHighestTsnNewlyAcked = spCurrNodeData->spChunk->uiTsn;

	  spCurrNodeData->spDest->iNumNewlyAckedBytes 
	    += spCurrNodeData->spChunk->sHdr.usLength;

	  /* only add to partial bytes acked if we are in congestion
	   * avoidance mode and if there was cwnd amount of data
	   * outstanding on the destination (implementor's guide) 
	   */
	  if(spCurrNodeData->spDest->iCwnd >spCurrNodeData->spDest->iSsthresh 
	     &&
	     ( spCurrNodeData->spDest->iOutstandingBytes 
	       >= spCurrNodeData->spDest->iCwnd) )
	    {
	      spCurrNodeData->spDest->iPartialBytesAcked 
		+= spCurrNodeData->spChunk->sHdr.usLength;
	    }
	}

      /* This is to ensure that Max.Burst is applied when a SACK
       * acknowledges a chunk which has been fast retransmitted. If it is
       * ineligible for fast rtx, that can only be because it was fast
       * rtxed or it timed out. If it timed out, a burst shouldn't be
       * possible, but shouldn't hurt either. The fast rtx case is what we
       * are really after. This is a proposed change to RFC2960 section
       * 7.2.4
       */
      if(spCurrNodeData->eIneligibleForFastRtx == TRUE)
	eApplyMaxBurst = TRUE;
	    
      /* We update the RTT estimate if the following hold true:
       *   1. RTO pending flag is set (6.3.1.C4 measured once per round trip)
       *   2. Timestamp is set for this chunk(ie, we were measuring this chunk)
       *   3. This chunk has not been retransmitted
       *   4. This chunk has not been gap acked already 
       *   5. This chunk has not been advanced acked (pr-sctp: exhausted rtxs)
       */
      if(spCurrNodeData->spDest->eRtoPending == TRUE &&
	 spCurrNodeData->dTxTimestamp > 0 &&
	 spCurrNodeData->iNumTxs == 1 &&
	 spCurrNodeData->eGapAcked == FALSE &&
	 spCurrNodeData->eAdvancedAcked == FALSE) 
	{
	  /* If the chunk is marked for timeout rtx, then the sender is an 
	   * ambigious state. Were the sacks lost or was there a failure? 
	   * (See below, where we talk about error count resetting)
	   * Since we don't clear the error counter below, we also don't
	   * update the RTT. This could be a problem for late arriving SACKs.
	   */
		if(spCurrNodeData->eMarkedForRtx != TIMEOUT_RTX)
		{
			RttUpdate(spCurrNodeData->dTxTimestamp, 
					spCurrNodeData->spDest);
			spCurrNodeData->spDest->eRtoPending = FALSE;
		}
	}

      /* if there is a timer running on the chunk's destination, then stop it
       */
      if(spCurrNodeData->spDest->eRtxTimerIsRunning == TRUE)
	StopT3RtxTimer(spCurrNodeData->spDest);

      /* We don't want to clear the error counter if it's cleared already;
       * otherwise, we'll unnecessarily trigger a trace event.
       *
       * Also, the error counter is cleared by SACKed data ONLY if the
       * TSNs are not marked for timeout retransmission and has not been
       * gap acked before. Without this condition, we can run into a
       * problem for failure detection. When a failure occurs, some data
       * may have made it through before the failure, but the sacks got
       * lost. When the sender retransmits the first outstanding, the
       * receiver will sack all the data whose sacks got lost. We don't
       * want these sacks to clear the error counter, or else failover
       * would take longer.
       */
      if(spCurrNodeData->spDest->iErrorCount != 0 &&
	 spCurrNodeData->eMarkedForRtx != TIMEOUT_RTX &&
	 spCurrNodeData->eGapAcked == FALSE)
	{
	  spCurrNodeData->spDest->iErrorCount = 0; // clear error counter
	  tiErrorCount++;                          // ... and trace it too!
	  spCurrNodeData->spDest->eStatus = SCTP_DEST_STATUS_ACTIVE;
	  if(spCurrNodeData->spDest == spPrimaryDest &&
	     spNewTxDest != spPrimaryDest) 
	    {
	      spNewTxDest = spPrimaryDest; // return to primary
	    }
	}

      /* PN: 5/2007. Simulate send window */
      usChunkSize = spCurrNodeData->spChunk->sHdr.usLength;
      
      spDeleteNode = spCurrNode;
      spCurrNode = spCurrNode->spNext;
      DeleteNode(&sSendBuffer, spDeleteNode);
			sendBufferIndex--;
      spDeleteNode = NULL;

      /* PN: 5/2007. Simulate send window */
      if (uiInitialSwnd) 
      {
        uiAvailSwnd += usChunkSize;
      }

    }

  if (channel_)
        (void)Tcl_Write(channel_, cpOutString, strlen(cpOutString));
}

void SctpRateHybrid::AddToSendBuffer(SctpDataChunkHdr_S *spChunk, 
				int iChunkSize,
				u_int uiReliability,
				SctpDest_S *spDest)
{
  Node_S *spNewNode = new Node_S;
  spNewNode->eType = NODE_TYPE_SEND_BUFFER;
  spNewNode->vpData = new SctpSendBufferNode_S;

  SctpSendBufferNode_S * spNewNodeData 
    = (SctpSendBufferNode_S *) spNewNode->vpData;

  /* This can NOT simply be a 'new SctpDataChunkHdr_S', because we need to
   * allocate the space for the ENTIRE data chunk and not just the data
   * chunk header.  
   */
  spNewNodeData->spChunk = (SctpDataChunkHdr_S *) new u_char[iChunkSize];
  memcpy(spNewNodeData->spChunk, spChunk, iChunkSize);

  spNewNodeData->eAdvancedAcked = FALSE;
  spNewNodeData->eGapAcked = FALSE;
  spNewNodeData->eAddedToPartialBytesAcked = FALSE;
  spNewNodeData->iNumMissingReports = 0;
  spNewNodeData->iUnrelRtxLimit = uiReliability;
  spNewNodeData->eMarkedForRtx = NO_RTX;
  spNewNodeData->eIneligibleForFastRtx = FALSE;
  spNewNodeData->iNumTxs = 1;
  spNewNodeData->spDest = spDest;

  /* Is there already a DATA chunk in flight measuring an RTT? 
   * (6.3.1.C4 RTT measured once per round trip)
   */
  if(spDest->eRtoPending == FALSE)  // NO?
    {
      spNewNodeData->dTxTimestamp = Scheduler::instance().clock();
      spDest->eRtoPending = TRUE;   // ...well now there is :-)
    }
  else
    spNewNodeData->dTxTimestamp = 0; // don't use this check for RTT estimate

  InsertNode(&sSendBuffer, sSendBuffer.spTail, spNewNode, NULL);
	sendBufferIndex++;
}
