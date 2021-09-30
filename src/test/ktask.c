/*
 * MIT License
 *
 * Copyright(c) 2018 Pedro Henrique Penna <pedrohenriquepenna@gmail.com>
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


#include <nanvix/sys/task.h>
#include "test.h"

#if __NANVIX_USE_TASKS

/*============================================================================*
 * Global definition                                                          *
 *============================================================================*/

/**
 * @brief Specific value for tests.
 */
#define TEST_TASK_SPECIFIC_VALUE 123

/*============================================================================*
 * Test functions and variables.                                              *
 *============================================================================*/

/*----------------------------------------------------------------------------*
 * Dummy test.                                                                *
 *----------------------------------------------------------------------------*/

/**
 * @brief Dummy thread function.
 *
 * @param arg Unused argument.
 */
PRIVATE void * task(void *arg)
{
	UNUSED(arg);

	return (NULL);
}

/**
 * @brief Dummy task function.
 *
 * @param arg Unused argument.
 */
PRIVATE int dummy(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	return (TASK_RET_SUCCESS);
}

/*----------------------------------------------------------------------------*
 * Setter test.                                                               *
 *----------------------------------------------------------------------------*/

PRIVATE int task_value;
PRIVATE int task_value2;

PRIVATE int setter(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg1);
	UNUSED(arg2);

	task_value = (int) arg0;

	//args->ret  = 0;
	//ktask_current()->retval = 0;

	return (TASK_RET_SUCCESS);
}

/*----------------------------------------------------------------------------*
 * Get current task test.                                                     *
 *----------------------------------------------------------------------------*/

PRIVATE spinlock_t master_lock;
PRIVATE spinlock_t slave_lock;

PRIVATE int current_fn(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	spinlock_unlock(&master_lock);
	spinlock_lock(&slave_lock);

	//args->ret = 0;

	return (TASK_RET_SUCCESS);
}

/*----------------------------------------------------------------------------*
 * Inheritance.                                                               *
 *----------------------------------------------------------------------------*/

PRIVATE int parent_simple(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value *= 5;
	//args->ret   = 0;

	return (TASK_RET_SUCCESS);
}

PRIVATE int child_simple(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value /= 10;
	//args->ret   =  0;

	return (TASK_RET_SUCCESS);
}

PRIVATE int parent_children(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value  *= 5;
	task_value2 /= 5;
	//args->ret    = 0;

	return (TASK_RET_SUCCESS);
}

PRIVATE int child0_children(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value /= 10;
	//args->ret   =  0;

	return (TASK_RET_SUCCESS);
}

PRIVATE int child1_children(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value2 *= 2;

	return (TASK_RET_SUCCESS);
}

PRIVATE int parent0_parent(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value *= 5;
	//args->ret   = (0);

	return (TASK_RET_SUCCESS);
}

PRIVATE int parent1_parent(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value2 /= 5;
	//args->ret    = 0;

	return (TASK_RET_SUCCESS);
}

PRIVATE int child_parent(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg0);
	UNUSED(arg1);
	UNUSED(arg2);

	task_value  /= 10;
	task_value2 *=  2;
	//args->ret    =  0;

	return (TASK_RET_SUCCESS);
}

/*----------------------------------------------------------------------------*
 * Periodic.                                                                  *
 *----------------------------------------------------------------------------*/

PRIVATE int periodic(word_t arg0, word_t arg1, word_t arg2)
{
	UNUSED(arg1);
	UNUSED(arg2);

	if ((++task_value) < (int) arg0)
		return (TASK_RET_AGAIN);

	return (TASK_RET_SUCCESS);
}

/*----------------------------------------------------------------------------*
 * Emission.                                                                  *
 *----------------------------------------------------------------------------*/

PRIVATE int emission(word_t arg0, word_t arg1, word_t arg2)
{
	int coreid = core_get_id();

	test_assert(((int) arg0) == coreid);
	test_assert(arg1 == 1);
	test_assert(arg2 == 2);

	return (TASK_RET_SUCCESS);
}

/*============================================================================*
 * API Testing Units                                                          *
 *============================================================================*/

/*----------------------------------------------------------------------------*
 * test_api_ktask_create()                                                    *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for task create.
 */
