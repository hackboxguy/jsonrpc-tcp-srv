#include "SysmgrCltCmdline.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
int main(int argc, const char *argv[]) {
  SysmgrCltCmdline CmdLine;
  CmdLine.parse_cmdline_arguments(argc, (char **)argv);
  return 0;
}
