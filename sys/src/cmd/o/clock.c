/*
 * Clock for O/mero using Plan 9 as the PC host.
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <draw.h>
#include <b.h>

Panel* drw;
char cmds[512];

Point
circlept(Point c, int r, int degrees)
{
	double rad;
	rad = (double) degrees * PI/180.0;
	c.x += cos(rad)*r;
	c.y -= sin(rad)*r;
	return c;
}

static void
update(int ival)
{
	static char cmds[1024];
	static int oanghr, oangmin = 666;
	int anghr, angmin;
	int tm;
	Tm tms;
	char*	s;
	char*	e;
	Point	c;
	Point	cpt;
	int	rad;
	int	i, d;
	char	msg[80];

	if(access(drw->path, AEXIST) < 0)
		threadexitsall(nil);
	tm = time(0);
	tms = *localtime(tm);
	anghr = 90-(tms.hour*5 + tms.min/10)*6;
	angmin = 90-tms.min*6;
	if (oanghr != anghr || oangmin != angmin){
		oanghr = anghr;
		oangmin= angmin;
		c = Pt(40,40);
		rad= 35;
		e = cmds + sizeof(cmds);
		s = seprint(cmds, e, "fillellipse %d %d %d %d back\n", c.x, c.y, rad, rad);
		s = seprint(s, e, "ellipse %d %d %d %d\n", c.x, c.y, rad, rad);
		for(i=0; i<12; i++){
			cpt = circlept(c, rad - 8, i*(360/12));
			s = seprint(s, e, "fillellipse %d %d %d %d mback\n", cpt.x,  cpt.y, 2, 2);
		}
		cpt = circlept(c, (rad*3)/4, angmin);
		s = seprint(s, e, "line %d %d %d %d 0 2 1 bord\n", c.x, c.y, cpt.x, cpt.y);
		cpt = circlept(c, rad/2, anghr);
		s = seprint(s, e, "line %d %d %d %d 0 2 1 bord\n", c.x, c.y, cpt.x, cpt.y);
		s = seprint(s, e, "fillellipse %d %d 3 3 bord\n", c.x, c.y);
		pctl(drw, "hold\n");
		if(pdata(drw, cmds, s-cmds) < 0)
			sysfatal("ui gone: %r");
		pctl(drw, "release\n");
	}
	if(tms.min == 0 && tms.sec < 5){
		seprint(msg, msg+sizeof(msg),
			"It is %d hundred hours\n", tms.hour);
		writefstr("/mnt/voice/speak", msg);
	}
	if(ival){
		d = 60 - tms.sec;
		if(d < 1)
			d = 1;
		sleep(d*1000);
	}
}

static void
mkclock(void)
{
	char*	scrs[10];
	int	nscrs;
	char*	pdir;

	scrs[0] = userscreen();
	nscrs = 1;
	if(scrs[0] == nil){
		nscrs = screens(scrs, nelem(scrs));
		if(nscrs < 1)
			sysfatal("no screens");
	}
	update(0);
	if(pctl(drw, "tag\n") < 0)
		sysfatal("can't set tag in ui: %r");
	pdir = getenv("omerodir");
	if(pdir == nil)
		pdir = smprint("/%s/row:stats", scrs[0]);
	if(pctl(drw, "copyto %s\n", pdir) < 0)
		sysfatal("can't show ui at %s: %r", scrs[0]);
	free(pdir);
	while(nscrs > 0)
		free(scrs[--nscrs]);
	
}

static void
clock(void*)
{
	for(;;)
		update(1);
}

static void
evthread(void* a)
{
	Channel*c = a;
	Pev*	ev;

	for(;;){
		ev = recvp(c);
		if(ev == nil || (ev->ev == Eclose && ev->id == 0)){
			fprint(2, "%s: ui gone: exiting\n", argv0);
			threadexitsall(nil);
		}
		pevfree(ev);
	}
}


void
threadmain(int argc, char* argv[])
{
	Channel*	evc;
	ARGBEGIN{
	case 'd':
		paneldebug++;
		break;
	default:
		fprint(2, "usage: %s [-d]\n", argv0);
		sysfatal("usage");
	}ARGEND;
	if (argc > 0){
		fprint(2, "usage: %s [-d]\n", argv0);
		sysfatal("usage");
	}
	drw = pinit("draw", "clock");
	if(drw == nil)
		sysfatal("o/clock: no ui");
	evc = pevc(drw);
	proccreate(evthread, evc, 8*1024);
	mkclock();
	proccreate(clock, nil, 16*1024);
	threadexits(nil);
}
