/*
 * MIT License
 *
 * Copyright(c) 2011-2019 The Maintainers of Nanvix
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

#include <nanvix/sys/portal.h>

#if __TARGET_HAS_PORTAL

#include <nanvix/sys/thread.h>
#include <nanvix/sys/noc.h>
#include <nanvix/runtime/stdikc.h>

/**
 * @brief Kernel standard input portal.
 */
static int __stdinportal[THREAD_MAX] = {
	[0 ... (THREAD_MAX - 1)] = -1
};

/**
 * @todo TODO: provide a detailed description for this function.
 */
int __stdportal_setup(void)
{
	int tid;
	int local;

	tid = kthread_self();
	local = knode_get_num();

	return (((__stdinportal[tid] = kportal_create(local)) < 0) ? -1 : 0);
}

/**
 * @todo TODO: provide a detailed description for this function.
 */
int __stdportal_cleanup(void)
{
	int tid;

	tid = kthread_self();

	return (kportal_unlink(__stdinportal[tid]));
}

/**
 * @todo TODO: provide a detailed description for this function.
 */
int stdinportal_get(void)
{
	int tid;

	tid = kthread_self();

	return (__stdinportal[tid]);
}

#else
extern int make_iso_compilers_happy;
#endif /* __TARGET_HAS_PORTAL */