#include "dtls.h"

static class DtlsClass : public TclClass {
public:
	DtlsClass() : TclClass("Agent/DTLS") {}
	TclObject* create(int, const char*const*) {
		return (new DtlsAgent());
	}
} class_dtls;


DtlsAgent::DtlsAgent() : Agent(PT_PING)
{
}

int DtlsAgent::command(int argc, const char*const* argv)
{
	return 0;
}


void DtlsAgent::recv(Packet* pkt, Handler*)
{
}
