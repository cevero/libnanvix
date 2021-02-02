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

#define __NANVIX_MICROKERNEL

#include <nanvix/kernel/kernel.h>

#if __TARGET_HAS_MAILBOX

#include <nanvix/sys/noc.h>
#include <nanvix/sys/task.h>
#include <posix/errno.h>

#if __NANVIX_USE_TASKS

	/**
	 * @brief Maximum of tasks.
	 */
	#define KMAILBOX_USER_TASK_MAX 32

	/**
	 * @brief Tasks per operate.
	 */
	PRIVATE struct {
		int mbxid;
		ktask_t operate;
		ktask_t wait;
	} kmailbox_tasks[KMAILBOX_USER_TASK_MAX];

#endif

/**
 * @brief Protections.
 */
PRIVATE spinlock_t kmailbox_lock;

#if __NANVIX_IKC_USES_ONLY_MAILBOX

/**
 * @brief Counter control.
 */
PRIVATE bool user_mailboxes[KMAILBOX_MAX] = {
	[0 ... (KMAILBOX_MAX - 1)] = false
};

/*============================================================================*
 * Counters structure.                                                        *
 *============================================================================*/

/**
 * @brief Communicator counters.
 */
PRIVATE struct
{
	uint64_t ncreates; /**< Number of creates. */
	uint64_t nunlinks; /**< Number of unlinks. */
	uint64_t nopens;   /**< Number of opens.   */
	uint64_t ncloses;  /**< Number of closes.  */
	uint64_t nreads;   /**< Number of reads.   */
	uint64_t nwrites;  /**< Number of writes.  */
} mailbox_counters;

#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

/*============================================================================*
 * kmailbox_create()                                                          *
 *============================================================================*/

/**
 * @details The kmailbox_create() function creates an input mailbox
 * and attaches it to the local NoC node @p local in the port @p port.
 */
PUBLIC int kmailbox_create(int local, int port)
{
	int ret;

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (!WITHIN(port, 0, KMAILBOX_PORT_NR))
		return (-EINVAL);
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	ret = kcall2(
		NR_mailbox_create,
		(word_t) local,
		(word_t) port
	);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (ret >= 0)
	{
		spinlock_lock(&kmailbox_lock);
			mailbox_counters.ncreates++;
		spinlock_unlock(&kmailbox_lock);

		user_mailboxes[ret] = true;
	}
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (ret);
}

/*============================================================================*
 * kmailbox_open()                                                            *
 *============================================================================*/

/**
 * @details The kmailbox_open() function opens an output mailbox to
 * the remote NoC node @p remote in the port @p port.
 */
PUBLIC int kmailbox_open(int remote, int remote_port)
{
	int ret;

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (!WITHIN(remote_port, 0, KMAILBOX_PORT_NR))
		return (-EINVAL);
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	ret = kcall2(
		NR_mailbox_open,
		(word_t) remote,
		(word_t) remote_port
	);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (ret >= 0)
	{
		spinlock_lock(&kmailbox_lock);
			mailbox_counters.nopens++;
		spinlock_unlock(&kmailbox_lock);

		user_mailboxes[ret] = true;
	}
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (ret);
}

/*============================================================================*
 * kmailbox_unlink()                                                          *
 *============================================================================*/

/**
 * @details The kmailbox_unlink() function removes and releases the underlying
 * resources associated to the input mailbox @p mbxid.
 */
PUBLIC int kmailbox_unlink(int mbxid)
{
	int ret;

	ret = kcall1(
		NR_mailbox_unlink,
		(word_t) mbxid
	);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (ret >= 0)
	{
		spinlock_lock(&kmailbox_lock);
			mailbox_counters.nunlinks++;
		spinlock_unlock(&kmailbox_lock);

		user_mailboxes[mbxid] = false;
	}
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (ret);
}

/*============================================================================*
 * kmailbox_close()                                                           *
 *============================================================================*/

/**
 * @details The kmailbox_close() function closes and releases the
 * underlying resources associated to the output mailbox @p mbxid.
 */
PUBLIC int kmailbox_close(int mbxid)
{
	int ret;

	ret = kcall1(
		NR_mailbox_close,
		(word_t) mbxid
	);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	if (ret >= 0)
	{
		spinlock_lock(&kmailbox_lock);
			mailbox_counters.ncloses++;
		spinlock_unlock(&kmailbox_lock);

		user_mailboxes[mbxid] = false;
	}
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (ret);
}

#if __NANVIX_USE_TASKS

/*============================================================================*
 * kmailbox_task_alloc()                                                      *
 *============================================================================*/

/**
 * @brief Allocates a user task.
 */
