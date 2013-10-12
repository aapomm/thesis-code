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
#define SCTP_TFRC_INIT_SEND_RATE 
#define SCTP_TFRC_INIT_RTO 2

class TfrcSctpAgent : public virtual SctpAgent {

	public:
		TfrcSctpAgent();
};

static class TfrcSctpClass : public TclClass {

	public:
		TfrcSctpClass() : TclClass("Agent/SCTP/TFRC") {}
		TclObject* create(int, const char*const*) {	
			return (new TfrcSctpAgent());
		}
} classSctpTfrc;

TfrcSctpAgent::TfrcSctpAgent() : SctpAgent() {

}
