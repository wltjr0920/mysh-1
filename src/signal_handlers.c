#include "signal_handlers.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void catch_sigint(int signalNo)
{
  fflush(stdout);
  signal(SIGINT,SIG_IGN);
  sleep(0);
}

void catch_sigtstp(int signalNo)
{
  fflush(stdin);
  signal(SIGTSTP,SIG_IGN);
  sleep(0);
}