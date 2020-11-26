#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>

int	
screens(char**v, int nv)
{
	int	fd;
	Dir*	d;
	int	nd;
	int	i, j;

	fd = open(omero, OREAD);
	if(fd  < 0)
		return -1;
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd <= 0)
		return -1;
	for(i = j = 0; i < nd && j < nv; i++)
		if(strcmp(d[i].name, "appl") && d[i].qid.type&QTDIR)
			v[j++] = strdup(d[i].name);
	free(d);
	return j;
}

char*
userscreen(void)
{
	char*	sel;
	char*	p;

	sel = readfstr("/dev/sel");
	if(sel != nil && *sel != 0){
		p = strchr(sel+1, '/');
		if(p != nil)
			*p = 0;
		p = strdup(sel+1);
		free(sel);
		if(strcmp(p, "mnt") == 0){
			fprint(2, "panel: /dev/sel: bad panel path: (/mnt/ui/...)\n");
			free(p);
			p = strdup("main");
		}
		return p;
	}
	return nil;
}

