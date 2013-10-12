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

SendTimer::SendTimer(SctpRateHybrid *agent) : TimerHandler(){
	agent_ = agent;
}

SctpRateHybrid::SctpRateHybrid() : SctpAgent()
{
	snd_rate = SEND_RATE;
	timer_send = new SendTimer(this);

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
    	// printf("timer: %d\n", timer_send->status());
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

 		int addDataSize = GenMultipleDataChunks(ucpOutData+iOutDataSize, 0);

	  // if(askPerm()){
		  iOutDataSize += addDataSize;
	  	SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
	  	// timer_send->resched(snd_rate);
		// }
		// else{
	 //  // iOutDataSize -= addDataSize;
	 //  spNewTxDest->iOutstandingBytes -= addDataSize;
		// }
	  // printf("outstandingbytes: %d\n", spNewTxDest->iOutstandingBytes);
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
		// printf("ASDFASDFSADFASDF\n");
	 //  break;
	}
	// getchar();
    }

    // printf("OUT!!!! %d < %d\n", spNewTxDest->iOutstandingBytes, spNewTxDest->iCwnd);


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
  if(askPerm()){
	  if(dRouteCalcDelay == 0){
	      send(opPacket, 0);
	      timer_send->resched(snd_rate);
	  }
	  else{
	  	if(spDest->eRouteCached == TRUE){
		  spDest->opRouteCacheFlushTimer->resched(dRouteCacheLifetime);
		  send(opPacket, 0);
	  	  timer_send->resched(snd_rate);
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
	}
	else{
		Packet::free(opPacket);
		total += 1;
		printf("total: %d\n", total);
	}
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
	// agent_->
	// printf("sendtimer expire!\n");
}