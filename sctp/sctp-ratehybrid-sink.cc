#ifndef lint
static const char rcsid[] =
"@(#) $Header: /cvsroot/nsnam/ns-2/sctp/sctp-ratehybrid.cc,v 1.5 2013/12/10 05:51:27 aaron_manaloto Exp $ (UD/PEL)";
#endif

#include "ip.h"
#include "sctp-ratehybrid-sink.h"
#include "flags.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define	MIN(x,y)	(((x)<(y))?(x):(y))
#define	MAX(x,y)	(((x)>(y))?(x):(y))

static class SctpRateHybridSinkClass : public TclClass 
{ 
public:
  SctpRateHybridSinkClass() : TclClass("Agent/SCTP/RatehybridSink") {}
  TclObject* create(int, const char*const*) 
  {
    return (new SctpRateHybridSink());
  }
} classSctpRateHybridSink;

SctpRateHybridSink::SctpRateHybridSink() : SctpAgent(), nack_timer_(this)
{
	bind("packetSize_", &size_);	
	bind("InitHistorySize_", &hsz);
	bind("NumFeedback_", &NumFeedback_);
	bind ("AdjustHistoryAfterSS_", &adjust_history_after_ss);
	bind ("printLoss_", &printLoss_);
	bind ("algo_", &algo); // algo for loss estimation
	bind ("PreciseLoss_", &PreciseLoss_);
	bind ("numPkts_", &numPkts_);
	bind("minDiscountRatio_", &minDiscountRatio_);

	// for WALI ONLY
	bind ("NumSamples_", &numsamples);
	bind ("discount_", &discount);
	bind ("smooth_", &smooth_);
	bind ("ShortIntervals_", &ShortIntervals_);
	bind ("ShortRtts_", &ShortRtts_);

	bind("bytes_", &bytes_);
	rtt_ =  0; 
	tzero_ = 0;
	last_timestamp_ = 0;
	last_arrival_ = 0;
	last_report_sent=0;
	total_received_ = 0;
	total_losses_ = 0;
	total_dropped_ = 0;

	maxseq = -1;
	maxseqList = -1;
	rcvd_since_last_report  = 0;
	losses_since_last_report = 0;
	loss_seen_yet = 0;
	lastloss = 0;
	lastloss_round_id = -1 ;
	numPktsSoFar_ = 0;

	rtvec_ = NULL;
	tsvec_ = NULL;
	lossvec_ = NULL;

	// used by WALI and EWMA
	last_sample = 0;

	// used only for WALI 
	false_sample = 0;
	sample = NULL ; 
	weights = NULL ;
	mult = NULL ;
        losses = NULL ;
	count_losses = NULL ;
        num_rtts = NULL ;
	sample_count = 1 ;
	mult_factor_ = 1.0;
	init_WALI_flag = 0;
}

SctpRateHybridSink::~SctpRateHybridSink(){
}

void SctpRateHybridSink::delay_bind_init_all()
{
  SctpAgent::delay_bind_init_all();
}

int SctpRateHybridSink::delay_bind_dispatch(const char *cpVarName, 
					  const char *cpLocalName, 
					  TclObject *opTracer)
{
  return SctpAgent::delay_bind_dispatch(cpVarName, cpLocalName, opTracer);
}