PRIVATE int kmailbox_task_alloc(int mbxid)
{
	int id = (-EINVAL);

	if (UNLIKELY(mbxid < 0))
		return (-EINVAL);

	spinlock_lock(&kmailbox_lock);

		for (int i = 0; i < KMAILBOX_USER_TASK_MAX; ++i)
		{
			/* Each mbxid only can use one task. */
			if (UNLIKELY(kmailbox_tasks[i].mbxid == mbxid))
			{
				id = (-EINVAL);
				break;
			}

			/* Looks for free tasks. */
			if (UNLIKELY(id < 0 && kmailbox_tasks[i].mbxid < 0))
			{
				id = i;
				kmailbox_tasks[i].mbxid = mbxid;
			}
		}

	spinlock_unlock(&kmailbox_lock);

	return (id);
}

/*============================================================================*
 * kmailbox_task_free()                                                       *
 *============================================================================*/

/**
 * @brief Frees a user task.
 */
PRIVATE int kmailbox_task_free(int id)
{
	if (UNLIKELY(!WITHIN(id, 0, KMAILBOX_USER_TASK_MAX)))
		return (-EINVAL);

	spinlock_lock(&kmailbox_lock);
		kmailbox_tasks[id].mbxid         = -1;
		kmailbox_tasks[id].operate.state = -1;
		kmailbox_tasks[id].wait.state    = -1;
	spinlock_unlock(&kmailbox_lock);

	return (0);
}

/*============================================================================*
 * kmailbox_task_search()                                                     *
 *============================================================================*/

/**
 * @brief Search for a user task.
 */
PRIVATE int kmailbox_task_search(int mbxid)
{
	int ret = (-EINVAL);

	if (UNLIKELY(mbxid < 0))
		return (-EINVAL);

	spinlock_lock(&kmailbox_lock);

		for (int i = 0; i < KMAILBOX_USER_TASK_MAX; ++i)
		{
			if (kmailbox_tasks[i].mbxid != mbxid)
				continue;

			ret = i;
			break;
		}

	spinlock_unlock(&kmailbox_lock);

	return (ret);
}

/*============================================================================*
 * __kmailbox_aread()                                                         *
 *============================================================================*/

/**
 * @brief Generic task operate.
 */
PRIVATE int __kmailbox_operate(ktask_args_t * args)
{
	int ret;

	ret = kcall3(
		args->arg0,
		args->arg1,
		args->arg2,
		args->arg3
	);

	if ((ret == -EBUSY) || (ret == -EAGAIN) || (ret == -ENOMSG) || (ret == -ETIMEDOUT))
		return (TASK_RET_AGAIN);

	args->ret = (ret);

	if (ret < 0)
		return (TASK_RET_ERROR);

	return (TASK_RET_SUCCESS);
}

/*============================================================================*
 * __kmailbox_wait()                                                          *
 *============================================================================*/

/**
 * @brief Generic task waiting.
 */
PRIVATE int __kmailbox_wait(ktask_args_t * args)
{
	if ((args->ret = kcall1(args->arg0, args->arg1)) < 0)
		return (TASK_RET_ERROR);

	return (TASK_RET_SUCCESS);
}

#endif /* __NANVIX_USE_TASKS */

/*============================================================================*
 * kmailbox_operate()                                                          *
 *============================================================================*/

/**
 * @details Generic user configuration.
 */
PRIVATE ssize_t kmailbox_operate(
	int mbxid,
	const void * buffer,
	size_t size,
	word_t NR_operate
)
{
#if __NANVIX_USE_TASKS

	int tid;
	struct task * operate;
	struct task * wait;

	/* Invalid buffer. */
	if (!WITHIN(mbxid, 0, KMAILBOX_MAX))
		return (-EINVAL);

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid buffer size. */
	if ((size == 0) || (size > KMAILBOX_MESSAGE_SIZE))
		goto error0;

	if ((tid = kmailbox_task_alloc(mbxid)) < 0)
		goto error0;

	/* Gets tasks. */
	operate = &kmailbox_tasks[tid].operate;
	wait    = &kmailbox_tasks[tid].wait;

	/* Build tasks arguments. */
	operate->args.arg0 = NR_operate;
	operate->args.arg1 = (word_t) mbxid;
	operate->args.arg2 = (word_t) buffer;
	operate->args.arg3 = (word_t) size;
	wait->args.arg0    = NR_mailbox_wait;
	wait->args.arg1    = (word_t) mbxid;

	if (ktask_create(operate, __kmailbox_operate, NULL, 0) != 0)
		goto error1;

	if (ktask_create(wait, __kmailbox_wait, NULL, 0) != 0)
		goto error1;

	if (ktask_connect(operate, wait) != 0)
		goto error1;

	if (ktask_dispatch(operate) != 0)
		goto error1;

	return (size);

error1:
	KASSERT(kmailbox_task_free(tid) == 0);

error0:
	return (-EINVAL);

#else

	int ret;

	/* Invalid buffer size. */
	if ((size == 0) || (size > KMAILBOX_MESSAGE_SIZE))
		return (-EINVAL);

	do
	{
		ret = kcall3(
			NR_operate,
			(word_t) mbxid,
			(word_t) buffer,
			(word_t) size
		);
	} while ((ret == -EBUSY) || (ret == -EAGAIN) || (ret == -ENOMSG) || (ret == -ETIMEDOUT));

	return (ret);

#endif /* __NANVIX_USE_TASKS */
}


