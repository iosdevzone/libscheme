/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
/* Boehm, July 27, 1994 9:45 am PDT */
/* An incomplete test for the garbage collector.  		*/
/* Some more obscure entry points are not tested at all.	*/

# include <stdlib.h>
# include <stdio.h>
# include "gc.h"
# include "gc_typed.h"
# include "gc_priv.h"	/* For output and some statistics	*/
# include "config.h"

# ifdef MSWIN32
#   include <windows.h>
# endif

# ifdef PCR
#   include "th/PCR_ThCrSec.h"
#   include "th/PCR_Th.h"
# endif

# ifdef SOLARIS_THREADS
#   include <thread.h>
#   include <synch.h>
# endif

# if defined(PCR) || defined(SOLARIS_THREADS)
#   define THREADS
# endif

# ifdef AMIGA
   long __stack = 200000;
# endif

# define FAIL (void)abort()

/* AT_END may be defined to excercise the interior pointer test	*/
/* if the collector is configured with ALL_INTERIOR_POINTERS.   */
/* As it stands, this test should succeed with either		*/
/* configuration.  In the FIND_LEAK configuration, it should	*/
/* find lots of leaks, since we free almost nothing.		*/

struct SEXPR {
    struct SEXPR * sexpr_car;
    struct SEXPR * sexpr_cdr;
};

# ifdef __STDC__
    typedef void * void_star;
# else
    typedef char * void_star;
# endif

typedef struct SEXPR * sexpr;

extern sexpr cons();

# undef nil
# define nil ((sexpr) 0)
# define car(x) ((x) -> sexpr_car)
# define cdr(x) ((x) -> sexpr_cdr)
# define is_nil(x) ((x) == nil)


int extra_count = 0;        /* Amount of space wasted in cons node */

/* Silly implementation of Lisp cons. Intentionally wastes lots of space */
/* to test collector.                                                    */
sexpr cons (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    register int *p;
    register my_extra = extra_count;
    
    r = (sexpr) GC_MALLOC_STUBBORN(sizeof(struct SEXPR) + my_extra);
    if (r == 0) {
        (void)GC_printf0("Out of memory\n");
        exit(1);
    }
    for (p = (int *)r;
         ((char *)p) < ((char *)r) + my_extra + sizeof(struct SEXPR); p++) {
	if (*p) {
	    (void)GC_printf1("Found nonzero at 0x%lx - allocator is broken\n",
	    		     (unsigned long)p);
	    FAIL;
        }
        *p = 13;
    }
#   ifdef AT_END
	r = (sexpr)((char *)r + (my_extra & ~7));
#   endif
    r -> sexpr_car = x;
    r -> sexpr_cdr = y;
    my_extra++;
    if ( my_extra >= 5000 ) {
        extra_count = 0;
    } else {
        extra_count = my_extra;
    }
    GC_END_STUBBORN_CHANGE((char *)r);
    return(r);
}

sexpr small_cons (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    
    r = (sexpr) GC_MALLOC(sizeof(struct SEXPR));
    if (r == 0) {
        (void)GC_printf0("Out of memory\n");
        exit(1);
    }
    r -> sexpr_car = x;
    r -> sexpr_cdr = y;
    return(r);
}

sexpr small_cons_uncollectable (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    
    r = (sexpr) GC_MALLOC_UNCOLLECTABLE(sizeof(struct SEXPR));
    if (r == 0) {
        (void)GC_printf0("Out of memory\n");
        exit(1);
    }
    r -> sexpr_car = x;
    r -> sexpr_cdr = (sexpr) (~(unsigned long)y);
    return(r);
}

/* Return reverse(x) concatenated with y */
sexpr reverse1(x, y)
sexpr x, y;
{
    if (is_nil(x)) {
        return(y);
    } else {
        return( reverse1(cdr(x), cons(car(x), y)) );
    }
}

sexpr reverse(x)
sexpr x;
{
    return( reverse1(x, nil) );
}

sexpr ints(low, up)
int low, up;
{
    if (low > up) {
	return(nil);
    } else {
        return(small_cons(small_cons((sexpr)low, (sexpr)0), ints(low+1, up)));
    }
}

