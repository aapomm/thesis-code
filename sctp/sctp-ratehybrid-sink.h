#ifndef sctp_rate_hybrid_sink_h
#define sctp_rate_hybrid_sink_h

#include "sctp.h" #include "agent.h" #include "packet.h" #include "ip.h"
#include "timer-handler.h"
#include "random.h"

#define SEND_RATE 0.003
#define LARGE_DOUBLE 9999999999.99 
#define SAMLLFLOAT 0.0000001

#define MAXRATE 25000000.0 
#define SMALLFLOAT 0.0000001

/* packet status */
#define UNKNOWN 0
#define RCVD 1
#define LOST 2		// Lost, and beginning of a new loss event
#define NOT_RCVD 3	// Lost, but not the beginning of a new loss event
#define ECNLOST 4	// ECN, and beginning of a new loss event
#define ECN_RCVD 5	// Received with ECN. 

#define DEFAULT_NUMSAMPLES  8

#define WALI 1
#define EWMA 2 
#define RBPH 3
#define EBPH 4 

class SctpRateHybridSink;

//SEND TIMER
class SendTimer : public TimerHandler
{
protected:
	SctpRateHybridSink *agent_;
	List_S pktQ_;
};

class SctpRateHybridSinkNackTimer : public TimerHandler {
public:
  SctpRateHybridSinkNackTimer(SctpRateHybridSink *a) : TimerHandler() { 
		a_ = a; 
	}
	virtual void expire(Event *e);
protected:
	SctpRateHybridSink *a_;
};

class SctpRateHybridSink : public SctpAgent {
	friend class SctpRateHybridSinkNackTimer;

protected:
  virtual void  delay_bind_init_all();
  virtual int   delay_bind_dispatch(const char *varName, const char *localName,
				    TclObject *tracer);
	double adjust_history(double);
	double est_loss();
	double est_thput(); 
	int command(int argc, const char*const* argv);
	void print_loss(int sample, double ave_interval);
	void print_loss_all(int *sample);
	void print_losses_all(int *losses);
	void print_count_losses_all(int *count_losses);
	void print_num_rtts_all(int *num_rtts);
	int new_loss(int i, double tstamp);
	double estimate_tstamp(int before, int after, int i);
	virtual void  TfrcUpdate(u_char *ucpInChunk);
  SctpTfrcAckChunk_S* createTfrcAckChunk(u_char *, double);
	int BundleControlChunks(u_char *);
  void SendPacket(u_char*, int, SctpDest_S*);
	void ProcessOptionChunk(u_char*);
	
	// algo specific
	double est_loss_WALI();
	void shift_array(int *a, int sz, int defval) ;
	void shift_array(double *a, int sz, double defval) ;
	void multiply_array(double *a, int sz, double multiplier);
	void init_WALI();
	double weighted_average(int start, int end, double factor, double *m, double *w, int *sample);
	int get_sample(int oldSample, int numLosses);
	int get_sample_rtts(int oldSample, int numLosses, int rtts);
	double weighted_average1(int start, int end, double factor, double *m, double *w, int *sample, int ShortIntervals, int *losses, int *count_losses, int *num_rtts);

	static double p_to_b(double p, double rtt, double tzero, int psize, int bval);
	double b_to_p(double b, double rtt, double tzero, int psize, int bval);

	//common variables

	SctpRateHybridSinkNackTimer nack_timer_;

  bool sendReport = false; // send report????
  bool data = false;

	int psize_;		// size of received packet
	int fsize_;		// size of large TCP packet, for VoIP mode.
	double rtt_;		// rtt value reported by sender
	double tzero_;		// timeout value reported by sender
	int smooth_;		// for the smoother method for incorporating
					//  incorporating new loss intervals
	int total_received_;	// total # of pkts rcvd by rcvr, 
				//   for statistics only
	int total_losses_;      // total # of losses, for statistics only
	int total_dropped_;	// total # of drops, for statistics
	int bval_;		// value of B used in the formula
	double last_report_sent; 	// when was last feedback sent
	double NumFeedback_; 	// how many feedbacks per rtt
	int rcvd_since_last_report; 	// # of packets rcvd since last report
	int losses_since_last_report;	// # of losses since last report
	int printLoss_;		// to print estimated loss rates
	int maxseq; 		// max seq number seen
	int maxseqList;         // max seq number checked for dropped packets
	int numPkts_;		// Num non-sequential packets before
				//  inferring loss
        double minDiscountRatio_; 	// Minimum for history discounting.
	int numPktsSoFar_;	// Num non-sequential packets so far
	int PreciseLoss_;       // to estimate loss events more precisely

	// an option for single-RTT loss intervals
	int ShortIntervals_ ;	// For calculating loss event rates for short 
				//  loss intervals:  "0" for counting a
				// single loss; "1" for counting the actual
				// number of losses; "2" for counting at
				// most a large packet of losses (not done);
				// "3" for decreasing fraction of losses
                                // counted for longer loss intervals;
                                // >10 for old methods that don't ignore the
                                // current short loss interval. 
        int ShortRtts_ ;	// Max num of RTTs in a short interval.

	// these assist in keep track of incoming packets and calculate flost_
	double last_timestamp_; // timestamp of last new, in-order pkt arrival.
	double last_arrival_;   // time of last new, in-order pkt arrival.
	int hsz;		// InitHistorySize_, number of pkts in history
	char *lossvec_;		// array with packet history
	double *rtvec_;		// array with time of packet arrival
	double *tsvec_;		// array with timestamp of packet
	int lastloss_round_id ; // round_id for start of loss event
	int round_id ;		// round_id of last new, in-order packet
	double lastloss; 	// when last loss occured

	// WALI specific
	int numsamples ;
	int *sample;		// array with size of loss interval
	double *weights ;	// weight for loss interval
	double *mult ;		// discount factor for loss interval
	int *losses ;		// array with number of losses per loss
				//   interval
	int *count_losses ;	// "1" to count losses in the loss interval
        int *num_rtts ;         // number of rtts per loss interval
	double mult_factor_;	// most recent multiple of mult array
	int sample_count ;	// number of loss intervals
	int last_sample ;  	// loss event rate estimated to here
	int init_WALI_flag;	// sample arrays initialized

	// these are for "faking" history after slow start
	int loss_seen_yet; 	// have we seen the first loss yet?
	int adjust_history_after_ss; // fake history after slow start? (0/1)
	int false_sample; 	// by how much?
	
	int algo;		// algo for loss estimation 
	int discount ;		// emphasize most recent loss interval
				//  when it is very large
	int bytes_ ;		// For reporting on received bytes.

  double p;

public:
	SctpRateHybridSink();
	~SctpRateHybridSink();

	virtual void  recv(Packet *pkt, Handler*);
	// virtual void  sendmsg(int iNumBytes, const char *cpFlags);
};

#endif
