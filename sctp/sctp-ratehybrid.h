#ifndef sctp_rate_hybrid_h
#define sctp_rate_hybrid_h

#include "sctp.h"
#include "random.h"

#define SEND_RATE 0.001
#define SMALLFLOAT 0.0000001

/* modes of rate change for TFRC */
#define SLOW_START 1
#define CONG_AVOID 2
#define RATE_DECREASE	 3
#define OUT_OF_SLOW_START 4 

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

class SctpTfrcNoFeedbackTimer : public TimerHandler {
public:
		SctpTfrcNoFeedbackTimer(SctpRateHybrid *a) : TimerHandler() { a_ = a;}
		virtual void expire(Event *e);
		virtual void setDest(SctpDest_S *d);
protected:
		SctpRateHybrid *a_;
		SctpDest_S  *spDest;
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
	virtual void TfrcUpdate(u_char *);
	virtual void addToList(Packet *p);
	virtual void FastRtx();
	virtual int BundleControlChunks(u_char *);
	virtual void ProcessOptionChunk(u_char *);
	virtual void ProcessSackChunk(u_char *);
  // virtual void recv(Packet *pkt, Handler*);
	
	// Variables for sender-side p computation
	int lossIntervals [8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int currentLossIntervalLength;
	int currentLossIntervalIndex = -1;

	/* variables for TFRC integration
	*/
	// Dynamic State
 	int first_pkt_rcvd ;	// first ack received yet?
  	
	double rcvrate  ; 	// TCP friendly rate based on current RTT 
				//  and recever-provded loss estimate
	double maxrate_;	// prevents sending at more than 2 times the 
				//  rate at which the receiver is _receving_ 
	double ss_maxrate_;	// max rate for during slowstart
	int round_id ;		// round id
  	TracedDouble true_loss_rate_; // true loss event rate,
  	int rate_change_; 	// slow start, cong avoid, decrease
	double oldrate_;	// allows rate to be changed gradually
	double last_change_;	// time last change in rate was made
	double lastlimited_;	// time sender was last datalimited.
  	//End of Dynamic State
	int seqno_;

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

	double firstSend;
	bool noRtt;

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

	/* variants in TCP formula*/
	int heavyrounds_;	// the number of RTTs so far when the
				//  sending rate > 2 * receiving rate
	int maxHeavyRounds_;	// the number of allowed rounds for
				//  sending rate > 2 * receiving rate
	int datalimited_;	// to send immediately when a new packet
				//   arrives after a data-limited period

	int UrgentFlag;		// urgent flag
	double overhead_;
	SctpTfrcNoFeedbackTimer NoFeedbacktimer_;

	double delta_;
	int printStatus_;
	int idleFix_;
	double ssmult_;	
	int sendData_; // are we sending data right now?
	int slow_increase_;
	int ca_;
	/* end of TFRC integration variables
	*/

public:
	SctpRateHybrid();
	~SctpRateHybrid();

	virtual void reduce_rate_on_no_feedback(SctpDest_S *spDest);
	virtual void  recv(Packet *pkt, Handler*);
	double rfc3390(int size);
	virtual void  update_rtt(double tao, double now);	
  virtual void increase_rate(double p);
  virtual void decrease_rate();
  virtual double initial_rate();
  virtual void nextpkt();
  virtual void slowstart();
  virtual void  sendmsg(int nbytes, const char *flags = 0);
	// virtual void  sendmsg(int iNumBytes, const char *cpFlags);
	// int command(int argc, const char*const* argv);	
  double rate_;
};

#endif