/* Too check uncollectable allocation we build lists with disguised cdr	*/
/* pointers, and make sure they don't go away.				*/
sexpr uncollectable_ints(low, up)
int low, up;
{
    if (low > up) {
	return(nil);
    } else {
        return(small_cons_uncollectable(small_cons((sexpr)low, (sexpr)0),
               uncollectable_ints(low+1, up)));
    }
}

void check_ints(list, low, up)
sexpr list;
int low, up;
{
    if ((int)(car(car(list))) != low) {
        (void)GC_printf0(
           "List reversal produced incorrect list - collector is broken\n");
        FAIL;
    }
    if (low == up) {
        if (cdr(list) != nil) {
           (void)GC_printf0("List too long - collector is broken\n");
           FAIL;
        }
    } else {
        check_ints(cdr(list), low+1, up);
    }
}

# define UNCOLLECTABLE_CDR(x) (sexpr)(~(unsigned long)(cdr(x)))

void check_uncollectable_ints(list, low, up)
sexpr list;
int low, up;
{
    if ((int)(car(car(list))) != low) {
        (void)GC_printf0(
           "Uncollectable list corrupted - collector is broken\n");
        FAIL;
    }
    if (low == up) {
        if (UNCOLLECTABLE_CDR(list) != nil) {
           (void)GC_printf0("Uncollectable ist too long - collector is broken\n");
           FAIL;
        }
    } else {
        check_uncollectable_ints(UNCOLLECTABLE_CDR(list), low+1, up);
    }
}

/* Not used, but useful for debugging: */
void print_int_list(x)
sexpr x;
{
    if (is_nil(x)) {
        (void)GC_printf0("NIL\n");
    } else {
        (void)GC_printf1("(%ld)", (long)(car(car(x))));
        if (!is_nil(cdr(x))) {
            (void)GC_printf0(", ");
            (void)print_int_list(cdr(x));
        } else {
            (void)GC_printf0("\n");
        }
    }
}

/* Try to force a to be strangely aligned */
struct {
  char dummy;
  sexpr aa;
} A;
#define a A.aa

/*
 * Repeatedly reverse lists built out of very different sized cons cells.
 * Check that we didn't lose anything.
 */
void reverse_test()
{
    int i;
    sexpr b;
    sexpr c;
    sexpr d;
    sexpr e;
#   if defined(MSWIN32) || defined(MACOS)
      /* Win32S only allows 128K stacks */
#     define BIG 1000
#   else
#     define BIG 4500
#   endif

    A.dummy = 17;
    a = ints(1, 49);
    b = ints(1, 50);
    c = ints(1, BIG);
    d = uncollectable_ints(1, 100);
    e = uncollectable_ints(1, 1);
    /* Superficially test interior pointer recognition on stack */
    c = (sexpr)((char *)c + sizeof(char *));
    d = (sexpr)((char *)d + sizeof(char *));
#   ifdef __STDC__
        GC_FREE((void *)e);
#   else
        GC_FREE((char *)e);
#   endif
    check_ints(b,1,50);
    check_ints(a,1,49);
    for (i = 0; i < 50; i++) {
        check_ints(b,1,50);
        b = reverse(reverse(b));
    }
    check_ints(b,1,50);
    check_ints(a,1,49);
    for (i = 0; i < 60; i++) {
    	/* This maintains the invariant that a always points to a list of */
    	/* 49 integers.  Thus this is thread safe without locks.	  */
        a = reverse(reverse(a));
#	if !defined(AT_END) && !defined(THREADS)
	  /* This is not thread safe, since realloc explicitly deallocates */
          if (i & 1) {
            a = (sexpr)GC_REALLOC((void_star)a, 500);
          } else {
            a = (sexpr)GC_REALLOC((void_star)a, 8200);
          }
#	endif
    }
    check_ints(a,1,49);
    check_ints(b,1,50);
    c = (sexpr)((char *)c - sizeof(char *));
    d = (sexpr)((char *)d - sizeof(char *));
    check_ints(c,1,BIG);
    check_uncollectable_ints(d, 1, 100);
#   ifndef THREADS
	a = 0;
#   endif  
    b = c = 0;
}

