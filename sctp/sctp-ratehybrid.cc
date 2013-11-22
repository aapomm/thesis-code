#ifndef lint
static const char rcsid[] =
"@(#) $Header: /cvsroot/nsnam/ns-2/sctp/sctp-ratehybrid.cc,v 1.5 2013/12/10 05:51:27 aaron_manaloto Exp $ (UD/PEL)";
#endif

#include "ip.h"
#include "sctp-ratehybrid.h"
#include "flags.h"

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

SendTimer::SendTimer(SctpRateHybrid *agent, List_S pktlist) : TimerHandler(){
	agent_ = agent;
	pktQ_ = pktlist;
}

SctpRateHybrid::SctpRateHybrid() : SctpAgent()
{
	snd_rate = SEND_RATE;
	memset(&pktQ_, 0, sizeof(List_S));

	// printf("isHEADEMPTY? %d\n", pktQ_.spHead == NULL);
	// printf("isTAILEMPTY? %d\n", pktQ_.spTail == NULL);
	// printf("length? %d\n", pktQ_.uiLength);

	timer_send = new SendTimer(this, pktQ_);

	timer_send->sched(snd_rate);
  total = 0;
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

  hdr_sctp::access(opPacket)->NumChunks() = uiNumChunks;
  hdr_sctp::access(opPacket)->SctpTrace() = new SctpTrace_S[uiNumChunks];
  memcpy(hdr_sctp::access(opPacket)->SctpTrace(), spSctpTrace, 
	 (uiNumChunks * sizeof(SctpTrace_S)) );

  uiNumChunks = 0; // reset the counter

// printf("time: %lf\n", Scheduler::instance().clock());
  // if(askPerm()){
  // if(timer_send->status() == TIMER_IDLE){
  // 	timer_send->sched(snd_rate);
  // }

	  if(dRouteCalcDelay == 0){
	  		timer_send->addToList(opPacket);
	      // send(opPacket, 0);
	      // timer_send->resched(snd_rate);
	  }
	  else{
	  	if(spDest->eRouteCached == TRUE){
		  spDest->opRouteCacheFlushTimer->resched(dRouteCacheLifetime);
		  timer_send->addToList(opPacket);
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
	// printf("sendtimer expire!\n");
	//DEQUEUE	
	if(pktQ_.spHead == NULL){
		resched(SEND_RATE);
		// if(pktQ_.uiLength>0){
		// 	printf("FUCK MAN! %d time: %lf\n", pktQ_.uiLength, Scheduler::instance().clock());
		// }
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

	agent_->send((Packet *)(node->vpData),0);
	// printf("send %lf!\n", SEND_RATE);
	resched(SEND_RATE);

	// delete (Packet *) node->vpData;
	node->vpData = NULL;
	delete node;

	pktQ_.uiLength--;
	// printf("length: %d time: %lf\n", pktQ_.uiLength, Scheduler::instance().clock());
}

void SendTimer::addToList(Packet *p){
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

 	do{
     	iOutDataSize += ProcessChunk(ucpCurrInChunk, &ucpCurrOutData);
     	NextChunk(&ucpCurrInChunk, &iRemainingDataLen);
    }
 	while(ucpCurrInChunk != NULL);

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

	double now = Scheduler::instance().clock();
	hdr_sctp *nck = hdr_sctp::access(opInPkt);
	//double ts = nck->timestamp_echo;
	double ts = nck->timestamp_echo + nck->timestamp_offset;
	double rate_since_last_report = nck->rate_since_last_report;
	// double NumFeedback_ = nck->NumFeedback_;
	double flost = nck->flost; 
	int losses = nck->losses;
	true_loss_rate_ = nck->true_loss;

	//FREE PACKET
 	delete hdr_sctp::access(opInPkt)->SctpTrace();
 	hdr_sctp::access(opInPkt)->SctpTrace() = NULL;
 	Packet::free(opInPkt);
 	opInPkt = NULL;
 	delete [] ucpOutData;
}