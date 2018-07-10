#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#include "cache.h"

extern uns64 SWP_CORE0_WAYS;
extern MODE   SIM_MODE;
extern Flag isL2access;
extern uns64 cycle; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////


Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
	Flag outcome=MISS;
	int set_number = lineaddr% c->num_sets;
	if(is_write)
	    c->stat_write_access++;
	else
	    c->stat_read_access++;

  uns64 ii=0;


  for(ii=0; ii< c->num_ways; ii++)
  {
    if(c->sets[set_number].line[ii].valid)
      if(c->sets[set_number].line[ii].tag == lineaddr && c->sets[set_number].line[ii].core_id == core_id)
      {
        c->sets[set_number].line[ii].last_access_time = cycle;
        if(is_write)
          c->sets[set_number].line[ii].dirty = 1;
        outcome = HIT;
        return outcome;
      }
  }

  if(is_write)
    c->stat_write_miss++;
  else
    c->stat_read_miss++;

  
  return outcome;
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){

  // Your Code Goes Here
  // Find victim using cache_find_victim
  // Initialize the evicted entry
  // Initialize the victime entry
  int flag=-1;
  int temp=0;
  if(is_write == TRUE)
    temp = 1;

  c->last_evicted_line.dirty=0;

  int set_number = lineaddr%c->num_sets;

  uns64 ii=0;
  for(ii=0; ii<c->num_ways; ii++)
  {
    if(c->sets[set_number].line[ii].valid)
      continue;
    else{
      flag=ii;
      break;
    }
  }

  if(flag==-1){
    int victimid = cache_find_victim(c , set_number , core_id);
    c->last_evicted_line = c->sets[set_number].line[victimid];
    if(c->sets[set_number].line[victimid].dirty)
      c->stat_dirty_evicts++;
    c->sets[set_number].line[victimid].valid = 1;
    c->sets[set_number].line[victimid].dirty = temp;
    c->sets[set_number].line[victimid].tag = lineaddr;
    c->sets[set_number].line[victimid].core_id = core_id;
    c->sets[set_number].line[victimid].last_access_time= cycle;
  }
  else
  {
  	//printf("No need for victim\n");
    c->sets[set_number].line[flag].valid = 1;
    c->sets[set_number].line[flag].dirty = temp;
    c->sets[set_number].line[flag].tag = lineaddr;
    c->sets[set_number].line[flag].core_id = core_id;
    c->sets[set_number].line[flag].last_access_time= cycle;

  }
 
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////


uns cache_find_victim(Cache *c, uns set_index, uns core_id){
  int victim=0;
  if(c->repl_policy == 1)
    victim = rand()%8;

  if(c->repl_policy ==0)
  {
    uns64 ii = 0;
    uns64 cycleflag = c->sets[set_index].line[0].last_access_time;

    for(ii=0; ii<c->num_ways ; ii++)
    {
      if(c->sets[set_index].line[ii].last_access_time < cycleflag ){
        victim = ii;
        cycleflag = c->sets[set_index].line[ii].last_access_time;
      }
    }
  }

  if (c->repl_policy == 2)
  {
  	uns core0count =0;
  	uns core0cycleflag=cycle;
  	int core0victim =0;
  	uns core1count =0;
  	uns core1cycleflag=cycle;
  	int core1victim = 0;


  	uns ii=0;
  	for(ii=0; ii<c->num_ways ; ii++)
  	{
  		if(c->sets[set_index].line[ii].core_id == 0){
  			core0count++;
  			if(c->sets[set_index].line[ii].last_access_time < core0cycleflag){
  				core0victim = ii;
  				core0cycleflag = c->sets[set_index].line[ii].last_access_time;
  			}
  		}

  		if(c->sets[set_index].line[ii].core_id == 1){
  			core1count++;
  			if(c->sets[set_index].line[ii].last_access_time < core1cycleflag){
  				core1victim = ii;
  				core1cycleflag = c->sets[set_index].line[ii].last_access_time;
  			}
  		}
  	}
  	//printf("Set ID %u\n", set_index );
  	//printf("core ID %u\n", core_id );
  	//printf("Core0Count %u\n", core0count );
  	//printf("Core1Count %u\n", core1count );

  	if(core_id==0)
  	{
  		if (core0count<SWP_CORE0_WAYS){
  			victim = core1victim;
  			//printf(" Swapping out core1\n");
  		}
  		else{
  			victim = core0victim;
  			//printf(" Swapping out core0\n");
  		}
  	}

  	if(core_id==1)
  	{
  		if(core0count>SWP_CORE0_WAYS){
  			victim = core0victim;
  			//printf(" Swapping out core0\n");
  		}
  		else{
  			victim = core1victim;
  			//printf(" Swapping out core1\n");
  		}
  	}

  }
  
  return victim;
}