/*
 * The rest of this builds balanced binary trees, checks that they don't
 * disappear, and tests finalization.
 */
typedef struct treenode {
    int level;
    struct treenode * lchild;
    struct treenode * rchild;
} tn;

int finalizable_count = 0;
int finalized_count = 0;
int dropped_something = 0;

# ifdef __STDC__
  void finalizer(void * obj, void * client_data)
# else
  void finalizer(obj, client_data)
  char * obj;
  char * client_data;
# endif
{
  tn * t = (tn *)obj;

# ifdef PCR
     PCR_ThCrSec_EnterSys();
# endif
# ifdef SOLARIS_THREADS
    static mutex_t incr_lock;
    mutex_lock(&incr_lock);
# endif
  if ((int)client_data != t -> level) {
     (void)GC_printf0("Wrong finalization data - collector is broken\n");
     FAIL;
  }
  finalized_count++;
# ifdef PCR
    PCR_ThCrSec_ExitSys();
# endif
# ifdef SOLARIS_THREADS
    mutex_unlock(&incr_lock);
# endif
}

size_t counter = 0;

# define MAX_FINALIZED 8000

# if !defined(MACOS)
  GC_FAR GC_word live_indicators[MAX_FINALIZED] = {0};
#else
  /* Too big for THINK_C. have to allocate it dynamically. */
  GC_word *live_indicators = 0;
#endif

int live_indicators_count = 0;

tn * mktree(n)
int n;
{
    tn * result = (tn *)GC_MALLOC(sizeof(tn));
    
#if defined(MACOS)
	/* get around static data limitations. */
	if (!live_indicators)
		live_indicators =
		    (GC_word*)NewPtrClear(MAX_FINALIZED * sizeof(GC_word));
	if (!live_indicators) {
        (void)GC_printf0("Out of memory\n");
        exit(1);
    }
#endif
    if (n == 0) return(0);
    if (result == 0) {
        (void)GC_printf0("Out of memory\n");
        exit(1);
    }
    result -> level = n;
    result -> lchild = mktree(n-1);
    result -> rchild = mktree(n-1);
    if (counter++ % 17 == 0 && n >= 2) {
        tn * tmp = result -> lchild -> rchild;
        
        result -> lchild -> rchild = result -> rchild -> lchild;
        result -> rchild -> lchild = tmp;
    }
    if (counter++ % 119 == 0) {
        int my_index;
        
        {
#	  ifdef PCR
 	    PCR_ThCrSec_EnterSys();
#	  endif
#	  ifdef SOLARIS_THREADS
	    static mutex_t incr_lock;
	    mutex_lock(&incr_lock);
#	  endif
		/* Losing a count here causes erroneous report of failure. */
          finalizable_count++;
          my_index = live_indicators_count++;
#	  ifdef PCR
 	    PCR_ThCrSec_ExitSys();
#	  endif
#	  ifdef SOLARIS_THREADS
	    mutex_unlock(&incr_lock);
#	  endif
	}

        GC_REGISTER_FINALIZER((void_star)result, finalizer, (void_star)n,
        		      (GC_finalization_proc *)0, (void_star *)0);
        if (my_index >= MAX_FINALIZED) {
		GC_printf0("live_indicators overflowed\n");
		FAIL;
	}
        live_indicators[my_index] = 13;
        if (GC_GENERAL_REGISTER_DISAPPEARING_LINK(
         	(void_star *)(&(live_indicators[my_index])),
         	(void_star)result) != 0) {
         	GC_printf0("GC_general_register_disappearing_link failed\n");
         	FAIL;
        }
        if (GC_unregister_disappearing_link(
         	(void_star *)
         	   (&(live_indicators[my_index]))) == 0) {
         	GC_printf0("GC_unregister_disappearing_link failed\n");
         	FAIL;
        }
        if (GC_GENERAL_REGISTER_DISAPPEARING_LINK(
         	(void_star *)(&(live_indicators[my_index])),
         	(void_star)result) != 0) {
         	GC_printf0("GC_general_register_disappearing_link failed 2\n");
         	FAIL;
        }
    }
    return(result);
}

