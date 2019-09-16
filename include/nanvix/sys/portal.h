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

/**
 * @addtogroup nanvix Nanvix System
 */
/**@{*/

#ifndef NANVIX_SYS_PORTAL_H_
#define NANVIX_SYS_PORTAL_H_

	#include <nanvix/kernel/kernel.h>
	#include <sys/types.h>
	#include <stdbool.h>
	#include <stdint.h>

	/**
	 * @name Portal Kernel Calls
	 *
	 * @todo TODO: provide description for these functions.
	 */
	/**@{*/
	extern int kportal_create(int);
	extern int kportal_allow(int, int);
	extern int kportal_open(int, int);
	extern int kportal_unlink(int);
	extern int kportal_close(int);
	extern int kportal_awrite(int, const void *, size_t);
	extern int kportal_write(int, const void *, size_t);
	extern int kportal_aread(int, void *, size_t);
	extern int kportal_read(int, void *, size_t);
	extern int kportal_wait(int);
	/**@}*/

#endif /* NANVIX_SYS_PORTAL_H_ */

/**@}*/
