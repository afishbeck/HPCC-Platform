/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */


#ifndef JATOMIC_HPP
#define JATOMIC_HPP
#include "platform.h"

#ifdef _WIN32

#include <intrin.h>

extern "C"
{
   LONG  __cdecl _InterlockedIncrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedDecrement(LONG volatile *Addend);
   LONG  __cdecl _InterlockedCompareExchange(LONG volatile * Dest, LONG Exchange, LONG Comp);
}

#pragma intrinsic (_InterlockedCompareExchange)
#define InterlockedCompareExchange _InterlockedCompareExchange
#pragma intrinsic (_InterlockedIncrement)
#define InterlockedIncrement _InterlockedIncrement
#pragma intrinsic (_InterlockedDecrement)
#define InterlockedDecrement _InterlockedDecrement
#pragma intrinsic (_InterlockedExchangeAdd)
#define InterlockedExchangeAdd _InterlockedExchangeAdd

typedef volatile long atomic_t;
#define ATOMIC_INIT(i)                  (i)
#define atomic_inc(v)                   InterlockedIncrement(v)
#define atomic_inc_and_test(v)          (InterlockedIncrement(v) == 0)
#define atomic_dec(v)                   InterlockedDecrement(v)
#define atomic_dec_and_test(v)          (InterlockedDecrement(v) == 0)
#define atomic_dec_and_read(v)           InterlockedDecrement(v)
#define atomic_read(v)                  (*v)
#define atomic_set(v,i)                 ((*v) = (i))
#define atomic_xchg(i, v)               InterlockedExchange(v, i)
#define atomic_add(v,i)                 InterlockedExchangeAdd(v,i)
#define atomic_add_exchange(v, i)       InterlockedExchangeAdd(v,i)
#define atomic_xchg_ptr(p, v)           InterlockedExchangePointer(v,p)
#if defined (_MSC_VER) && (_MSC_VER <= 1200)
#define atomic_cas(v,newvalue,expectedvalue)    (InterlockedCompareExchange((PVOID *)(v),(PVOID)(long)(newvalue),(PVOID)(long)(expectedvalue))==(PVOID)(long)(expectedvalue))
#define atomic_cas_ptr(v, newvalue,expectedvalue)       atomic_cas(v,(long)newvalue,(long)expectedvalue)
#else
#define atomic_cas(v,newvalue,expectedvalue)    (InterlockedCompareExchange(v,newvalue,expectedvalue)==expectedvalue)
#define atomic_cas_ptr(v, newvalue,expectedvalue)       (InterlockedCompareExchangePointer(v,newvalue,expectedvalue)==expectedvalue)
#endif

//Used to prevent a compiler reordering volatile and non-volatile loads/stores
#define compiler_memory_barrier()           _ReadWriteBarrier()

#elif defined(__GNUC__)

typedef struct { volatile int counter; } atomic_t;
#define ATOMIC_INIT(i)          { (i) }
#define atomic_read(v)          ((v)->counter)
#define atomic_set(v,i)         (((v)->counter) = (i))

static __inline__ bool atomic_dec_and_test(atomic_t *v)
{   
    // returns (--*v==0)
    return (__sync_add_and_fetch(&v->counter,-1)==0);
}

static __inline__ bool atomic_inc_and_test(atomic_t *v)
{
    // returns (++*v==0)
    return (__sync_add_and_fetch(&v->counter,1)==0);
}

static __inline__ void atomic_inc(atomic_t *v)
{
    // (*v)++
    __sync_add_and_fetch(&v->counter,1);
}

static __inline__ void atomic_dec(atomic_t *v)
{
    // (*v)--
    __sync_add_and_fetch(&v->counter,-1);
}

static __inline__ int atomic_dec_and_read(atomic_t *v)
{
    // (*v)--, return *v;
    return __sync_add_and_fetch(&v->counter,-1);
}

static __inline__ int atomic_xchg(int i, atomic_t *v)
{
    // int ret = *v; *v = i; return v;
    return __sync_lock_test_and_set(&v->counter,i);  // actually an xchg
}



static __inline__ void atomic_add(atomic_t *v,int i)
{
    // (*v)+=i;
    __sync_add_and_fetch(&v->counter,i);
}

static __inline__ int atomic_add_exchange(atomic_t *v,int i)
{
    // int ret = *v; (*v) += i; return ret;
    return __sync_fetch_and_add(&v->counter,i);
}

static __inline__ bool atomic_cas(atomic_t *v,int newvalue, int expectedvalue)
{
    // bool ret = (*v==expectedvalue); if (ret) *v = newvalue; return ret;
    return __sync_bool_compare_and_swap(&v->counter, expectedvalue, newvalue);
}

static __inline__ void * atomic_xchg_ptr(void *p, void **v)
{
    // void * ret = *v; (*v) = p; return ret;
    return (void *)__sync_lock_test_and_set((memsize_t *)v,(memsize_t)p);
}

static __inline__ bool atomic_cas_ptr(void **v,void *newvalue, void *expectedvalue)
{
    // bool ret = (*v==expectedvalue); if (ret) *v = newvalue; return ret;
    return __sync_bool_compare_and_swap((memsize_t *)v, (memsize_t)expectedvalue, (memsize_t)newvalue);
}

#define compiler_memory_barrier() asm volatile("": : :"memory")

#else // other unix

//Truely awful implementations of atomic operations...
typedef volatile int atomic_t;
int jlib_decl poor_atomic_dec_and_read(atomic_t * v);
bool jlib_decl poor_atomic_inc_and_test(atomic_t * v);
int jlib_decl poor_atomic_xchg(int i, atomic_t * v);
void jlib_decl poor_atomic_add(atomic_t * v, int i);
int jlib_decl poor_atomic_add_exchange(atomic_t * v, int i);
bool jlib_decl poor_atomic_cas(atomic_t * v, int newvalue, int expectedvalue);
void jlib_decl *poor_atomic_xchg_ptr(void *p, void **v);
bool   jlib_decl poor_atomic_cas_ptr(void ** v, void *newvalue, void *expectedvalue);
void jlib_decl poor_compiler_memory_barrier();

#define ATOMIC_INIT(i)                  (i)
#define atomic_inc(v)                   (void)poor_atomic_inc_and_test(v)
#define atomic_inc_and_test(v)          poor_atomic_inc_and_test(v)
#define atomic_dec(v)                   (void)poor_atomic_dec_and_read(v)
#define atomic_dec_and_read(v)          poor_atomic_dec_and_read(v)
#define atomic_dec_and_test(v)          (poor_atomic_dec_and_read(v)==0)
#define atomic_read(v)                  (*v)
#define atomic_set(v,i)                 ((*v) = (i))
#define atomic_xchg(i, v)               poor_atomic_xchg(i, v)
#define atomic_add(v,i)                 poor_atomic_add(v, i)
#define atomic_add_exchange(v, i)       poor_atomic_add_exchange(v, i)
#define atomic_cas(v,newvalue,expectedvalue)    poor_atomic_cas(v,newvalue,expectedvalue)
#define atomic_xchg_ptr(p, v)               poor_atomic_xchg_ptr(p, v)
#define atomic_cas_ptr(v,newvalue,expectedvalue)    poor_atomic_cas_ptr(v,newvalue,expectedvalue)
#define compiler_memory_barrier()       poor_compiler_memory_barrier()

#endif


#endif
