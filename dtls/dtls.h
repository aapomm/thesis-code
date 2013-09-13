#define ns_ping_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "sctp.h"
#include "node.h"

class DtlsAgent : public Agent {
public:
	DtlsAgent();
	virtual int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*);

 /* generic list functions
 */
 void       InsertNode(List_S *, Node_S *, Node_S *, Node_S *);
 void       AddDestination(int, int);

 List_S              sDestList;
 SctpDest_S         *spPrimaryDest;       // primary destination
 SctpDest_S         *spNewTxDest;         // destination for new transmissions
};
