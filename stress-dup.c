/*
 * Copyright (C) 2013-2020 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#include "stress-ng.h"

static const stress_help_t help[] = {
	{ NULL,	"dup N",	"start N workers exercising dup/close" },
	{ NULL,	"dup-ops N",	"stop after N dup/close bogo operations" },
	{ NULL,	NULL,		NULL }
};

/*
 *  stress_dup()
 *	stress system by rapid dup/close calls
 */
static int stress_dup(const stress_args_t *args)
{
	static int fds[STRESS_FD_MAX];
	size_t max_fd = stress_get_file_limit();
	size_t i;
#if defined(HAVE_DUP3)
	bool do_dup3 = true;
#endif
	const int bad_fd = stress_get_bad_fd();

	if (max_fd > SIZEOF_ARRAY(fds))
		max_fd = SIZEOF_ARRAY(fds);

	fds[0] = open("/dev/zero", O_RDONLY);
	if (fds[0] < 0) {
		pr_dbg("%s: open failed on /dev/zero, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}

	do {
		size_t n;

		for (n = 1; n < max_fd; n++) {
			int tmp;

			fds[n] = dup(fds[0]);
			if (fds[n] < 0)
				break;

			/* do an invalid dup on an invalid fd */
			tmp = dup(bad_fd);
			if (tmp >= 0)
				(void)close(tmp);

#if defined(HAVE_DUP3)
			/* do an invalid dup3 on an invalid fd */
			tmp = shim_dup3(fds[0], bad_fd, O_CLOEXEC);
			if (tmp >= 0)
				(void)close(tmp);

			if (do_dup3 && stress_mwc1()) {
				int fd;

				fd = shim_dup3(fds[0], fds[n], O_CLOEXEC);
				/* No dup3 support? then fallback to dup2 */
				if ((fd < 0) && (errno == ENOSYS)) {
					fd = dup2(fds[0], fds[n]);
					do_dup3 = false;
				}
				fds[n] = fd;
			} else {
				fds[n] = dup2(fds[0], fds[n]);
			}
#else
			fds[n] = dup2(fds[0], fds[n]);
#endif
			if (fds[n] < 0)
				break;

			/* dup2 on the same fd should be a no-op */
			tmp = dup2(fds[n], fds[n]);
			if (tmp != fds[n]) {
				pr_fail("%s: dup2 failed with same fds, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				break;
			}
			/* do an invalid dup2 on an invalid fd */
			tmp = dup2(fds[0], bad_fd);
			if (tmp >= 0)
				(void)close(tmp);

#if defined(F_DUPFD)
			/* POSIX.1-2001 fcntl() */

			(void)close(fds[n]);
			fds[n] = fcntl(fds[0], F_DUPFD, fds[0]);
			if (fds[n] < 0)
				break;
#endif

			if (!keep_stressing())
				break;
			inc_counter(args);
		}
		for (i = 1; i < n; i++) {
			if (fds[i] < 0)
				break;
			if (!keep_stressing_flag())
				break;
			(void)close(fds[i]);
		}
	} while (keep_stressing());
	(void)close(fds[0]);

	return EXIT_SUCCESS;
}

stressor_info_t stress_dup_info = {
	.stressor = stress_dup,
	.class = CLASS_FILESYSTEM | CLASS_OS,
	.help = help
};
