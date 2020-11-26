#include <u.h>
#include <libc.h>
#include <thread.h>
#include <panel.h>
#include <error.h>
#include <plumb.h>
#include <regexp.h>
#include <ctype.h>
#include <b.h>

typedef struct Face Face;

struct Face {
	char*	key;
	char*	addr;
	char*	file;
	char*	label;
	Panel*	col;
	Panel*	tag;
	Panel*	img;
	int	id;
	int	nb;
};

#define dprint if(debug)fprint

int	debug;
Channel*plumbc;
Panel*	grow;
Face**	faces;
int	nfaces;
int	nsfaces;
int	maxfaces = 10;
char*	voiceinmsg;
char*	voiceoutmsg;

static char*
translatedomain(char *dom)
{
	static char buf[200];
	char *p, *ep, *q, *nextp, *file;
	char *bbuf, *ebuf;
	Reprog *exp;

	file = readfstr("/lib/face/.machinelist");
	if (file == nil)
		return dom;

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil || q-p > sizeof(buf)-2)
			continue;

		bbuf = buf+1;
		ebuf = buf+(1+(q-p));
		strncpy(bbuf, p, ebuf-bbuf);
		*ebuf = 0;
		if(*bbuf != '^')
			*--bbuf = '^';
		if(ebuf[-1] != '$') {
			*ebuf++ = '$';
			*ebuf = 0;
		}

		if((exp = regcomp(bbuf)) == nil){
			fprint(2, "bad regexp in machinelist: %s\n", bbuf);
			sysfatal("regexp");
		}

		if(regexec(exp, dom, 0, 0)){
			free(exp);
			ep = p+strlen(p);
			q += strspn(q, " \t");
			if(ep-q+2 > sizeof buf) {
				fprint(2, "huge replacement in machinelist: %.*s\n", utfnlen(q, ep-q), q);
				sysfatal("bad big replacement");
			}
			strncpy(buf, q, ep-q);
			ebuf = buf+(ep-q);
			*ebuf = 0;
			while(ebuf > buf && (ebuf[-1] == ' ' || ebuf[-1] == '\t'))
				*--ebuf = 0;
			free(file);
			return buf;
		}
		free(exp);
	}
	free(file);

	return dom;

}

static char*
tryfindpicture_user(char *dom, char *user, int depth)
{
	static char buf[200];
	char *p, *q, *nextp, *file, *usr;
	usr = getuser();

	sprint(buf, "/usr/%s/lib/face/48x48x%d/.dict", usr, depth);
	if((file = readfstr(buf)) == nil)
		return nil;

	snprint(buf, sizeof buf, "%s/%s", dom, user);

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil)
			continue;
		*q++ = 0;

		if(strcmp(buf, p) == 0) {
			q += strspn(q, " \t");
			q = buf+snprint(buf, sizeof buf, "/usr/%s/lib/face/48x48x%d/%s", usr, depth, q);
			while(q > buf && (q[-1] == ' ' || q[-1] == '\t'))
				*--q = 0;
			free(file);
			return buf;
		}
	}
	free(file);
	return nil;			
}

static char*
tryfindpicture_global(char *dom, char *user, int depth)
{
	static char buf[200];
	char *p, *q, *nextp, *file;

	sprint(buf, "/lib/face/48x48x%d/.dict", depth);
	if((file = readfstr(buf)) == nil)
		return nil;

	snprint(buf, sizeof buf, "%s/%s", dom, user);

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil)
			continue;
		*q++ = 0;

		if(strcmp(buf, p) == 0) {
			q += strspn(q, " \t");
			q = buf+snprint(buf, sizeof buf, "/lib/face/48x48x%d/%s", depth, q);
			while(q > buf && (q[-1] == ' ' || q[-1] == '\t'))
				*--q = 0;
			free(file);
			return buf;
		}
	}
	free(file);
	return nil;			
}

static char*
tryfindpicture(char *dom, char *user, int depth)
{
	char* result;

	if((result = tryfindpicture_user(dom, user, depth)) != nil)
		return result;

	return tryfindpicture_global(dom, user, depth);
}

static char*
tryfindfile(char *dom, char *user, int depth)
{
	char *p, *q;

	for(;;){
		for(p=dom; p; (p=strchr(p, '.')) && p++)
			if(q = tryfindpicture(p, user, depth))
				return q;
		depth >>= 1;
		if(depth == 0)
			break;
	}
	return nil;
}

void
parseaddr(char* sender, char** dp, char** up)
{
	char*	dom;
	char*	user;
	char *at, *bang;
	char *p;


	/* works with UTF-8, although it's written as ASCII */
	for(p=sender; *p!='\0'; p++)
		*p = tolower(*p);
	user = sender;
	dom = nil;
	at = strchr(sender, '@');
	if(at){
		*at++ = '\0';
		dom = at; // estrdup(at);
		goto done;
	}
	bang = strchr(sender, '!');
	if(bang){
		*bang++ = '\0';
		user = bang; // estrdup(bang);
		dom = sender;
	}
done:
	*dp = dom;
	*up = user;
}