void SctpRateHybridSink::recv(Packet *opInPkt, Handler*)
{
  /* Let's make sure that a Reset() is called, because it isn't always
   * called explicitly with the "reset" command. For example, wireless
   * nodes don't automatically "reset" their agents, but wired nodes do. 
   */
  if(eState == SCTP_STATE_UNINITIALIZED)
    Reset(); 

  //DBG_I(recv);

  // printf("time: %lf\n", Scheduler::instance().clock());
  // getchar();

  hdr_ip *spIpHdr = hdr_ip::access(opInPkt);
  PacketData *opInPacketData = (PacketData *) opInPkt->userdata();
  u_char *ucpInData = opInPacketData->data();
  u_char *ucpCurrInChunk = ucpInData;
  int iRemainingDataLen = opInPacketData->size();

  u_char *ucpOutData = new u_char[uiMaxPayloadSize];
  u_char *ucpCurrOutData = ucpOutData;

  /* local variable which maintains how much data has been filled in the 
   * current outgoing packet
   */
  int iOutDataSize = 0; 

  memset(ucpOutData, 0, uiMaxPayloadSize);
  memset(spSctpTrace, 0,
   (uiMaxPayloadSize / sizeof(SctpChunkHdr_S)) * sizeof(SctpTrace_S) );

  spReplyDest = GetReplyDestination(spIpHdr);

  eStartOfPacket = TRUE;

  // compute TFRC response
  processTFRCResponse(opInPkt);
  do
    {
      //DBG_PL(recv, "iRemainingDataLen=%d"), iRemainingDataLen DBG_PR;

      /* processing chunks may need to generate response chunks, so the
       * current outgoing packet *may* be filled in and our out packet's data
       * size is incremented to reflect the new data
       */
      iOutDataSize += ProcessChunk(ucpCurrInChunk, &ucpCurrOutData);
      NextChunk(&ucpCurrInChunk, &iRemainingDataLen);
    }
  while(ucpCurrInChunk != NULL);

  /* Let's see if we have any response chunks(currently only handshake related)
   * to transmit. 
   *
   * Note: We don't bundle these responses (yet!)
   */
  if(iOutDataSize > 0) 
    {
      SendPacket(ucpOutData, iOutDataSize, spReplyDest);
      //DBG_PL(recv, "responded with control chunk(s)") DBG_PR;
    }

  /* Let's check to see if we need to generate and send a SACK chunk.
   *
   * Note: With uni-directional traffic, SACK and DATA chunks will not be 
   * bundled together in one packet.
   * Perhaps we will implement this in the future?  
   */
  if(eSackChunkNeeded == TRUE)
    {
      memset(ucpOutData, 0, uiMaxPayloadSize);
      iOutDataSize = BundleControlChunks(ucpOutData);

      /* PN: 6/17/2007
       * Send NR-Sacks instead of Sacks?
       */
      if (eUseNonRenegSacks == TRUE) 
  {
    iOutDataSize += GenChunk(SCTP_CHUNK_NRSACK, ucpOutData+iOutDataSize);
  }
      else
  {
    iOutDataSize += GenChunk(SCTP_CHUNK_SACK, ucpOutData+iOutDataSize);
  }

      SendPacket(ucpOutData, iOutDataSize, spReplyDest);

      if (eUseNonRenegSacks == TRUE)
  {
    //DBG_PL(recv, "NR-SACK sent (%d bytes)"), iOutDataSize DBG_PR;
  }
      else
  {
    //DBG_PL(recv, "SACK sent (%d bytes)"), iOutDataSize DBG_PR;
  }

      eSackChunkNeeded = FALSE;  // reset AFTER sent (o/w breaks dependencies)
    }

  /* Do we need to transmit a FORWARD TSN chunk??
   */
  if(eForwardTsnNeeded == TRUE)
    {
      memset(ucpOutData, 0, uiMaxPayloadSize);
      iOutDataSize = BundleControlChunks(ucpOutData);
      iOutDataSize += GenChunk(SCTP_CHUNK_FORWARD_TSN,ucpOutData+iOutDataSize);
      SendPacket(ucpOutData, iOutDataSize, spNewTxDest);
      //DBG_PL(recv, "FORWARD TSN chunk sent") DBG_PR;
      eForwardTsnNeeded = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

  /* Do we want to send out new DATA chunks in addition to whatever we may have
   * already transmitted? If so, we can only send new DATA if no marked chunks
   * are pending retransmission.
   * 
   * Note: We aren't bundling with what was sent above, but we could. Just 
   * avoiding that for now... why? simplicity :-)
   */
  if(eSendNewDataChunks == TRUE && eMarkedChunksPending == FALSE) 
    {
      SendMuch();       // Send new data till our cwnd is full!
      eSendNewDataChunks = FALSE; // reset AFTER sent (o/w breaks dependencies)
    }

  delete hdr_sctp::access(opInPkt)->SctpTrace();
  hdr_sctp::access(opInPkt)->SctpTrace() = NULL;
  Packet::free(opInPkt);
  opInPkt = NULL;
  delete [] ucpOutData;
}

void SctpRateHybridSink::processTFRCResponse(Packet *pkt)
{
  hdr_sctp* tfrch = hdr_sctp::access(pkt);
	hdr_flags* hf = hdr_flags::access(pkt);
	double now = Scheduler::instance().clock();
	double p = -1;
	int ecnEvent = 0;
	int congestionEvent = 0;
	int UrgentFlag = 0;	// send loss report immediately

	if (algo == WALI && !init_WALI_flag) {
		init_WALI () ;
	}
	rcvd_since_last_report ++;
	total_received_ ++;
	// bytes_ was added by Tom Phelan, for reporting bytes received.
	bytes_ += hdr_cmn::access(pkt)->size();

	if (maxseq < 0) {
		// This is the first data packet.
    maxseq = tfrch->seqno - 1 ;
		maxseqList = tfrch->seqno;
		rtvec_=(double *)malloc(sizeof(double)*hsz);
		tsvec_=(double *)malloc(sizeof(double)*hsz);
		lossvec_=(char *)malloc(sizeof(double)*hsz);
		if (rtvec_ && lossvec_) {
			int i;
			for (i = 0; i < hsz ; i ++) {
				lossvec_[i] = UNKNOWN;
				rtvec_[i] = -1; 
				tsvec_[i] = -1; 
			}
		}
		else {
			printf ("error allocating memory for packet buffers\n");
			abort (); 
		}
	}
	/* for the time being, we will ignore out of order and duplicate 
	   packets etc. */
  int seqno = tfrch->seqno ;
	fsize_ = tfrch->fsize;
	int oldmaxseq = maxseq;
	// if this is the highest packet yet, or an unknown packet
	//   between maxseqList and maxseq  
	if ((seqno > maxseq) || 
	  (seqno > maxseqList && lossvec_[seqno%hsz] == UNKNOWN )) {
		if (seqno > maxseqList + 1)
			++ numPktsSoFar_;
		  UrgentFlag = tfrch->UrgentFlag;
		  round_id = tfrch->round_id ;
	    rtt_=tfrch->rtt;
		  tzero_=tfrch->tzero;
		  psize_=tfrch->psize;
		  last_arrival_=now;
		  last_timestamp_=tfrch->timestamp;
		rtvec_[seqno%hsz]=now;	
		tsvec_[seqno%hsz]=last_timestamp_;	
		if (hf->ect() == 1 && hf->ce() == 1) {
			// ECN action
			lossvec_[seqno%hsz] = ECN_RCVD;
			++ total_losses_;
			losses_since_last_report++;
			if (new_loss(seqno, tsvec_[seqno%hsz])) {
				ecnEvent = 1;
				lossvec_[seqno%hsz] = ECNLOST;
			} 
			if (algo == WALI) {
                       		++ losses[0];
			}
		} else lossvec_[seqno%hsz] = RCVD;
	}
	if (seqno > maxseq) {
		int i = maxseq + 1;
		while (i < seqno) {
			// Added 3/1/05 in case we have wrapped around
			//   in packet sequence space.
			lossvec_[i%hsz] = UNKNOWN;
			++ i;
			++ total_losses_;
			++ total_dropped_;
		}
	}
	if (seqno > maxseqList && 
	  (ecnEvent || numPktsSoFar_ >= numPkts_ ||
	     tsvec_[seqno%hsz] - tsvec_[maxseqList%hsz] > rtt_)) {
		// numPktsSoFar_ >= numPkts_:
		// Number of pkts since we last entered this procedure
		//   at least equal numPkts_, the number of non-sequential 
		//   packets that must be seen before inferring loss.
		// maxseqList: max seq number checked for dropped packets
		// Decide which losses begin new loss events.
		int i = maxseqList ;
		while(i < seqno) {
			if (lossvec_[i%hsz] == UNKNOWN) {
				rtvec_[i%hsz]=now;	
				tsvec_[i%hsz]=estimate_tstamp(oldmaxseq, seqno, i);	
				if (new_loss(i, tsvec_[i%hsz])) {
					congestionEvent = 1;
					lossvec_[i%hsz] = LOST;
				} else {
					// This lost packet is marked "NOT_RCVD"
					// as it does not begin a loss event.
					lossvec_[i%hsz] = NOT_RCVD; 
				}
				if (algo == WALI) {
			    		++ losses[0];
				}
				losses_since_last_report++;
			}
			i++;
		}
		maxseqList = seqno;
		numPktsSoFar_ = 0;
	} else if (seqno == maxseqList + 1) {
		maxseqList = seqno;
		numPktsSoFar_ = 0;
	} 
	if (seqno > maxseq) {
		 maxseq = tfrch->seqno ;
		// if we are in slow start (i.e. (loss_seen_yet ==0)), 
		// and if we saw a loss, report it immediately
		if ((algo == WALI) && (loss_seen_yet ==0) && 
		  (tfrch->seqno - oldmaxseq > 1 || ecnEvent )) {
			UrgentFlag = 1 ; 
			loss_seen_yet = 1;
			if (adjust_history_after_ss) {
				p = adjust_history(tfrch->timestamp);
			}

		}
		if ((rtt_ > SMALLFLOAT) && 
			(now - last_report_sent >= rtt_/NumFeedback_)) {
			UrgentFlag = 1 ;
		}
	}
	if (UrgentFlag || ecnEvent || congestionEvent) {
    sendReport = true;
	}
}

/*
 * Schedule sending this report, and set timer for the next one.
 */
//void SctpRateHybridSink::nextpkt(Packet* pkt, double p) {

//	sendpkt(pkt, p);

	/* schedule next report rtt/NumFeedback_ later */
	/* note from Sally: why is this 1.5 instead of 1.0? */
	// if (rtt_ > 0.0 && NumFeedback_ > 0) 
	//	nack_timer_.resched(1.5*rtt_/NumFeedback_);
//}

/*
 * Create report message, and send it.
 */
Packet* SctpRateHybridSink::addTFRCHeaders(Packet* pkt, double p)
{
	double now = Scheduler::instance().clock();

	/*don't send an ACK unless we've received new data*/
	/*if we're sending slower than one packet per RTT, don't need*/
	/*multiple responses per data packet.*/
        /*
	 * Do we want to send a report even if we have not received
	 * any new data?
         */ 

	if (last_arrival_ >= last_report_sent) {

		if (pkt == NULL) {
			printf ("error allocating packet\n");
			abort(); 
		}
	
		hdr_sctp *tfrc_ackh = hdr_sctp::access(pkt);
	
		tfrc_ackh->seqno=maxseq;
		tfrc_ackh->timestamp_echo=last_timestamp_;
		tfrc_ackh->timestamp_offset=now-last_arrival_;
		tfrc_ackh->timestamp=now;
		tfrc_ackh->NumFeedback_ = NumFeedback_;
		if (p < 0) 
			tfrc_ackh->flost = est_loss (); 
		else
			tfrc_ackh->flost = p;
		tfrc_ackh->rate_since_last_report = est_thput ();
		tfrc_ackh->losses = losses_since_last_report;
		if (total_received_ <= 0) 
			tfrc_ackh->true_loss = 0.0;
		else 
			tfrc_ackh->true_loss = 1.0 * 
			    total_losses_/(total_received_+total_dropped_);
		last_report_sent = now; 
		rcvd_since_last_report = 0;
		losses_since_last_report = 0;
    return pkt;
	}

	return NULL;
}
/*
 * This takes as input the packet drop rate, and outputs the sending 
 *   rate in bytes per second.
 */
double SctpRateHybridSink::p_to_b(double p, double rtt, double tzero, int psize, int bval) 
{
	double tmp1, tmp2, res;

	if (p < 0 || rtt < 0) {
		return MAXRATE ; 
	}
	res=rtt*sqrt(2*bval*p/3);
	tmp1=3*sqrt(3*bval*p/8);
	if (tmp1>1.0) tmp1=1.0;
	tmp2=tzero*p*(1+32*p*p);
	res+=tmp1*tmp2;
//	if (formula_ == 1 && p > 0.25) { 
//		// Get closer to RFC 3714, Table 1.
//		// This is for TCP-friendliness with a TCP flow without ECN
//		//   and without timestamps.
//		// Just for experimental purposes. 
//		if p > 0.70) {
//			res=res*18.0;
//		} else if p > 0.60) {
//			res=res*7.0;
//		} else if p > 0.50) {
//			res=res*5.0;
//		} else if p > 0.45) {
//			res=res*4.0;
//		} else if p > 0.40) {
//			res=res*3.0;
//		} else if p > 0.25) {
//			res=res*2.0;
//		}
//	}
	// At this point, 1/res gives the sending rate in pps:
	// 1/(rtt*sqrt(2*bval*p/3) + 3*sqrt(3*bval*p/8)*tzero*p*(1+32*p*p))
	if (res < SAMLLFLOAT) {
		res=MAXRATE;
	} else {
		// change from 1/pps to Bps.
		res=psize/res;
	}
	if (res > MAXRATE) {
		res = MAXRATE ; 
	}
	return res;
}

double SctpRateHybridSink::b_to_p(double b, double rtt, double tzero, int psize, int bval) 
{
	double p, pi, bres;
	int ctr=0;
	p=0.5;pi=0.25;
	while(1) {
		bres=p_to_b(p,rtt,tzero,psize, bval);
		/*
		 * if we're within 5% of the correct value from below, this is OK
		 * for this purpose.
		 */
		if ((bres>0.95*b)&&(bres<1.05*b)) 
			return p;
		if (bres>b) {
			p+=pi;
		} else {
			p-=pi;
		}
		pi/=2.0;
		ctr++;
		if (ctr>30) {
			return p;
		}
	}
}

int SctpRateHybridSink::command(int argc, const char*const* argv)
{
  double dCurrTime = Scheduler::instance().clock();

  Tcl& oTcl = Tcl::instance();
  Node *opNode = NULL;
  int iNsAddr;
  int iNsPort;
  NsObject *opTarget = NULL;
  NsObject *opLink = NULL;
  int iRetVal;

  if(argc == 2)   
    {
      if (strcmp(argv[1], "reset") == 0) 
	{
	  Reset();
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "close") == 0) 
	{
	  Close();
	  return (TCL_OK);
	}
    }
  else if(argc == 3) 
    {
      if (strcmp(argv[1], "advance") == 0) 
	{
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "set-multihome-core") == 0) 
	{ 
	  opCoreTarget = (Classifier *) TclObject::lookup(argv[2]);
	  if(opCoreTarget == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[4]);
	      return (TCL_ERROR);
	    }
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "set-primary-destination") == 0)
	{
	  opNode = (Node *) TclObject::lookup(argv[2]);
	  if(opNode == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[2]);
	      return (TCL_ERROR);
	    }
	  iRetVal = SetPrimary( opNode->address() );

	  if(iRetVal == TCL_ERROR)
	    {
	      fprintf(stderr, "[SctpAgent::command] ERROR:"
		      "%s is not a valid destination\n", argv[2]);
	      return (TCL_ERROR);
	    }
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "force-source") == 0)
	{
	  opNode = (Node *) TclObject::lookup(argv[2]);
	  if(opNode == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[2]);
	      return (TCL_ERROR);
	    }
	  iRetVal = ForceSource( opNode->address() );

	  if(iRetVal == TCL_ERROR)
	    {
	      fprintf(stderr, "[SctpAgent::command] ERROR:"
		      "%s is not a valid source\n", argv[2]);
	      return (TCL_ERROR);
	    }
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "print") == 0) 
	{
	  if(eTraceAll == TRUE)
	    TraceAll();
	  else 
	    TraceVar(argv[2]);
	  return (TCL_OK);
	}
	else if (strcmp(argv[1], "weights") == 0) {
			/* 
			 * weights is a string of numbers, seperated by + signs
			 * the firs number is the total number of weights.
			 * the rest of them are the actual weights
			 * this overrides the defaults
			 */
			char *w ;
			w = (char *)calloc(strlen(argv[2])+1, sizeof(char)) ;
			if (w == NULL) {
				printf ("error allocating w\n");
				abort();
			}
			strcpy(w, (char *)argv[2]);
			numsamples = atoi(strtok(w,"+"));
			sample = (int *)malloc((numsamples+1)*sizeof(int));
			losses = (int *)malloc((numsamples+1)*sizeof(int));
                        count_losses = (int *)malloc((numsamples+1)*sizeof(int));
                        num_rtts = (int *)malloc((numsamples+1)*sizeof(int));
			weights = (double *)malloc((numsamples+1)*sizeof(double));
			mult = (double *)malloc((numsamples+1)*sizeof(double));
			fflush(stdout);
			if (sample && weights) {
				int count = 0 ;
				while (count < numsamples) {
					sample[count] = 0;
					losses[count] = 1;
					count_losses[count] = 0;
                                        num_rtts[count] = 0;
					mult[count] = 1;
					char *w;
					w = strtok(NULL, "+");
					if (w == NULL)
						break ; 
					else {
						weights[count++] = atof(w);
					}	
				}
				if (count < numsamples) {
					printf ("error in weights string %s\n", argv[2]);
					abort();
				}
				sample[count] = 0;
				losses[count] = 1;
				count_losses[count] = 0;
				num_rtts[count] = 0;
				weights[count] = 0;
				mult[count] = 1;
				free(w);
				return (TCL_OK);
			}
			else {
				printf ("error allocating memory for smaple and weights:2\n");
				abort();
			}
		}
    }
  else if(argc == 4)
    {
      if (strcmp(argv[1], "add-multihome-destination") == 0) 
	{ 
	  iNsAddr = atoi(argv[2]);
	  iNsPort = atoi(argv[3]);
	  AddDestination(iNsAddr, iNsPort);
	  return (TCL_OK);
	}
      else if (strcmp(argv[1], "set-destination-lossrate") == 0)
	{
	  opNode = (Node *) TclObject::lookup(argv[2]);
	  if(opNode == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[2]);
	      return (TCL_ERROR);
	    }
	  iRetVal = SetLossrate( opNode->address(), atof(argv[3]) );

	  if(iRetVal == TCL_ERROR)
	    {
	      fprintf(stderr, "[SctpAgent::command] ERROR:"
		      "%s is not a valid destination\n", argv[2]);
	      return (TCL_ERROR);
	    }
	  return (TCL_OK);
	}
    }
  else if(argc == 6)
    {
      if (strcmp(argv[1], "add-multihome-interface") == 0) 
	{ 
	  iNsAddr = atoi(argv[2]);
	  iNsPort = atoi(argv[3]);
	  opTarget = (NsObject *) TclObject::lookup(argv[4]);
	  if(opTarget == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[4]);
	      return (TCL_ERROR);
	    }
	  opLink = (NsObject *) TclObject::lookup(argv[5]);
	  if(opLink == NULL) 
	    {
	      oTcl.resultf("no such object %s", argv[5]);
	      return (TCL_ERROR);
	    }
	  AddInterface(iNsAddr, iNsPort, opTarget, opLink);
	  return (TCL_OK);
	}
    }
  return (Agent::command(argc, argv));
}

