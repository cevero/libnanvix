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

#include <nanvix/kernel/kernel.h>

#if __TARGET_HAS_PORTAL && !__NANVIX_IKC_USES_ONLY_MAILBOX

#include <nanvix/sys/noc.h>
#include <nanvix/sys/task.h>
#include <posix/errno.h>

/**
 * @brief Protection for allows variable.
 */
PRIVATE spinlock_t kportal_lock;

/**
 * @brief Store allows information.
 */
PRIVATE struct
{
	int remote; /**< Remote ID.      */
	int port;   /**< Remote port ID. */
} kportal_allows[KPORTAL_MAX];

/**
 * @brief Maximum number of tasks.
 */
#define KPORTAL_USER_TASK_MAX 32

/**
 * @brief Tasks per operate.
 */
PRIVATE struct {
	int portalid;
	ktask_t operate;
	ktask_t wait;
} kportal_tasks[KPORTAL_USER_TASK_MAX];

/*============================================================================*
 * kportal_create()                                                           *
 *============================================================================*/

/**
 * @details The kportal_create() function creates an input portal and
 * attaches it to the local port @p local_port in the local NoC node @p
 * local.
 */
PUBLIC int kportal_create(int local, int local_port)
{
	int ret;

	/* Invalid local number for the requesting core ID. */
	if (local != knode_get_num())
		return (-EINVAL);

	ret = kcall2(
		NR_portal_create,
		(word_t) local,
		(word_t) local_port
	);

	if (ret >= 0)
	{
		spinlock_lock(&kportal_lock);
			kportal_allows[ret].remote = -1;
			kportal_allows[ret].port   = -1;
		spinlock_unlock(&kportal_lock);
	}

	return (ret);
}

/*============================================================================*
 * kportal_allow()                                                            *
 *============================================================================*/

/**
 * @details The kportal_allow() function allow read data from a input portal
 * associated with the NoC node @p remote.
 */
PUBLIC int kportal_allow(int portalid, int remote, int remote_port)
{
	int ret;

	ret = kcall3(
		NR_portal_allow,
		(word_t) portalid,
		(word_t) remote,
		(word_t) remote_port
	);

	if (ret == 0)
	{
		spinlock_lock(&kportal_lock);
			kportal_allows[portalid].remote = remote;
			kportal_allows[portalid].port   = remote_port;
		spinlock_unlock(&kportal_lock);
	}

	return (ret);
}

/*============================================================================*
 * kportal_open()                                                             *
 *============================================================================*/

/**
 * @details The kportal_open() function opens an output portal to the remote
 * NoC node @p remote and attaches it to the local NoC node @p local.
 */
PUBLIC int kportal_open(int local, int remote, int remote_port)
{
	int ret;

	/* Invalid local number for the requesting core ID. */
	if (local != knode_get_num())
		return (-EINVAL);

	ret = kcall3(
		NR_portal_open,
		(word_t) local,
		(word_t) remote,
		(word_t) remote_port
	);

	return (ret);
}

/*============================================================================*
 * kportal_unlink()                                                           *
 *============================================================================*/

/**
 * @details The kportal_unlink() function removes and releases the underlying
 * resources associated to the input portal @p portalid.
 */
PUBLIC int kportal_unlink(int portalid)
{
	int ret;

	ret = kcall1(
		NR_portal_unlink,
		(word_t) portalid
	);

	return (ret);
}

/*============================================================================*
 * kportal_close()                                                            *
 *============================================================================*/

/**
 * @details The kportal_close() function closes and releases the
 * underlying resources associated to the output portal @p portalid.
 */
PUBLIC int kportal_close(int portalid)
{
	int ret;

	ret = kcall1(
		NR_portal_close,
		(word_t) portalid
	);

	return (ret);
}

/*============================================================================*
 * kportal_task_alloc()                                                       *
 *============================================================================*/

/**
 * @brief Allocates a user task.
 */
PRIVATE int kportal_task_alloc(int portalid)
{
	int id = (-EINVAL);

	if (UNLIKELY(portalid < 0))
		return (-EINVAL);

	spinlock_lock(&kportal_lock);

		for (int i = 0; i < KPORTAL_USER_TASK_MAX; ++i)
		{
			/* Each portalid only can use one task. */
			if (UNLIKELY(kportal_tasks[i].portalid == portalid))
			{
				id = (-EINVAL);
				break;
			}

			/* Looks for free tasks. */
			if (UNLIKELY(id < 0 && kportal_tasks[i].portalid < 0))
			{
				id = i;
				kportal_tasks[i].portalid = portalid;
			}
		}

	spinlock_unlock(&kportal_lock);

	return (id);
}

/*============================================================================*
 * kportal_task_free()                                                        *
 *============================================================================*/

/**
 * @brief Frees a user task.
 */
