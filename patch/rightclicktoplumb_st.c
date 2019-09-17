#if defined(__OpenBSD__)
 #include <sys/sysctl.h>
#endif

int
subprocwd(char *path)
{
#if   defined(__linux)
	if (snprintf(path, PATH_MAX, "/proc/%d/cwd", pid) < 0)
		return -1;
	return 0;
#elif defined(__OpenBSD__)
	size_t sz = PATH_MAX;
	int name[3] = {CTL_KERN, KERN_PROC_CWD, pid};
	if (sysctl(name, 3, path, &sz, 0, 0) == -1)
		return -1;
	return 0;
#endif
}