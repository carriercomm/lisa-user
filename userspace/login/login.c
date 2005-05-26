#include <stdio.h>
#include <ctype.h>
#define __USE_XOPEN
#include <unistd.h>
#include <getopt.h>
#include <memory.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <termios.h>
#include <string.h>
#define index strchr
#define rindex strrchr
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <utmp.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/sysmacros.h>
#include <netdb.h>
#include <paths.h>
#define	_PATH_DEFPATH_ROOT "/sbin:/bin:/usr/sbin:/usr/bin"

#include <sys/sysmacros.h>
#include <linux/major.h>
#include <lastlog.h>

#define SLEEP_EXIT_TIMEOUT 5
#define TTY_MODE 0600

static int timeout = 6000;
struct passwd *pwd;

void
xstrncpy(char *dest, const char *src, size_t n) {
	strncpy(dest, src, n-1);
	dest[n-1] = 0;
}

static void getloginname (void);
static void checknologin (void);
static void timedout (int);
static void dolastlog (int quiet);
void badlogin(const char *name);
void sleepexit(int eval);

#ifndef MAXPATHLEN
#  define MAXPATHLEN 1024
#endif

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
char    hostaddress[4];		/* used in checktty.c */
char	*hostname;		/* idem */
static char	*username, *tty_name, *tty_number;
static char	thishost[100];
static int	failures = 1;
static pid_t	pid;

/* Nice and simple code provided by Linus Torvalds 16-Feb-93 */
/* Nonblocking stuff by Maciej W. Rozycki, macro@ds2.pg.gda.pl, 1999.
   He writes: "Login performs open() on a tty in a blocking mode.
   In some cases it may make login wait in open() for carrier infinitely,
   for example if the line is a simplistic case of a three-wire serial
   connection. I believe login should open the line in the non-blocking mode
   leaving the decision to make a connection to getty (where it actually
   belongs). */
