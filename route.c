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
struct routing_tuple {
	struct routing_tuple* next;
	rimeaddr_t R_dest_addr;
	rimeaddr_t R_next_addr;
	struct dist_tuple R_dist;
	uint16_t R_seq_num;
	clock_time_t R_valid_time;
	uint8_t R_metric:4;
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

/*---------------------------------------------------------------------------*/
//Allocates and initializes route tables.
void
route_init(void)
{

}
/*---------------------------------------------------------------------------*/
//Adds a route entry to the Routing Table.
struct routing_tuple *
route_add(const rimeaddr_t *dest, const rimeaddr_t *nexthop,
		struct dist_tuple ∗dist, uint16_t seqno)
{

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
