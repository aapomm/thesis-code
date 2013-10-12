#ifndef sctp_rate_hybrid_h
#define sctp_rate_hybrid_h

#include "sctp.h"

class SctpRateHybrid;

class SctpRateHybrid : public virtual SctpAgent {

private:
	int sendRate_;

protected:

  virtual void  delay_bind_init_all();
  virtual int   delay_bind_dispatch(const char *varName, const char *localName,
				    TclObject *tracer);
  virtual void OptionReset();

public:
	SctpRateHybrid();
	// ~SctpRateHybrid();

	// virtual void  recv(Packet *pkt, Handler*);
	virtual void  sendmsg(int iNumBytes, const char *cpFlags);
	// int command(int argc, const char*const* argv);
};

#endif