char*
dblookup(char* a)
{
	static char	*facedom;
	char*	dom;
	char*	user;
	char*	p;
	static char addr[80];

	strecpy(addr, addr+80, a);
	parseaddr(addr, &dom, &user);

	if(facedom == nil){
		facedom = getenv("facedom");
		if(facedom == nil)
			facedom = "lsub.org";
	}
	if(dom == nil)
		dom = facedom;
	dom = translatedomain(dom);
	dprint(2,"dom %s\n", dom);
	if(p = tryfindfile(dom, user, 8))
		return p;
	p = tryfindfile(dom, "unknown", 8);
	if(p != nil || strcmp(dom, facedom)==0)
		return p;
	p =  tryfindfile("unknown", "unknown", 8);
	if (!p)
		return "/dev/null";
	else
		return p;
}

void
freeface(Face* f)
{
	free(f->key);
	free(f->addr);
	free(f->file);
	free(f->label);
	if (f->img)
		pclose(f->img);
	if (f->tag)
		pclose(f->tag);
	if (f->col)
		pclose(f->col);
	free(f);
}

static void
gc1(void)
{
	int	i;
	int	first;
	int	firsti;

	first = firsti = -1;
	for(i = 0; i < nfaces; i++)
		if(faces[i] != nil)
		if(first < 0)
			first = faces[firsti=i]->nb;
		else if(faces[i]->nb < first)
			first = faces[firsti=i]->nb;
	if(firsti >= 0){
		freeface(faces[firsti]);
		faces[firsti] = nil;
		nsfaces--;
	}		
}

Face*
newface(char* addr, char* key)
{
	static int seq = 0;
	int	i;
	int	pos = -1;
	Face*	f;
	char*	s;

	for (i = 0; i < nfaces; i++){
		if (faces[i] == nil)
			pos = i;
		else if (key && !strcmp(faces[i]->key, key))
			return nil;	// already there
	}
	if (pos < 0){
		if ((nfaces%32) == 0)
			faces = erealloc(faces, (nfaces+32)*sizeof(Face*));
		pos = nfaces++;
	}
	f = faces[pos] = emalloc(sizeof(Face));
	f->nb = seq++;
	f->id = 2+pos; // ids 0 and 1 are used
	f->key = key ? estrdup(key) : nil;
	f->addr = estrdup(addr);
	f->label = nil;
	if(s = strchr(addr, ':')){
		f->label = estrdup(s+1);
		*s = 0;
	}
	f->file = estrdup(dblookup(addr));
	dprint(2,"new face addr %s key %s file %s\n", f->addr, f->key, f->file);
	if (f->file == nil){
		faces[pos] = nil;
		freeface(f);
		f = nil;
	} else {
		nsfaces++;
		if(nsfaces > maxfaces)
			gc1();
	}
	return f;
}

void
say(char* fmt, char* name)
{
	char	voice[80];
	char*	s;

	seprint(voice, voice+sizeof(voice), fmt, name);
	s = strchr(voice, '@');
	if (!s)
		s = strchr(voice, ':');
	if(s)
		*s = 0;
	writefstr("/mnt/voice/speak", voice);

}

void
delfacei(int i)
{
	Face*	f;

	f = faces[i];
	faces[i] = nil;
	dprint(2, "ofaces: del %s\n", f->addr);
	if (voiceoutmsg)
		say(voiceoutmsg, f->addr);
	freeface(f);
	nsfaces--;
}

void
delface(char* key)
{
	int	i;

	for (i = 0; i < nfaces; i++)
		if (faces[i] && !strcmp(faces[i]->key, key)){
			delfacei(i);
			break;
		}
}

int
hasface(char* key)
{
	int	i;

	for (i = 0; i < nfaces; i++)
		if (faces[i] && !strcmp(faces[i]->key, key))
			return 1;
	return 0;
}

void
addfaceimg(Face* f)
{
	long	l;
	void*	data;
	char	addr[15];
	char*	s;
	char*	fname;
	int	fd;

	if (f == nil)
		return;
	f->col = pnew(grow, "col:face", f->id);
	if (f->col == nil){
		fprint(2, "ofaces: %r\n");
		return;
	}
	pctl(f->col, "notag\n");
	f->img = pnew(f->col, "image:face", f->id);
	if (f->img == nil){
		fprint(2, "ofaces: %r\n");
		return;
	}
	seprint(addr, addr+sizeof(addr), "label:%s", f->label ? f->label : f->addr);
	s = strchr(addr, '@');
	if (s)
		*s = 0;
	f->tag = pnewnamed(f->col, addr, f->id);
	if (f->tag == nil){
		fprint(2, "ofaces: %r\n");
		return;
	}
	pctl(f->tag, "font S");
	data = readf(f->file, nil, 0, &l);
	if (data && l>0){
		fname = smprint("%s/data", f->img->path);
		fd = open(fname, OWRITE|OTRUNC);
		write(fd, data, l);
		close(fd);
		free(fname);
	}
	free(data);
	dprint(2, "ofaces: add %s\n", f->addr);
	if (voiceinmsg)
		say(voiceinmsg, f->addr);
}