void chktree(t,n)
tn *t;
int n;
{
    if (n == 0 && t != 0) {
        (void)GC_printf0("Clobbered a leaf - collector is broken\n");
        FAIL;
    }
    if (n == 0) return;
    if (t -> level != n) {
        (void)GC_printf1("Lost a node at level %lu - collector is broken\n",
        		 (unsigned long)n);
        FAIL;
    }
    if (counter++ % 373 == 0) (void) GC_MALLOC(counter%5001);
    chktree(t -> lchild, n-1);
    if (counter++ % 73 == 0) (void) GC_MALLOC(counter%373);
    chktree(t -> rchild, n-1);
}

# ifdef SOLARIS_THREADS
thread_key_t fl_key;

void * alloc8bytes()
{
    void ** my_free_list_ptr;
    void * my_free_list;
    
    if (thr_getspecific(fl_key, (void **)(&my_free_list_ptr)) != 0) {
    	(void)GC_printf0("thr_getspecific failed\n");
    	FAIL;
    }
    if (my_free_list_ptr == 0) {
        my_free_list_ptr = GC_NEW_UNCOLLECTABLE(void *);
        if (thr_setspecific(fl_key, my_free_list_ptr) != 0) {
    	    (void)GC_printf0("thr_setspecific failed\n");
    	    FAIL;
        }
    }
    my_free_list = *my_free_list_ptr;
    if (my_free_list == 0) {
        my_free_list = GC_malloc_many(8);
        if (my_free_list == 0) {
            (void)GC_printf0("alloc8bytes out of memory\n");
    	    FAIL;
        }
    }
    *my_free_list_ptr = GC_NEXT(my_free_list);
    GC_NEXT(my_free_list) = 0;
    return(my_free_list);
}

#else
# define alloc8bytes() GC_MALLOC_ATOMIC(8)
#endif

void alloc_small(n)
int n;
{
    register int i;
    
    for (i = 0; i < n; i += 8) {
        if (alloc8bytes() == 0) {
            (void)GC_printf0("Out of memory\n");
            FAIL;
        }
    }
}

# if defined(THREADS) && defined(GC_DEBUG)
#   define TREE_HEIGHT 15
# else
#   define TREE_HEIGHT 16
# endif
void tree_test()
{
    tn * root;
    register int i;
    
    root = mktree(TREE_HEIGHT);
    alloc_small(5000000);
    chktree(root, TREE_HEIGHT);
    if (finalized_count && ! dropped_something) {
        (void)GC_printf0("Premature finalization - collector is broken\n");
        FAIL;
    }
    dropped_something = 1;
    root = mktree(TREE_HEIGHT);
    chktree(root, TREE_HEIGHT);
    for (i = TREE_HEIGHT; i >= 0; i--) {
        root = mktree(i);
        chktree(root, i);
    }
    alloc_small(5000000);
}

unsigned n_tests = 0;

/* A very simple test of explicitly typed allocation	*/
void typed_test()
{
    GC_word * old, * new;
    GC_word bm3 = 0x3;
    GC_word bm2 = 0x2;
    GC_word bm_large = 0xf7ff7fff;
    GC_descr d1 = GC_make_descriptor(&bm3, 2);
    GC_descr d2 = GC_make_descriptor(&bm2, 2);
#   ifndef LINT
      GC_descr dummy = GC_make_descriptor(&bm_large, 32);
#   endif
    GC_descr d3 = GC_make_descriptor(&bm_large, 32);
    register int i;
    
    old = 0;
    for (i = 0; i < 4000; i++) {
        new = (GC_word *) GC_malloc_explicitly_typed(4 * sizeof(GC_word), d1);
        new[0] = 17;
        new[1] = (GC_word)old;
        old = new;
        new = (GC_word *) GC_malloc_explicitly_typed(4 * sizeof(GC_word), d2);
        new[0] = 17;
        new[1] = (GC_word)old;
        old = new;
        new = (GC_word *) GC_malloc_explicitly_typed(33 * sizeof(GC_word), d3);
        new[0] = 17;
        new[1] = (GC_word)old;
        old = new;
        new = (GC_word *) GC_calloc_explicitly_typed(4, 2 * sizeof(GC_word),
        					     d1);
        new[0] = 17;
        new[1] = (GC_word)old;
        old = new;
        if (i & 0xff) {
          new = (GC_word *) GC_calloc_explicitly_typed(7, 3 * sizeof(GC_word),
        					     d2);
        } else {
          new = (GC_word *) GC_calloc_explicitly_typed(1001,
          					       3 * sizeof(GC_word),
        					       d2);
        }
        new[0] = 17;
        new[1] = (GC_word)old;
        old = new;
    }
    for (i = 0; i < 20000; i++) {
        if (new[0] != 17) {
            (void)GC_printf1("typed alloc failed at %lu\n",
            		     (unsigned long)i);
            FAIL;
        }
        new[0] = 0;
        old = new;
        new = (GC_word *)(old[1]);
    }
}

