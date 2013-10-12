#ifndef sctp_rate_hybrid_h
#define sctp_rate_hybrid_h

#include "sctp.h"

#define SEND_RATE 0.000001

class SctpRateHybrid;

//SEND TIMER
class SendTimer : public TimerHandler
{
public:
  SendTimer(SctpRateHybrid *a);
  virtual void expire(Event *e);

protected:
	SctpRateHybrid *agent_;

};

class SctpRateHybrid : public SctpAgent {

private:
	SendTimer	*timer_send;
	double snd_rate;
	int total;

protected:
  virtual void  delay_bind_init_all();
  virtual int   delay_bind_dispatch(const char *varName, const char *localName,
				    TclObject *tracer);
  virtual void SendPacket(u_char *ucpData, int iDataSize, SctpDest_S *spDest);
  virtual void SendMuch();
  virtual bool askPerm();
  virtual void cancelTimer();

public:
	SctpRateHybrid();
	~SctpRateHybrid();

	// virtual void  recv(Packet *pkt, Handler*);
	// virtual void  sendmsg(int iNumBytes, const char *cpFlags);
	// int command(int argc, const char*const* argv);
};

#endif