void SctpRateHybridSink::SendPacket(u_char *ucpData, int iDataSize, SctpDest_S *spDest)
{
  Node_S *spNewNode = NULL;
  Packet *opPacket = NULL;
  PacketData *opPacketData = NULL;

  SetSource(spDest); // set src addr, port, target based on "routing table"
  SetDestination(spDest); // set dest addr & port 

  opPacket = allocpkt();
  opPacketData = new PacketData(iDataSize);
  memcpy(opPacketData->data(), ucpData, iDataSize);
  opPacket->setdata(opPacketData);
  hdr_cmn::access(opPacket)->size() = iDataSize + SCTP_HDR_SIZE+uiIpHeaderSize;

  hdr_sctp::access(opPacket)->NumChunks() = uiNumChunks;
  hdr_sctp::access(opPacket)->SctpTrace() = new SctpTrace_S[uiNumChunks];
  memcpy(hdr_sctp::access(opPacket)->SctpTrace(), spSctpTrace, 
	 (uiNumChunks * sizeof(SctpTrace_S)) );

  uiNumChunks = 0; // reset the counter

  if (sendReport == true)
  { 
    opPacket = addTFRCHeaders(opPacket, p);
    sendReport = false;
  }

  if(dRouteCalcDelay == 0) // simulating reactive routing overheads?
    {
      send(opPacket, 0); // no... so send immediately
    }
  else
    {
      if(spDest->eRouteCached == TRUE)  
	{
	  /* Since the route is "cached" already, we can reschedule the route
	   * flushing and send the packet immediately.
	   */
	  spDest->opRouteCacheFlushTimer->resched(dRouteCacheLifetime);
	  send(opPacket, 0);
	}
      else
	{
	  /* The route isn't cached, so queue the packet and "calculate" the 
	   * route.
	   */
	  spNewNode = new Node_S;
	  spNewNode->eType = NODE_TYPE_PACKET_BUFFER;
	  spNewNode->vpData = opPacket;
	  InsertNode(&spDest->sBufferedPackets,spDest->sBufferedPackets.spTail,
		     spNewNode, NULL);

	  if(spDest->opRouteCalcDelayTimer->status() != TIMER_PENDING)
	    spDest->opRouteCalcDelayTimer->sched(dRouteCalcDelay);
	}
    }
}

