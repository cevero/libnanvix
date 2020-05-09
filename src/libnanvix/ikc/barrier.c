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

#include <nanvix/sys/sync.h>

#if __TARGET_HAS_SYNC

#include <nanvix/sys/thread.h>
#include <nanvix/runtime/stdikc.h>
#include <nanvix/runtime/barrier.h>

/**
 * The barrier_create() function creates a barrier between the nodes
 * listed in the array pointed to by @p nodes.
 */
barrier_t barrier_create(const int *nodes, int nnodes)
{
	barrier_t barrier;

	/* Invalid list of nodes. */
	if (nodes == NULL)
		return (BARRIER_NULL);

	/* Invalid number of nodes. */
	if (nnodes < 2)
		return (BARRIER_NULL);

	/* Leader. */
	if (knode_get_num() == nodes[0])
	{
		barrier.syncs[0] = ksync_create(
			nodes,
			nnodes,
			SYNC_ALL_TO_ONE
		);
		barrier.syncs[1] = ksync_open(
			nodes,
			nnodes,
			SYNC_ONE_TO_ALL
		);
	}

	/* Follower. */
	else
	{
		barrier.syncs[0] = ksync_open(
			nodes,
			nnodes,
			SYNC_ALL_TO_ONE
		);
		barrier.syncs[1] = ksync_create(
			nodes,
			nnodes,
			SYNC_ONE_TO_ALL
		);
	}

	barrier.leader = nodes[0];

	return (barrier);
}

/**
 * The barrier_destroy() function closes underlying resources of the
 * barrier @p barrier.
 */
int barrier_destroy(barrier_t barrier)
{
	int ret;
	int err;

	/* Invalid barrier. */
	if (!BARRIER_IS_VALID(barrier))
		return (-EINVAL);

	ret = 0;

	/* Leader. */
	if (knode_get_num() == barrier.leader)
	{
		err = ksync_unlink(barrier.syncs[0]);
		ret = (err < 0) ? err : ret;
		err = ksync_close(barrier.syncs[1]);
		ret = (err < 0) ? err : ret;
	}

	/* Follower. */
	else
	{
		err = ksync_close(barrier.syncs[0]);
		ret = (err < 0) ? err : ret;
		err = ksync_unlink(barrier.syncs[1]);
		ret = (err < 0) ? err : ret;
	}

	return (ret);
}

/**
 * The barrier_wait() function causes the calling peer to block
 * until all other participants of the barrier @p barrier have reached
 * it.
 */
int barrier_wait(barrier_t barrier)
{
	int ret;
	int err;

	/* Invalid barrier. */
	if (!BARRIER_IS_VALID(barrier))
		return (-EINVAL);

	ret = 0;

	/* Leader */
	if (knode_get_num() == barrier.leader)
	{
		err = ksync_wait(barrier.syncs[0]);
		ret = (err < 0) ? err : ret;
		err = ksync_signal(barrier.syncs[1]);
		ret = (err < 0) ? err : ret;
	}

	/* Follower. */
	else
	{
		err = ksync_signal(barrier.syncs[0]);
		ret = (err < 0) ? err : ret;
		err = ksync_wait(barrier.syncs[1]);
		ret = (err < 0) ? err : ret;
	}

	return (ret);
}

#else
extern int make_iso_compilers_happy;
#endif /* __TARGET_HAS_MAILBOX */
