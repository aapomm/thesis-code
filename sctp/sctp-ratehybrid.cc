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

SctpRateHybrid::SctpRateHybrid() : SctpAgent()
{
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

void SctpRateHybrid::OptionReset()
{
  uiRecover = 0;
}

void SctpRateHybrid::sendmsg(int iNumBytes, const char *cpFlags){

	u_char *ucpOutData = new u_char[uiMaxPayloadSize];
	int iOutDataSize = 0;
	AppData_S *spAppData = (AppData_S *) cpFlags;
	Node_S *spNewNode = NULL;
	int iMsgSize = 0;
	u_int uiMaxFragSize = uiMaxDataSize - sizeof(SctpDataChunkHdr_S);

	for(iMsgSize = iNumBytes; iMsgSize > 0; iMsgSize -= MIN(iMsgSize, (int) uiMaxFragSize) )
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