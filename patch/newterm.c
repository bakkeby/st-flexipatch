extern char* argv0;

static char*
getcwd_by_pid(pid_t pid) {
	static char cwd[32];
	snprintf(cwd, sizeof cwd, "/proc/%d/cwd", pid);
	return cwd;
}

void
newterm(const Arg* a)
{
	switch (fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		switch (fork()) {
		case -1:
			die("fork failed: %s\n", strerror(errno));
			break;
		case 0:
			#if OSC7_PATCH
			if (term.cwd) {
				if (chdir(term.cwd) == 0) {
					/* We need to put the working directory also in PWD, so that
					* the shell starts in the right directory if `cwd` is a
					* symlink. */
					setenv("PWD", term.cwd, 1);
				}
			} else {
				chdir(getcwd_by_pid(pid));
			}
			#else
			chdir(getcwd_by_pid(pid));
			#endif // OSC7_PATCH

			execl("/proc/self/exe", argv0, NULL);
			exit(1);
		default:
			exit(0);
		}
	default:
		wait(NULL);
	}
}
