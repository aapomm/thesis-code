#ifndef lint
static const char rcsid[] =
"@(#) $Header: /cvsroot/nsnam/ns-2/sctp/sctp-ratehybrid.cc,v 1.5 2013/12/10 05:51:27 aaron_manaloto Exp $ (UD/PEL)";
#endif

#include "ip.h"
#include "sctp-ratehybrid-sink.h"
#include "flags.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define	MIN(x,y)	(((x)<(y))?(x):(y))
#define	MAX(x,y)	(((x)>(y))?(x):(y))

static class SctpRateHybridSinkClass : public TclClass 
{ 
public:
  SctpRateHybridSinkClass() : TclClass("Agent/SCTP/RatehybridSink") {}
  TclObject* create(int, const char*const*) 
  {
    return (new SctpRateHybridSink());
  }
} classSctpRateHybridSink;

SctpRateHybridSink::SctpRateHybridSink() : SctpAgent()
{
	// printf("isHEADEMPTY? %d\n", pktQ_.spHead == NULL);
	// printf("isTAILEMPTY? %d\n", pktQ_.spTail == NULL);
	// printf("length? %d\n", pktQ_.uiLength);
}

SctpRateHybridSink::~SctpRateHybridSink(){
}

void SctpRateHybridSink::delay_bind_init_all()
{
  SctpAgent::delay_bind_init_all();
}

int SctpRateHybridSink::delay_bind_dispatch(const char *cpVarName, 
					  const char *cpLocalName, 
					  TclObject *opTracer)
{
  return SctpAgent::delay_bind_dispatch(cpVarName, cpLocalName, opTracer);
}

void SctpRateHybridSink::recv(Packet *opInPkt, Handler*)
{
  /* Let's make sure that a Reset() is called, because it isn't always
   * called explicitly with the "reset" command. For example, wireless
   * nodes don't automatically "reset" their agents, but wired nodes do. 
   */
  if(eState == SCTP_STATE_UNINITIALIZED)
    Reset(); 

  // printf("time: %lf\n", Scheduler::instance().clock());
  // getchar();

  hdr_ip *spIpHdr = hdr_ip::access(opInPkt);
  PacketData *opInPacketData = (PacketData *) opInPkt->userdata();
  u_char *ucpInData = opInPacketData->data();
  u_char *ucpCurrInChunk = ucpInData;
  int iRemainingDataLen = opInPacketData->size();

  u_char *ucpOutData = new u_char[uiMaxPayloadSize];
  u_char *ucpCurrOutData = ucpOutData;

  /* local variable which maintains how much data has been filled in the 
   * current outgoing packet
   */
  int iOutDataSize = 0; 

  memset(ucpOutData, 0, uiMaxPayloadSize);
  memset(spSctpTrace, 0,
   (uiMaxPayloadSize / sizeof(SctpChunkHdr_S)) * sizeof(SctpTrace_S) );

  spReplyDest = GetReplyDestination(spIpHdr);

  eStartOfPacket = TRUE;

  do
    {

      /* processing chunks may need to generate response chunks, so the
       * current outgoing packet *may* be filled in and our out packet's data
       * size is incremented to reflect the new data
       */
      iOutDataSize += ProcessChunk(ucpCurrInChunk, &ucpCurrOutData);
      NextChunk(&ucpCurrInChunk, &iRemainingDataLen);
    }
  while(ucpCurrInChunk != NULL);

  /* Let's see if we have any response chunks(currently only handshake related)
   * to transmit. 
   *
   * Note: We don't bundle these responses (yet!)
   */
  if(iOutDataSize > 0) 
    {
      SendPacket(ucpOutData, iOutDataSize, spReplyDest);
    }

  /* Let's check to see if we need to generate and send a SACK chunk.
   *
   * Note: With uni-directional traffic, SACK and DATA chunks will not be 
   * bundled together in one packet.
   * Perhaps we will implement this in the future?  
   */
  if(eSackChunkNeeded == TRUE)
    {
      memset(ucpOutData, 0, uiMaxPayloadSize);
      iOutDataSize = BundleControlChunks(ucpOutData);

      /* PN: 6/17/2007
       * Send NR-Sacks instead of Sacks?
       */
      if (eUseNonRenegSacks == TRUE) 
  {
    iOutDataSize += GenChunk(SCTP_CHUNK_NRSACK, ucpOutData+iOutDataSize);
  }
      else
  {
    iOutDataSize += GenChunk(SCTP_CHUNK_SACK, ucpOutData+iOutDataSize);
  }

      SendPacket(ucpOutData, iOutDataSize, spReplyDest);

      if (eUseNonRenegSacks == TRUE)
  {
  }
      else
  {
  }

      eSackChunkNeeded = FALSE;  // reset AFTER sent (o/w breaks dependencies)
    }

  /* Do we need to transmit a FORWARD TSN chunk??
   */
  if(eForwardTsnNeeded == TRUE)
    {
      memset(ucpOutData, 0, uiMaxPayloadSize);
      iOutDataSize = BundleControlChunks(ucpOutData);
      iOutDataSize += GenChunk(SCTP_CHUNK_FORWARD_TSN,ucpOutData+iOutDataSize);
      SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
      eForwardTsnNeeded = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

  /* Do we want to send out new DATA chunks in addition to whatever we may have
   * already transmitted? If so, we can only send new DATA if no marked chunks
   * are pending retransmission.
   * 
   * Note: We aren't bundling with what was sent above, but we could. Just 
   * avoiding that for now... why? simplicity :-)
   */
  if(eSendNewDataChunks == TRUE && eMarkedChunksPending == FALSE) 
    {
      SendMuch();       // Send new data till our cwnd is full!
      eSendNewDataChunks = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

  delete hdr_sctp::access(opInPkt)->SctpTrace();
  hdr_sctp::access(opInPkt)->SctpTrace() = NULL;
  printf("Sequence number: %d\n", hdr_sctp::access(opInPkt)->uiTsn);
  Packet::free(opInPkt);
  opInPkt = NULL;
  delete [] ucpOutData;
}
