#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <b.h>

enum {
	Bufsz = 4096
};

typedef struct Rdarg Rdarg;

struct Rdarg {
	int	fd;
	char*	name;
	Channel*	ec;
	Channel*	pc;
};

static char* evname[Nevs] = {
	[Elook]	"look",
	[Eexec]	"exec",
	[Eclose]	"close",
	[Eclean]	"clean",
	[Edirty]	"dirty",
	[Eintr]	"interrupt",
	[Eclick]	"click",
	[Ekeys]	"keys",
	[Efocus]	"focus",
};

char*
pevname(int ev)
{
	if(ev  < 0 || ev >= nelem(evname))
		return "unknown";
	return evname[ev];
}

static int
evnb(char* ev)
{
	int	i;

	if(ev != nil)
		for(i = 0; i < nelem(evname); i++)
			if(!strcmp(ev, evname[i]))
				return i;
	return -1;
}


void
pevfree(Pev* ev)
{
	if(ev  != nil){
		free(ev->path);
		free(ev->arg);
		free(ev);
	}
}

static char*
eparsearg(char** bp)
{
	char*	buf;
	char*	s;

	buf = *bp;
	if(buf == nil || *buf == 0)
		return nil;
	s = strchr(buf, ' ');
	if(s == nil)
		s = buf + strlen(buf);
	else
		*s++ = 0;
	*bp = s;
	return strdup(buf);
}
static Pev*
eparse(char* buf)
{
	static char  Pref[] = "o/mero: ";
	Pev*	ev;
	int	l;
	int	pl;
	char*	id;
	char*	evs;

	ev = mallocz(sizeof(Pev), 1);
	if(ev == nil)
		return nil;
	l = strlen(buf);
	if(l > 0 && buf[l-1] == '\n'){
		buf[l-1] = 0;
		l--;
	}
	pl = strlen(Pref);
	if(l <= pl){
		fprint(2,  "panel: short event\n");
		return nil;
	}
	buf += pl;
	ev->path = eparsearg(&buf);
	if((id = eparsearg(&buf)) != nil)
		ev->id = atoi(id);
	evs = eparsearg(&buf);
	ev->ev = evnb(evs);
	if(*buf != 0)
		ev->arg = strdup(buf);
	if(ev->path == nil || ev->ev == -1){
		fprint(2, "panel: nil path %s or bad event %s\n", ev->path, evs);
		goto fail;
	}
	switch(ev->ev){
	case Elook:
	case Eexec:
	case Eclick:
	case Ekeys:
		if(ev->arg == nil){
			fprint(2,   "panel: missing event arg\n");
			goto fail;
		}
		break;
	case Eclose:
	case Eintr:
	case Eclean:
	case Edirty:
	case Efocus:
		if(ev->arg != nil){
			fprint(2, "panel: extra event arg %s\n", ev->arg);
			goto fail;
		}
		break;
	default:
		fprint(2, "panel: unknown event type %d\n", ev->ev);
		goto fail;
	}
	return ev;
fail:
	pevfree(ev);
	return nil;
}

static void
readerproc(void* a)
{
	Rdarg*	arg;
	int	fd;
	Channel*	pc;
	Channel*	ec;
	char	name[40];
	char*	buf;
	int	nr;
	Pev*	ev;

	arg = a;
	seprint(name, name+sizeof(name), "readerproc:%s", arg->name);
	threadsetname(name);
	fd = arg->fd;
	pc = arg->pc;
	ec = arg->ec;
	sendul(pc, getpid());
	buf = malloc(Bufsz);
	if(buf == nil)
		sysfatal("readerproc: not enough memory");
	for(;;){
		nr = read(fd, buf, Bufsz-1);
		if(nr  <= 0){
			sendp(ec, nil);
			break;
		}
		buf[nr] = 0;
		if(paneldebug > 1)
			fprint(2, "panel: event: %s\n", buf);
		ev = eparse(buf);
		if(ev == nil)
			fprint(2, "panel: bad event %s\n", buf);
		else
			sendp(ec, ev);
	}
	free(buf);
	threadexits(nil);
}

/*
 * The process receiving from the channel is responsible for
 * calling chanfree for it once a null event has been received.
 */
Channel*	
pevc(Panel* p)
{
	Rdarg	arg;

	if(p->evpath != nil){
		werrstr("already has an event port");
		return nil;
	}
	p->evpath = smprint("/mnt/ports/%s", p->name);
	arg.fd = create(p->evpath, ORDWR|ORCLOSE, 0664);
	if(arg.fd  < 0)
		return nil;
	if(fprint(arg.fd, "o/mero: %s.*", p->path + strlen(omero)) < 0){
		remove(p->evpath);
		return nil;
	}
	arg.pc = chancreate(sizeof(ulong), 0);
	arg.ec = chancreate(sizeof(Pev*), 0);
	arg.name = p->name;
	proccreate(readerproc, &arg, 24*1024);
	p->rpid = recvul(arg.pc);
	return arg.ec;
}
