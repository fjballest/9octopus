#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>
int	
pctl(Panel* p, char* fmt, ...)
{
	va_list	arg;
	int	ec;
	int	fd;
	char*	fname;
	char*	ctl;

	fname = smprint("%s/ctl", p->path);
	if(fname == nil)
		return -1;
	fd = open(fname,  ORDWR);
	free(fname);
	if(fd < 0)
		return -1;
	va_start(arg, fmt);
	ctl = vsmprint(fmt, arg);
	va_end(arg);
	if(ctl == nil)
		ec = -1;
	else
		ec = write(fd, ctl, strlen(ctl));
	if(paneldebug && ec > 0)
		fprint(2, "panel: ctl %s [%s]\n", p->path, ctl);
	close(fd);
	free(ctl);
	return ec;
}

long
pdata(Panel* p, void* data, long cnt)
{
	int	nw;
	char*	fname;

	fname = smprint("%s/data", p->path);
	if(fname == nil)
		return -1;
	nw = writef(fname, data, cnt);
	free(fname);
	return nw;
}

