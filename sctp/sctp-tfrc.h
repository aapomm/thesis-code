#ifndef ns_sctp_tfrc_h
#define ns_sctp_tfrc_h

#include "packet.h"

struct hdr_sctp_tfrc_fback {

	// RFC 3448 specifies that feedback packets must include the
	// timestamp_echo, timestamp_offset, flost, and 
	// rate_since_last_report. 
	double timestamp_offset;	//offset since we received data packet (t_delay)
	double timestamp_echo;		//timestamp from the last data packet (t_recvdata)
	double flost;		//frequency of loss indications (p)
	double rate_since_last_report;	//what it says ... (X_recv)
	// Used in optional variants:
	int losses;		// number of losses in last RTT
	double NumFeedback_;	//number of times/RTT feedback is to be sent 
	// Used for statistics-reporting only:
	double true_loss;	// true loss event rate.  
	// Not used:
	int seqno;	 	// not sure yet
	double timestamp;		//time this nack was sent


	static int offset_;		 // offset for this header
	inline static int& offset() { 
		return offset_; 
	}
	inline static hdr_tfrc_ack* access(const Packet* p) {
		return (hdr_tfrc_ack*) p->access(offset_);
	}
};

#endif