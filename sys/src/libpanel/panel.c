/*
 * Convenience library for using Octopus
 * o/mero from Plan 9.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>

char* omero;
int paneldebug;
static int apid;

static void
init(void)
{
	omero = getenv("omero");
	if(omero == nil)
		if(access("/mnt/ui/appl", AEXIST) >= 0)
			omero = strdup("/mnt/ui");
	if(omero == nil)
		sysfatal("panel: $omero not set");
	apid = getpid();
}

static Panel*
mkpanel(char* path, char* name, int mkit)
{
	Panel*	p;
	int	fd;

	p = mallocz(sizeof(Panel), 1);
	if(p == nil)
		return nil;
	p->rpid = p->gcfd = -1;
	p->name = strdup(name);
	p->path = strdup(path);
	if(p->name == nil || p->path == nil)
		goto fail;
	if(mkit)
		fd = create(path, OREAD, DMDIR|0775);
	else
		fd = open(path, OREAD); /* be sure it's there */
	if(fd < 0)
		goto fail;
	close(fd);
	if(paneldebug)
		fprint(2, "panel: new %s\n", p->path);
	return p;
fail:
	if(mkit)
		remove(path);
	if(p != nil){
		free(p->name);
		free(p->path);
		free(p);
	}
	return nil;
}

Panel*	
pinit(char* type, char* name)
{
	Panel*	p;
	char*	nm;
	char*	unm;
	char	nbuf[50];
	char	path[80];

	if(omero == nil)
		init();
	unm = nm = strrchr(name, '/');
	if(unm != nil){
		nm++;
		strecpy(path, path+sizeof(path), name);
	} else {
		seprint(nbuf, nbuf+sizeof(nbuf), "%s:%s.%d", type, name, apid);
		nm = nbuf;
		seprint(path, path+sizeof(path), "%s/appl/%s", omero, nm);
	}
	p = mkpanel(path, nm, unm == nil);
	if(p == nil)
		return nil;
	strecpy(path + strlen(path), path+sizeof(path), "/ctl");
	p->gcfd = open(path, ORDWR|ORCLOSE);
	if(p->gcfd < 0){
		pclose(p);
		return nil;
	}
	pctl(p, "appl 0 %d\n", apid);
	return p;
}

Panel*	
pnew(Panel* pp, char* name, int id)
{
	char*	path;
	char*	nm;
	char*	unm;
	char	nbuf[40];
	Panel*	p;
	int	rnd;

	unm = nm = strrchr(name, '/');
	if(unm != nil){
		nm++;
		path = strdup(name);
	} else {
		rnd = ntruerand(0xFFFF);
		seprint(nbuf, nbuf+sizeof(nbuf), "%s.%4.4ux", name, rnd);
		nm = nbuf;
		path = smprint("%s/%s", pp->path, nm);
	}
	if(path == nil)
		return nil;
	p = mkpanel(path, nm, unm == nil);
	free(path);
	path = nil;
	if(p == nil)
		return nil;
	p->id = id;
	if(id != 0){
		path = smprint("%s/ctl", p->path);
		if(path == nil)
			goto fail;
		pctl(p, "appl %d %d\n", id, apid);
	}
	free(path);
	return p;
fail:
	free(path);
	pclose(p);
	return nil;
}

void	
pclose(Panel* p)
{
	int	fd;
	char	path[40];

	if(p->rpid >= 0){
		seprint(path, path+sizeof(path), "/proc/%d/ctl", p->rpid);
		fd = open(path, OWRITE);
		if(fd  >= 0){
			fprint(fd, "kill");
			close(fd);
		}
		p->rpid = -1;
	}
	/* removing a panel causes a close event to be sent.
	 * the event handler must free the event channel and exit
	 * if the panel is gone.
	 */
	if(paneldebug)
		fprint(2, "panel: close %s\n", p->path);
	remove(p->path);
	if(p->evpath != nil)
		remove(p->evpath);
	if(p->gcfd  >= 0)
		close(p->gcfd);
	free(p->path);
	free(p->name);
	free(p->evpath);
	free(p);
}

