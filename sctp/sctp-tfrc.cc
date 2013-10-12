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

class TfrcSctpAgent : public virtual SctpAgent {

	public:
		TfrcSctpAgent();
};
