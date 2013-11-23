#ifndef sctp_rate_hybrid_h
#define sctp_rate_hybrid_h

#include "sctp.h"

#define SEND_RATE 0.003

class SctpRateHybrid;

//SEND TIMER
class SendTimer : public TimerHandler
{
public:
  SendTimer(SctpRateHybrid *a, List_S pktlist);
  virtual void addToList(Packet *p);
  virtual void expire(Event *e);

protected:
	SctpRateHybrid *agent_;
	List_S pktQ_;
};

class SctpTfrcNoFeedbackTimer : public TimerHandler {
public:
		SctpTfrcNoFeedbackTimer(SctpRateHybrid *a) : TimerHandler() { a_ = a; }
		virtual void expire(Event *e);
protected:
		SctpRateHybrid *a_;
}; 

class SctpRateHybrid : public SctpAgent {

private:
	SendTimer	*timer_send;
	double snd_rate;
	int total;
	List_S pktQ_;

protected:
  virtual void  delay_bind_init_all();
  virtual int   delay_bind_dispatch(const char *varName, const char *localName,
				    TclObject *tracer);
  virtual void SendPacket(u_char *ucpData, int iDataSize, SctpDest_S *spDest);
  virtual void SendMuch();
  virtual bool askPerm();
  virtual void cancelTimer();
  virtual void TFRC_update(Packet *pkt);
  // virtual void recv(Packet *pkt, Handler*);

	/* variables for TFRC integration
	*/
	// Dynamic State
  	int first_pkt_rcvd ;	// first ack received yet?
  	double rate_;
	double rcvrate  ; 	// TCP friendly rate based on current RTT 
				//  and recever-provded loss estimate
	double maxrate_;	// prevents sending at more than 2 times the 
				//  rate at which the receiver is _receving_ 
	double ss_maxrate_;	// max rate for during slowstart
	int round_id ;		// round id
  	TracedDouble true_loss_rate_; // true loss event rate,
  	int rate_change_; 	// slow start, cong avoid, decrease
	double oldrate_;	// allows rate to be changed gradually
  	//End of Dynamic State

	/* Responses to heavy congestion. */
	int conservative_;	// set to 1 for an experimental, conservative 
				//   response to heavy congestion
	double scmult_;         // self clocking parameter for conservative_
	/* End of responses to heavy congestion.  */

	/* TCP variables for tracking RTT */
	int t_srtt_; 
	int t_rtt_;
	int t_rttvar_;
	int rttvar_exp_;
	double t_rtxcur_;
	double tcp_tick_;
	int T_SRTT_BITS; 
	int T_RTTVAR_BITS;
	int srtt_init_; 
	int rttvar_init_;
	double rtxcur_init_;
	/* End of TCP variables for tracking RTT */

	/* "accurate" estimates for formula */
	double rtt_; /*EWMA version*/
	double rttcur_; /*Instantaneous version*/
	double rttvar_;
	double tzero_;
	double sqrtrtt_; /*The mean of the sqrt of the RTT*/

	//Parameters
	double df_;		// decay factor for accurate RTT estimate
	double minrto_ ;	// for experimental purposes, for a minimum
				//  RTO value (for use in the TCP-friendly
				//  equation)
	int bval_;		// value of B for the formula
	int fsize_;		// Default size for large TCP packets 
				//  (e.g., 1460 bytes).
	/* end of TFRC integration variables
	*/

public:
	SctpRateHybrid();
	~SctpRateHybrid();

	virtual void reduce_rate_on_no_feedback();
	virtual void  recv(Packet *pkt, Handler*);
	double rfc3390(int size);
	virtual void  update_rtt(double tao, double now);	
	// virtual void  sendmsg(int iNumBytes, const char *cpFlags);
	// int command(int argc, const char*const* argv);	
};

#endif
