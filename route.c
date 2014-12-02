/**
 * \addtogroup rimeroute
 * @{
 */

/*
 * Copyright (c) 2014, University of Southern California.
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
 *         Rime route table(LOADng)
 * \author
 *         {Jiahao Liang, Zhikun Liu, Haimo Bai} @ USC
 */

#include <stdio.h>
#include "lib/list.h"
#include "lib/memb.h"
#include "sys/ctimer.h"
#include "net/rime/route.h"
#include "contiki-conf.h"
#include "net/uip.h"

/*---------------------------------------------------------------------------*/
/*Data Structures*/

//Routing tuple entry structure for the LOADng routing table.
//struct routing_tuple {
//	struct routing_tuple* next;
//	rimeaddr_t R_dest_addr;
//	rimeaddr_t R_next_addr;
//	struct dist_tuple R_dist;
//	uint16_t R_seq_num;
//	clock_time_t R_valid_time;
//	uint8_t R_metric:4;	//R_metric: type of routing metric. 0, by default, means using hop-count
//	uint8_t padding:4;	//not used, initialized to 0;
//};

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*Parameters and constants*/
#define NUM_RS_ENTRIES 8
#define NUM_BLACKLIST_ENTRIES 2 * NUM_RS_ENTRIES
#define NUM_pending_ENTRIES NUM_RS_ENTRIES
#define ROUTE_TIMEOUT 50
/*
 * not used
 * #define NET_TRAVERSAL_TIME 2
 * #define BLACKLIST_TIME 10
 */
#define METRICS 0
/*---------------------------------------------------------------------------*/

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*
 * List of Routing Set.
 */
LIST(route_set);
MEMB(route_set_mem, struct route_entry, NUM_RS_ENTRIES);
/*
 * List of Blacklisted Neighbor Set
 */
LIST(blacklist_set);
MEMB(blacklist_set_mem, struct blacklist_tuple, NUM_BLACKLIST_ENTRIES);
/*
 * List of Pending Acknowledgement Set
 */
LIST(pending_set);
MEMB(pending_set_mem, struct pending_entry, NUM_pending_ENTRIES);

static struct ctimer t;

static int max_route_time = ROUTE_TIMEOUT;

/*---------------------------------------------------------------------------*/
//Periodically remove entries in Routing Set
static void
periodic(void *ptr)
{
  struct route_entry *e;
  struct blacklist_tuple *b;
  struct pending_entry *p;

  for(e = list_head(route_set); e != NULL; e = list_item_next(e)) {
    ++(e->R_valid_time);
    if(e->R_valid_time >= max_route_time) {
      PRINTF("route periodic: removing entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
	     e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
	     e->R_next_addr.u8[0], e->R_next_addr.u8[1],
	     e->R_metric, (e->R_dist).route_cost, (e->R_dist).weak_links);
      route_remove(e);
    }
  }
  e = NULL;

  //periodically remove entries in blacklist set while time out
  for(b = list_head(blacklist_set); b != NULL; b = list_item_next(b)) {
    --(b->B_valid_time);
    if(b->B_valid_time == 0) {
      PRINTF("route periodic: removing blacklisted neighbor %d.%d\n",
		 b->B_neighbor_address.u8[0], b->B_neighbor_address.u8[1]);
      blacklist_remove(b);
    }
  }
  b = NULL;

  //periodically add entries in pending set  to blacklist set while time out
  for(p = list_head(pending_set); p != NULL; p = list_item_next(p)) {
    --(p->P_ack_timeout);
    if(p->P_ack_timeout == 0) {
      PRINTF("route periodic: removing entry to %d.%d with nexthop %d.%d and seq_num %d\n",
				p->P_originator.u8[0], p->P_originator.u8[1],
				p->P_next_hop.u8[0], p->P_next_hop.u8[1],
				p->P_seq_num);
      pending_remove(p);
    }
  }
  p = NULL;

  ctimer_set(&t, CLOCK_SECOND, periodic, NULL);
}
/*---------------------------------------------------------------------------*/
//Allocates and initializes route tables.
void
route_init(void)
{
	  list_init(route_set);
	  memb_init(&route_set_mem);

	  list_init(blacklist_set);
	  memb_init(&blacklist_set_mem);

	  list_init(pending_set);
	  memb_init(&pending_set_mem);

	  ctimer_set(&t, CLOCK_SECOND, periodic, NULL);

	  PRINTF("route_init: done\n");
}

