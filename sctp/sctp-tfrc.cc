// Sender initialization
/*
 * STD_PACKET_SIZE = 0
 * INIT_RTO = 2
 * INIT_SEND_RATE = STD_PACKET_SIZE
 * X, tld;
 *
 * if (ready_to_send && X == NULL)
 *	s = INIT_SEND_RATE
 *	X = s;
 *	set nofeedback timer (INIT_RTO)
 *	tld = 0 (or -1)
 */

#include "sctp.h"

#define SCTP_TFRC_STD_PACKET_SIZE 500
#define SCTP_TFRC_INIT_SEND_RATE SCTP_TFRC_STD_PACKET_SIZE
#define SCTP_TFRC_INIT_RTO 2.0
#define SCTP_TFRC_RTT_Q 0.9
#define SCTP_TFRC_RTT_Q2 0.9

class TfrcSctpAgent;

class SCTPTFRCSendTimer : public TimerHandler {

	protected:
		TfrcSctpAgent *agent_;
	public:
		SCTPTFRCSendTimer(TfrcSctpAgent* agent);
		virtual void expire(Event *e);
};

class SCTPTFRCNoFeedbackTimer : public TimerHandler {

	protected:
		TfrcSctpAgent *agent_;
	public:
		SCTPTFRCNoFeedbackTimer(TfrcSctpAgent* agent);
		virtual void expire(Event *e);
};

SCTPTFRCSendTimer::SCTPTFRCSendTimer(TfrcSctpAgent* agent) : TimerHandler() {
	agent_ = agent;
};

void SCTPTFRCSendTimer::expire(Event *e) {

};

SCTPTFRCNoFeedbackTimer::SCTPTFRCNoFeedbackTimer(TfrcSctpAgent* agent) : TimerHandler() {
	agent_ = agent;
};

void SCTPTFRCNoFeedbackTimer::expire(Event *e) {

};

class TfrcSctpAgent : public virtual SctpAgent {

	public:
		TfrcSctpAgent();
	private:
		double s_x_; 
		double s_x_inst_;
		double s_x_recv_;
		double s_r_sample_;
		double s_rtt_;
		double s_r_sqmean_;
		double s_p_;
		double s_initial_x_;
		double s_initial_rto_;
		double s_rtt_q_;
		double s_rtt_q2_;
		double s_t_ld_;
		
		SCTPTFRCSendTimer *s_timer_send_;
		SCTPTFRCNoFeedbackTimer *s_timer_no_feedback_;

		/* Sender state */
		/* Calculated send rate */
		/* Packet size */

};

static class TfrcSctpClass : public TclClass {

	public:
		TfrcSctpClass() : TclClass("Agent/SCTP/TFRC") {}
		TclObject* create(int, const char*const*) {	
			return (new TfrcSctpAgent());
		}
} classSctpTfrc;

TfrcSctpAgent::TfrcSctpAgent() : SctpAgent() {

	s_x_ = SCTP_TFRC_INIT_SEND_RATE;
	s_x_inst_ = SCTP_TFRC_INIT_SEND_RATE;

	s_r_sample_ = 0.0;
	s_rtt_ = 0.0;
	s_r_sqmean_ = 0.0;
	s_p_ = 0.0;

	s_t_ld_ = -1.0;

	s_timer_send_ = new SCTPTFRCSendTimer(this);
	s_timer_no_feedback_ = new SCTPTFRCNoFeedbackTimer(this);

	// Creating the agent, we want to initialize the values and the timers.
	// The initialization is described in RFC 3448, section 4,
	// "Sender Initialization".
}

// Define send and receive methods for sender and receiver.
// t_ipi and delta calculations.

// Send:
// The first time that TFRC something, it has no information about
// the network yet, so

// The important thing is that the SendTimer is rescheduled 
// whenever the sending rate X changes, and whenever the Timer
// times out, we call the sendmsg function.

// And that's it!