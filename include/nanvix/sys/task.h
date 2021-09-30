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

/**
 * @addtogroup nanvix Nanvix System
 */
/**@{*/

#ifndef NANVIX_SYS_TASK_H_
#define NANVIX_SYS_TASK_H_

	#include <nanvix/kernel/kernel.h>

	/**
	 * @brief Task Types.
	 */
	/**@{*/
	typedef struct task ktask_t;
	typedef int (*ktask_fn)(word_t arg0, word_t arg1, word_t arg2);
	/**@}*/

	/**
	 * @name Task Kernel Calls.
	 *
	 * @details Work in Progress.
	 */
	/**@{*/
	#define ktask_get_id(task)             task_get_id(task)
	#define ktask_get_return(task)         task_get_return(task)
	#define ktask_get_period(task)         task_get_return(task)
	#define ktask_set_period(task, period) task_set_period(task, period)

	extern int ktask_create(ktask_t *, ktask_fn, int);
	extern int ktask_unlink(ktask_t *);
	extern int ktask_connect(ktask_t *, ktask_t *);
	extern int ktask_disconnect(ktask_t *, ktask_t *);

	#define ktask_dispatch0(task)                   ktask_dispatch(task,    0,    0,    0)
	#define ktask_dispatch1(task, arg0)             ktask_dispatch(task, arg0,    0,    0)
	#define ktask_dispatch2(task, arg0, arg1)       ktask_dispatch(task, arg0, arg1,    0)
	#define ktask_dispatch3(task, arg0, arg1, arg2) ktask_dispatch(task, arg0, arg1, arg2)
	extern int ktask_dispatch(ktask_t *, word_t arg0, word_t arg1, word_t arg2);

	#define ktask_emit0(task, coreid)                   ktask_emit(task, coreid,    0,    0,    0)
	#define ktask_emit1(task, coreid, arg0)             ktask_emit(task, coreid, arg0,    0,    0)
	#define ktask_emit2(task, coreid, arg0, arg1)       ktask_emit(task, coreid, arg0, arg1,    0)
	#define ktask_emit3(task, coreid, arg0, arg1, arg2) ktask_emit(task, coreid, arg0, arg1, arg2)
	extern int ktask_emit(ktask_t *, int coreid, word_t arg0, word_t arg1, word_t arg2);

	extern int ktask_wait(ktask_t *);
	extern int ktask_trywait(ktask_t *);
	extern int ktask_continue(ktask_t *);
	extern int ktask_complete(ktask_t *);
	extern ktask_t * ktask_current(void);
	/**@}*/

#endif /* NANVIX_SYS_TASK_H_ */

/**@}*/
