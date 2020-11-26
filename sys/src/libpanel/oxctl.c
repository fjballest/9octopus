#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>

static char* oxname;

static char*
findox(void)
{
	static char Oxcol[] = "col:ox.";
	char	mnt[80];
	Dir*	d;
	int	nd;
	int	i;
	int	l;
	char*	name;
	int	fd;

	seprint(mnt, mnt+sizeof(mnt), "%s/appl", omero);
	fd = open(mnt, OREAD);
	if(fd  < 0)
		return nil;
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd <= 0)
		return nil;
	l = strlen(Oxcol);
	name = nil;
	for(i = 0; i < nd; i++)
		if(strlen(d[i].name) > l && strncmp(d[i].name, Oxcol, l) == 0){
			name = smprint("%s/%s/ctl", mnt, d[i].name);
			break;
		}
	free(d);
	return name;
}

int
oxctl(char* c)
{
	int	fd;
	int	rc;

	if(oxname == nil)
		oxname = findox();
	if(oxname == nil)
		return -1;
	fd = open(oxname, OWRITE);
	if(fd < 0)
		return -1;
	rc = fprint(fd, "%s", c);
	close(fd);
	return rc;
}
