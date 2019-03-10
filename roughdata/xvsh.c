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

int forkWrapper(void);

void showError(char *);

struct cmd *parseCmd(char *);

void runCommand(struct cmd *cmd) {
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;

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
      int pipeFd[2];
      pcmd = (struct pipecmd *) cmd;

      if (pipe(pipeFd) < 0) {
        showError("pipeFail");
      }

      if (forkWrapper() == 0) {
        close(1);
        dup(pipeFd[1]);
        close(pipeFd[0]);
        close(pipeFd[1]);
        runCommand(pcmd->left);
      }

      if (forkWrapper() == 0) {
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
    if (buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't') {
      //  printf(2,"in Exit.. check me");
      exit();
    }

    if (forkWrapper() == 0) {
      runCommand(parseCmd(buf));
    }

    wait();
  }
  exit();
}

void showError(char *msg) {
  printf(2, "%s\n", msg);
  exit();
}

int forkWrapper(void) {
  //  printf(2,"in forkWrapper..\n");
  int pid = fork();
  if (pid == -1) {
    showError("forkFailed");
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

int getTokens(char **pStr, char *eStr, char **qPtr, char **eqPtr) {
  char *str;
  int retVal;

  str = *pStr;
  while (str < eStr && strchr(whitespace, *str)) {
    str++;
  }

  if (qPtr) {
    *qPtr = str;
  }
  retVal = *str;

  switch (*str) {
    case 0:
      break;

    case '|':
    case '&':
      str++;
      break;

    default:
      retVal = 'a';
      while (str < eStr && !strchr(whitespace, *str) && !strchr(symbols, *str)){
        str++;
      }
      break;
  }

  if (eqPtr) {
    *eqPtr = str;
  }

  while (str < eStr && strchr(whitespace, *str)) {
    str++;
  }

  *pStr = str;
  return retVal;
}

int findChar(char **pStr, char *eStr, char *tokens) {
  char *str;

  str = *pStr;
  while (str < eStr && strchr(whitespace, *str)) {
    str++;
  }
  *pStr = str;
  return *str && strchr(tokens, *str);
}

struct cmd *parseLine(char **, char *);

struct cmd *parsePipe(char **, char *);

struct cmd *parseExec(char **, char *);

struct cmd *flushStructs(struct cmd *);

struct cmd *parseCmd(char *str) {
  char *eStr;
  struct cmd *cmd;

  eStr = str + strlen(str);
  cmd = parseLine(&str, eStr);
  findChar(&str, eStr, "");
  if (str != eStr) {
    printf(2, "remains: %s\n", str);
    showError("syntaxError");
  }
  flushStructs(cmd);
  return cmd;
}

struct cmd *parseLine(char **pStr, char *eStr) {
  struct cmd *cmd;

  cmd = parsePipe(pStr, eStr);
  while (findChar(pStr, eStr, "&")) {
    getTokens(pStr, eStr, 0, 0);
    cmd = backcmd(cmd);
  }
  return cmd;
}

struct cmd *parsePipe(char **pStr, char *eStr) {
  struct cmd *cmd;

  cmd = parseExec(pStr, eStr);
  if (findChar(pStr, eStr, "|")) {
    getTokens(pStr, eStr, 0, 0);
    cmd = pipecmd(cmd, parsePipe(pStr, eStr));
  }
  return cmd;
}

struct cmd *parseExec(char **pStr, char *eStr) {
  char *qPtr, *eqPtr;
  int token, argc = 0;
  struct execcmd *cmd;
  struct cmd *retVal;

  retVal = execcmd();
  cmd = (struct execcmd *) retVal;

  while (!findChar(pStr, eStr, "|&")) {
    if ((token = getTokens(pStr, eStr, &qPtr, &eqPtr)) == 0) {
      break;
    }
    if (token != 'a') {
      showError("syntax");
    }
    cmd->argv[argc] = qPtr;
    cmd->eargv[argc] = eqPtr;
    argc++;
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return retVal;
}

//flush all data
struct cmd *flushStructs(struct cmd *cmd) {
  struct backcmd *backc;
  struct execcmd *execc;
  struct pipecmd *pipec;

  switch (cmd->type) {
    case EXEC:
      execc = (struct execcmd *) cmd;
      for (int i = 0; execc->argv[i]; i++) {
        *execc->eargv[i] = 0;
      }
      break;

    case PIPE:
      pipec = (struct pipecmd *) cmd;
      flushStructs(pipec->left);
      flushStructs(pipec->right);
      break;

    case BACK:
      backc = (struct backcmd *) cmd;
      flushStructs(backc->cmd);
      break;
  }
  return cmd;
}
