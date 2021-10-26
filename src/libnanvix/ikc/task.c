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

#define __NEED_RESOURCE

#include <nanvix/sys/thread.h>

#include "task.h"

#if __NANVIX_USE_COMM_WITH_TASKS

/**
 * @brief Number of flows.
 */
#define IKC_FLOWS_MAX THREAD_MAX

/**
 * @brief Invalid communicator ID.
 */
#define IKC_FLOW_CID_INVALID (~0UL)

/**
 * @brief Get the flow pointer from a task pointer.
 */
#define IKC_FLOW_FROM_TASK(task, is_config) ((struct ikc_flow *) (task - (is_config ? 0 : 1)))

/**
 * @brief Get the kernel ID of a thread.
 */
#define KERNEL_TID (((thread_get_curr() - KTHREAD_MASTER) - SYS_THREAD_MAX))

/**
 * @name Communication status from noc/active.h.
 */
/**@{*/
#define ACTIVE_COMM_SUCCESS  (0)
#define ACTIVE_COMM_AGAIN    (1)
#define ACTIVE_COMM_RECEIVED (2)
/**@}*/

/**
 * @brief Compare the return value with flags that results on a Again action.
 */
#define IKC_FLOW_AGAIN_FLAGS(ret) ( \
	(ret == -EBUSY)                 \
	|| (ret == -EAGAIN)             \
	|| (ret == -EACCES)             \
	|| (ret == -ENOMSG)             \
	|| (ret == -ETIMEDOUT)          \
)

/**
 * @brief Communication Flow
 *
 * @details The 'config' and 'wait' tasks are permanently connected. On
 * a succeded configuration, the handler task is connected to the 'wait' task.
 * When the communication completes, the handler releases 'wait' task to
 * complete the user-side communication.
 *
 *             +--------------------------------+
 *             v                                |
 *          config -------------------------> wait
 *                                             ^
 *          handler (setted on active) - - - - +
 *
 */
struct ikc_flow
{
	ktask_t config;
	ktask_t wait;

	int ret;
	int type;
	word_t cid;
	struct resource resource;
};

/**
 * @brief Communication Flow Pool
 *
 * @details This structure holds the tasks that compose a communication flow by
 * any thread. Currently, we only support one communication per thread. So,
 * a thread can request a read/write and wait for its conclusion. In the future,
 * we can modify to support more flows but we must study how we will preserve
 * memory because this can request huge static arrays of structures.
 * However, when we are dealing with the Dispatcher, we provide more
 * communication because its can executes multiples types of flows and merge
 * them.
 */
PRIVATE struct ikc_flow_pool
{
	struct ikc_flow dispatchers[IKC_FLOWS_MAX];
	struct ikc_flow users[IKC_FLOWS_MAX];
} ikc_flows;

/**
 * @brief Protection.
 */
PRIVATE spinlock_t ikc_flow_lock;

/**
 * @brief Get current communication flow.
 *
 * @param is_config Specify if we are on __ikc_flow_config or __ikc_flow_wait function.
 *
 * @return Current communication flow.
 */
PRIVATE struct ikc_flow * ikc_flow_get_flow(bool is_config)
{
	struct ikc_flow * flow;

#ifndef NDEBUG
	if (UNLIKELY(kthread_self() != KTHREAD_DISPATCHER_TID))
		kpanic("[kernel][ikc][task] Communication must be executed by the dispatcher.");
#endif

	flow = IKC_FLOW_FROM_TASK(ktask_current(), is_config);

	if (UNLIKELY(!resource_is_used(&flow->resource)))
		kpanic("[kernel][ikc][task] Communication flow not used.");

	if (UNLIKELY(!WITHIN(flow->type, IKC_FLOW_MAILBOX_READ, IKC_FLOW_INVALID)))
		kpanic("[kernel][ikc][task] Unknown flow.");

	return (flow);
}

/**
 * @brief Configure the communication.
 *
 * @param arg0 communicator ID
 * @param arg1 buffer pointer
 * @param arg2 size
 *
 * @return Non-negative on success, negative on failure.
 */
PRIVATE int ___ikc_flow_config(word_t arg0, word_t arg1, word_t arg2)
{
	switch (ikc_flow_get_flow(true)->type)
	{
		case IKC_FLOW_MAILBOX_READ:
			return (
				__kmailbox_aread(
					(int) arg0,
					(void *)(long) arg1,
					(size_t) arg2
				)
			);

		case IKC_FLOW_MAILBOX_WRITE:
			return (
				__kmailbox_awrite(
					(int) arg0,
					(const void *)(long) arg1,
					(size_t) arg2
				)
			);

		case IKC_FLOW_PORTAL_READ:
			return (
				__kportal_aread(
					(int) arg0,
					(void *)(long) arg1,
					(size_t) arg2
				)
			);

		case IKC_FLOW_PORTAL_WRITE:
			return (
				__kportal_awrite(
					(int) arg0,
					(const void *)(long) arg1,
					(size_t) arg2
				)
			);

		default:
			kpanic("[kernel][ikc][task] Incorrect communication type.");
			return (-1);
	}
}