/*---------------------------------------------------------------------------*/
//Looks for a Routing Tuple in the Routing Set.
struct route_entry *
route_lookup(const rimeaddr_t *dest)
{
	struct route_entry *e;
	uint8_t lowest_cost;
	struct route_entry *best_entry;

	lowest_cost = -1;	//lowest_cost is an unsigned int, -1 means the largest number
	best_entry = NULL;

	/* Find the route with the lowest cost. */
	for(e = list_head(route_set); e != NULL; e = list_item_next(e)) {
	/*    printf("route_lookup: comparing %d.%d.%d.%d with %d.%d.%d.%d\n",
	   uip_ipaddr_to_quad(dest), uip_ipaddr_to_quad(&e->dest));*/

		if(rimeaddr_cmp(dest, &e->R_dest_addr)) {
		  if((e->R_dist).route_cost < lowest_cost) {
			best_entry = e;
			lowest_cost = (e->R_dist).route_cost;
		  }
		}
	}
	if (best_entry != NULL) {
		PRINTF("route_lookup: found entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
				best_entry->R_dest_addr.u8[0], best_entry->R_dest_addr.u8[1],
				best_entry->R_next_addr.u8[0], best_entry->R_next_addr.u8[1],
				best_entry->R_metric, (best_entry->R_dist).route_cost, (best_entry->R_dist).weak_links);
		return best_entry;
	} else {
		PRINTF("route_lookup: cannot found entry to %d.%d\n",
			 (*dest).u8[0], (*dest).u8[1]);
		return NULL;
	}

}

/*---------------------------------------------------------------------------*/
//Adds a route entry to the Routing Table.
struct routing_entry *
route_add(const rimeaddr_t *dest, const rimeaddr_t *nexthop,
		struct dist_tuple *dist, uint16_t seqno)
{
	struct route_entry *e;

	/* Avoid inserting duplicate entries. */
	e = route_lookup(dest);
	if(e != NULL && rimeaddr_cmp(&e->R_next_addr, nexthop)) {
		list_remove(route_set, e);
	} else {
		/* Allocate a new entry or reuse the oldest entry with highest cost. */
		e = memb_alloc(&route_set_mem);
		if(e == NULL) {
		  /* Remove oldest entry.  XXX */
		  e = list_chop(route_set);
		  PRINTF("route_add: removing entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
			 e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
			 e->R_next_addr.u8[0], e->R_next_addr.u8[1],
			 e->R_metric, (e->R_dist).route_cost, (e->R_dist).weak_links);
		}
	}

	rimeaddr_copy(&e->R_dest_addr, dest);
	rimeaddr_copy(&e->R_next_addr, nexthop);
	e->R_dist.route_cost = dist->route_cost;
	e->R_dist.weak_links = dist->weak_links;
	e->R_seq_num = seqno;
	e->R_valid_time = 0;
	e->R_metric = METRICS;

	/* New entry goes first. */
	list_push(route_set, e);

	PRINTF("route_add: new entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
		 e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
		 e->R_next_addr.u8[0], e->R_next_addr.u8[1],
		 e->R_metric, e->R_dist.route_cost, e->R_dist.weak_links);

	return (struct routing_entry*)e;
}

