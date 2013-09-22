#ifndef ns_dtls_h
#define ns_dtls_h

#include "config.h"
#include "object.h"
#include "agent.h"
#include "packet.h"

enum ContentType
{
	SCTP,
	UDP
};

enum ProtocolVersion
{
	_254,
	_253
};

/* dtls packet header*/
struct hdr_dtls
{ 
	ContentType type_;
	ProtocolVersion version_;
	u_int16_t epoch_;
	u_int32_t sequence_number_; //should be 48-bit uint
	u_int16_t length_;

	static int offset_;
	inline static int& offset() { return offset_; }
	inline static hdr_dtls* access(const Packet* p) 
	{
		return (hdr_dtls*) p->access(offset_);
	}

	/* per-field member functions */
	u_int16_t& epoch() { return (epoch_); }
	u_int32_t& seqno() { return (sequence_number_); }
	u_int16_t& length() { return (length_); }

};

#endif