/**
 * @brief Configure the communication.
 *
 * @param arg0 communicator ID
 * @param arg1 buffer pointer
 * @param arg2 size
 *
 * @return Non-negative on success, negative on failure.
 *
 * @details Exit action based on return value:
 * CONTINUE : Continue the loop to waiting task (don't release semaphore).
 * AGAIN    : Retries to configure the communication.
 * ERROR    : Complete the task with error.
 */
PRIVATE int __ikc_flow_config(word_t arg0, word_t arg1, word_t arg2)
{
	int ret;

	/* Successful configure. */
	if ((ret = ___ikc_flow_config(arg0, arg1, arg2)) >= 0)
		ktask_exit1(KTASK_MANAGEMENT_SUCCESS, KTASK_MERGE_ARGS_FN_REPLACE, arg0);

	/* Must do again. */
	else if (IKC_FLOW_AGAIN_FLAGS(ret))
		ktask_exit0(KTASK_MANAGEMENT_AGAIN);

	/* Error. */
	else
		ktask_exit0(KTASK_MANAGEMENT_ERROR);

	return (ret);
}

/**
 * @brief Wait form the communication.
 *
 * @param arg0 communicator ID
 *
 * @return Non-negative on success, negative on failure.
 */
PRIVATE int ___ikc_flow_wait(word_t arg0)
{
	switch (ikc_flow_get_flow(false)->type)
	{
		case IKC_FLOW_MAILBOX_READ:
		case IKC_FLOW_MAILBOX_WRITE:
			return (__kmailbox_wait((int) arg0));

		case IKC_FLOW_PORTAL_READ:
		case IKC_FLOW_PORTAL_WRITE:
			return (__kportal_wait((int) arg0));

		default:
			kpanic("[kernel][ikc][task] Incorrect communication type.");
			return (-1);
	}
}

/**
 * @brief Wait the communication.
 *
 * @param arg0 communicator ID
 *
 * @return Non-negative on success, negative on failure.
 *
 * @details Exit action based on return value:
 * FINISH   : Complete the task and stop de loop.
 * CONTINUE : Continue the loop to reconfigure (don't release semaphore).
 * ERROR    : Complete the task with error.
 */
PRIVATE int __ikc_flow_wait(word_t arg0, word_t arg1, word_t arg2)
{
	int ret;
	int management;

	UNUSED(arg1);
	UNUSED(arg2);

	/* Successful waits for. */
	if (LIKELY((ret = ___ikc_flow_wait(arg0)) >= 0))
	{
		if (UNLIKELY(ret == ACTIVE_COMM_RECEIVED))
			kpanic("[kernel][ikc][task] Wait shouldn't return RECEIVED constant.");

		management = (ret == ACTIVE_COMM_SUCCESS) ?
			KTASK_MANAGEMENT_FINISH :
			KTASK_MANAGEMENT_CONTINUE;
	}

	/* Error. */
	else
		management = KTASK_MANAGEMENT_ERROR;

	/* Configure the exit action. */
	ktask_exit0(management);

	return (ret);
}

/**
 * @brief Allocate and dispatch a communication flow.
 *
 * @param type Communication type
 * @param cid  Communicator ID
 * @param buf  Buffer pointer
 * @param size Size
 *
 * @return Zero if successful configure the communication flow, non-zero
 * otherwise.
 */
PUBLIC int ikc_flow_config(int type, word_t cid, word_t buf, word_t size)
{
	int ret;
	bool is_user;
	struct ikc_flow * flow;

	/* Invalid type. */
	if (UNLIKELY(!WITHIN(type, IKC_FLOW_MAILBOX_READ, IKC_FLOW_INVALID)))
		return (-EINVAL);

	flow    = NULL;
	is_user = kthread_self() != KTHREAD_DISPATCHER_TID;

	spinlock_lock(&ikc_flow_lock);

		/* User thread. */
		if (is_user)
			flow = &ikc_flows.users[KERNEL_TID];

		/* Dispatcher. */
		else
		{
			for (int i = 0; i < IKC_FLOWS_MAX; ++i)
			{
				if (!resource_is_used(&ikc_flows.dispatchers[i].resource))
					flow = (flow == NULL) ? &ikc_flows.dispatchers[i] : flow;

				/* Only one task per communinator. */
				else if (ikc_flows.dispatchers[i].type == type && ikc_flows.dispatchers[i].cid == cid)
				{
					ret = (-EINVAL);
					goto error;
				}
			}
		}

		if (UNLIKELY(flow == NULL))
		{
			ret = (-EBUSY);
			goto error;
		}

		flow->cid  = cid;
		flow->type = type;
		resource_set_used(&flow->resource);

	spinlock_unlock(&ikc_flow_lock);

	if (UNLIKELY((ret = ktask_dispatch3(&flow->config, cid, buf, size)) < 0))
		return (ret);

	if (UNLIKELY(is_user && (flow->ret = ktask_wait(&flow->wait)) < 0))
	{
		if (ktask_get_return(&flow->config) < 0)
		{
			flow->cid  = IKC_FLOW_CID_INVALID;
			flow->type = IKC_FLOW_INVALID;
			resource_set_unused(&flow->resource);

			return (ktask_get_return(&flow->config));
		}
	}

	return (size);

error:
	spinlock_unlock(&ikc_flow_lock);

	return (ret);
}