static void
opentty(const char * tty) {
	int i, fd, flags;

	fd = open(tty, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		syslog(LOG_ERR, "FATAL: can't reopen tty: %s",
		       strerror(errno));
		sleep(1);
		exit(1);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
    
	for (i = 0; i < fd; i++)
		close(i);
	for (i = 0; i < 3; i++)
		if (fd != i)
			dup2(fd, i);
	if (fd >= 3)
		close(fd);
}

/* In case login is suid it was possible to use a hardlink as stdin
   and exploit races for a local root exploit. (Wojciech Purczynski). */
/* More precisely, the problem is  ttyn := ttyname(0); ...; chown(ttyn);
   here ttyname() might return "/tmp/x", a hardlink to a pseudotty. */
/* All of this is a problem only when login is suid, which it isnt. */
static void
check_ttyname(char *ttyn) {
	struct stat statbuf;

	if (lstat(ttyn, &statbuf)
	    || !S_ISCHR(statbuf.st_mode)
	    || (statbuf.st_nlink > 1 && strncmp(ttyn, "/dev/", 5))) {
		syslog(LOG_ERR, "FATAL: bad tty");
		sleep(1);
		exit(1);
	}
}

int
main(int argc, char **argv)
{
    extern int optind;
    extern char *optarg, **environ;
    register int ch;
    register char *p;
    int ask, fflag, hflag, pflag, cnt, errsv;
    int quietlog, passwd_req;
    char *domain, *ttyn;
    char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
    char *termenv;
    char *childArgv[10];
    char *buff;
    int childArgc = 0;
    char *salt, *pp;

    pid = getpid();

    signal(SIGALRM, timedout);
    alarm((unsigned int)timeout);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    setpriority(PRIO_PROCESS, 0, 0);
    
    /*
     * -p is used by getty to tell login not to destroy the environment
     * -f is used to skip a second login authentication 
     * -h is used by other servers to pass the name of the remote
     *    host to login so that it may be placed in utmp and wtmp
     */
    gethostname(tbuf, sizeof(tbuf));
    xstrncpy(thishost, tbuf, sizeof(thishost));
    domain = index(tbuf, '.');
    
    username = tty_name = hostname = NULL;
    fflag = hflag = pflag = 0;
    passwd_req = 1;

    while ((ch = getopt(argc, argv, "fh:p")) != -1)
      switch (ch) {
	case 'f':
	  fflag = 1;
	  break;
	  
	case 'h':
	  if (getuid()) {
	      fprintf(stderr,
		      "login: -h for super-user only.\n");
	      exit(1);
	  }
	  hflag = 1;
	  if (domain && (p = index(optarg, '.')) &&
	      strcasecmp(p, domain) == 0)
	    *p = 0;

	  hostname = strdup(optarg); 	/* strdup: Ambrose C. Li */
	  {
		  struct hostent *he = gethostbyname(hostname);

		  /* he points to static storage; copy the part we use */
		  hostaddress[0] = 0;
		  if (he && he->h_addr_list && he->h_addr_list[0])
			  memcpy(hostaddress, he->h_addr_list[0],
				 sizeof(hostaddress));
	  }
	  break;
	  
	case 'p':
	  pflag = 1;
	  break;

	case '?':
	default:
	  fprintf(stderr,
		  "usage: login [-fp] [username]\n");
	  exit(1);
      }
    argc -= optind;
    argv += optind;
    if (*argv) {
	char *p = *argv;
	username = strdup(p);
	ask = 0;
	/* wipe name - some people mistype their password here */
	/* (of course we are too late, but perhaps this helps a little ..) */
	while(*p)
	    *p++ = ' ';
    } else
        ask = 1;

    for (cnt = getdtablesize(); cnt > 2; cnt--)
      close(cnt);
    
    ttyn = ttyname(0);

    if (ttyn == NULL || *ttyn == '\0') {
	/* no snprintf required - see definition of tname */
	sprintf(tname, "%s??", _PATH_TTY);
	ttyn = tname;
    }

    check_ttyname(ttyn);

    if (strncmp(ttyn, "/dev/", 5) == 0)
	tty_name = ttyn+5;
    else
	tty_name = ttyn;

    if (strncmp(ttyn, "/dev/tty", 8) == 0)
	tty_number = ttyn+8;
    else {
	char *p = ttyn;
	while (*p && !isdigit(*p)) p++;
	tty_number = p;
    }

    /* set pgid to pid */
    setpgrp();
    /* this means that setsid() will fail */
    
    {
	struct termios tt, ttt;
	
	tcgetattr(0, &tt);
	ttt = tt;
	ttt.c_cflag &= ~HUPCL;

	/* These can fail, e.g. with ttyn on a read-only filesystem */
	chown(ttyn, 0, 0);
	chmod(ttyn, TTY_MODE);

	/* Kill processes left on this tty */
	tcsetattr(0,TCSAFLUSH,&ttt);
	signal(SIGHUP, SIG_IGN); /* so vhangup() wont kill us */
	//vhangup();
	signal(SIGHUP, SIG_DFL);

	/* open stdin,stdout,stderr to the tty */
	opentty(ttyn);
	
	/* restore tty modes */
	tcsetattr(0,TCSAFLUSH,&tt);
    }

    openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

#if 0
    /* other than iso-8859-1 */
    printf("\033(K");
    fprintf(stderr,"\033(K");
#endif

    for (cnt = 0;; ask = 1) {

	if (ask) {
	    fflag = 0;
	    getloginname();
	}

	/* Dirty patch to fix a gigantic security hole when using 
	   yellow pages. This problem should be solved by the
	   libraries, and not by programs, but this must be fixed
	   urgently! If the first char of the username is '+', we 
	   avoid login success.
	   Feb 95 <alvaro@etsit.upm.es> */
	
	if (username[0] == '+') {
	    puts("Illegal username");
	    badlogin(username);
	    sleepexit(1);
	}
	
	/* (void)strcpy(tbuf, username); why was this here? */
	if ((pwd = getpwnam(username))) {
	    salt = pwd->pw_passwd;
	} else
	  salt = "xx";
	
	if (pwd) {
	    initgroups(username, pwd->pw_gid);
	}
	
	/* if user not super-user, check for disabled logins */
	if (pwd == NULL || pwd->pw_uid)
	  checknologin();
	
	/*
	 * Disallow automatic login to root; if not invoked by
	 * root, disallow if the uid's differ.
	 */
	if (fflag && pwd) {
	    int uid = getuid();
	    
	    passwd_req = pwd->pw_uid == 0 ||
	      (uid && uid != pwd->pw_uid);
	}

	/*
	 * If no pre-authentication and a password exists
	 * for this user, prompt for one and verify it.
	 */
	if (!passwd_req || (pwd && !*pwd->pw_passwd))
	  break;
	
	setpriority(PRIO_PROCESS, 0, -4);
	pp = getpass("Password: ");
	
	p = crypt(pp, salt);
	setpriority(PRIO_PROCESS, 0, 0);

	memset(pp, 0, strlen(pp));

	if (pwd && !strcmp(p, pwd->pw_passwd))
	  break;
	
	printf("Login incorrect\n");
	badlogin(username); /* log ALL bad logins */
	failures++;
	
	/* we allow 10 tries, but after 3 we start backing off */
	if (++cnt > 3) {
	    if (cnt >= 10) {
		sleepexit(1);
	    }
	    sleep((unsigned int)((cnt - 3) * 5));
	}
    }
    
    /* committed to login -- turn off timeout */
    alarm((unsigned int)0);
    
    endpwent();
    
    /* This requires some explanation: As root we may not be able to
       read the directory of the user if it is on an NFS mounted
       filesystem. We temporarily set our effective uid to the user-uid
       making sure that we keep root privs. in the real uid. 
       
       A portable solution would require a fork(), but we rely on Linux
       having the BSD setreuid() */
    
	quietlog = 0;
    dolastlog(quietlog);
    
    chown(ttyn, pwd->pw_uid, pwd->pw_gid);
    chmod(ttyn, TTY_MODE);

    setgid(pwd->pw_gid);
    
    if (*pwd->pw_shell == '\0')
      pwd->pw_shell = _PATH_BSHELL;
    
    /* preserve TERM even without -p flag */
    {
	char *ep;
	
	if(!((ep = getenv("TERM")) && (termenv = strdup(ep))))
	  termenv = "dumb";
    }
    
    /* destroy environment unless user has requested preservation */
    if (!pflag)
      {
          environ = (char**)malloc(sizeof(char*));
	  memset(environ, 0, sizeof(char*));
      }
    
    setenv("HOME", pwd->pw_dir, 0);      /* legal to override */
    if(pwd->pw_uid)
      setenv("PATH", _PATH_DEFPATH, 1);
    else
      setenv("PATH", _PATH_DEFPATH_ROOT, 1);
    
    setenv("SHELL", pwd->pw_shell, 1);
    setenv("TERM", termenv, 1);
    
    /* mailx will give a funny error msg if you forget this one */
    {
      char tmp[MAXPATHLEN];
      /* avoid snprintf */
      if (sizeof(_PATH_MAILDIR) + strlen(pwd->pw_name) + 1 < MAXPATHLEN) {
	      sprintf(tmp, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
	      setenv("MAIL",tmp,0);
      }
    }
    
    /* LOGNAME is not documented in login(1) but
       HP-UX 6.5 does it. We'll not allow modifying it.
       */
    setenv("LOGNAME", pwd->pw_name, 1);

    if (!strncmp(tty_name, "ttyS", 4))
      syslog(LOG_INFO, "DIALUP AT %s BY %s", tty_name, pwd->pw_name);
    
    /* allow tracking of good logins.
       -steve philp (sphilp@mail.alliance.net) */
    
    if (pwd->pw_uid == 0) {
	if (hostname)
	  syslog(LOG_NOTICE, "ROOT LOGIN ON %s FROM %s",
		 tty_name, hostname);
	else
	  syslog(LOG_NOTICE, "ROOT LOGIN ON %s", tty_name);
    } else {
	if (hostname) 
	  syslog(LOG_INFO, "LOGIN ON %s BY %s FROM %s", tty_name, 
		 pwd->pw_name, hostname);
	else 
	  syslog(LOG_INFO, "LOGIN ON %s BY %s", tty_name, 
		 pwd->pw_name);
    }
    
    if (!quietlog) {
	struct stat st;
	char *mail;
	
	mail = getenv("MAIL");
	if (mail && stat(mail, &st) == 0 && st.st_size != 0) {
		if (st.st_mtime > st.st_atime)
			printf("You have new mail.\n");
		else
			printf("You have mail.\n");
	}
    }
    
    signal(SIGALRM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_DFL);
    
    /* discard permissions last so can't get killed and drop core */
    if(setuid(pwd->pw_uid) < 0 && pwd->pw_uid) {
	syslog(LOG_ALERT, "setuid() failed");
	exit(1);
    }
    
    /* wait until here to change directory! */
    if (chdir(pwd->pw_dir) < 0) {
	printf("No directory %s!\n", pwd->pw_dir);
	if (chdir("/"))
	  exit(0);
	pwd->pw_dir = "/";
	printf("Logging in with home = \"/\".\n");
    }
    
    /* if the shell field has a space: treat it like a shell script */
    if (strchr(pwd->pw_shell, ' ')) {
	buff = malloc(strlen(pwd->pw_shell) + 6);

	if (!buff) {
	    fprintf(stderr, "login: no memory for shell script.\n");
	    exit(0);
	}

	strcpy(buff, "exec ");
	strcat(buff, pwd->pw_shell);
	childArgv[childArgc++] = "/bin/sh";
	childArgv[childArgc++] = "-sh";
	childArgv[childArgc++] = "-c";
	childArgv[childArgc++] = buff;
    } else {
	tbuf[0] = '-';
	xstrncpy(tbuf + 1, ((p = rindex(pwd->pw_shell, '/')) ?
			   p + 1 : pwd->pw_shell),
		sizeof(tbuf)-1);
	
	childArgv[childArgc++] = pwd->pw_shell;
	childArgv[childArgc++] = tbuf;
    }

    childArgv[childArgc++] = NULL;

    execvp(childArgv[0], childArgv + 1);

    errsv = errno;

    if (!strcmp(childArgv[0], "/bin/sh"))
	fprintf(stderr, "login: couldn't exec shell script: %s.\n",
		strerror(errsv));
    else
	fprintf(stderr, "login: no shell: %s.\n", strerror(errsv));

    exit(0);
}

static void
getloginname(void) {
    int ch, cnt, cnt2;
    char *p;
    static char nbuf[UT_NAMESIZE + 1];
    
    cnt2 = 0;
    for (;;) {
	cnt = 0;
	printf("\n%s login: ", thishost); fflush(stdout);
	for (p = nbuf; (ch = getchar()) != '\n'; ) {
	    if (ch == EOF) {
		badlogin("EOF");
		exit(0);
	    }
	    if (p < nbuf + UT_NAMESIZE)
	      *p++ = ch;
	    
	    cnt++;
	    if (cnt > UT_NAMESIZE + 20) {
		fprintf(stderr, "login name much too long.\n");
		badlogin("NAME too long");
		exit(0);
	    }
	}
	if (p > nbuf) {
	  if (nbuf[0] == '-')
	    fprintf(stderr,
		    "login names may not start with '-'.\n");
	  else {
	      *p = '\0';
	      username = nbuf;
	      break;
	  }
	}
	
	cnt2++;
	if (cnt2 > 50) {
	    fprintf(stderr, "too many bare linefeeds.\n");
	    badlogin("EXCESSIVE linefeeds");
	    exit(0);
	}
    }
}

/*
 * Robert Ambrose writes:
 * A couple of my users have a problem with login processes hanging around
 * soaking up pts's.  What they seem to hung up on is trying to write out the
 * message 'Login timed out after %d seconds' when the connection has already
 * been dropped.
 * What I did was add a second timeout while trying to write the message so
 * the process just exits if the second timeout expires.
 */

static void
timedout2(int sig) {
	struct termio ti;
    
	/* reset echo */
	ioctl(0, TCGETA, &ti);
	ti.c_lflag |= ECHO;
	ioctl(0, TCSETA, &ti);
	exit(0);			/* %% */
}

static void
timedout(int sig) {
	signal(SIGALRM, timedout2);
	alarm(10);
	fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	timedout2(0);
}

void
checknologin(void) {
    int fd, nchars;
    char tbuf[8192];
    
    if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
	  write(fileno(stdout), tbuf, nchars);
	close(fd);
	sleepexit(0);
    }
}

void
dolastlog(int quiet) {
    struct lastlog ll;
    int fd;
    
    if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
	lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
	if (!quiet) {
	    if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
		ll.ll_time != 0) {
		    time_t ll_time = (time_t) ll.ll_time;

		    printf("Last login: %.*s ",
			   24-5, ctime(&ll_time));
		
		    if (*ll.ll_host != '\0')
			    printf("from %.*s\n",
				   (int)sizeof(ll.ll_host), ll.ll_host);
		    else
			    printf("on %.*s\n",
				   (int)sizeof(ll.ll_line), ll.ll_line);
	    }
	    lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
	}
	memset((char *)&ll, 0, sizeof(ll));
	time(&ll.ll_time);
	xstrncpy(ll.ll_line, tty_name, sizeof(ll.ll_line));
	if (hostname)
	    xstrncpy(ll.ll_host, hostname, sizeof(ll.ll_host));

	write(fd, (char *)&ll, sizeof(ll));
	close(fd);
    }
}

void
badlogin(const char *name) {
    if (failures == 1) {
	if (hostname)
	  syslog(LOG_NOTICE, "LOGIN FAILURE FROM %s, %s",
		 hostname, name);
	else
	  syslog(LOG_NOTICE, "LOGIN FAILURE ON %s, %s",
		 tty_name, name);
    } else {
	if (hostname)
	  syslog(LOG_NOTICE, "%d LOGIN FAILURES FROM %s, %s",
		 failures, hostname, name);
	else
	  syslog(LOG_NOTICE, "%d LOGIN FAILURES ON %s, %s",
		 failures, tty_name, name);
    }
}

/* Should not be called from PAM code... */
void
sleepexit(int eval) {
    sleep(SLEEP_EXIT_TIMEOUT);
    exit(eval);
}
