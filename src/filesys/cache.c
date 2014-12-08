#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/interrupt.h"

static struct lock cache_lock;

bool fs_cache_initiated;
void print_cache(void);

void cache_init(void){
  printf("called cache_init\n");
  list_init(&cache_list);
  list_init(&cache_lock);
  fs_cache_initiated = true;
}

struct cache_block *is_block_in_cache(struct inode *inode, 
				      block_sector_t sector_id){
  printf("in is_block_in_cache, cache size is %d\n",list_size(&cache_list));
  struct list_elem *e;
  struct cache_block *ret_block;
  
  //printf("going to ask for cache_lock in is_block_in_cache\n");
  //lock_acquire(&cache_lock);
  //printf("got the lock in is_block_in_cache\n");
  for (e = list_begin(&cache_list);
       
       e != list_end(&cache_list);
       e = list_next(e)){
    ret_block = list_entry(e, struct cache_block, elem);
    if ((ret_block->inode == inode) && (ret_block->sector_id == sector_id)){
      //lock_release(&cache_lock);
      //printf("cache in block\n");
      return ret_block;
    }
  }
  //lock_release(&cache_lock);
  printf("cache not in block\n");
  return NULL;
}

void *cache_read(struct inode *inode, block_sector_t sector_id){
  printf("in cache_read going to ask for sector_id %u\n", sector_id);
  struct cache_block *query_block;
  query_block = is_block_in_cache(inode, sector_id);
  if(query_block != NULL){
    query_block->accessed == true;
    query_block->accessed_cnt++;
    query_block->readers++;
    
    return query_block->buffer;
  }
  else{
    void* buffer;
    buffer = (void*) malloc(BLOCK_SECTOR_SIZE);
    if(buffer == NULL){
      printf("no space to create cache_buffer\n");
      return NULL;
    }
    block_read(fs_device, query_block->sector_id, buffer);
    if(buffer == NULL){
      printf("read file to buffer, but buffer is null\n");
      return NULL;
    }
    query_block = (struct cache_block *) malloc(sizeof(struct cache_block));
    if(query_block == NULL){
      printf("couldn't allocate cache block\n");
      return NULL;
    }
    query_block->inode = inode;
    query_block->sector_id = sector_id;
    query_block->buffer = buffer;
    query_block->accessed_cnt = 0;
    query_block->dirty = false;
    query_block->accessed = true;
    lock_init(&query_block->block_lock);
    while(list_size(&cache_list)>CACHE_BLOCK_LIMIT){
      evict_block();
     }
    //lock_acquire(&cache_lock);
    list_push_back(&cache_list, &query_block->elem);
    //lock_release(&cache_lock);
  } 
  printf("left cache_read\n");
  return query_block->buffer;
}

void print_cache(void){
  printf("in print_cache\n");
  struct list_elem *e;
  struct cache_block *ret_block;
  for (e = list_begin(&cache_list);
       
       e != list_end(&cache_list);
       e = list_next(e)){
    ret_block = list_entry(e, struct cache_block, elem);
    printf("ret_block sector is is %u\n",ret_block->sector_id);
  }
}
void *cache_write(struct inode *inode, block_sector_t sector_id){
  printf("in cache_write, going to write sector %u\n",sector_id);
  struct cache_block *query_block;
  query_block = is_block_in_cache(inode, sector_id);
  if(query_block != NULL){
    query_block->dirty=true;
    query_block->accessed_cnt++;
    printf("in cache_write, block was already cached, return buffer\n");
    return query_block->buffer;
  }
  else{
    void *buffer = (void*) malloc(BLOCK_SECTOR_SIZE);
    query_block = (struct cache_block *) malloc(sizeof(struct cache_block));
    block_read(fs_device, sector_id, buffer);
    query_block->inode = inode;
    query_block->sector_id=sector_id;
    query_block->buffer = buffer;
    query_block->accessed_cnt = 0;
    query_block->accessed = true;
    query_block->dirty = true;
    lock_init(&query_block->block_lock);
    
    while(list_size(&cache_list)>=CACHE_BLOCK_LIMIT){
      evict_block();
    }
    printf("going to ask for lock in cache_write\n");
    //lock_acquire(&cache_lock);
    printf("got cache_lock in cache_write\n");
    list_push_front(&cache_list,&query_block->elem);
    printf("added block to cache, sector_id is %u\n",query_block->sector_id);
    printf("cache size is %d\n", list_size(&cache_list));
    //lock_release(&cache_lock);
  }
  print_cache();
  printf("left cache_write\n");
  return query_block->buffer;
}

void move_cache_to_disk(struct cache_block* cache){
  printf("in move_cache_to_disk\n");
  if(lock_try_acquire(&cache->block_lock)){
    block_write(fs_device, cache->sector_id, cache->buffer);
    lock_release(&cache->block_lock);
    cache->dirty = false;
  }
}

void evict_block(void){
  printf("in evict_block\n");
  struct list_elem* e;
  struct cache_block *evict = NULL;
  struct cache_block *cur;
  
  lock_acquire(&cache_lock);
  e = list_begin(&cache_list);
  if(e != NULL){
    cur = list_entry(e, struct cache_block, elem);
    evict = cur;
    //remove_elem = e;
    for(e; e != list_end(&cache_list); e = list_next(e)){
      cur = list_entry(e, struct cache_block, elem);
      if(cur->accessed_cnt<evict->accessed_cnt){
	evict = cur;
      }
    }
    if(evict != NULL){
      if(evict->dirty == true){
	move_cache_to_disk(evict);
      }
      printf("removing in evict_block\n");
      list_remove(&evict->elem);
      free(evict->buffer);
      free(evict);
    }
  }
  else printf("cache is empty\n");
}


void cache_periodic_write(int64_t cur_ticks){
  printf("in cache_periodic_write\n");
  if(!fs_cache_initiated){
    return;
  }
  else{
    
    struct list_elem *e;
    struct cache_block *cur;
    if(cur_ticks%CACHE_WRITE_ALL_FREQ == 0){
      enum intr_level old_level = intr_disable();
      printf("going to ask for lock in periodic_write\n");
      if(lock_try_acquire(&cache_lock)){
	printf("got lock in periodic_write\n");
	for(e = list_begin(&cache_list);
	    e != list_end(&cache_list);
	    e = list_next(e)){
	  cur = list_entry(e, struct cache_block, elem);
	  cur->accessed = false;
	  if(cur->dirty == true){
	    move_cache_to_disk(cur);
	    cur->dirty == false;
	  }
	}
	printf("exiting cache_periodic_write\n");
	lock_release(&cache_lock);
      }
      intr_set_level(old_level);
    }
  }
}
      

void cache_flush(void){
  printf("in cache_flush\n");
  struct list_elem *e;
  struct cache_block *cur;
  for(e = list_begin(&cache_list);
      e != list_end(&cache_list);
      e = list_next(e)){
    cur = list_entry(e, struct cache_block, elem);
    if(cur->dirty == true){
      move_cache_to_disk(cur);
      cur->dirty == false;
    }
  }
}