/**
 * @brief Wait for the dispatched communication flow.
 *
 * @param type Communication type
 * @param cid  Communicator ID
 *
 * @return Zero if successful waits for the communication flow, non-zero
 * otherwise.
 */
PUBLIC int ikc_flow_wait(int type, word_t cid)
{
	int ret;
	bool is_user;
	struct ikc_flow * flow;

	/* Invalid type. */
	if (UNLIKELY(!WITHIN(type, IKC_FLOW_MAILBOX_READ, IKC_FLOW_INVALID)))
		return (-EINVAL);

	flow    = NULL;
	is_user = kthread_self() != KTHREAD_DISPATCHER_TID;

	spinlock_lock(&ikc_flow_lock);

		/* User thread. */
		if (is_user)
			flow = &ikc_flows.users[KERNEL_TID];

		/* Dispatcher. */
		else
		{
			for (int i = 0; i < IKC_FLOWS_MAX; ++i)
			{
				if (!resource_is_used(&ikc_flows.dispatchers[i].resource))
					continue;

				if (ikc_flows.dispatchers[i].type != type)
					continue;

				if (ikc_flows.dispatchers[i].cid != cid)
					continue;

				flow = &ikc_flows.dispatchers[i];
				break;
			}
		}

	spinlock_unlock(&ikc_flow_lock);

	if (UNLIKELY(flow == NULL))
		return (-EINVAL);

	/**
	 * [user] The waiting function is done on config because we must detect if
	 * the config task failed. As we only release the waiting semaphore, we must
	 * wait for it on config to get the error coming from config failure.
	 *
	 * [dispatcher] This function only will be executed when the waiting task
	 * completes (on-demand dependency between handler task and this task). So,
	 * we just decrement the waiting semaphore incremented before.
	 */
	ret = is_user ? flow->ret : ktask_wait(&flow->wait);

	spinlock_lock(&ikc_flow_lock);

		flow->cid  = IKC_FLOW_CID_INVALID;
		flow->type = IKC_FLOW_INVALID;
		resource_set_unused(&flow->resource);

	spinlock_unlock(&ikc_flow_lock);

	return (ret);
}

/**
 * @brief Configure the base flow connections.
 *
 * @param flow Communication flow
 */
PRIVATE void __ikc_flow_init(struct ikc_flow * flow)
{
	KASSERT(ktask_create(&flow->config, __ikc_flow_config, 0) == 0);
	KASSERT(ktask_create(&flow->wait, __ikc_flow_wait, 0) == 0);

	KASSERT(
		ktask_connect(
			&flow->config,
			&flow->wait,
			KTASK_DEPENDENCY_HARD,
			KTASK_TRIGGER_DEFAULT
		) == 0
	);
	KASSERT(
		ktask_connect(
			&flow->wait,
			&flow->config,
			KTASK_DEPENDENCY_HARD,
			KTASK_TRIGGER_CONTINUE
		) == 0
	);

	if (IKC_FLOW_FROM_TASK(&flow->config, true) != flow)
		kpanic("[kernel][ikc][task] FLOW_FROM_TASK Macro doesn't work.");

	if (IKC_FLOW_FROM_TASK(&flow->wait, false) != flow)
		kpanic("[kernel][ikc][task] FLOW_FROM_TASK Macro doesn't work.");
}

/**
 * @brief Initialization of communication flows using tasks.
 */
PUBLIC void ikc_flow_init(void)
{
	for (int i = 0; i < IKC_FLOWS_MAX; ++i)
	{
		/* Setup management variables. */
		ikc_flows.users[i].resource       = RESOURCE_INITIALIZER;
		ikc_flows.users[i].type           = IKC_FLOW_INVALID;
		ikc_flows.users[i].cid            = IKC_FLOW_CID_INVALID;
		ikc_flows.users[i].ret            = 0;
		ikc_flows.dispatchers[i].resource = RESOURCE_INITIALIZER;
		ikc_flows.dispatchers[i].type     = IKC_FLOW_INVALID;
		ikc_flows.dispatchers[i].cid      = IKC_FLOW_CID_INVALID;
		ikc_flows.dispatchers[i].ret      = 0;

		/* Configure base flow. */
		__ikc_flow_init(&ikc_flows.users[i]);
		__ikc_flow_init(&ikc_flows.dispatchers[i]);
	}

	spinlock_init(&ikc_flow_lock);
}

#endif /* __NANVIX_USE_COMM_WITH_TASKS */