/*============================================================================*
 * kmailbox_awrite()                                                          *
 *============================================================================*/

/**
 * @brief The kmailbox_awrite() asynchronously write @p size bytes
 * of data pointed to by @p buffer to the output mailbox @p mbxid.
 */
PUBLIC ssize_t kmailbox_awrite(int mbxid, const void * buffer, size_t size)
{
	return (
		kmailbox_operate(
			mbxid,
			buffer,
			size,
			NR_mailbox_awrite
		)
	);
}

/*============================================================================*
 * kmailbox_aread()                                                           *
 *============================================================================*/

/**
 * @brief The kmailbox_aread() asynchronously read @p size bytes
 * of data pointed to by @p buffer to the input mailbox @p mbxid.
 */
PUBLIC ssize_t kmailbox_aread(int mbxid, const void * buffer, size_t size)
{
	return (
		kmailbox_operate(
			mbxid,
			buffer,
			size,
			NR_mailbox_aread
		)
	);
}

/*============================================================================*
 * kmailbox_wait()                                                            *
 *============================================================================*/

/**
 * @details The kmailbox_wait() waits for asyncrhonous operates in
 * the input/output mailbox @p mbxid to complete.
 */
PUBLIC int kmailbox_wait(int mbxid)
{
	int ret;

#if __NANVIX_USE_TASKS

	int tid;

	/* Invalid buffer. */
	if (!WITHIN(mbxid, 0, KMAILBOX_MAX))
		return (-EINVAL);

	tid = kmailbox_task_search(mbxid);

	if ((ret = ktask_wait(&kmailbox_tasks[tid].operate)) < 0)
		goto error;

	ret = ktask_wait(&kmailbox_tasks[tid].wait);

error:
	KASSERT(kmailbox_task_free(tid) == 0);

#else

	ret = kcall1(
		NR_mailbox_wait,
		(word_t) mbxid
	);

#endif /* __NANVIX_USE_TASKS */

	return (ret > 0) ? (-EAGAIN) : (ret);
}

/*============================================================================*
 * kmailbox_write()                                                           *
 *============================================================================*/

/**
 * @details The kmailbox_write() synchronously write @p size bytes of
 * data pointed to by @p buffer to the output mailbox @p mbxid.
 *
 * @todo Uncomment kmailbox_wait() call when microkernel properly supports it.
 */
PUBLIC ssize_t kmailbox_write(int mbxid, const void * buffer, size_t size)
{
	int ret;

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid buffer size. */
	if ((size == 0) || (size > KMAILBOX_MESSAGE_SIZE))
		return (-EINVAL);

	if ((ret = kmailbox_awrite(mbxid, buffer, size)) < 1)
		return (ret);

	if ((ret = kmailbox_wait(mbxid)) < 0)
		return (ret);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	spinlock_lock(&kmailbox_lock);
		if (user_mailboxes[mbxid])
			mailbox_counters.nwrites++;
	spinlock_unlock(&kmailbox_lock);
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (size);
}

/*============================================================================*
 * kmailbox_read()                                                            *
 *============================================================================*/

/**
 * @details The kmailbox_read() synchronously read @p size bytes of
 * data pointed to by @p buffer from the input mailbox @p mbxid.
 */
PUBLIC ssize_t kmailbox_read(int mbxid, void * buffer, size_t size)
{
	int ret;

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid buffer size. */
	if ((size == 0) || (size > KMAILBOX_MESSAGE_SIZE))
		return (-EINVAL);

	/* Repeat while reading valid messages for another ports. */
	do
	{
		if ((ret = kmailbox_aread(mbxid, buffer, size)) < 0)
			return (ret);
	} while ((ret = kmailbox_wait(mbxid)) == (-EAGAIN));

	/* Wait failed. */
	if (ret < 0)
		return (ret);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	spinlock_lock(&kmailbox_lock);
		if (user_mailboxes[mbxid])
			mailbox_counters.nreads++;
	spinlock_unlock(&kmailbox_lock);
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	return (size);
}

/*============================================================================*
 * kmailbox_ioctl()                                                           *
 *============================================================================*/

