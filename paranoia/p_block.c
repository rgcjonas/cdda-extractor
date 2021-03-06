/***
 * CopyPolicy: GNU Lesser General Public License 2.1 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 ***/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef WIN32
#  include <windows.h>
#else
#  include <time.h>
#endif
#include "p_block.h"
#include "paranoia.h"

linked_list *new_list(void *(*newp)(void),void (*freep)(void *)){
  linked_list *ret=calloc(1,sizeof(linked_list));
  ret->new_poly=newp;
  ret->free_poly=freep;
  return(ret);
}

linked_element *add_elem(linked_list *l,void *elem){

  linked_element *ret=calloc(1,sizeof(linked_element));
  ret->stamp=l->current++;
  ret->ptr=elem;
  ret->list=l;

  if(l->head)
    l->head->prev=ret;
  else
    l->tail=ret;
  ret->next=l->head;
  ret->prev=NULL;
  l->head=ret;
  l->active++;

  return(ret);
}

linked_element *new_elem(linked_list *list){
  void *new=list->new_poly();
  return(add_elem(list,new));
}

void free_elem(linked_element *e,int free_ptr){
  linked_list *l=e->list;
  if(free_ptr)l->free_poly(e->ptr);

  if(e==l->head)
    l->head=e->next;
  if(e==l->tail)
    l->tail=e->prev;

  if(e->prev)
    e->prev->next=e->next;
  if(e->next)
    e->next->prev=e->prev;

  l->active--;
  free(e);
}

void free_list(linked_list *list,int free_ptr){
  while(list->head)
    free_elem(list->head,free_ptr);
  free(list);
}

void *get_elem(linked_element *e){
  return(e->ptr);
}

linked_list *copy_list(linked_list *list){
  linked_list *new=new_list(list->new_poly,list->free_poly);
  linked_element *i=list->tail;

  while(i){
    add_elem(new,i->ptr);
    i=i->prev;
  }
  return(new);
}

/**** C_block stuff ******************************************************/

static c_block *i_cblock_constructor(cdrom_paranoia *p){
  c_block *ret=calloc(1,sizeof(c_block));
  return(ret);
}

void i_cblock_destructor(c_block *c){
  if(c){
    if(c->vector)free(c->vector);
    if(c->flags)free(c->flags);
    c->e=NULL;
    free(c);
  }
}

c_block *new_c_block(cdrom_paranoia *p){
  linked_element *e=new_elem(p->cache);
  c_block *c=e->ptr;
  c->e=e;
  c->p=p;
  return(c);
}

void free_c_block(c_block *c){
  /* also rid ourselves of v_fragments that reference this block */
  v_fragment *v=v_first(c->p);

  while(v){
    v_fragment *next=v_next(v);
    if(v->one==c)free_v_fragment(v);
    v=next;
  }

  free_elem(c->e,1);
}

static v_fragment *i_vfragment_constructor(void){
  v_fragment *ret=calloc(1,sizeof(v_fragment));
  return(ret);
}

static void i_v_fragment_destructor(v_fragment *v){
  free(v);
}

v_fragment *new_v_fragment(cdrom_paranoia *p,c_block *one,
                           long begin, long end, int last){
  linked_element *e=new_elem(p->fragments);
  v_fragment *b=e->ptr;

  b->e=e;
  b->p=p;

  b->one=one;
  b->begin=begin;
  b->vector=one->vector+begin-one->begin;
  b->size=end-begin;
  b->lastsector=last;

  return(b);
}

void free_v_fragment(v_fragment *v){
  free_elem(v->e,1);
}

c_block *c_first(cdrom_paranoia *p){
  if(p->cache->head)
    return(p->cache->head->ptr);
  return(NULL);
}

c_block *c_last(cdrom_paranoia *p){
  if(p->cache->tail)
    return(p->cache->tail->ptr);
  return(NULL);
}

c_block *c_next(c_block *c){
  if(c->e->next)
    return(c->e->next->ptr);
  return(NULL);
}

c_block *c_prev(c_block *c){
  if(c->e->prev)
    return(c->e->prev->ptr);
  return(NULL);
}

v_fragment *v_first(cdrom_paranoia *p){
  if(p->fragments->head){
    return(p->fragments->head->ptr);
  }
  return(NULL);
}

v_fragment *v_last(cdrom_paranoia *p){
  if(p->fragments->tail)
    return(p->fragments->tail->ptr);
  return(NULL);
}

v_fragment *v_next(v_fragment *v){
  if(v->e->next)
    return(v->e->next->ptr);
  return(NULL);
}

v_fragment *v_prev(v_fragment *v){
  if(v->e->prev)
    return(v->e->prev->ptr);
  return(NULL);
}

void recover_cache(cdrom_paranoia *p){
  linked_list *l=p->cache;

  /* Are we at/over our allowed cache size? */
  while(l->active>p->cache_limit)
    /* cull from the tail of the list */
    free_c_block(c_last(p));

}

