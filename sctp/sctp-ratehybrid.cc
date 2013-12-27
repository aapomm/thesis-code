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

SctpRateHybrid::SctpRateHybrid() : SctpAgent(), NoFeedbacktimer_(this)
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

	timer_send = new SendTimer(this);

	// printf("size_: %d, rate_: %lf, initrate_: %lf\n", size_, rate_, size_/rate_);
	size_ = 1460;
	timer_send->sched(size_/rate_);
	sendData_ = 0;
	firstSend = 0;
	noRtt = true;
}

SctpRateHybrid::~SctpRateHybrid(){
	cancelTimer();

	delete timer_send;
}

void SctpTfrcNoFeedbackTimer::expire(Event *) {
	a_->reduce_rate_on_no_feedback (spDest);
}

void SctpTfrcNoFeedbackTimer::setDest(SctpDest_S *d) {
	spDest = d;
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
 		noRtt = false;
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

	//FREE PACKET
 	delete hdr_sctp::access(opInPkt)->SctpTrace();
 	hdr_sctp::access(opInPkt)->SctpTrace() = NULL;
 	Packet::free(opInPkt);
 	opInPkt = NULL;
 	delete [] ucpOutData;
}

void SctpRateHybrid::TfrcUpdate(u_char *ucpInChunk){
    /* 
   		TFRC INTEGRATION
    */
	double now = Scheduler::instance().clock();
	//Extract SCTP headers for TFRC calculation
	SctpTfrcAckChunk_S *nck = (SctpTfrcAckChunk_S *) ucpInChunk;
	//double ts = nck->timestamp_echo;
	double ts = nck->timestamp_echo + nck->timestamp_offset;
	double rate_since_last_report = nck->rate_since_last_report;
	// printf("tfrc rslr: %lf\n", rate_since_last_report);
	// double NumFeedback_ = nck->NumFeedback_;
	double flost = nck->flost; 
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

	rcvrate = p_to_b(flost, rtt_, tzero_, fsize_, bval_);
	/* if we get no more feedback for some time, cut rate in half */
	double t = 2*rtt_ ; 
  if(rate_ == 0.00) rate_ = 1.00;
	if (t < 2*size_/rate_) 
		t = 2*size_/rate_ ; 
	NoFeedbacktimer_.resched(t);
	
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

void SctpRateHybrid::reduce_rate_on_no_feedback(SctpDest_S *spDest)
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
	UrgentFlag = 1;
	round_id ++ ;
	double t = 2*rtt_ ; 
	// Set the nofeedback timer.
	if (t < 2*uiMaxPayloadSize/rate_) 
		t = 2*uiMaxPayloadSize/rate_ ; 
	NoFeedbacktimer_.resched(t);
	/*
	if (datalimited_) {
		all_idle_ = 1;
		if (debug_) printf("Time: %5.2f Datalimited now.\n", now);
	}
	*/
	nextpkt();
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

	if (eState == SCTP_STATE_ESTABLISHED) {
		/* Get TFRC chunk */
		Packet *pktToSend = (Packet *) node->vpData;
		PacketData *pktToSendData = (PacketData*) pktToSend->userdata(); 
		SctpTfrcChunk_S *firstChunk = (SctpTfrcChunk_S *) pktToSendData->data(); // TFRC chunk is always the 1st chunk

		/* Add TFRC details */	
		firstChunk->timestamp = Scheduler::instance().clock();
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
				TfrcUpdate(ucpInChunk);	
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
		printf("Loss!\n");
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