/*
 * This is a new loss event if it is at least an RTT after the beginning
 *   of the last one.
 * If PreciseLoss_ is set, the new_loss also checks that there is a
 *     new round_id.
 * The sender updates the round_id when it receives a new report from
 *   the receiver, and when it reduces its rate after no feedback.
 * Sometimes the rtt estimates can be less than the actual RTT, and
 *   the round_id will catch this.  This can be useful if the actual
 *   RTT increases dramatically.
 */
int SctpRateHybridSink::new_loss(int i, double tstamp)
{
	double time_since_last_loss_interval = tsvec_[i%hsz]-lastloss;
	if ((time_since_last_loss_interval > rtt_)
	     && (PreciseLoss_ == 0 || (round_id > lastloss_round_id))) {
		lastloss = tstamp;
		lastloss_round_id = round_id ;
                if (time_since_last_loss_interval < ShortRtts_ * rtt_ &&
				algo == WALI) {
                        count_losses[0] = 1;
                }
                if (rtt_ > 0 && algo == WALI) {
                        num_rtts[0] = (int) ceil(time_since_last_loss_interval / rtt_);
                        if (num_rtts[0] < 1) num_rtts[0] = 1;
                }
		return TRUE;
	} else return FALSE;
}

double SctpRateHybridSink::estimate_tstamp(int before, int after, int i)
{
	double delta = (tsvec_[after%hsz]-tsvec_[before%hsz])/(after-before) ; 
	double tstamp = tsvec_[before%hsz]+(i-before)*delta ;
	return tstamp;
}

