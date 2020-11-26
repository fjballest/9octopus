#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>
int	
cols(char* scr, char** v, int nv)
{
	char*	path;
	Dir*	d;
	int	nd;
	int	fd;
	int	i, j;

	path = smprint("%s/%s/row:wins", omero, scr);
	if(path == nil)
		return -1;
	fd = open(path, OREAD);
	if(fd  < 0){
		free(path);
		return -1;
	}
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd <= 0){
		free(path);
		return -1;
	}
	for(i = j = 0; i < nd  && j < nv; i++)
		if(d[i].qid.type&QTDIR)
			v[j++] = smprint("/%s/row:wins/%s", scr, d[i].name);
	free(d);
	free(path);
	return j;
}