int16_t *v_buffer(v_fragment *v){
  if(!v->one)return(NULL);
  if(!cv(v->one))return(NULL);
  return(v->vector);
}

/* alloc a c_block not on a cache list */
c_block *c_alloc(int16_t *vector,long begin,long size){
  c_block *c=calloc(1,sizeof(c_block));
  c->vector=vector;
  c->begin=begin;
  c->size=size;
  return(c);
}

void c_set(c_block *v,long begin){
  v->begin=begin;
}

/* pos here is vector position from zero */
void c_insert(c_block *v,long pos,int16_t *b,long size){
  int vs=cs(v);
  if(pos<0 || pos>vs)return;

  if(v->vector)
    v->vector=realloc(v->vector,sizeof(int16_t)*(size+vs));
  else
    v->vector=malloc(sizeof(int16_t)*size);

  if(pos<vs)memmove(v->vector+pos+size,v->vector+pos,
                       (vs-pos)*sizeof(int16_t));
  memcpy(v->vector+pos,b,size*sizeof(int16_t));

  v->size+=size;
}

void c_remove(c_block *v,long cutpos,long cutsize){
  int vs=cs(v);
  if(cutpos<0 || cutpos>vs)return;
  if(cutpos+cutsize>vs)cutsize=vs-cutpos;
  if(cutsize<0)cutsize=vs-cutpos;
  if(cutsize<1)return;

  memmove(v->vector+cutpos,v->vector+cutpos+cutsize,
            (vs-cutpos-cutsize)*sizeof(int16_t));

  v->size-=cutsize;
}

void c_overwrite(c_block *v,long pos,int16_t *b,long size){
  int vs=cs(v);

  if(pos<0)return;
  if(pos+size>vs)size=vs-pos;

  memcpy(v->vector+pos,b,size*sizeof(int16_t));
}

void c_append(c_block *v, int16_t *vector, long size){
  int vs=cs(v);

  /* update the vector */
  if(v->vector)
    v->vector=realloc(v->vector,sizeof(int16_t)*(size+vs));
  else
    v->vector=malloc(sizeof(int16_t)*size);
  memcpy(v->vector+vs,vector,sizeof(int16_t)*size);

  v->size+=size;
}

void c_removef(c_block *v, long cut){
  c_remove(v,0,cut);
  v->begin+=cut;
}



/**** Initialization *************************************************/

cdrom_paranoia *paranoia_init(long (*cdda_read_func)(void * /*cdda_closure*/, void * /*buffer*/, long /*begin*/, long /*sectors*/),
                              void *cdda_closure,
                              int nsectors,
                              long firstsector, long lastsector){
  cdrom_paranoia *p=calloc(1,sizeof(cdrom_paranoia));

  p->cache=new_list((void *)&i_cblock_constructor,
                    (void *)&i_cblock_destructor);

  p->fragments=new_list((void *)&i_vfragment_constructor,
                        (void *)&i_v_fragment_destructor);

  p->cdcache_begin= 9999999;
  p->cdcache_end= 9999999;
  p->cdcache_size=CACHEMODEL_SECTORS;
  p->sortcache=sort_alloc(p->cdcache_size*CD_FRAMEWORDS);
  p->cdda_closure=cdda_closure;
  p->cdda_read_func=cdda_read_func;
  p->cdda_nsectors=nsectors;
  p->cdda_disc_firstsector=firstsector;
  p->cdda_disc_lastsector=lastsector;
  p->dynoverlap=MAX_SECTOR_OVERLAP*CD_FRAMEWORDS;
  p->cache_limit=JIGGLE_MODULO;
  p->enable=PARANOIA_MODE_FULL;

  return(p);
}

/* sectors < 0 indicates a query.  Returns the number of sectors before the call */
int paranoia_cachemodel_size(cdrom_paranoia *p,int sectors){
  int ret = p->cdcache_size;
  if(sectors>=0)
    p->cdcache_size=sectors;
  return ret;
}

long cdda_read(cdrom_paranoia *p, void *buffer, long beginsector, long sectors){
  return p->cdda_read_func(p->cdda_closure, buffer, beginsector, sectors);
}

long cdda_read_timed(cdrom_paranoia *p, void *buffer, long beginsector, long sectors, int *milliseconds){
#ifdef WIN32
  DWORD t1, t2;
  t1 = GetTickCount();
#else
  struct timespec ts1, ts2;
  clock_gettime(CLOCK_MONOTONIC, &ts1);
#endif

  long rv = cdda_read(p, buffer, beginsector, sectors);

#ifdef WIN32
  t2 = GetTickCount();
  *milliseconds = (int)(t2 - t1);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts2);

  double starttime = (double)ts1.tv_sec + 1.0e-9 * (double)ts1.tv_nsec;
  double endtime = (double)ts2.tv_sec + 1.0e-9 * (double)ts2.tv_nsec;
  *milliseconds = (int)((endtime - starttime) * 1000.0);
#endif

  return rv;
}