double SctpRateHybridSink::est_thput () 
{
	double time_for_rcv_rate;
	double now = Scheduler::instance().clock();
	double thput = 1 ;

	if ((rtt_ > 0) && ((now - last_report_sent) >= rtt_)) {
		// more than an RTT since the last report
		time_for_rcv_rate = (now - last_report_sent);
		if (rcvd_since_last_report > 0) {
			thput = rcvd_since_last_report/time_for_rcv_rate;
		}
	}
	else {
		// count number of packets received in the last RTT
		if (rtt_ > 0){
			double last = rtvec_[maxseq%hsz]; 
			int rcvd = 0;
			int i = maxseq;
			while (i > 0) {
				if (lossvec_[i%hsz] == RCVD) {
					if ((rtvec_[i%hsz] + rtt_) > last) 
						rcvd++; 
					else
						break ;
				}
				i--; 
			}
			if (rcvd > 0)
				thput = rcvd/rtt_; 
		}
	}
	return thput ;
}

void SctpRateHybridSink::print_loss(int sample, double ave_interval)
{
	double now = Scheduler::instance().clock();
	double drops = 1/ave_interval;
	// This is ugly to include this twice, but the first one is
	//   for backward compatibility with earlier scripts. 
	printf ("time: %7.5f loss_rate: %7.5f \n", now, drops);
	printf ("time: %7.5f sample 0: %5d loss_rate: %7.5f \n", 
		now, sample, drops);
	//printf ("time: %7.5f send_rate: %7.5f\n", now, sendrate);
	//printf ("time: %7.5f maxseq: %d\n", now, maxseq);
}

