#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <plumb.h>
#include <error.h>
#include <b.h>

typedef struct Player	Player;

enum {
	KF=	0xF000,	/* Rune: beginning of private Unicode space */
	Spec=	0xF800,
	Kup=	KF|0x0E,
	Kdown=	Spec|0x00,

	Ptag = 1,
	Pcontrols,
	Psongs,
	Pplay,
	Pstop,
	Pdone,
	Ppause,
	Pnext,
	Pvolume
};

struct Player{
	char*	dir;
	char**	songs;
	int	nsongs;
	int	active;
	int	pause;
	int	pid;	// of player process

	Panel*	gtag;
	Panel*	gsongcol;
	Panel*	gsongs;
	Panel*	gpause;
	Panel*	gvolume;
};

Player p;
Panel* ui;
static char 		playbuf[4*1024];
static Channel*	plumbc;

void update(Player*);

static void
playproc(void *a)
{
	int	fd, afd;
	int	n, wn;
	int	playing;
	Player*	p = a;

	threadsetname("playproc");
again:
	if (p->active < 0 || p->active >= p->nsongs)
		threadexits("bug");
	fd = open(p->songs[p->active], OREAD);
	afd= open("/devs/audio/audio", OWRITE);
	if (fd < 0 || afd < 0){
		close(fd);
		close(afd);
		threadexits("open");
	}
	playing = p->active;
	do{
		if (p->pause){
			while(p->pause)
				sleep(100);
			if (playing != p->active){
				wn = -1;
				n = 0;
				break;
			}
		}
		wn = n = read(fd, playbuf, sizeof(playbuf));
		if (n > 0)
			wn = write(afd, playbuf, n);
	} while(n  > 0 && wn == n && playing == p->active);
	close(fd);
	close(afd);
	if (n < 0 || n != wn)
		threadexits("io");
	if (playing == p->active){
		p->active++;
		if (p->active == p->nsongs)
			p->active = 0;
		update(p);
		goto again;
	}
	threadexits(nil);
}

void
play(Player* p)
{
	proccreate(playproc, p, 16*1024);
}

void
stop(Player* p)
{
	p->active = -1;
}

static void
plumbimg(char* dir, char* arg)
{
	int fd;

	fd = open("/mnt/ports/post", OWRITE|OCEXEC);
	if (fd < 0)
		return;
	fprint(fd, "/poster: %s/%s\n", dir, arg);
	close(fd);
}

static int
mayplumbimg(char* dir, char* f)
{
	int	n;

	n = strlen(f);
	if (n < 5)
		return 0;
	n -= 4;
	if (!cistrcmp(f+n, ".jpg") || !strcmp(f+n, ".gif") || !strcmp(f+n, ".img") || !strcmp(f+n, ".png")){
		plumbimg(dir, f);
		return 1;
	}
	return 0;
}

int
readsongs(Player* p, char* dir)
{
	Dir*	ents;
	int	fd;
	int	i, n;
	char	buf[512];
	int	hasfiles;

	if (chdir(dir) < 0)
		return -1;
	getwd(buf, sizeof(buf));
	fd = open(".", OREAD);
	if (fd < 0)
		return -1;
	free(p->dir);
	p->dir = estrdup(buf);
	for (i = 0; i < p->nsongs; i++)
		free(p->songs[i]);
	free(p->songs);
	p->songs = 0;
	p->nsongs = dirreadall(fd, &ents);
	close(fd);
	if (p->nsongs < 0){
		return -1;
	}
	p->songs = emalloc(p->nsongs * sizeof(char*));
	hasfiles = 0;
	for (i = n = 0; i < p->nsongs; i++)
		if (ents[i].qid.type&QTDIR)
			p->songs[n++] = smprint("%s/", ents[i].name);
		else {
			if (!mayplumbimg(p->dir, ents[i].name)){
				hasfiles++;
				p->songs[n++] = estrdup(ents[i].name);
			};
		}
	p->nsongs = n;
	free(ents);
	p->active = -1;
	p->pause = 0;
	return hasfiles;
}

void
showlist(Player* p)
{
	int	i;
	char*	s;
	char*	txt;
	char*	mrk;
	char*	ap;

	txt = emalloc(32*1024);
	s = ap = txt;
	*txt = 0;
	for (i = 0; i < p->nsongs; i++){
		if (i != p->active)
			mrk = "";
		else {
			ap = s;
			mrk = "->";
		}
		s = seprint(s, txt+(32*1024), "%s\t%s\n", mrk, p->songs[i]);
	}
	if (!txt[0])
		strcpy(txt, "no songs.");
	pdata(p->gsongs, txt, strlen(txt));
	pctl(p->gsongs, "sel %d %d\n", ap - txt, ap - txt);
	free(txt);
}

void
controls(Panel* w)
{
	pnewnamed(w, "button:Play", Pplay);
	pnewnamed(w, "button:Stop", Pstop);
	pnewnamed(w, "button:Next", Pnext);
	pnewnamed(w, "button:End", Pdone);
	p.gpause = pnewnamed(w, "button:Pause", Ppause);
	p.gvolume = pnew(w, "slider:volume", Pvolume);
	pdata(p.gvolume, "75", 2);
}

