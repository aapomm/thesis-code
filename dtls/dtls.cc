#include "dtls.h"

static class DtlsClass : public TclClass {
public:
	DtlsClass() : TclClass("Agent/DTLS") {}
	TclObject* create(int, const char*const*) {
		return (new DtlsAgent());
	}
} class_dtls;


PingAgent::PingAgent() : Agent(PT_PING)
{
}

int PingAgent::command(int argc, const char*const* argv)
{
	return 0;
}


void PingAgent::recv(Packet* pkt, Handler*)
{
}


