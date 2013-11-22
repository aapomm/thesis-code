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