/* event must be of the form:
 * key username new|gone
 */
void
updateuser(char* ev)
{
	char*	toks[10];
	int	ntoks;

	ntoks = tokenize(ev, toks, nelem(toks));
	if(ntoks < 3)
		return;
	pctl(grow, "hold\n");
	if(!strcmp(toks[2], "gone"))
		delface(toks[0]);
	else if(!hasface(toks[0]))
		addfaceimg(newface(toks[1], toks[0]));
	pctl(grow, "release\n");
}

void
lookface(int id)
{
	int i;
	char* c;

	for(i = 0; i < nfaces; i++)
		if(faces[i] != nil && faces[i]->id == id)
			break;
	if(i < nfaces){
		c = smprint("look %s/text\n", faces[i]->key);
		oxctl(c);
		free(c);
	}
}

void
portsproc(void* a)
{
	Channel*  c = a;
	char*	pref;
	int	fd;
	char*	fname;
	char	buf[256];
	int	nr;
	int	l;

	pref = recvp(c);
	fname = smprint("/mnt/ports/faces.%d", getpid());
	fd = create(fname, ORDWR|ORCLOSE, 0664);
	if(fd < 0){
		sendp(c, smprint("create: %r"));
		threadexits("ports");
	}
	l = strlen(pref);
	pref = smprint("^%s.*", pref);
	if(write(fd, pref, strlen(pref)) < 0){
		close(fd);
		sendp(c, smprint("write: %r"));
		threadexits("ports");
	}
	sendp(c, nil);
	for(;;){
		nr = read(fd, buf, sizeof(buf)-1);
		if(nr <= l){
			sendp(c, nil);
			threadexits(nil);
		}
		buf[nr] = 0;
		sendp(c, strdup(buf + l));
	}
}

Channel*
portsc(char* prefix)
{
	Channel*	c;
	char*	sts;

	c = chancreate(sizeof(char*), 0);
	proccreate(portsproc, c, 16*1024);
	sendp(c, prefix);
	sts = recvp(c);
	if(sts != nil)
		sysfatal("ports: %s", sts);
	return c;
}

void
usage(void)
{
	fprint(2, "usage: %s [-d] [-n max] [-i inmsg] [-o outmsg] [-l label] [prefix]\n", argv0);
	sysfatal("usage");
}

void
threadmain(int argc, char* argv[])
{
	char*	scrs[10];
	int	nscrs;
	char* 	label;
	char*	prefix;
	char	str[40];
	Panel*	lg;
	Pev*	e;
	char*	ev;
	char*	pdir;
	Alt	a[] = {
		{ nil, &e, CHANRCV },
		{ nil, &ev, CHANRCV },
		{ nil, nil, CHANEND }};

	label = "Mail";
	prefix = "/mails: ";
	ARGBEGIN{
	case 'i':
		voiceinmsg = EARGF(usage());
		break;
	case 'o':
		voiceoutmsg = EARGF(usage());
		break;
	case 'd':
		paneldebug = debug++;
		break;
	case 'n':
		maxfaces = atoi(EARGF(usage()));
		break;
	case 'l':
		label = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;
	switch(argc){
	case 0:
		break;
	case 1:
		prefix = argv[0];
		break;
	default:
		usage();
	}
	grow = pinit("row", "faces");
	if (grow == nil)
		sysfatal("no ui: %r\n");
	if (label){
		seprint(str, str+40, "label:%s", label);
		lg = pnewnamed(grow, str, 1);
		if (lg == nil)
			sysfatal("pnew: %r");
	}
	scrs[0] = userscreen();
	nscrs = 1;
	if(scrs[0] == nil){
		nscrs = screens(scrs, nelem(scrs));
		if(nscrs < 1)
			sysfatal("no screens");
	}
	pdir = getenv("omerodir");
	if(pdir == nil)
		pdir = smprint("/%s/row:stats", scrs[0]);
	if(pctl(grow, "copyto %s\n", pdir) < 0)
		sysfatal("can't show ui: %r");
	free(pdir);
	while(nscrs > 0)
		free(scrs[--nscrs]);
	a[0].c = pevc(grow);
	a[1].c = portsc(prefix);
	for(;;){
		switch(alt(a)){
		case 0:
			dprint(2, "event %s %d %d\n", e->path, e->id, e->ev);
			if(e->ev == Eclose && e->id < 2){
				fprint(2, "%s: ui gone: exiting\n", argv0);
				threadexitsall(nil);
			}
			if(e->ev == Elook || e->ev == Eexec)
				lookface(e->id);
			pevfree(e);
			break;
		case 1:
			if(ev != nil){
				dprint(2, "pevent %s\n", ev);
				updateuser(ev);
				free(ev);
			}
			break;
		default:
			threadexitsall("alt");
		}
	}
}