PRIVATE int kportal_task_free(int id)
{
	if (UNLIKELY(!WITHIN(id, 0, KPORTAL_USER_TASK_MAX)))
		return (-EINVAL);

	spinlock_lock(&kportal_lock);
		kportal_tasks[id].portalid      = -1;
		kportal_tasks[id].operate.state = -1;
		kportal_tasks[id].wait.state    = -1;
	spinlock_unlock(&kportal_lock);

	return (0);
}

/*============================================================================*
 * kportal_task_search()                                                      *
 *============================================================================*/

/**
 * @brief Search for a user task.
 */
PRIVATE int kportal_task_search(int portalid)
{
	int ret = (-EINVAL);

	if (UNLIKELY(portalid < 0))
		return (-EINVAL);

	spinlock_lock(&kportal_lock);

		for (int i = 0; i < KPORTAL_USER_TASK_MAX; ++i)
		{
			if (kportal_tasks[i].portalid != portalid)
				continue;

			ret = i;
			break;
		}

	spinlock_unlock(&kportal_lock);

	return (ret);
}

/*============================================================================*
 * __kportal_aread()                                                          *
 *============================================================================*/

/**
 * @brief Generic task operate.
 */
PRIVATE int __kportal_operate(ktask_args_t * args)
{
	int ret;

	ret = kcall3(
		args->arg0,
		args->arg1,
		args->arg2,
		args->arg3
	);

	if ((args->arg0 == NR_portal_awrite) && (ret == -EACCES || ret == -EBUSY))
		return (TASK_RET_AGAIN);
	else if ((args->arg0 == NR_portal_aread) && (ret == -EBUSY || ret == -ENOMSG))
		return (TASK_RET_AGAIN);

	args->ret = (ret);

	if (ret < 0)
		return (TASK_RET_ERROR);

	return (TASK_RET_SUCCESS);
}

/*============================================================================*
 * __kportal_wait()                                                          *
 *============================================================================*/

/**
 * @brief Generic task waiting.
 */
PRIVATE int __kportal_wait(ktask_args_t * args)
{
	if ((args->ret = kcall1(args->arg0, args->arg1)) < 0)
		return (TASK_RET_ERROR);

	return (TASK_RET_SUCCESS);
}

/*============================================================================*
 * kportal_operate()                                                          *
 *============================================================================*/

/**
 * @details Generic user configuration.
 */
PRIVATE ssize_t kportal_operate(
	int portalid,
	const void * buffer,
	size_t size,
	word_t NR_operate
)
{
	int tid;
	struct task * operate;
	struct task * wait;

	/* Invalid buffer. */
	if (!WITHIN(portalid, 0, KPORTAL_MAX))
		return (-EINVAL);

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid size. */
	if (size == 0 || size > KPORTAL_MESSAGE_DATA_SIZE)
		return (-EINVAL);

	if ((tid = kportal_task_alloc(portalid)) < 0)
		goto error0;

	/* Gets tasks. */
	operate = &kportal_tasks[tid].operate;
	wait    = &kportal_tasks[tid].wait;

	/* Build tasks arguments. */
	operate->args.arg0 = NR_operate;
	operate->args.arg1 = (word_t) portalid;
	operate->args.arg2 = (word_t) buffer;
	operate->args.arg3 = (word_t) size;
	wait->args.arg0    = NR_portal_wait;
	wait->args.arg1    = (word_t) portalid;

	if (ktask_create(operate, __kportal_operate, NULL, 0) != 0)
		goto error1;

	if (ktask_create(wait, __kportal_wait, NULL, 0) != 0)
		goto error1;

	if (ktask_connect(operate, wait) != 0)
		goto error1;

	if (ktask_dispatch(operate) != 0)
		goto error1;

	return (size);

error1:
	KASSERT(kportal_task_free(tid) == 0);

error0:
	return (-EINVAL);
}

/*============================================================================*
 * kportal_awrite()                                                          *
 *============================================================================*/

/**
 * @brief The kportal_awrite() asynchronously write @p size bytes
 * of data pointed to by @p buffer to the output portal @p portalid.
 */
PUBLIC ssize_t kportal_awrite(int portalid, const void * buffer, size_t size)
{
	return (
		kportal_operate(
			portalid,
			buffer,
			size,
			NR_portal_awrite
		)
	);
}

/*============================================================================*
 * kportal_aread()                                                           *
 *============================================================================*/

/**
 * @brief The kportal_aread() asynchronously read @p size bytes
 * of data pointed to by @p buffer to the input portal @p portalid.
 */
PUBLIC ssize_t kportal_aread(int portalid, const void * buffer, size_t size)
{
	return (
		kportal_operate(
			portalid,
			buffer,
			size,
			NR_portal_aread
		)
	);
}

/*============================================================================*
 * kportal_wait()                                                            *
 *============================================================================*/

/**
 * @details The kportal_wait() waits for asyncrhonous operates in
 * the input/output portal @p portalid to complete.
 */
