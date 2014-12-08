#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

/*#of Ticks*/
#define CACHE_TIMER_FREQ 100

#define CACHE_BLOCK_LIMIT 64

#define CACHE_WRITE_ALL_FREQ 10000



enum lock_type{
  NON_EXCLUSIVE,
  EXCLUSIVE
};


struct cache_block {
  struct inode *inode;
  uint32_t sector_id;
  void *buffer;
  int accessed_cnt;
  int readers;
  int writers;
  bool dirty;
  bool accessed;
  struct lock block_lock;
  struct list_elem elem;
};



struct list cache_list;


void cache_init(void);
void cache_flush(void);
//struct cache_block *cache_lock(block_sector_t sector_id, enum lock_type);
void *cache_read (struct inode *inode, block_sector_t sector_id);
void *cache_write (struct inode *inode, block_sector_t sector_id);

void cache_periodic_write(int64_t cur_ticks);
void move_cache_to_disk(struct cache_block *cache);
void *cache_zero (struct cache_block *c);
void cache_dirty (struct cache_block *c);
void cache_unlock (struct cache_block *c);
void cache_free(block_sector_t sector_id);
void cache_readahead(block_sector_t sector_id);
struct cache_block *is_block_in_cache(struct inode *inode, 
				      block_sector_t sector_id);

#endif