/*---------------------------------------------------------------------------*/
//Looks for an entry in the Pending List.
struct pending_entry *
route_pending_list_lookup (const rimeaddr_t *from,
		const rimeaddr_t *orig, uint16_t seq_num)
{
	struct pending_entry *e;

	/* Find the pending entry*/
	for(e = list_head(route_set); e != NULL; e = list_item_next(e)) {
		if (rimeaddr_cmp(&e->P_next_hop, from) &&
				rimeaddr_cmp(&e->P_originator, orig) &&
				e->P_seq_num == seq_num) {

			PRINTF("pending_lookup: found entry to %d.%d with nexthop %d.%d and seq_num %d\n",
				 e->P_originator.u8[0], e->P_originator.u8[1],
				 e->P_next_hop.u8[0], e->P_next_hop.u8[1],
				 e->P_seq_num);

			return e;
		}
	}

	PRINTF("pending_lookup: cannot found entry to %d.%d with nexthop %d.%d and seq_num %d\n",
		 (*orig).u8[0], (*orig).u8[1],
		 (*from).u8[0], (*from).u8[1],
		 seq_num);

	return NULL;
}

/*---------------------------------------------------------------------------*/
//Adds a pending entry to the Pending List.
/*
 * The Pending Acknowledgement Set contains information of the RREPs
 * transmitted with the ACK–REQUIRED flag set, and for which a RREP–ACK
 * has not yet been received.
 * const rimeaddr_t *dest contains the address of the orignator
 */
struct pending_entry *
route_pending_add(const rimeaddr_t *nexthop,
		const rimeaddr_t *dest, uint16_t RREQ_ID, clock_time_t timeout)
{
	/*TODO: Do we need to add the tuple to blacklist immediately?? Not added now*/
	struct pending_entry *e;

	/* Avoid inserting duplicate entries. */
	e = route_pending_list_lookup(nexthop, dest, RREQ_ID);
	if(e != NULL && rimeaddr_cmp(&e->P_next_hop, nexthop)) {
		list_remove(pending_set, e);
	} else {
		/* Allocate a new entry or reuse the oldest entry with highest cost. */
		e = memb_alloc(&pending_set_mem);
		if(e == NULL) {
		  /* Remove oldest entry.  XXX */
		  e = list_chop(pending_set);
		  PRINTF("pending_add: removing entry to %d.%d with nexthop %d.%d and seq_num %d\n",
			 e->P_originator.u8[0], e->P_originator.u8[1],
			 e->P_next_hop.u8[0], e->P_next_hop.u8[1],
			 e->P_seq_num);
		}
	}

	rimeaddr_copy(&e->P_originator, dest);
	rimeaddr_copy(&e->P_next_hop, nexthop);
	e->P_seq_num = RREQ_ID;
	e->P_ack_timeout = timeout;

	/* New entry goes first. */
	list_push(pending_set, e);

	PRINTF("pending_add: new entry to %d.%d with nexthop %d.%d and seq_num %d\n",
		 e->P_originator.u8[0], e->P_originator.u8[1],
		 e->P_next_hop.u8[0], e->P_next_hop.u8[1],
		 e->P_seq_num);

	return (struct pending_entry*)e;

}

/*---------------------------------------------------------------------------*/
//Searches a blacklist tuple in the Blacklist Table.
struct blacklist_tuple *
route_blacklist_lookup(const rimeaddr_t *addr )
{
	struct blacklist_tuple *e;

	/* Find the blacklisted neighbor with same addr. */
	for(e = list_head(blacklist_set); e != NULL; e = list_item_next(e)) {
	/*    printf("route_lookup: comparing %d.%d.%d.%d with %d.%d.%d.%d\n",
	   uip_ipaddr_to_quad(dest), uip_ipaddr_to_quad(&e->dest));*/

		if(rimeaddr_cmp(addr, &e->B_neighbor_address)) {
			PRINTF("blacklist_lookup: found blacklisted neighbor %d.%d\n",
					e->B_neighbor_address.u8[0], e->B_neighbor_address.u8[1]);

			return e;
		}
	}


	PRINTF("blacklist_lookup: cannot found blacklisted neighbor %d.%d\n",
			(*addr).u8[0], (*addr).u8[1]);

	return NULL;
}

