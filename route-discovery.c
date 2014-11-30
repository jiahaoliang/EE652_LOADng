/**
 * \addtogroup routediscovery
 * @{
 */

/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Route discovery protocol(Using LOADng)
 * \author
 *         {Jiahao Liang, Zhikun Liu, Haimo Bai} @ USC
 */

#include "contiki.h"
#include "net/rime.h"
#include "net/rime/route.h"
#include "net/rime/route-discovery.h"

#include <stddef.h> /* For offsetof */
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/*
//This structure stores a TLV vector of the TLV block.
struct tlv {
	uint8_t count:4;
	uint8_t type:4;
	uint8_t flags:4;
	uint8_t *value;		
};*/

//This structure stores the <message> field of a RREQ and RREPpacket
//RREQ-Specific and RREP Message
typedef struct general_message{
//TODO: addr-length maybe neglect
//	uint8_t addr-length:4;
	uint8_t type;
	uint16_t seq_num;
	//TODO: Table 8
	/*if metric_type set to 0 hop_count is used otherwise route_metric is used*/
	uint8_t metric_type;
	uint32_t route_metric;
	uint8_t hop_limit;
	uint8_t hop_count;
	linkaddr_t orginator;
	//TODO:need modified to meet TLV requirement
	linkaddr_t destination;
}rreq_message,rrep_message;

//This structure stores the <message> field of a RREP-ACK packet.
typedef struct rrep_ack_message_struture {
	//uint8_t addr-length:4;
	uint16_t rreq_num;
	//TODO:need modified to meet TLV requirement
	linkaddr_t destination;
}rrep_ack_message;

//This structure stores the <message> field of a RERR packet.
typedef struct rerr_message_struture {
	//TODO:uint8_t addr-length:4;
	uint8_t hop_limit;
	uint8_t errorcode;
	//TODO: how to implement unreachable addr
	linkaddr_t unreachable;
	linkaddr_t orginator;
	//TODO:need modified to meet TLV requirement
	linkaddr_t destination;
}rerr_message;

struct routing_set{
	linkaddr_t R_dest_addr;
	linkaddr_t R_next_addr;
    unint32_t R_metric;
    uint8_t R_metric_type;
    uint8_t hop_count;
    uint16_t R_seq_num;
    uint8_t R_bidirectional;
    linkaddr_t R_local_iface_addr;
    clock_time_t R_valid_time;
    struct routing_set *next;
}

struct local_interface_set{
	linkaddr_t I_local_iface_addr;
    struct local_interface_set *next;
}

struct blacklist_neighbor_set{
	linkaddr_t B_neighbor_address;
	clock_time_t B_valid_time;
    struct blacklist_neighbor_set *next;
}

struct destination_address_set{
	linkaddr_t D_address;
    struct destination_address_set *next;
}

struct pending_acknowledgment_set{
	linkaddr_t P_next_hop;
	linkaddr_t P_originator;
    uint16_t P_seq_num;
	uint8_t P_ack_received;
	clock_time_t P_ack_timeout;
    struct pending_acknowledgment_set *next;
}

/*---------------------------------------------------------------------------*/

	
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

#if CONTIKI_TARGET_NETSIM
#include "ether.h"
#endif


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
/*Parameters and constants*/
#define ROUTE_TIMEOUT 5 ∗ CLOCK_SECOND
#define RREP_ACK_TIMEOUT 0.1 ∗ CLOCK_SECOND
#define ROUTE_DISCOVERY_ENTRIES 8
#define MAX_RETRIES 3
#define MAX_REPAIR_RETRIES max_retries
#define NUM_REQ_ENTRIES 8
#define MEAN_BACKOFF 50
#define UNIFORM 1
#define EXPONENTIAL 1-uniform
#define ACK_REQUIRED 1
#define METRICS 0
/*Defines*/
#define MAXVALUE 255
#define VERBOSE 1
#define BACKOFF 1
#define MAXA(A,B) ( ((A>B)&&((A-B)<=(MAXVALUE/2)))||((A<B)&&((B-A)>(MAXVALUE/2))) )

