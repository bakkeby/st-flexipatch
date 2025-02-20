void
selopen(const Arg *dummy)
{
	pid_t chpid;

	if ((chpid = fork()) == 0) {
		if (fork() == 0)
			execlp("xdg-open", "xdg-open", getsel(), NULL);
		exit(1);
	}
	if (chpid > 0)
		waitpid(chpid, NULL, 0);
}