void SctpRateHybridSink::print_loss_all(int *sample) 
{
	double now = Scheduler::instance().clock();
	printf ("%f: sample 0: %5d 1: %5d 2: %5d 3: %5d 4: %5d\n", 
		now, sample[0], sample[1], sample[2], sample[3], sample[4]); 
}

void SctpRateHybridSink::print_losses_all(int *losses) 
{
	double now = Scheduler::instance().clock();
	printf ("%f: losses 0: %5d 1: %5d 2: %5d 3: %5d 4: %5d\n", 
		now, losses[0], losses[1], losses[2], losses[3], losses[4]); 
}

void SctpRateHybridSink::print_count_losses_all(int *count_losses) 
{
	double now = Scheduler::instance().clock();
	printf ("%f: count? 0: %5d 1: %5d 2: %5d 3: %5d 4: %5d\n", 
		now, count_losses[0], count_losses[1], count_losses[2], count_losses[3], count_losses[4]); 
}

void SctpRateHybridSink::print_num_rtts_all(int *) 
{
	double now = Scheduler::instance().clock();
	printf ("%f: rtts 0: %5d 1: %5d 2: %5d 3: %5d 4: %5d\n", 
 	   now, num_rtts[0], num_rtts[1], num_rtts[2], num_rtts[3], num_rtts[4]); 
}


double SctpRateHybridSink::est_loss () 
{	
	double p = 0 ;
	switch (algo) {
		case WALI:
			p = est_loss_WALI () ;
			break;
		default:
			printf ("invalid algo specified\n");
			abort();
			break ; 
	}
	return p;
}


double SctpRateHybridSink::est_loss_WALI () 
{
	int i;
	double ave_interval1, ave_interval2; 
	int ds ; 
		
	if (!init_WALI_flag) {
		init_WALI () ;
	}
	// sample[i] counts the number of packets in the i-th loss interval
	// sample[0] contains the most recent sample.
        // losses[i] contains the number of losses in the i-th loss interval
        // count_losses[i] is 1 if the i-th loss interval is short.
        // num_rtts[i] contains the number of rtts in the i-th loss interval
	for (i = last_sample; i <= maxseq ; i ++) {
		sample[0]++;
		if (lossvec_[i%hsz] == LOST || lossvec_[i%hsz] == ECNLOST) {
		        //  new loss event
			sample_count ++;
			shift_array (sample, numsamples+1, 0); 
			shift_array (losses, numsamples+1, 1); 
			shift_array (count_losses, numsamples+1, 1); 
			shift_array (num_rtts, numsamples+1, 0); 
			multiply_array(mult, numsamples+1, mult_factor_);
			shift_array (mult, numsamples+1, 1.0); 
			mult_factor_ = 1.0;
		}
	}
	last_sample = maxseq+1 ; 
	double now = Scheduler::instance().clock();
        //if (ShortIntervals_ > 0 && printLoss_ > 0) {
        //    printf ("now: %5.2f lastloss: %5.2f ShortRtts_: %d rtt_: %5.2f\n",
        //         now, lastloss, ShortRtts_, rtt_);
        //}
        if (ShortIntervals_ > 0 && 
            now - lastloss > ShortRtts_ * rtt_) {
              // Check if the current loss interval is short.
              count_losses[0] = 0;
        }
        if (ShortIntervals_ > 0 && rtt_ > 0) {
              // Count number of rtts in current loss interval.
              num_rtts[0] = (int) ceil((now - lastloss) / rtt_);
              if (num_rtts[0] < 1) num_rtts[0] = 1;
        }
	if (sample_count>numsamples+1)
		// The array of loss intervals is full.
		ds=numsamples+1;
    	else
		ds=sample_count;

	if (sample_count == 1 && false_sample == 0) 
		// no losses yet
		return 0; 
	/* do we need to discount weights? */
	if (sample_count > 1 && discount && sample[0] > 0) {
                double ave = weighted_average1(1, ds, 1.0, mult, weights, sample, ShortIntervals_, losses, count_losses, num_rtts);
		int factor = 2;
		double ratio = (factor*ave)/sample[0];
		if ( ratio < 1.0) {
			// the most recent loss interval is very large
			mult_factor_ = ratio;
			double min_ratio = minDiscountRatio_;
			if (mult_factor_ < min_ratio) 
				mult_factor_ = min_ratio;
		}
	}
	// Calculations including the most recent loss interval.
        ave_interval1 = weighted_average1(0, ds, mult_factor_, mult, weights, sample, ShortIntervals_, losses, count_losses, num_rtts);
        // Calculations not including the most recent loss interval.
        ave_interval2 = weighted_average1(1, ds, mult_factor_, mult, weights, sample, ShortIntervals_, losses, count_losses, num_rtts);
	// The most recent loss interval does not end in a loss
	// event.  Include the most recent interval in the 
	// calculations only if this increases the estimated loss
	// interval. 
        // If ShortIntervals is less than 10, do not count the most
        //   recent interval if it is a short interval.
        //   Values of ShortIntervals greater than 10 are only for
        //   validation purposes, and for backwards compatibility.
        //
	if (ave_interval2 > ave_interval1 ||
             (ShortIntervals_ > 1 && ShortIntervals_ < 10 
                     && count_losses[0] == 1))
                // The second condition is to check if the first interval
                //  is a short interval.  If so, we must use ave_interval2.
		ave_interval1 = ave_interval2;
	if (ave_interval1 > 0) { 
		if (printLoss_ > 0) {
			print_loss(sample[0], ave_interval1);
			print_loss_all(sample);
			if (ShortIntervals_ > 0) {
				print_losses_all(losses);
				print_count_losses_all(count_losses);
                                print_num_rtts_all(num_rtts);
			}
		}
		return 1/ave_interval1; 
	} else return 999;     
}