/*---------------------------------------------------------------------------*/
/*check if rreq or rrep is valid return 0 means valid return -1 means invalid*/
int valid_check(struct general_message *input){
	//address check is skipped;
	struct local_interface_set *cur = local_inter;
	for(cur != NULL){
		if( cur->I_local_iface_addr == input->originator){
			reutrn -1;
		}
		cur = cur->next;
	}
	struct routing_set *cur1 = route_list_head;
	for(cur1 != NULL){
		if( (cur1->R_dest_addr == input->originator) &&MAXA(cur1->R_seq_num,input->seq_num)){
			reutrn -1;
		}
		cur1 = cur1->next;
	}
	if(input->type == RREQ_TYPE){
		struct blacklisted_neighbor_set *cur2 = black_list_head;
		for(cur2 != NULL){
			if( cur2->B_neighbor_address == input->originator){
				reutrn -1;
			}
			cur2 = cur2->next;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
static void
send_rreq(struct route_discovery_conn *c, const linkaddr_t *dest)
{
  linkaddr_t dest_copy;
  rreq_message *msg;

  linkaddr_copy(&dest_copy, dest);
  dest = &dest_copy;

  packetbuf_clear();
  msg = packetbuf_dataptr();
  packetbuf_set_datalen(sizeof(struct rreq_message));
  msg->seq_num = c->rreq_id;
  /*if metric_type set to 0 hop_count is used otherwise route_metric is used*/
  msg->metric_type = 0;
  //uint32_t route_metric;
  msg->hop_limit = c->hop_limit - 1;
  msg->hop_count = c->hop_count +1;
  msg->orginator = c->originator;
  //TODO:need modified to meet TLV requirement
  msg->destination = c->destination;
	
  linkaddr_copy(&msg->dest, dest);

  netflood_send(&c->rreqconn, c->rreq_id);
  c->rreq_id++;
}

static void
send_rrep(struct route_discovery_conn *c, const linkaddr_t *dest)
{
  rrep_message *rrepmsg;
  linkaddr_t saved_dest;
  
  linkaddr_copy(&saved_dest, dest);

  packetbuf_clear();
  dest = &saved_dest;
  rrepmsg = packetbuf_dataptr();
  packetbuf_set_datalen(sizeof(rrep_message));
  rrepmsg->hops = 0;
  linkaddr_copy(&rrepmsg->dest, dest);
  linkaddr_copy(&rrepmsg->originator, &linkaddr_node_addr);
  rt = route_lookup(dest);
  if(rt != NULL) {
    PRINTF("%d.%d: send_rrep to %d.%d via %d.%d\n",
	   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	   dest->u8[0],dest->u8[1],
	   rt->nexthop.u8[0],rt->nexthop.u8[1]);
    unicast_send(&c->rrepconn, &rt->nexthop);
  } else {
    PRINTF("%d.%d: no route for rrep to %d.%d\n",
	   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	   dest->u8[0],dest->u8[1]);
  }
}

static const struct unicast_callbacks rrep_callbacks = {rrep_packet_received};
static const struct netflood_callbacks rreq_callbacks = {rreq_packet_received, NULL, NULL};


/*---------------------------------------------------------------------------*/
//Opens a route discovery connection.
void
route_discovery_open(struct route_discovery_conn *c,
		     clock_time_t time,
		     uint16_t channels,
		     const struct route_discovery_callbacks *callbacks)
{
  netflood_open(&c->rreqconn, time, channels + 0, &rreq_callbacks);
  unicast_open(&c->rrepconn, channels + 1, &rrep_callbacks);
  c->cb = callbacks;

}
/*---------------------------------------------------------------------------*/
//Closes a route discovery connection.
void
route_discovery_close(struct route_discovery_conn *c)
{
  unicast_close(&c->rrepconn);
  netflood_close(&c->rreqconn);
  ctimer_stop(&c->t);

}

static void
rrep_packet_received(struct unicast_conn *uc, const linkaddr_t *from)
{
  struct rrep_message *msg = packetbuf_dataptr();
  linkaddr_t dest;
  struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)uc - offsetof(struct route_discovery_conn, rrepconn));

  PRINTF("%d.%d: rrep_packet_received from %d.%d towards %d.%d len %d\n",
	 linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	 from->u8[0],from->u8[1],
	 msg->dest.u8[0],msg->dest.u8[1],
	 packetbuf_datalen());

  PRINTF("from %d.%d hops %d rssi %d lqi %d\n",
	 from->u8[0], from->u8[1],
	 msg->hops,
	 packetbuf_attr(PACKETBUF_ATTR_RSSI),
	 packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));

	list_insert_next(linklist);
	if(linkaddr_cmp(&msg->dest, &linkaddr_node_addr)) {
    PRINTF("rrep for us!\n");
    rrep_pending = 0;
    ctimer_stop(&c->t);
    if(c->cb->new_route) {
      linkaddr_t originator;

      /* If the callback modifies the packet, the originator address
         will be lost. Therefore, we need to copy it into a local
         variable before calling the callback. */
      linkaddr_copy(&originator, &msg->originator);
      c->cb->new_route(c, &originator);
    }

  } else {
    linkaddr_copy(&dest, &msg->dest);

    rt = route_lookup(&msg->dest);
    if(rt != NULL) {
      PRINTF("forwarding to %d.%d\n", rt->nexthop.u8[0], rt->nexthop.u8[1]);
      msg->hops++;
      unicast_send(&c->rrepconn, &rt->nexthop);
    } else {
      PRINTF("%d.%d: no route to %d.%d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], msg->dest.u8[0], msg->dest.u8[1]);
    }
  }
}
/*---------------------------------------------------------------------------*/
static int
rreq_packet_received(struct netflood_conn *nf, const linkaddr_t *from,
		     const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
  struct rreq_message *msg = packetbuf_dataptr();
  struct route_discovery_conn *c = (struct route_discovery_conn *)
    ((char *)nf - offsetof(struct route_discovery_conn, rreqconn));

  PRINTF("%d.%d: rreq_packet_received from %d.%d hops %d rreq_id %d last %d.%d/%d\n",
	 linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1],
	 hops, msg->rreq_id,
     c->last_rreq_originator.u8[0],
     c->last_rreq_originator.u8[1],
	 c->last_rreq_id);

  if(!(linkaddr_cmp(&c->last_rreq_originator, originator) &&
       c->last_rreq_id == msg->rreq_id)) {

    PRINTF("%d.%d: rreq_packet_received: request for %d.%d originator %d.%d / %d\n",
	   linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	   msg->dest.u8[0], msg->dest.u8[1],
	   originator->u8[0], originator->u8[1],
	   msg->rreq_id);

    linkaddr_copy(&c->last_rreq_originator, originator);
    c->last_rreq_id = msg->rreq_id;

    if(linkaddr_cmp(&msg->dest, &linkaddr_node_addr)) {
      PRINTF("%d.%d: route_packet_received: route request for our address\n",
	     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
      PRINTF("from %d.%d hops %d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     hops,
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
		new->R_dest_addr = MSG.originator;
		new->R_next_addr = previous-hop;
new->R_metric_type = used-metric-type;
new->R_metric = route-metric;
new->R_hop_count = hop-count;
new->R_seq_num = c.seq-num
new->R_valid_time = current time + R_HOLD_TIME
      /* Send route reply back to source. */
      send_rrep(c, originator);
      return 0; /* Don't continue to flood the rreq packet. */
    } else {
      /*      PRINTF("route request for %d\n", msg->dest_id);*/
      PRINTF("from %d.%d hops %d rssi %d lqi %d\n",
	     from->u8[0], from->u8[1],
	     hops,
	     packetbuf_attr(PACKETBUF_ATTR_RSSI),
	     packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));
      insert_route(originator, from, hops);
    }
    
    return 1;
  }
  return 0; /* Don't forward packet. */
}




static void
timeout_handler(void *ptr)
{
  struct route_discovery_conn *c = ptr;
  PRINTF("route_discovery: timeout, timed out\n");
  rrep_pending = 0;
  if(c->cb->timedout) {
    c->cb->timedout(c);
  }
}
/*---------------------------------------------------------------------------*/
//Starts a LOADng route discovery protocol.
int
route_discovery_discover(struct route_discovery_conn *c, const rimeaddr_t *addr,
			 clock_time_t timeout){

  if(rrep_pending) {
    PRINTF("route_discovery_send: ignoring request because of pending response\n");
    return 0;
  }

  PRINTF("route_discovery_send: sending route request\n");
  ctimer_set(&c->t, timeout, timeout_handler, c);
  rrep_pending = 1;
  send_rreq(c, addr);
  return 1;
}
/*---------------------------------------------------------------------------*/
//Opens a route discovery repair procedure.
int
route_discovery_repair(struct route_discovery_conn *c,const rimeaddr_t *addr,
		clock_time_t timeout)
{

}

/*---------------------------------------------------------------------------*/
//Initiates the route error communication procedure.
int
route_discovery_rerr (struct route_discovery_conn *c, uint8_t Error_Code,
		const rimeaddr_t *broken_source_addr, const rimeaddr_t *broken_dest_addr)
{

}

/*---------------------------------------------------------------------------*/
/** @} */