void run_one_test()
{
    DCL_LOCK_STATE;
    
#   ifndef GC_DEBUG
	if (GC_size(GC_MALLOC(7)) != 8
	    || GC_size(GC_MALLOC(15)) != 16) {
	    (void)GC_printf0("GC_size produced unexpected results\n");
	    FAIL;
	}
#   endif
    reverse_test();
#   ifdef PRINTSTATS
	GC_printf0("-------------Finished reverse_test\n");
#   endif
    typed_test();
#   ifdef PRINTSTATS
	GC_printf0("-------------Finished typed_test\n");
#   endif
    tree_test();
    LOCK();
    n_tests++;
    UNLOCK();
    
}

void check_heap_stats()
{
    unsigned long max_heap_sz;
    register int i;
    int still_live;
    
    if (sizeof(char *) > 4) {
        max_heap_sz = 13000000;
    } else {
    	max_heap_sz = 11000000;
    }
#   ifdef GC_DEBUG
	max_heap_sz *= 2;
#       ifdef SPARC
	    max_heap_sz *= 2;
#       endif
#   endif
    /* Garbage collect repeatedly so that all inaccessible objects	*/
    /* can be finalized.						*/
      while (GC_collect_a_little()) { }
      for (i = 0; i < 16; i++) {
        GC_gcollect();
      }
    (void)GC_printf1("Completed %lu tests\n", (unsigned long)n_tests);
    (void)GC_printf2("Finalized %lu/%lu objects - ",
    		     (unsigned long)finalized_count,
    		     (unsigned long)finalizable_count);
    if (finalized_count > finalizable_count
        || finalized_count < finalizable_count/2) {
        (void)GC_printf0("finalization is probably broken\n");
        FAIL;
    } else {
        (void)GC_printf0("finalization is probably ok\n");
    }
    still_live = 0;
    for (i = 0; i < MAX_FINALIZED; i++) {
    	if (live_indicators[i] != 0) {
    	    still_live++;
    	}
    }
    if (still_live != finalizable_count - finalized_count) {
        (void)GC_printf1
            ("%lu disappearing links remain - disappearing links are broken\n",
             (unsigned long) still_live);
        FAIL;
    }
    (void)GC_printf1("Total number of bytes allocated is %lu\n",
    		(unsigned long)
    	           WORDS_TO_BYTES(GC_words_allocd + GC_words_allocd_before_gc));
    (void)GC_printf1("Final heap size is %lu bytes\n",
    		     (unsigned long)GC_get_heap_size());
    if (WORDS_TO_BYTES(GC_words_allocd + GC_words_allocd_before_gc)
        < 33500000*n_tests) {
        (void)GC_printf0("Incorrect execution - missed some allocations\n");
        FAIL;
    }
    if (GC_get_heap_size() > max_heap_sz*n_tests) {
        (void)GC_printf0("Unexpected heap growth - collector may be broken\n");
        FAIL;
    }
    (void)GC_printf0("Collector appears to work\n");
}

#if defined(MACOS)
void SetMinimumStack(long minSize)
{
	long newApplLimit;

	if (minSize > LMGetDefltStack())
	{
		newApplLimit = (long) GetApplLimit()
				- (minSize - LMGetDefltStack());
		SetApplLimit((Ptr) newApplLimit);
		MaxApplZone();
	}
}

