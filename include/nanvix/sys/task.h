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
	 * @brief Number of arguments in the task function.
	 */
	#define KTASK_ARGS_NUM TASK_ARGS_NUM

	/**
	 * @brief Maximum number of children.
	 */
	#define KTASK_CHILDREN_MAX TASK_CHILDREN_MAX

	/**
	 * @name Dependency's type.
	 *
	 * @details Types' description:
	 * - Hard dependencies are lifetime dependencies and must be explicitly
	 *   disconnected.
	 * - Soft dependencies are (on-demand) temporary dependencies that cease
	 *   to exist when the parent completes.
	 * - Invalid dependency is used internally to specify an invalid node.
	 */
	/**@{*/
	#define KTASK_DEPENDENCY_HARD    TASK_DEPENDENCY_HARD    /**< Lifetime dependency.  */
	#define KTASK_DEPENDENCY_SOFT    TASK_DEPENDENCY_SOFT    /**< Temporary dependency. */
	#define KTASK_DEPENDENCY_INVALID TASK_DEPENDENCY_INVALID /**< Invalid dependency.   */
	/**@}*/

	/**
	 * @name Management behaviors on a task.
	 *
	 * @details The management value indicates which action the Dispatcher must
	 * perform over the current task.
	 */
	/**@{*/
	#define KTASK_MANAGEMENT_SUCCESS  TASK_MANAGEMENT_SUCCESS  /**< Release the task with success.  */
	#define KTASK_MANAGEMENT_AGAIN    TASK_MANAGEMENT_AGAIN    /**< Reschedule the task.            */
	#define KTASK_MANAGEMENT_STOP     TASK_MANAGEMENT_STOP     /**< Move the task to stopped state. */
	#define KTASK_MANAGEMENT_PERIODIC TASK_MANAGEMENT_PERIODIC /**< Periodic reschedule the task.   */
	#define KTASK_MANAGEMENT_ABORT    TASK_MANAGEMENT_ABORT    /**< Abort the task.                 */
	#define KTASK_MANAGEMENT_ERROR    TASK_MANAGEMENT_ERROR    /**< Release the task with error.    */
	/**@}*/

	/**
	 * @name Default values used on task_exit.
	 */
	/**@{*/
	#define KTASK_MANAGEMENT_DEFAULT    TASK_MANAGEMENT_DEFAULT    /**< Release the task with success.  */
	#define KTASK_MERGE_ARGS_FN_DEFAULT TASK_MERGE_ARGS_FN_DEFAULT /**< Overwrite arguments.            */
	/**@}*/

	/**
	 * @name Task Types.
	 */
	/**@{*/
	typedef struct task ktask_t;                                     /**< Task structure.                     */
	typedef int (* ktask_fn)(word_t arg0, word_t arg1, word_t arg2); /**< Task function.                      */
	typedef void (* ktask_merge_args_fn)(                            /**< How pass arguments to a child task. */
		const word_t exit_args[KTASK_ARGS_NUM],
		word_t child_args[KTASK_ARGS_NUM]
	);
	/**@}*/

	/**
	 * @name Task Kernel Calls.
	 *
	 * @details Work in Progress.
	 */
	/**@{*/
	#define ktask_get_id(task)                          task_get_id(task)
	#define ktask_get_return(task)                      task_get_return(task)
	#define ktask_get_number_parents(task)              task_get_number_parents(task)
	#define ktask_get_number_children(task)             task_get_number_children(task)
	#define ktask_get_children(task)                    task_get_children(task)
	#define ktask_get_period(task)                      task_get_return(task)
	#define ktask_set_period(task, period)              task_set_period(task, period)
	#define ktask_set_arguments(task, arg0, arg1, arg2) task_set_arguments(task, arg0, arg1, arg2)

	extern ktask_t * ktask_current(void);

	extern int ktask_create(ktask_t *, ktask_fn, int);
	extern int ktask_unlink(ktask_t *);
	extern int ktask_connect(ktask_t *, ktask_t *, int);
	extern int ktask_disconnect(ktask_t *, ktask_t *);

	#define ktask_dispatch0(task)                   ktask_dispatch(task,    0,    0,    0)
	#define ktask_dispatch1(task, arg0)             ktask_dispatch(task, arg0,    0,    0)
	#define ktask_dispatch2(task, arg0, arg1)       ktask_dispatch(task, arg0, arg1,    0)
	#define ktask_dispatch3(task, arg0, arg1, arg2) ktask_dispatch(task, arg0, arg1, arg2)
	extern int ktask_dispatch(ktask_t *, word_t, word_t, word_t);

	#define ktask_emit0(task, coreid)                   ktask_emit(task, coreid,    0,    0,    0)
	#define ktask_emit1(task, coreid, arg0)             ktask_emit(task, coreid, arg0,    0,    0)
	#define ktask_emit2(task, coreid, arg0, arg1)       ktask_emit(task, coreid, arg0, arg1,    0)
	#define ktask_emit3(task, coreid, arg0, arg1, arg2) ktask_emit(task, coreid, arg0, arg1, arg2)
	extern int ktask_emit(ktask_t *, int, word_t, word_t, word_t);

	#define ktask_exit0(mgt)                       ktask_exit(mgt, NULL,    0,    0,    0)
	#define ktask_exit1(mgt, fn, arg0)             ktask_exit(mgt,   fn, arg0,    0,    0)
	#define ktask_exit2(mgt, fn, arg0, arg1)       ktask_exit(mgt,   fn, arg0, arg1,    0)
	#define ktask_exit3(mgt, fn, arg0, arg1, arg2) ktask_exit(mgt,   fn, arg0, arg1, arg2)
	extern int ktask_exit(int, ktask_merge_args_fn, word_t, word_t, word_t);

	extern int ktask_wait(ktask_t *);
	extern int ktask_trywait(ktask_t *);
	extern int ktask_stop(ktask_t *);
	extern int ktask_continue(ktask_t *);
	extern int ktask_complete(ktask_t *);
	/**@}*/

#endif /* NANVIX_SYS_TASK_H_ */

/**@}*/