// Calculate the weighted average.
double SctpRateHybridSink::weighted_average(int start, int end, double factor, double *m, double *w, int *sample)
{
	int i; 
	double wsum = 0;
	double answer = 0;
	if (smooth_ == 1 && start == 0) {
		if (end == numsamples+1) {
			// the array is full, but we don't want to uses
			//  the last loss interval in the array
			end = end-1;
		} 
		// effectively shift the weight arrays 
		for (i = start ; i < end; i++) 
			if (i==0)
				wsum += m[i]*w[i+1];
			else 
				wsum += factor*m[i]*w[i+1];
		for (i = start ; i < end; i++)  
			if (i==0)
			 	answer += m[i]*w[i+1]*sample[i]/wsum;
			else 
				answer += factor*m[i]*w[i+1]*sample[i]/wsum;
	        return answer;

	} else {
		for (i = start ; i < end; i++) 
			if (i==0)
				wsum += m[i]*w[i];
			else 
				wsum += factor*m[i]*w[i];
		for (i = start ; i < end; i++)  
			if (i==0)
			 	answer += m[i]*w[i]*sample[i]/wsum;
			else 
				answer += factor*m[i]*w[i]*sample[i]/wsum;
	        return answer;
	}
}

int SctpRateHybridSink::get_sample(int oldSample, int numLosses) 
{
	int newSample;
	if (numLosses == 0) {
		newSample = oldSample;
	} else {
		newSample = oldSample / numLosses;
	}
	return newSample;
}

int SctpRateHybridSink::get_sample_rtts(int oldSample, int numLosses, int rtts) 
{
	int newSample;
	if (numLosses == 0) {
		newSample = oldSample;
                //printf ("sample: %d numLosses: %d\n", oldSample, numLosses);
	} else {
                double fraction;
                if (ShortRtts_ != 0)
                     fraction = (ShortRtts_ + 1.0 - rtts) / ShortRtts_;
                else fraction = 1.0;
		int numLoss = (int) (floor(fraction * numLosses ));
                if (numLoss != 0)
		  newSample = oldSample / numLoss;
                else newSample = oldSample;
                //printf ("sample: %d rtts: %d numLosses: %d newSample: %d fraction: %5.2f numLoss %d\n",
                //  oldSample, rtts, numLosses, newSample, fraction, numLoss);
	}
	return newSample;
}