PUBLIC int kportal_wait(int portalid)
{
	int ret;
	int tid;

	/* Invalid buffer. */
	if (!WITHIN(portalid, 0, KPORTAL_MAX))
		return (-EINVAL);

	tid = kportal_task_search(portalid);

	if ((ret = ktask_wait(&kportal_tasks[tid].operate)) < 0)
		goto error;

	ret = ktask_wait(&kportal_tasks[tid].wait);

error:
	KASSERT(kportal_task_free(tid) == 0);

	return (ret > 0) ? (-EAGAIN) : (ret);
}

/*============================================================================*
 * kportal_write()                                                            *
 *============================================================================*/

/**
 * @details The kportal_write() synchronously write @p size bytes of
 * data pointed to by @p buffer to the output portal @p portalid.
 */
PUBLIC ssize_t kportal_write(int portalid, const void * buffer, size_t size)
{
	ssize_t ret;      /* Return value.               */
	size_t n;         /* Size of current data piece. */
	size_t remainder; /* Remainder of total data.    */
	size_t times;     /* Number of pieces.           */

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid size. */
	if (size == 0 || size > KPORTAL_MAX_SIZE)
		return (-EINVAL);

	times     = size / KPORTAL_MESSAGE_DATA_SIZE;
	remainder = size - (times * KPORTAL_MESSAGE_DATA_SIZE);

	for (size_t t = 0; t < times + (remainder != 0); ++t)
	{
		n = (t != times) ? KPORTAL_MESSAGE_DATA_SIZE : remainder;

		/* Sends a piece of the message. */
		if ((ret = kportal_awrite(portalid, buffer, n)) < 0)
			return (ret);

		/* Waits for the asynchronous operate to complete. */
		if ((ret = kportal_wait(portalid)) != 0)
			return (ret);

		/* Next pieces. */
		buffer += n;
	}

	return (size);
}

/*============================================================================*
 * kportal_read()                                                             *
 *============================================================================*/

/**
 * @details The kportal_read() synchronously read @p size bytes of
 * data pointed to by @p buffer from the input portal @p portalid.
 */
PUBLIC ssize_t kportal_read(int portalid, void * buffer, size_t size)
{
	ssize_t ret;      /* Return value.               */
	size_t n;         /* Size of current data piece. */
	size_t remainder; /* Remainder of total data.    */
	size_t times;     /* Number of pieces.           */
	int remote;       /* Number of target remote.    */
	int port;         /* Number of target port.      */

	/* Invalid buffer. */
	if (buffer == NULL)
		return (-EINVAL);

	/* Invalid size. */
	if (size == 0 || size > KPORTAL_MAX_SIZE)
		return (-EINVAL);

	times     = size / KPORTAL_MESSAGE_DATA_SIZE;
	remainder = size - (times * KPORTAL_MESSAGE_DATA_SIZE);
	ret       = (-EINVAL);
	spinlock_lock(&kportal_lock);
		remote = kportal_allows[portalid].remote;
		port   = kportal_allows[portalid].port;
	spinlock_unlock(&kportal_lock);

	for (size_t t = 0; t < times + (remainder != 0); ++t)
	{
		n = (t != times) ? KPORTAL_MESSAGE_DATA_SIZE : remainder;

		/* Repeat while reading valid messages for another ports. */
		do
		{
			/* Consecutive reads must be allowed. */
			if (t != 0 && ret >= 0)
				kportal_allow(portalid, remote, port);

			/* Reads a piece of the message. */
			if ((ret = kportal_aread(portalid, buffer, n)) < 0)
				return (ret);

		/* Waits for the asynchronous operate to complete. */
		} while ((ret = kportal_wait(portalid)) == (-EAGAIN));

		/* Wait failed. */
		if (ret < 0)
			return (ret);

		/* Next pieces. */
		buffer += n;
	}

	/* Complete a allowed read. */
	spinlock_lock(&kportal_lock);
		kportal_allows[portalid].remote = -1;
		kportal_allows[portalid].port   = -1;
	spinlock_unlock(&kportal_lock);

	return (size);
}

/*============================================================================*
 * kportal_ioctl()                                                            *
 *============================================================================*/

/**
 * @details The kportal_ioctl() reads the measurement parameter associated
 * with the request id @p request of the portal @p portalid.
 */
PUBLIC int kportal_ioctl(int portalid, unsigned request, ...)
{
	int ret;
	va_list args;

	va_start(args, request);

		dcache_invalidate();

		ret = kcall3(
			NR_portal_ioctl,
			(word_t) portalid,
			(word_t) request,
			(word_t) &args
		);

		dcache_invalidate();

	va_end(args);

	return (ret);
}

/*============================================================================*
 * kportal_init()                                                             *
 *============================================================================*/

/**
 * @details The kportal_init() Initializes portal system.
 */
PUBLIC void kportal_init(void)
{
	kprintf("[user][portal] Initializes portal module");

	for (int i = 0; i < KPORTAL_USER_TASK_MAX; ++i)
		kportal_tasks[i].portalid = -1;

	spinlock_init(&kportal_lock);
}

#else
extern int make_iso_compilers_happy;
#endif /* __TARGET_HAS_PORTAL && !__NANVIX_IKC_USES_ONLY_MAILBOX */
