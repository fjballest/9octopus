#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>

Panel*	
pnewnamed(Panel* pp, char* name, int id)
{
	char*	path;
	Panel*	p;
	int	fd;

	path = smprint("%s/%s", pp->path, name);
	if(path == nil)
		return nil;
	fd = create(path, OREAD, DMDIR|0775);
	close(fd);
	p = pnew(pp, path, id);
	free(path);
	return p;
}