PRIVATE void test_api_ktask_create(void)
{
	ktask_t t;

	test_assert(ktask_create(&t, dummy, 0) == 0);

	/* Configuration. */
	test_assert(t.state == TASK_STATE_NOT_STARTED);
	test_assert(t.id    != TASK_NULL_ID);

	/* Arguments and returns. */
	test_assert(t.fn == dummy);

	/* Dependency. */
	test_assert(t.parents == 0);
	test_assert(t.children.size == 0);

	test_assert(ktask_unlink(&t) == 0);

	test_assert(t.id == TASK_NULL_ID);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_connect()                                                   *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for task connect two tasks.
 */
PRIVATE void test_api_ktask_connect(void)
{
	ktask_t t0, t1, t2, t3;

	test_assert(ktask_create(&t0, dummy, 0) == 0);
	test_assert(ktask_create(&t1, dummy, 0) == 0);
	test_assert(ktask_create(&t2, dummy, 0) == 0);
	test_assert(ktask_create(&t3, dummy, 0) == 0);

	test_assert(ktask_connect(&t1, &t2) == 0);

	/* Parent. */
	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 1);

	/* Children. */
	test_assert(t2.parents == 1);
	test_assert(t2.children.size == 0);

	test_assert(ktask_connect(&t1, &t3) == 0);

	/* Parent. */
	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 2);

	/* Children. */
	test_assert(t3.parents == 1);
	test_assert(t3.children.size == 0);

	test_assert(ktask_connect(&t0, &t1) == 0);
	test_assert(ktask_connect(&t0, &t3) == 0);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 2);

	test_assert(t1.parents == 1);
	test_assert(t1.children.size == 2);

	/* Children. */
	test_assert(t2.parents == 1);
	test_assert(t2.children.size == 0);

	test_assert(t3.parents == 2);
	test_assert(t3.children.size == 0);

	test_assert(ktask_disconnect(&t0, &t3) == 0);
	test_assert(ktask_disconnect(&t0, &t1) == 0);
	test_assert(ktask_disconnect(&t1, &t3) == 0);
	test_assert(ktask_disconnect(&t1, &t2) == 0);

	test_assert(ktask_unlink(&t0) == 0);
	test_assert(ktask_unlink(&t1) == 0);
	test_assert(ktask_unlink(&t2) == 0);
	test_assert(ktask_unlink(&t3) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_dispatch()                                                  *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch a task.
 */
PRIVATE void test_api_ktask_dispatch(void)
{
	ktask_t t;

	task_value  = 0ULL;

	test_assert(ktask_create(&t, setter, 0) == 0);

	test_assert(ktask_dispatch1(&t, TEST_TASK_SPECIFIC_VALUE) == 0);
	test_assert(ktask_wait(&t) == 0);

	/* Complete the task. */
	test_assert(t.state   == TASK_STATE_COMPLETED);
	test_assert(t.id      != TASK_NULL_ID);
	test_assert(t.args[0] == TEST_TASK_SPECIFIC_VALUE);
	test_assert(t.args[1] == 0);
	test_assert(t.args[2] == 0);
	test_assert(t.retval  == 0);

	/* Check the value. */
	test_assert(task_value == TEST_TASK_SPECIFIC_VALUE);

	test_assert(ktask_unlink(&t) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_identification()                                            *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for identificate a task.
 */
PRIVATE void test_api_ktask_identification(void)
{
	int tid;
	ktask_t t;

	test_assert(ktask_create(&t, dummy, 0) == 0);

	test_assert((tid = ktask_get_id(&t)) != TASK_NULL_ID);

		test_assert(ktask_dispatch0(&t) == 0);
		test_assert(ktask_wait(&t) == 0);

	test_assert(ktask_get_id(&t) == tid);

	test_assert(ktask_unlink(&t) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_current()                                                   *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for get the current task.
 */
PRIVATE void test_api_ktask_current(void)
{
	ktask_t t;

	spinlock_init(&master_lock);
	spinlock_init(&slave_lock);

	spinlock_lock(&master_lock);
	spinlock_lock(&slave_lock);

	test_assert(ktask_create(&t, current_fn, 0) == 0);

	test_assert(ktask_dispatch0(&t) == 0);

		spinlock_lock(&master_lock);

			test_assert(ktask_current() == &t);

		spinlock_unlock(&slave_lock);

	test_assert(ktask_wait(&t) == 0);

	test_assert(ktask_unlink(&t) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_dependendy()                                                *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch tasks with dependency.
 */
PRIVATE void test_api_ktask_dependendy(void)
{
	ktask_t t0, t1;

	task_value = 20;

	test_assert(ktask_create(&t0, parent_simple, 0) == 0);
	test_assert(ktask_create(&t1, child_simple, 0) == 0);

	test_assert(ktask_connect(&t0, &t1) == 0);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 1);

	/* Children. */
	test_assert(t1.parents == 1);
	test_assert(t1.children.size == 0);

	test_assert(ktask_dispatch0(&t1) < 0);
	test_assert(ktask_dispatch0(&t0) == 0);
	test_assert(ktask_wait(&t0) == 0);
	test_assert(ktask_wait(&t1) == 0);

	test_assert(task_value == 10);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 0);

	/* Children. */
	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 0);

	test_assert(ktask_disconnect(&t0, &t1) < 0);

	test_assert(ktask_unlink(&t0) == 0);
	test_assert(ktask_unlink(&t1) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_children()                                                  *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch a task with multiple children.
 */
PRIVATE void test_api_ktask_children(void)
{
	ktask_t t0, t1, t2;

	task_value  = 20;
	task_value2 = 20;

	test_assert(ktask_create(&t0, parent_children, 0) == 0);
	test_assert(ktask_create(&t1, child0_children, 0) == 0);
	test_assert(ktask_create(&t2, child1_children, 0) == 0);

	test_assert(ktask_connect(&t0, &t1) == 0);
	test_assert(ktask_connect(&t0, &t2) == 0);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 2);

	/* Children. */
	test_assert(t1.parents == 1);
	test_assert(t1.children.size == 0);
	test_assert(t2.parents == 1);
	test_assert(t2.children.size == 0);

	test_assert(ktask_dispatch0(&t2) < 0);
	test_assert(ktask_dispatch0(&t1) < 0);
	test_assert(ktask_dispatch0(&t0) == 0);

	test_assert(ktask_wait(&t1) == 0);
	test_assert(task_value == 10);

	test_assert(ktask_wait(&t2) == 0);
	test_assert(task_value2 == 8);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 0);

	/* Children. */
	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 0);
	test_assert(t2.parents == 0);
	test_assert(t2.children.size == 0);

	test_assert(ktask_disconnect(&t0, &t2) < 0);
	test_assert(ktask_disconnect(&t0, &t1) < 0);

	test_assert(ktask_unlink(&t0) == 0);
	test_assert(ktask_unlink(&t1) == 0);
	test_assert(ktask_unlink(&t2) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_parent()                                                    *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch multiple parents with the same children.
 */
PRIVATE void test_api_ktask_parent(void)
{
	ktask_t t0, t1, t2;

	task_value  = 20;
	task_value2 = 20;

	test_assert(ktask_create(&t0, parent0_parent, 0) == 0);
	test_assert(ktask_create(&t1, parent1_parent, 0) == 0);
	test_assert(ktask_create(&t2, child_parent, 0) == 0);

	test_assert(ktask_connect(&t0, &t2) == 0);
	test_assert(ktask_connect(&t1, &t2) == 0);

	/* Parent. */
	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 1);
	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 1);

	/* Children. */
	test_assert(t2.parents == 2);
	test_assert(t2.children.size == 0);

	test_assert(ktask_dispatch0(&t0) == 0);
	test_assert(ktask_wait(&t0) == 0);

	test_assert(t0.parents == 0);
	test_assert(t0.children.size == 0);
	test_assert(t2.parents == 1);
	test_assert(t2.state == TASK_STATE_NOT_STARTED);

	test_assert(ktask_dispatch0(&t1) == 0);
	test_assert(ktask_wait(&t1) == 0);

	test_assert(ktask_wait(&t2) == 0);
	test_assert(task_value == 10);
	test_assert(task_value2 == 8);

	test_assert(t1.parents == 0);
	test_assert(t1.children.size == 0);

	test_assert(ktask_disconnect(&t1, &t2) < 0);
	test_assert(ktask_disconnect(&t0, &t2) < 0);

	test_assert(ktask_unlink(&t0) == 0);
	test_assert(ktask_unlink(&t1) == 0);
	test_assert(ktask_unlink(&t2) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_periodic()                                                  *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch a task.
 */
PRIVATE void test_api_ktask_periodic(void)
{
	ktask_t t;

	task_value = 0;

	test_assert(ktask_create(&t, periodic, 10) == 0);

	test_assert(ktask_dispatch1(&t, 10) == 0);
	test_assert(ktask_wait(&t) == 0);

	/* Check the value. */
	test_assert(task_value == 10);

	test_assert(ktask_unlink(&t) == 0);
}

/*----------------------------------------------------------------------------*
 * test_api_ktask_emit()                                                      *
 *----------------------------------------------------------------------------*/

/**
 * @brief API test for dispatch a task.
 */
PRIVATE void test_api_ktask_emit(void)
{
	ktask_t t;

	/* Create the task. */
	test_assert(ktask_create(&t, emission, 10) == 0);

	/* Emit the task. */
	for (int coreid = 0; coreid < CORES_NUM; ++coreid)
	{
		test_assert(ktask_emit3(&t, coreid, coreid, 1, 2) == 0);
		test_assert(ktask_wait(&t) == 0);
	}

	/* Unlink the task. */
	test_assert(ktask_unlink(&t) == 0);
}

/*============================================================================*
 * Test Driver                                                                *
 *============================================================================*/

/**
 * @brief API tests.
 */
PRIVATE struct test task_mgmt_tests_api[] = {
	{ test_api_ktask_create,         "[test][task][api] task create         [passed]" },
	{ test_api_ktask_connect,        "[test][task][api] task connect        [passed]" },
	{ test_api_ktask_dispatch,       "[test][task][api] task dispatch       [passed]" },
	{ test_api_ktask_identification, "[test][task][api] task identification [passed]" },
	{ test_api_ktask_current,        "[test][task][api] task current        [passed]" },
	{ test_api_ktask_dependendy,     "[test][task][api] task dependency     [passed]" },
	{ test_api_ktask_children,       "[test][task][api] task children       [passed]" },
	{ test_api_ktask_parent,         "[test][task][api] task parent         [passed]" },
	{ test_api_ktask_periodic,       "[test][task][api] task periodic       [passed]" },
	{ test_api_ktask_emit,           "[test][task][api] task emit           [passed]" },
	{ NULL,                           NULL                                            },
};

/**
 * @brief Fault tests.
 */
PRIVATE struct test task_mgmt_tests_fault[] = {
	{ NULL,                           NULL                                            },
};

/**
 * @brief Stress tests.
 */
PRIVATE struct test task_mgmt_tests_stress[] = {
	{ NULL,                           NULL                                            },
};

#endif /* __NANVIX_USE_TASKS */

/**
 * The test_task_mgmt() function launches testing units on task manager.
 *
 * @author João Vicente Souto
 */
void test_task_mgmt(void)
{
#if __NANVIX_USE_TASKS

	/* API Tests */
	nanvix_puts("--------------------------------------------------------------------------------");
	for (int i = 0; task_mgmt_tests_api[i].test_fn != NULL; i++)
	{
		task_mgmt_tests_api[i].test_fn();
		nanvix_puts(task_mgmt_tests_api[i].name);
	}

	/* Fault Tests */
	nanvix_puts("--------------------------------------------------------------------------------");
	for (int i = 0; task_mgmt_tests_fault[i].test_fn != NULL; i++)
	{
		task_mgmt_tests_fault[i].test_fn();
		nanvix_puts(task_mgmt_tests_fault[i].name);
	}

	/* Stress Tests */
	nanvix_puts("--------------------------------------------------------------------------------");
	for (int i = 0; task_mgmt_tests_stress[i].test_fn != NULL; i++)
	{
		task_mgmt_tests_stress[i].test_fn();
		nanvix_puts(task_mgmt_tests_stress[i].name);
	}

#endif /* __NANVIX_USE_TASKS */
}

