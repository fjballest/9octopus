/*
 * Convenience library for using Octopus
 * o/mero from Plan 9.
 */
#pragma	lib	"libpanel.a"
#pragma src "/sys/src/libpanel"

typedef struct Panel Panel;
typedef struct Pev Pev;

struct Panel {
	int	id;
	char*	name;
	char*	path;
	char*	evpath;
	int	gcfd;
	int	rpid;
};

enum {
	Elook,
	Eexec,
	Eclose,
	Eclean,
	Edirty,
	Eintr,
	Eclick,
	Ekeys,
	Efocus,
	Nevs
};

struct Pev {
	char*	path;
	int	id;
	int	ev;
	char*	arg;
};

#pragma	varargck	argpos	panelctl	2

void	pclose(Panel* p);
int	cols(char* scr, char** v, int nv);
long	pdata(Panel* p, void* data, long cnt);
Panel*	pnew(Panel* p, char* name, int id);
Panel*	pnewnamed(Panel* p, char* name, int id);
int	pctl(Panel* p, char* fmt, ...);
Channel*	pevc(Panel* p);
void	pevfree(Pev* ev);
char*	pevname(int evid);
Panel*	pinit(char* type, char* name);
int	screens(char**v, int nv);
char*	userscreen(void);
int	oxctl(char* c);
extern char* omero;
extern int paneldebug;

