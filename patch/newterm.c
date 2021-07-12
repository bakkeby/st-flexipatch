void
newterm(const Arg* a)
{
	int res;
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
			res = chdir(getcwd_by_pid(pid));
			execlp("st", "./st", NULL);
			break;
		default:
			exit(0);
		}
	default:
		wait(NULL);
	}
}

static char *getcwd_by_pid(pid_t pid) {
	char buf[32];
	snprintf(buf, sizeof buf, "/proc/%d/cwd", pid);
	return realpath(buf, NULL);
}