#define cMinStackSpace (512L * 1024L)

#endif

#if !defined(PCR) && !defined(SOLARIS_THREADS) || defined(LINT)
#ifdef MSWIN32
  int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int n)
#else
  int main()
#endif
{
    n_tests = 0;
    
#   if defined(MACOS)
	/* Make sure we have lots and lots of stack space. 	*/
	SetMinimumStack(cMinStackSpace);
	/* Cheat and let stdio initialize toolbox for us.	*/
	printf("Testing GC Macintosh port.\n");
#   endif
#   if defined(MPROTECT_VDB) || defined(PROC_VDB)
      GC_enable_incremental();
      (void) GC_printf0("Switched to incremental mode\n");
#     if defined(MPROTECT_VDB)
	(void)GC_printf0("Emulating dirty bits with mprotect/signals\n");
#     else
	(void)GC_printf0("Reading dirty bits from /proc\n");
#      endif
#   endif
    run_one_test();
    check_heap_stats();
    (void)fflush(stdout);
#   ifdef LINT
	/* Entry points we should be testing, but aren't.		   */
	/* Some can be tested by defining GC_DEBUG at the top of this file */
	/* This is a bit SunOS4 specific.				   */			
	GC_noop(GC_expand_hp, GC_add_roots, GC_clear_roots,
	        GC_register_disappearing_link,
	        GC_print_obj, GC_debug_change_stubborn,
	        GC_debug_end_stubborn_change, GC_debug_malloc_uncollectable,
	        GC_debug_free, GC_debug_realloc, GC_generic_malloc_words_small,
	        GC_init, GC_make_closure, GC_debug_invoke_finalizer,
	        GC_page_was_ever_dirty, GC_is_fresh,
		GC_malloc_ignore_off_page, GC_malloc_atomic_ignore_off_page);
#   endif
    return(0);
}
# endif

#ifdef PCR
test()
{
    PCR_Th_T * th1;
    PCR_Th_T * th2;
    int code;

    n_tests = 0;
    GC_enable_incremental();
    th1 = PCR_Th_Fork(run_one_test, 0);
    th2 = PCR_Th_Fork(run_one_test, 0);
    run_one_test();
    if (PCR_Th_T_Join(th1, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        (void)GC_printf0("Thread 1 failed\n");
    }
    if (PCR_Th_T_Join(th2, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        (void)GC_printf0("Thread 2 failed\n");
    }
    check_heap_stats();
    (void)fflush(stdout);
    return(0);
}
#endif

#ifdef SOLARIS_THREADS
void * thr_run_one_test(void * arg)
{
    run_one_test();
    return(0);
}

#ifdef GC_DEBUG
#  define GC_free GC_debug_free
#endif

main()
{
    thread_t th1;
    thread_t th2;
    int code;

    n_tests = 0;
    GC_enable_incremental();
    if (thr_keycreate(&fl_key, GC_free) != 0) {
        (void)GC_printf1("Key creation failed %lu\n", (unsigned long)code);
    	FAIL;
    }
    if ((code = thr_create(0, 1024*1024, thr_run_one_test, 0, 0, &th1)) != 0) {
    	(void)GC_printf1("Thread 1 creation failed %lu\n", (unsigned long)code);
    	FAIL;
    }
    if ((code = thr_create(0, 1024*1024, thr_run_one_test, 0, THR_NEW_LWP, &th2)) != 0) {
    	(void)GC_printf1("Thread 2 creation failed %lu\n", (unsigned long)code);
    	FAIL;
    }
    run_one_test();
    if ((code = thr_join(th1, 0, 0)) != 0) {
        (void)GC_printf1("Thread 1 failed %lu\n", (unsigned long)code);
        FAIL;
    }
    if (thr_join(th2, 0, 0) != 0) {
        (void)GC_printf1("Thread 2 failed %lu\n", (unsigned long)code);
        FAIL;
    }
    check_heap_stats();
    (void)fflush(stdout);
    return(0);
}
#endif
