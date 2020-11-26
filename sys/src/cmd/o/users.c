#include <u.h>
#include <libc.h>
#include <error.h>
#include <b.h>

enum {Nusers = 50};

typedef struct User User;

struct User {
	char* name;
	char* loc;
};

int debug;
User users[2][Nusers];
int ue;
int dry;

User*
finduser(char* name, int uepoch)
{
	User*	usrs;
	int	i;

	usrs = users[uepoch];
	for(i = 0; i < Nusers; i++)
		if(usrs[i].name && !strcmp(usrs[i].name, name))
			return &usrs[i];
	return nil;
}

void
freeuser(User* u)
{
	free(u->name);
	free(u->loc);
	u->name = nil;
	u->loc = nil;
}

int
readuser(char* name, User* u)
{
	char	fname[50];
	char*	sts;
	char*	loc;

	seprint(fname, fname+sizeof(fname), "/who/%s", name);
	if(chdir(fname) < 0)
		return -1;
	sts = readfstr("status");
	loc = readfstr("where");
	if(sts == nil || loc == nil || *loc == 0){
		free(sts);
		free(loc);
		return -1;
	}
	if(debug>2)
		fprint(2, "users: %s %s %s\n", name, sts, loc);
	if(strcmp(sts, "online\n") && strcmp(sts, "busy\n")  && strcmp(loc, "home\n")){
		free(sts);
		free(loc);
		return -1;
	}
	freeuser(u); // should be free anyway
	u->name = estrdup(name);
	u->loc = loc;
	u->loc[strlen(u->loc)-1] = 0; // \n
	return 0;
}

void
readusers(int uepoch)
{
	User*	usrs;
	int	i, ui;
	Dir*	d;
	int	nd;
	int	fd;

	usrs = users[uepoch];
	for(i = 0; i < Nusers; i++)
		freeuser(&usrs[i]);
	fd = open("/who", OREAD);
	if(fd < 0)
		sysfatal("/who: %r");
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd <= 0)
		sysfatal("no users");
	for(i = ui = 0; i < nd; i++)
		if(readuser(d[i].name, &usrs[ui]) >= 0)
			ui++;
	free(d);
}

void
olduser(User* u)
{
	char ev[80];

	seprint(ev, ev+sizeof(ev), "/who: %s %s:%s gone\n",
		u->name, u->name, u->loc);
	if(dry)
		fprint(2, "users: post: %s", ev);
	else
		writefstr("/mnt/ports/post", ev);
}

void
newuser(User* u)
{
	char ev[80];

	seprint(ev, ev+sizeof(ev), "/who: %s %s:%s new\n",
		u->name, u->name, u->loc);
	if(dry)
		fprint(2, "users: post: %s", ev);
	else
		writefstr("/mnt/ports/post", ev);
}

void
changes(void)
{
	User*	ouser;
	User*	nuser;
	int	i;
	int	oue;

	oue = ue;
	ue = 1-ue;
	readusers(ue);
	for(i = 0; i < Nusers; i++){
		nuser = &users[ue][i];
		if(nuser->name == nil)
			continue;
		ouser = finduser(nuser->name, oue);
		if(ouser == nil){
			if(debug)
				fprint(2, "users: new %s\n", nuser->name);
			newuser(nuser);
		} else if(strcmp(ouser->loc, nuser->loc)){
			if(debug)
				fprint(2, "users: chg %s\n", nuser->name);
			freeuser(ouser);
			olduser(nuser);
			newuser(nuser);
		} else {
			freeuser(ouser);
			if(debug>1)
				fprint(2, "users: %s %s\n", nuser->name, nuser->loc);
		}
	}
	for(i = 0; i < Nusers; i++){
		ouser = &users[oue][i];
		if(ouser->name != nil){
			if(debug)
				fprint(2, "users: old %s\n", ouser->name);
			olduser(ouser);
			freeuser(ouser);
		}
	}
}
		
void
usage(void)
{
	fprint(2, "usage: %s [-dn] \n", argv0);
	sysfatal("usage");
}

void
main(int argc, char* argv[])
{
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'n':
		dry++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc > 0)
		usage();
	if(!dry)
	switch(rfork(RFPROC|RFFDG|RFNOWAIT)){
	case -1:
		sysfatal("rfork: %r");
	case 0:
		execl("/bin/o/faces", "faces", 
			"-i", "%s is logged in",
			"-o","%s is logged out",
			"-l", "Who", "/who: ", nil);
		sysfatal("exec: %r");
	}
	sleep(5 * 1000); // time for o/faces to start and listen...
	for(;;){
		changes();
		sleep(30 * 1000);
	}
}

