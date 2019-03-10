#include "types.h"
#include "user.h"
#include "fcntl.h"

#define EXEC  1
#define PIPE  2
#define BACK  3

#define MAXARGS 10

struct cmd {
    int type;
};

struct execcmd {
    int type;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
};

struct pipecmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct backcmd {
    int type;
    struct cmd *cmd;
};

int fork1(void);

void showError(char *);

struct cmd *parsecmd(char *);

void runCommand(struct cmd *cmd) {
  int pipeFd[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;

  if (cmd == 0)
    exit();

  switch (cmd->type) {
    case EXEC:
      ecmd = (struct execcmd *) cmd;
      if (ecmd->argv[0] == 0) {
        exit();
      }
      exec(ecmd->argv[0], ecmd->argv);
      printf(2, "Cannot run this command %s \n", ecmd->argv[0]);
      break;


    case PIPE:
      pcmd = (struct pipecmd *) cmd;

      if (pipe(pipeFd) < 0){
        showError("pipe");
      }

      if (fork1() == 0) {
        close(1);
        dup(pipeFd[1]);
        close(pipeFd[0]);
        close(pipeFd[1]);
        runCommand(pcmd->left);
      }

      if (fork1() == 0) {
        close(0);
        dup(pipeFd[0]);
        close(pipeFd[0]);
        close(pipeFd[1]);
        runCommand(pcmd->right);
      }

      close(pipeFd[0]);
      close(pipeFd[1]);
      wait();
      wait();
      break;


    case BACK:
      bcmd = (struct backcmd *) cmd;
      printf(2, "[pid %d] runs as a background process. \nxvsh> ", getpid());
      runCommand(bcmd->cmd);
      break;


    default:
      showError("runCommand");
  }

  exit();
}

int getcmd(char *buf, int nbuf) {
//  printf(2,"in here...");
  printf(2, "xvsh> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  return buf[0] == 0 ? -1 : 0;
}

int main(void) {
  static char buf[100];

  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't'){
      //  printf(2,"in Exit.. check me");
      exit();
    }

    if (fork1() == 0) {
      runCommand(parsecmd(buf));
    }

    wait();
  }
  exit();
}

void showError(char *msg) {
  printf(2, "%s\n", msg);
  exit();
}

int fork1(void) {
  int pid = fork();
  if (pid == -1){
    showError("fork");
  }
  return pid;
}

struct cmd *execcmd(void) {
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *) cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *) cmd;
}

struct cmd *backcmd(struct cmd *subcmd) {
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *) cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "|&";

int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) {
    s++;
  }

  if (q) {
    *q = s;
  }
  ret = *s;

  switch (*s) {
    case 0:
      break;

    case '|':
    case '&':
      s++;
      break;

    default:
      ret = 'a';
      while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
        s++;
      break;
  }

  if (eq) {
    *eq = s;
  }

  while (s < es && strchr(whitespace, *s)) {
    s++;
  }

  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks) {
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);

struct cmd *parsepipe(char **, char *);

struct cmd *parseexec(char **, char *);

struct cmd *flushStructs(struct cmd *);

struct cmd *parsecmd(char *s) {
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    printf(2, "leftovers: %s\n", s);
    showError("syntax");
  }
  flushStructs(cmd);
  return cmd;
}

struct cmd *parseline(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  return cmd;
}

struct cmd *parsepipe(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *parseexec(char **ps, char *es) {
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  ret = execcmd();
  cmd = (struct execcmd *) ret;

  argc = 0;
  while (!peek(ps, es, "|&")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0){
      break;
    }
    if (tok != 'a'){
      showError("syntax");
    }
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *flushStructs(struct cmd *cmd) {
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
    case EXEC:
      ecmd = (struct execcmd *) cmd;
      for (int i = 0; ecmd->argv[i]; i++)
        *ecmd->eargv[i] = 0;
      break;

    case PIPE:
      pcmd = (struct pipecmd *) cmd;
      flushStructs(pcmd->left);
      flushStructs(pcmd->right);
      break;

    case BACK:
      bcmd = (struct backcmd *) cmd;
      flushStructs(bcmd->cmd);
      break;
  }
  return cmd;
}