// Calculate the weighted average, factor*m[i]*w[i]*sample[i]/wsum.
// "factor" is "mult_factor_", for weighting the most recent interval
//    when it is very large
// "m[i]" is "mult[]", for old values of "mult_factor_".
//
// When ShortIntervals_%10 is 1, the length of a loss interval is
//   "sample[i]/losses[i]" for short intervals, not just "sample[i]".
//   This is equivalent to a loss event rate of "losses[i]/sample[i]",
//   instead of "1/sample[i]".
//
// When ShortIntervals_%10 is 2, it is like ShortIntervals_ of 1,
//   except that the number of losses per loss interval is at
//   most 1460/byte-size-of-small-packets.
//
// When ShortIntervals_%10 is 3, short intervals are up to three RTTs,
//   and the number of losses counted is a function of the interval size.
//
double SctpRateHybridSink::weighted_average1(int start, int end, double factor, double *m, double *w, int *sample, int ShortIntervals, int *losses, int *count_losses, int *num_rtts)
{
        int i;
        int ThisSample;
        double wsum = 0;
        double answer = 0;
        if (smooth_ == 1 && start == 0) {
                if (end == numsamples+1) {
                        // the array is full, but we don't want to use
                        //  the last loss interval in the array
                        end = end-1;
                }
                // effectively shift the weight arrays
                for (i = start ; i < end; i++)
                        if (i==0)
                                wsum += m[i]*w[i+1];
                        else
                                wsum += factor*m[i]*w[i+1];
                for (i = start ; i < end; i++) {
                        ThisSample = sample[i];
                        if (ShortIntervals%10 == 1 && count_losses[i] == 1) {
			       ThisSample = get_sample(sample[i], losses[i]);
                        }
                        if (ShortIntervals%10 == 2 && count_losses[i] == 1) {
			       int adjusted_losses = int(fsize_/size_);
			       if (losses[i] < adjusted_losses) {
					adjusted_losses = losses[i];
			       }
			       ThisSample = get_sample(sample[i], adjusted_losses);
                        }
                        if (ShortIntervals%10 == 3 && count_losses[i] == 1) {
			       ThisSample = get_sample_rtts(sample[i], losses[i], num_rtts[i]);
                        }
                        if (i==0)
                                answer += m[i]*w[i+1]*ThisSample/wsum;
                                //answer += m[i]*w[i+1]*sample[i]/wsum;
                        else
                                answer += factor*m[i]*w[i+1]*ThisSample/wsum;
                                //answer += factor*m[i]*w[i+1]*sample[i]/wsum;
		}
                return answer;

        } else {
                for (i = start ; i < end; i++)
                        if (i==0)
                                wsum += m[i]*w[i];
                        else
                                wsum += factor*m[i]*w[i];
                for (i = start ; i < end; i++) {
                       ThisSample = sample[i];
                       if (ShortIntervals%10 == 1 && count_losses[i] == 1) {
			       ThisSample = get_sample(sample[i], losses[i]);
                       }
                       if (ShortIntervals%10 == 2 && count_losses[i] == 1) {
			       ThisSample = get_sample(sample[i], 7);
			       // Replace 7 by 1460/packet size.
                               // NOT FINISHED.
                       }
                        if (ShortIntervals%10 == 3 && count_losses[i] == 1) {
			       ThisSample = get_sample_rtts(sample[i], losses[i], (int) num_rtts[i]);
                        }
                       if (i==0)
                                answer += m[i]*w[i]*ThisSample/wsum;
                                //answer += m[i]*w[i]*sample[i]/wsum;
                        else
                                answer += factor*m[i]*w[i]*ThisSample/wsum;
                                //answer += factor*m[i]*w[i]*sample[i]/wsum;
		}
                return answer;
        }
}

// Shift array a[] up, starting with a[sz-2] -> a[sz-1].
void SctpRateHybridSink::shift_array(int *a, int sz, int defval) 
{
	int i ;
	for (i = sz-2 ; i >= 0 ; i--) {
		a[i+1] = a[i] ;
	}
	a[0] = defval;
}
void SctpRateHybridSink::shift_array(double *a, int sz, double defval) 
{
	int i ;
	for (i = sz-2 ; i >= 0 ; i--) {
		a[i+1] = a[i] ;
	}
	a[0] = defval;
}

// Multiply array by value, starting with array index 1.
// Array index 0 of the unshifted array contains the most recent interval.
void SctpRateHybridSink::multiply_array(double *a, int sz, double multiplier) {
	int i ;
	for (i = 1; i <= sz-1; i++) {
		double old = a[i];
		a[i] = old * multiplier ;
	}
}

/*
 * We just received our first loss, and need to adjust our history.
 */
double SctpRateHybridSink::adjust_history (double ts)
{
	int i;
	double p;
	for (i = maxseq; i >= 0 ; i --) {
		if (lossvec_[i%hsz] == LOST || lossvec_[i%hsz] == ECNLOST ) {
			lossvec_[i%hsz] = NOT_RCVD; 
		}
	}
	lastloss = ts; 
	lastloss_round_id = round_id ;
	p=b_to_p(est_thput()*psize_, rtt_, tzero_, fsize_, 1);
	false_sample = (int)(1.0/p);
	sample[1] = false_sample;
	sample[0] = 0;
	losses[1] = 0;
	losses[0] = 1;
	count_losses[1] = 0;
	count_losses[0] = 0;
        num_rtts[0]=0;
        num_rtts[1]=0;
	sample_count++; 
	if (printLoss_) {
		print_loss_all (sample);
		if (ShortIntervals_ > 0) {
			print_losses_all(losses);
			print_count_losses_all(count_losses);
			print_num_rtts_all(num_rtts);
		}
	}
	false_sample = -1 ; 
	return p;
}


/*
 * Initialize data structures for weights.
 */
void SctpRateHybridSink::init_WALI () {
	int i;
	if (numsamples < 0)
		numsamples = DEFAULT_NUMSAMPLES ;	
	if (smooth_ == 1) {
		numsamples = numsamples + 1;
	}
	sample = (int *)malloc((numsamples+1)*sizeof(int));
        losses = (int *)malloc((numsamples+1)*sizeof(int));
        count_losses = (int *)malloc((numsamples+1)*sizeof(int));
        num_rtts = (int *)malloc((numsamples+1)*sizeof(int));
	weights = (double *)malloc((numsamples+1)*sizeof(double));
	mult = (double *)malloc((numsamples+1)*sizeof(double));
	for (i = 0 ; i < numsamples+1 ; i ++) {
		sample[i] = 0 ; 
	}
	if (smooth_ == 1) {
		int mid = int(numsamples/2);
		for (i = 0; i < mid; i ++) {
			weights[i] = 1.0;
		}
		for (i = mid; i <= numsamples; i ++){
			weights[i] = 1.0 - (i-mid)/(mid + 1.0);
		}
	} else {
		int mid = int(numsamples/2);
		for (i = 0; i < mid; i ++) {
			weights[i] = 1.0;
		}
		for (i = mid; i <= numsamples; i ++){
			weights[i] = 1.0 - (i+1-mid)/(mid + 1.0);
		}
	}
	for (i = 0; i < numsamples+1; i ++) {
		mult[i] = 1.0 ; 
	}
	init_WALI_flag = 1;  /* initialization done */
}