/*---------------------------------------------------------------------------*/
//Adds a blacklist entry to the Blacklist.
struct blacklist_tuple *
route_blacklist_add(const rimeaddr_t *neighbor, clock_time_t timeout ){
	struct blacklist_tuple *e;

	/* Avoid inserting duplicate entries. */
	e = route_blacklist_lookup(neighbor);
	if(e != NULL && rimeaddr_cmp(&e->B_neighbor_address, neighbor)) {
		list_remove(blacklist_set, e);
	} else {
		/* Allocate a new entry or reuse the oldest entry with highest cost. */
		e = memb_alloc(&blacklist_set_mem);
		if(e == NULL) {
		  /* Remove oldest entry.  XXX */
		  e = list_chop(blacklist_set);
		  PRINTF("blacklist_add: removing blacklisted neighbor %d.%d\n",
			 e->B_neighbor_address.u8[0], e->B_neighbor_address.u8[1]);
		}
	}

	rimeaddr_copy(&e->B_neighbor_address, neighbor);
	e->B_valid_time = timeout;

	/* New entry goes first. */
	list_push(blacklist_set, e);

	PRINTF("blacklist_add: new blacklisted neighbor %d.%d\n",
			e->B_neighbor_address.u8[0], e->B_neighbor_address.u8[1]);

	return (struct blacklist_tuple*)e;
}


/*---------------------------------------------------------------------------*/
//Refreshes a route entry in the Routing Set.
void
route_refresh(struct route_entry *e)
{
	  if(e != NULL) {
	    /* Refresh age of route so that used routes do not get thrown
	       out. */
	    e->R_valid_time = 0;

	    PRINTF("route_refresh: time %ld for entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
	           e->R_valid_time,
	           e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
	           e->R_next_addr.u8[0], e->R_next_addr.u8[1],
	           e->R_metric, e->R_dist.route_cost, e->R_dist.weak_links);

	  } else {
		  PRINTF("route_refresh: input is NULL\n");
	  }
}
/*---------------------------------------------------------------------------*/
//Removes a route entry in the Routing Set.
void
route_remove(struct route_entry *e)
{
	if (e != NULL) {
		  PRINTF("route_remove: removing entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
			 e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
			 e->R_next_addr.u8[0], e->R_next_addr.u8[1],
			 e->R_metric, (e->R_dist).route_cost, (e->R_dist).weak_links);
		  list_remove(route_set, e);
		  memb_free(&route_set_mem, e);
	}
}


/*TODO: do we need to add a pending_remove() & blacklist_remove()?? And make it public??*/
/*---------------------------------------------------------------------------*/
//Removes a pending_entry in the pending_set.
void
pending_remove(struct pending_entry *e)
{
	if( e != NULL) {
		PRINTF("pending_remove: removing entry to %d.%d with nexthop %d.%d and seq_num %d\n",
				e->P_originator.u8[0], e->P_originator.u8[1],
				e->P_next_hop.u8[0], e->P_next_hop.u8[1],
				e->P_seq_num);
		list_remove(pending_set, e);
		memb_free(&pending_set_mem, e);
	}
}
/*---------------------------------------------------------------------------*/
//Removes a blacklist_tuple in the blacklist_set.
void
blacklist_remove(struct blacklist_tuple *e)
{
	if (e != NULL) {
	  PRINTF("blacklist_remove: removing blacklisted neighbor %d.%d\n",
		 e->B_neighbor_address.u8[0], e->B_neighbor_address.u8[1]);
	  list_remove(blacklist_set, e);
	  memb_free(&blacklist_set_mem, e);
	}
}

/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
void
route_decay(struct route_entry *e)
{

}

/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
void
route_flush_all(void)
{

}
/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
void
route_set_lifetime(int seconds)
{

}
/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
int
route_num(void)
{
	return -1;
}
/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
struct route_entry *
route_get(int num)
{
	return NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */
