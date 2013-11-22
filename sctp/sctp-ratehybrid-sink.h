#ifndef sctp_rate_hybrid_sink_h
#define sctp_rate_hybrid_sink_h

#include "sctp.h"

#define SEND_RATE 0.003

class SctpRateHybridSink;

//SEND TIMER
class SendTimer : public TimerHandler
{
protected:
	SctpRateHybridSink *agent_;
	List_S pktQ_;
};

class SctpRateHybridSink : public SctpAgent {

protected:
  virtual void  delay_bind_init_all();
  virtual int   delay_bind_dispatch(const char *varName, const char *localName,
				    TclObject *tracer);

public:
	SctpRateHybridSink();
	~SctpRateHybridSink();

	// virtual void  recv(Packet *pkt, Handler*);
	// virtual void  sendmsg(int iNumBytes, const char *cpFlags);
	// int command(int argc, const char*const* argv);
};

#endif