PRIVATE int kmailbox_ioctl_valid(void * ptr, size_t size)
{
	return ((ptr != NULL) && mm_check_area(VADDR(ptr), size, UMEM_AREA));
}

/**
 * @details The kmailbox_ioctl() reads the measurement parameter associated
 * with the request id @p request of the mailbox @p mbxid.
 */
PUBLIC int kmailbox_ioctl(int mbxid, unsigned request, ...)
{
	int ret;
	va_list args;

	va_start(args, request);

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	spinlock_lock(&kmailbox_lock);

		/* Parse request. */
		switch (request)
		{
			/* Get the amount of data transferred so far. */
			case KMAILBOX_IOCTL_GET_VOLUME:
			case KMAILBOX_IOCTL_GET_LATENCY:
			{
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

				dcache_invalidate();

				ret = kcall3(
					NR_mailbox_ioctl,
					(word_t) mbxid,
					(word_t) request,
					(word_t) &args
				);

				dcache_invalidate();

#if __NANVIX_IKC_USES_ONLY_MAILBOX
			} break;

			case KMAILBOX_IOCTL_GET_NCREATES:
			case KMAILBOX_IOCTL_GET_NUNLINKS:
			case KMAILBOX_IOCTL_GET_NOPENS:
			case KMAILBOX_IOCTL_GET_NCLOSES:
			case KMAILBOX_IOCTL_GET_NREADS:
			case KMAILBOX_IOCTL_GET_NWRITES:
			{
				ret = (-EINVAL);

				/* Bad mailbox. */
				if ((ret = kcomm_get_port(mbxid, COMM_TYPE_MAILBOX)) < 0)
					goto error;

				uint64_t * var = va_arg(args, uint64_t *);

				ret = (-EFAULT);

				/* Bad buffer. */
				if (!kmailbox_ioctl_valid(var, sizeof(uint64_t)))
					goto error;

				ret = 0;

				switch(request)
				{
					case KMAILBOX_IOCTL_GET_NCREATES:
						*var = mailbox_counters.ncreates;
						break;

					case KMAILBOX_IOCTL_GET_NUNLINKS:
						*var = mailbox_counters.nunlinks;
						break;

					case KMAILBOX_IOCTL_GET_NOPENS:
						*var = mailbox_counters.nopens;
						break;

					case KMAILBOX_IOCTL_GET_NCLOSES:
						*var = mailbox_counters.ncloses;
						break;

					case KMAILBOX_IOCTL_GET_NREADS:
						*var = mailbox_counters.nreads;
						break;

					case KMAILBOX_IOCTL_GET_NWRITES:
						*var = mailbox_counters.nwrites;
						break;

					/* operate not supported. */
					default:
						ret = (-ENOTSUP);
						break;
				}

			} break;

			/* operate not supported. */
			default:
				ret = (-ENOTSUP);
				break;
		}

error:
	spinlock_unlock(&kmailbox_lock);
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

	va_end(args);

	return (ret);
}

/*============================================================================*
 * kmailbox_set_remote()                                                      *
 *============================================================================*/

/**
 * @todo Provide a detailed description about this function.
 */
int kmailbox_set_remote(int mbxid, int remote, int remote_port)
{
	int ret;

	/* Invalid nodenum. */
	if (!WITHIN(remote, 0, (MAILBOX_ANY_SOURCE + 1)))
		return (-EINVAL);

	/* Invalid port number. */
	if (!WITHIN(remote_port, 0, (MAILBOX_ANY_PORT + 1)))
		return (-EINVAL);

	ret = kmailbox_ioctl(mbxid, KMAILBOX_IOCTL_SET_REMOTE, remote, remote_port);

	return (ret);
}

/*============================================================================*
 * kmailbox_init()                                                            *
 *============================================================================*/

/**
 * @details The kmailbox_init() Initializes mailbox system.
 */
PUBLIC void kmailbox_init(void)
{
	kprintf("[user][mailbox] Initializes mailbox module");

#if __NANVIX_IKC_USES_ONLY_MAILBOX
	mailbox_counters.ncreates = 0ULL;
	mailbox_counters.nunlinks = 0ULL;
	mailbox_counters.nopens   = 0ULL;
	mailbox_counters.ncloses  = 0ULL;
	mailbox_counters.nreads   = 0ULL;
	mailbox_counters.nwrites  = 0ULL;
#endif /* __NANVIX_IKC_USES_ONLY_MAILBOX */

#if __NANVIX_USE_TASKS

	for (int i = 0; i < KMAILBOX_USER_TASK_MAX; ++i)
		kmailbox_tasks[i].mbxid = -1;

#endif

	spinlock_init(&kmailbox_lock);
}

#else
extern int make_iso_compilers_happy;
#endif /* __TARGET_HAS_MAILBOX */
