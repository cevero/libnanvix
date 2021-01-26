/*
 * MIT License
 *
 * Copyright(c) 2011-2020 The Maintainers of Nanvix
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define __NANVIX_MICROKERNEL

#include <nanvix/kernel/excp.h>
#include <nanvix/sys/page.h>
#include <nanvix/sys/tlb.h>
#include "test.h"

#if (CLUSTER_HAS_HW_TLB_SHOOTDOWN || CLUSTER_HAS_SW_TLB_SHOOTDOWN)

/**
 * @brief Affinity of each thread.
 */
#define CORE_T0 (1)
#define CORE_T1 (2)
#define CORE_T2 (3)

#define MAGIC_NUMBER 0xdeadbeef

/*============================================================================*
 * API Tests                                                                  *
 *============================================================================*/

/**
 * @brief API Test: TLB shootdown
 *
 * @author João Vicente Souto
 */
static void test_api_tlb_shootdown(void)
{
	unsigned *pg;

	KASSERT(kthread_set_affinity(1 << 1) == KTHREAD_AFFINITY_DEFAULT);

	pg = (unsigned *) UBASE_VIRT;

	KASSERT(page_alloc(VADDR(pg)) == 0);

		pg[10] = MAGIC_NUMBER;

		KASSERT(ktlb_shootdown(VADDR(pg)) == 0);
		KASSERT(ktlb_shootdown(VADDR(pg)) == 0);

		KASSERT(pg[10] == MAGIC_NUMBER);

	KASSERT(page_free(VADDR(pg)) == 0);

	KASSERT(kthread_set_affinity(KTHREAD_AFFINITY_DEFAULT) == (1 << 1));
}

static spinlock_t s0;
static spinlock_t s1;
static spinlock_t s2;

/**
 * @brief API Test: TLB shootdown
 *
 * @author João Vicente Souto
 */
static void * test_api_tlb_shootdown_allocator(void * args)
{
	unsigned *pg;

	UNUSED(args);

	KASSERT(kthread_set_affinity(1 << CORE_T0) == KTHREAD_AFFINITY_DEFAULT);

	pg = (unsigned *) UBASE_VIRT;

	/* Waits t2. */
	spinlock_lock(&s2);

	KASSERT(page_alloc(VADDR(pg)) == 0);

		pg[0] = MAGIC_NUMBER;

		/* Release t1. */
		spinlock_unlock(&s1);

		/* Waits the t1 release the page. */
		spinlock_lock(&s0);

		/* TLB exception. t2 will realloc. */
		pg[0] = MAGIC_NUMBER;

	KASSERT(page_free(VADDR(pg)) == 0);

	KASSERT(kthread_set_affinity(KTHREAD_AFFINITY_DEFAULT) == (1 << CORE_T0));

	return (NULL);
}

static void * test_api_tlb_shootdown_releaser(void * args)
{
	unsigned *pg;

	UNUSED(args);

	KASSERT(kthread_set_affinity(1 << CORE_T1) == KTHREAD_AFFINITY_DEFAULT);

	pg = (unsigned *) UBASE_VIRT;

	/* Waits t0. */
	spinlock_lock(&s1);

		KASSERT(page_free(VADDR(pg)) == 0);

	/* Release t0. */
	spinlock_unlock(&s0);

	KASSERT(kthread_set_affinity(KTHREAD_AFFINITY_DEFAULT) == (1 << CORE_T1));

	return (NULL);
}

static void * test_api_tlb_shootdown_handler(void * args)
{
	unsigned *pg;
	struct exception excp;

	UNUSED(args);

	KASSERT(kthread_set_affinity(1 << CORE_T2) == KTHREAD_AFFINITY_DEFAULT);

	pg = (unsigned *) UBASE_VIRT;

	KASSERT(exception_control(EXCEPTION_PAGE_FAULT, EXCP_ACTION_HANDLE) == 0);

		/* Fence on t2. */
		spinlock_unlock(&s2);

		/* Waits the exception. */
		KASSERT(exception_pause(&excp) == 0);

			/* Alloc the released page. */
			KASSERT(page_alloc(VADDR(pg)) == 0);

		/* Release excepted thread. */
		KASSERT(exception_resume() == 0);

	KASSERT(exception_control(EXCEPTION_PAGE_FAULT, EXCP_ACTION_IGNORE) == 0);

	KASSERT(kthread_set_affinity(KTHREAD_AFFINITY_DEFAULT) == (1 << CORE_T2));

	return (NULL);
}

static void test_api_tlb_shootdown_thread(void)
{
#ifdef __k1bio__
	return;
#endif

	kthread_t t1;
	kthread_t t2;

	spinlock_init(&s0);
	spinlock_init(&s1);
	spinlock_init(&s2);

	spinlock_lock(&s0);
	spinlock_lock(&s1);
	spinlock_lock(&s2);

	/* Spawn threads. */
	KASSERT(kthread_create(&t1, test_api_tlb_shootdown_releaser, NULL) == 0);
	KASSERT(kthread_create(&t2, test_api_tlb_shootdown_handler, NULL) == 0);

	test_api_tlb_shootdown_allocator(NULL);

	/* Join threads. */
	KASSERT(kthread_join(t1, NULL) == 0);
	KASSERT(kthread_join(t2, NULL) == 0);
}

/*============================================================================*
 * Fault Injection Tests                                                      *
 *============================================================================*/

/**
 * @brief Fault Injection Test: Invalid TLB shootdown.
 *
 * @author João Vicente Souto
 */
static void test_fault_tlb_shootdown(void)
{
	KASSERT(ktlb_shootdown(KBASE_VIRT) == -EINVAL);
	KASSERT(ktlb_shootdown(UBASE_VIRT + UMEM_SIZE) == -EINVAL);
}

/*============================================================================*
 * Test Driver                                                                *
 *============================================================================*/

/**
 * @brief API tests.
 */
static struct test tests_api_tlb[] = {
	{ test_api_tlb_shootdown,        "[test][tlb][api] tlb shootdown       " },
	{ test_api_tlb_shootdown_thread, "[test][tlb][api] tlb shootdown thread" },
	{ NULL,                           NULL                                   },
};

/**
 * @brief Fault injection tests.
 */
static struct test tests_fault_tlb[] = {
	{ test_fault_tlb_shootdown, "[test][tlb][fault] kernel shootdown" },
	{ NULL,                      NULL                                 },
};


#endif /* (CLUSTER_HAS_HW_TLB_SHOOTDOWN || CLUSTER_HAS_SW_TLB_SHOOTDOWN) */

/**
 * The test_page_mgmt() function launches testing units on the pageory
 * manager.
 *
 * @author Pedro Henrique Penna
 */
void test_tlb_mgmt(void)
{
#if (CLUSTER_HAS_HW_TLB_SHOOTDOWN || CLUSTER_HAS_SW_TLB_SHOOTDOWN)

	/* API Tests */
	nanvix_puts("--------------------------------------------------------------------------------");
	for (int i = 0; tests_api_tlb[i].test_fn != NULL; i++)
	{
		tests_api_tlb[i].test_fn();
		nanvix_puts(tests_api_tlb[i].name);
	}

	/* Fault Tests */
	nanvix_puts("--------------------------------------------------------------------------------");
	for (int i = 0; tests_fault_tlb[i].test_fn != NULL; i++)
	{
		tests_fault_tlb[i].test_fn();
		nanvix_puts(tests_fault_tlb[i].name);
	}

#endif /* (CLUSTER_HAS_HW_TLB_SHOOTDOWN || CLUSTER_HAS_SW_TLB_SHOOTDOWN) */
}
