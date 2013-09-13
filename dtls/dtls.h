#define ns_ping_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "sctp.h"

class DtlsAgent : public Agent {
public:
	DtlsAgent();
	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*);
};