Panel*
mkui(void)
{
	Panel*	c;
	Panel*	ui;
	char*	scrs[10];
	int	nscrs;
	char*	cs[10];
	int	ncs;
	char*	pdir;

	ui = pinit("col", "player");
	if (ui == nil)
		sysfatal("no ui: %r\n");
	p.gtag = pnew(ui, "tag:player", Ptag);
	c = pnew(ui, "row:controls", Pcontrols);
	controls(c);
	p.gsongs = pnew(ui, "text:songs", Psongs);
	scrs[0] = userscreen();
	nscrs = 1;
	if(scrs[0] == nil){
		nscrs = screens(scrs, nelem(scrs));
		if(nscrs < 1)
			sysfatal("no screens");
	}
	ncs = cols(scrs[0], cs, nelem(cs));
	if(ncs < 1)
		sysfatal("no columns at %s", scrs[0]);
	pdir = getenv("omerodir");
	if(pdir == nil)
		pdir = strdup(cs[0]);
	if(pctl(ui, "copyto %s\n", pdir) < 0)
		sysfatal("can't show ui at %s: %r", cs[0]);
	free(pdir);
	while(nscrs > 0)
		free(scrs[--nscrs]);
	while(ncs > 0)
		free(cs[--ncs]);
	return ui;
}

void
update(Player* p)
{
	char*	tag;

	pctl(ui, "hold\n");
	tag = smprint("player — %s ..", p->dir ? p->dir : "no dir");
	pdata(p->gtag, tag, strlen(tag));
	free(tag);
	showlist(p);
	pctl(ui, "release\n");
}

int
isdir(char* p)
{
	Dir*	d;
	int	r;

	d = dirstat(p);
	if (d == nil)
		return 0;
	r = (d->qid.type&QTDIR);
	free(d);
	return r;
}

static int
findsong(Player* p, char* s)
{
	int	i;

	for (i = 0; i < p->nsongs; i++)
		if (!strcmp(p->songs[i], s))
			return i;
	// no exact match, try substring
	for (i = 0; i < p->nsongs; i++)
		if (strstr(p->songs[i], s))
			return i;

	return 0;
}

void
playfile(Player* p, char* file)
{
	char*	d;

	if (isdir(file)){
		if (readsongs(p, file) > 0){
			p->active = p->pause = 0;
			play(p);
		}
		update(p);
	} else if (access(file, AREAD) != -1){
		d = utfrrune(file, '/');
		if (d)
			*d++ = 0;
		readsongs(p, d ? file : ".");
		if (d){
			p->active = findsong(p, d);
			p->pause = 0;
			play(p);
		}
		update(p);
	}
}

void
handle(Player* p, Pev* e)
{
	char	str[40];
	int	vol;
	char	fname[80];
	char*	vs;
	char*	arg;
	switch(e->ev){
	case Eclose:
		fprint(2, "%s: ui gone: exiting\n", argv0);
		threadexitsall(nil);
	case Elook:
	case Eexec:
		// processed below
		break;
	case Edirty:
		if(e->id != Pvolume)
			return;
		// processed below
		break;
	default:
		return;
	}
	if(arg = e->arg){
		while(*arg == ' ' || *arg == '\t' || *arg == '\n')
			arg++;
		for(vs = arg; *vs != 0 && *vs != ' ' && *vs != '\t' && *vs != '/' && *vs != '\n'; vs++)
			;
		*vs = 0;
	}			
	switch(e->id){
	case Ptag:
		if (readsongs(p, arg) != -1)
			update(p);
		break;
	case Psongs:
		if (isdir(e->arg))
			readsongs(p, arg);
		else {
			p->active = findsong(p, arg);
			play(p);
		}
		update(p);
		break;
	case Pplay:
		p->active = 0;
		play(p);
		update(p);
		break;
	case Pstop:
		stop(p);
		update(p);
		break;
	case Pdone:
		stop(p);
		threadexitsall(nil);
	case Ppause:
		if (p->pause){
			pdata(p->gpause,"Pause", 5);
			p->pause = 0;
		} else {
			pdata(p->gpause,"Resume", 6);
			p->pause++;
		}
		break;
	case Pnext:
		p->active++;
		if (p->active == p->nsongs)
			p->active = 0;
		update(p);
		play(p);
		break;
	case Pvolume:
		seprint(fname, fname+sizeof(fname), "%s/data", p->gvolume->path);
		vs = readfstr(fname);
		vol = atoi(vs);
		free(vs);
		seprint(str, str+sizeof(str), "audio out %d", vol);
		writefstr("/devs/audio/volume", str);
		break;
	}
}

void
threadmain(int argc, char* argv[])
{
	Channel*	ec;
	Pev*	e;

	ARGBEGIN{
	case 'd':
		paneldebug++;
		break;
	default:
		fprint(2, "usage: %s [-d] [dir]\n", argv0);
		sysfatal("usage");
	}ARGEND;
	if (argc > 1){
		fprint(2, "usage: %s [-d] [dir]\n", argv0);
		sysfatal("usage");
	}
	readsongs(&p, argc ? argv[0] : "/n/music");
	ui = mkui();
	ec = pevc(ui);
	update(&p);
	while(e = recvp(ec)){
		handle(&p, e);
		pevfree(e);
	}
	threadexitsall(nil);
}
