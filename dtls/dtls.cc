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

/* Given params: sList, spPrevNode, spNextNode, spNewNode... insert spNewNode
 * into sList between spPrevNode and spNextNode.
 */
void DtlsAgent::InsertNode(List_S *spList, Node_S *spPrevNode, 
			  Node_S *spNewNode,  Node_S *spNextNode)
{
  if(spPrevNode == NULL)
    spList->spHead = spNewNode;
  else
    spPrevNode->spNext = spNewNode;

  spNewNode->spPrev = spPrevNode;
  spNewNode->spNext = spNextNode;

  if(spNextNode == NULL)
    spList->spTail = spNewNode;
  else
    spNextNode->spPrev = spNewNode;

  spList->uiLength++;
}

void DtlsAgent::AddDestination(int iNsAddr, int iNsPort)
{

  Node_S *spNewNode = new Node_S;
  spNewNode->eType = NODE_TYPE_DESTINATION_LIST;
  spNewNode->vpData = new SctpDest_S;

  SctpDest_S *spNewDest = (SctpDest_S *) spNewNode->vpData;
  memset(spNewDest, 0, sizeof(SctpDest_S));
  spNewDest->iNsAddr = iNsAddr;
  spNewDest->iNsPort = iNsPort;

  /* set the primary to the last destination added just in case the user does
   * not set a primary
   */
  spPrimaryDest = spNewDest;
  spNewTxDest = spPrimaryDest;

  /* allocate packet with the dest addr. to be used later for setting src dest
   
  daddr() = spNewDest->iNsAddr;
  dport() = spNewDest->iNsPort;
  spNewDest->opRoutingAssistPacket = allocpkt();
	*/
  InsertNode(&sDestList, sDestList.spTail, spNewNode, NULL);
}

int DtlsAgent::command(int argc, const char*const* argv)
{
	int iNsAddr;
	int iNsPort;

	if(argc==4){
		if (strcmp(argv[1], "add-multihome-destination") == 0) { 
			  iNsAddr = atoi(argv[2]);
			  iNsPort = atoi(argv[3]);
			  AddDestination(iNsAddr, iNsPort);
			  return (TCL_OK);
		}
	}

	return 0;
}

void DtlsAgent::recv(Packet* pkt, Handler*)
{
}