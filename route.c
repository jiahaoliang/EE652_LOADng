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
//#include "net/rime/route.h"
#include "contiki-conf.h"
#include "net/uip.h"

#include "route.h"	//just for eclipse

/*---------------------------------------------------------------------------*/
/*Data Structures*/

//Routing tuple entry structure for the LOADng routing table.
struct routing_tuple {
	struct routing_tuple* next;
	rimeaddr_t R_dest_addr;
	rimeaddr_t R_next_addr;
	struct dist_tuple R_dist;
	uint16_t R_seq_num;
	clock_time_t R_valid_time;
	uint8_t R_metric:4;	//R_metric: type of routing metric. 0, by default, means using hop-count
	uint8_t padding:4;	//not used, initialized to 0;
};


//Pending entry tuple structure for the Pending Acknowledgement Set.
struct pending_entry {
	struct pending_entry* next;
	rimeaddr_t P_next_hop;
	rimeaddr_t P_originator;
	clock_time_t P_ack_timeout;
};
//Blacklist tuple entry structure for the Blacklisted Neighbor Set.
struct blacklist_tuple {
	struct blacklist_tuple* next;
	rimeaddr_t B_neighbor_address;
	clock_time_t B_valid_time;
};

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*Parameters and constants*/
#define NUM_RS_ENTRIES 8
#define NUM_BLACKLIST_ENTRIES 2 ∗ NUM_RS_ENTRIES
#define NUM_pending_ENTRIES NUM_RS_ENTRIES
#define ROUTE_TIMEOUT 5 ∗ CLOCK_SECOND
#define NET_TRAVERSAL_TIME 2 ∗ CLOCK_SECOND
#define BLACKLIST_TIME 10 ∗ CLOCK_SECOND
#define METRICS 0
/*---------------------------------------------------------------------------*/

#define DEBUG 0
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
MEMB(route_set_mem, struct routing_tuple, NUM_RS_ENTRIES);
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
static int max_blacklist_time = BLACKLIST_TIME;

/*---------------------------------------------------------------------------*/
//Periodically remove entries in Routing Set
static void
periodic(void *ptr)
{
  struct routing_tuple *e;

  for(e = list_head(route_set); e != NULL; e = list_item_next(e)) {
    e->R_valid_time++;
    if(e->R_valid_time >= max_route_time) {
      PRINTF("route periodic: removing entry to %d.%d with nexthop %d.%d and metric type:%d cost: %d weak links: %d\n",
	     e->R_dest_addr.u8[0], e->R_dest_addr.u8[1],
	     e->R_next_addr.u8[0], e->R_next_addr.u8[1],
	     e->R_metric, (e->R_dist).route_cost, (e->R_dist).weak_links);
      list_remove(route_set, e);
      memb_free(&route_set_mem, e);
    }
  }

  //TODO: periodically remove entries in blacklist set every BLACKLIST_TIME

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
}
/*---------------------------------------------------------------------------*/
//Adds a route entry to the Routing Table.
struct routing_entry *
route_add(const rimeaddr_t *dest, const rimeaddr_t *nexthop,
		struct dist_tuple ∗dist, uint16_t seqno)
{
	struct routing_tuple *e;

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
	list_push(route_table, e);

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

}

/*---------------------------------------------------------------------------*/
//Adds a pending entry to the Pending List.
struct pending_entry *
route_pending_add(const rimeaddr_t *nexthop,
		const rimeaddr_t *dest, uint16_t RREQ_ID, clock_time_t timeout)
{

}

/*---------------------------------------------------------------------------*/
//Looks for a Routing Tuple in the Routing Set.
struct route_entry *
route_lookup(const rimeaddr_t *dest)
{
	struct routing_tuple *e;
	uint8_t lowest_cost;
	struct routing_tuple *best_entry;

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
	return best_entry;
}


/*---------------------------------------------------------------------------*/
//Searches a blacklist tuple in the Blacklist Table.
struct blacklist_tuple *
route_blacklist_lookup(const rimeaddr_t *addr )
{

}

/*---------------------------------------------------------------------------*/
//Adds a blacklist entry to the Blacklist.
struct blacklist_tuple *
route_blacklist_add(const rimeaddr_t *neighbor, clock_time_t timeout ){

}


/*---------------------------------------------------------------------------*/
//Refreshes a route entry in the Routing Set.
void
route_refresh(struct route_entry *e)
{

}
/*---------------------------------------------------------------------------*/
//Removes a route entry in the Routing Set.
void
route_remove(struct route_entry *e)
{

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

}
/*---------------------------------------------------------------------------*/
//Not implemented and only maintained for compatibility.
struct route_entry *
route_get(int num)
{

}
/*---------------------------------------------------------------------------*/
/** @} */
