#include "ADSysInfo.hpp"
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
using namespace std;
ADSysInfo::ADSysInfo() {}
ADSysInfo::~ADSysInfo() {}
int ADSysInfo::count_string_occurance(char const *str, char *match_string) {
  char const *p = str;
  int count = 0;
  for (count = 0;; ++count) {
    p = strstr(p, match_string);
    if (!p)
      break;
    p++;
  }
  return count;
}
int ADSysInfo::get_cpu_cores(void) {
  FILE *cmdline = fopen("/proc/cpuinfo", "rb");
  char *arg = 0;
  size_t size = 0;
  int cpu_count = 0;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  cpu_count = count_string_occurance(arg, (char *)"model name");
  free(arg);
  fclose(cmdline);
  return cpu_count;
}
int ADSysInfo::get_current_and_average_load(float *current, float *average) {
  FILE *cmdline = fopen("/proc/loadavg", "rb");
  char *arg = 0;
  size_t size = 0;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  sscanf(arg, "%f %f", current, average);
  free(arg);
  fclose(cmdline);
  return 0;
}
int ADSysInfo::get_mem_and_free_mem(int *mem_kb, int *mem_free_kb) {
  FILE *cmdline = fopen("/proc/meminfo", "rb");
  char *arg = 0;
  size_t size = 0;
  char *find_str;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  find_str = strstr(arg, "MemTotal:");
  if (find_str == NULL)
    return -1;
  sscanf(find_str, "MemTotal: %d", mem_kb);
  find_str = strstr(arg, "MemFree:");
  if (find_str == NULL)
    return -1;
  sscanf(find_str, "MemFree: %d", mem_free_kb);
  free(arg);
  fclose(cmdline);
  return 0;
}
int ADSysInfo::get_uptime(int *uptime) {
  FILE *cmdline = fopen("/proc/uptime", "rb");
  char *arg = 0;
  size_t size = 0;
  float tmp_time = 0.0;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  sscanf(arg, "%f", &tmp_time);
  *uptime = (int)tmp_time;
  free(arg);
  fclose(cmdline);
  return 0;
}
int ADSysInfo::get_cpu_model_name(char *model) {
  FILE *cmdline = fopen("/proc/cpuinfo", "rb");
  char *arg = 0;
  size_t size = 0;
  char *find_str;
  char tmp_str[256];
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  find_str = strstr(arg, "Processor	:");
  if (find_str != NULL) {
    sscanf(find_str, "Processor	: %s", model);
    strcpy(tmp_str, model);
    tmp_str[3] = '\0';
    if (strcmp(tmp_str, "ARM") == 0) {
      free(arg);
      fclose(cmdline);
      return 0;
    }
  }
  find_str = strstr(arg, "model name	:");
  if (find_str == NULL)
    return -1;
  sscanf(find_str, "model name	: %s", model);
  free(arg);
  fclose(cmdline);
  return 0;
}
int ADSysInfo::get_cpu_frequency(char *freq) {
  FILE *cmdline = fopen("/proc/cpuinfo", "rb");
  char *arg = 0;
  size_t size = 0;
  char *find_str;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  sprintf(freq, "unknown");
  find_str = strstr(arg, "BogoMIPS        :");
  if (find_str != NULL) {
    sscanf(find_str, "BogoMIPS        : %s", freq);
    {
      free(arg);
      fclose(cmdline);
      return 0;
    }
  }
  find_str = strstr(arg, "cpu MHz      :");
  if (find_str == NULL) {
    return -1;
  }
  sscanf(find_str, "cpu MHz      : %s", freq);
  free(arg);
  fclose(cmdline);
  return 0;
}
int ADSysInfo::probe_eth_interface_details(int *total_detected,
                                           eth_name_ip_str *list) {
  struct ifreq *ifr, *ifend;
  struct ifreq ifreq;
  struct ifconf ifc;
  struct ifreq ifs[MAX_IFS];
  int SockFD;
  int i = 0;
  *total_detected = 0;
  SockFD = socket(AF_INET, SOCK_DGRAM, 0);
  ifc.ifc_len = sizeof(ifs);
  ifc.ifc_req = ifs;
  if (ioctl(SockFD, SIOCGIFCONF, &ifc) < 0) {
    close(SockFD);
    return -1;
  }
  ifend = ifs + (ifc.ifc_len / sizeof(struct ifreq));
  for (ifr = ifc.ifc_req; ifr < ifend; ifr++) {
    if (ifr->ifr_addr.sa_family == AF_INET) {
      strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
      if (ioctl(SockFD, SIOCGIFHWADDR, &ifreq) < 0) {
        close(SockFD);
        return -1;
      }
      if (i > MAX_ALLOWED_INTERFACES) {
        *total_detected = i;
        close(SockFD);
        return 0;
      }
      strcpy(list[i].dev, ifreq.ifr_name);
      strcpy(list[i].ip,
             inet_ntoa(((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr));
      sprintf(list[i].mac, "%02X:%02X:%02X:%02X:%02X:%02X",
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[0],
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[1],
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[2],
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[3],
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[4],
              (int)((unsigned char *)&ifreq.ifr_hwaddr.sa_data)[5]);
      strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
      if (ioctl(SockFD, SIOCGIFNETMASK, &ifreq) < 0) {
        strcpy(list[i].netmask, "not_available");
      } else
        strcpy(list[i].netmask,
               inet_ntoa((*(struct sockaddr_in *)&ifreq.ifr_netmask).sin_addr));
      i++;
    }
  }
  *total_detected = i;
  close(SockFD);
  return 0;
}
int ADSysInfo::read_system_status(int *mem, int *memfree, int *cores,
                                  int *cur_load, int *avg_load, int *uptime,
                                  char *cpu_model) {
  float cload = 0, aload = 0;
  *cores = get_cpu_cores();
  if (*cores < 1)
    *cores = 1;
  if (get_current_and_average_load(&cload, &aload) == 0) {
    *cur_load = (int)((cload / (float)*cores) * 100.0);
    *avg_load = (int)((aload / (float)*cores) * 100.0);
  } else {
    *cur_load = -1;
    *avg_load = -1;
  }
  if (get_mem_and_free_mem(mem, memfree) != 0) {
    *mem = -1;
    *memfree = -1;
  }
  if (get_uptime(uptime) != 0)
    *uptime = -1;
  if (get_cpu_model_name(cpu_model) != 0)
    strcpy(cpu_model, "unknown");
  return 0;
}
int ADSysInfo::read_network_info(char *eth, char *mac, char *ip,
                                 char *netmask) {
  int total_detected = 0, i = 0;
  eth_name_ip_str if_list[MAX_ALLOWED_INTERFACES];
  sprintf(mac, "unknown");
  sprintf(ip, "unknown");
  sprintf(netmask, "unknown");
  if (probe_eth_interface_details(&total_detected, if_list) != 0) {
    return -1;
  }
  for (i = 0; i < total_detected; i++) {
    if (strcmp(if_list[i].dev, "lo") == 0)
      continue;
    else if (strcmp(if_list[i].dev, eth) == 0) {
      strcpy(mac, if_list[i].mac);
      strcpy(ip, if_list[i].ip);
      strcpy(netmask, if_list[i].netmask);
      return 0;
    }
  }
  return -1;
}
int ADSysInfo::read_network_info_ifconfig(char *eth, char *mac, char *ip,
                                          char *netmask) {
  char cmd[512];
  char temp_str[256];
  sprintf(cmd, "ifconfig | grep %s >/dev/null", eth);
  if (system(cmd) != 0)
    return -1;
  sprintf(cmd, "ifconfig | grep %s | awk '{print $5}'", eth);
  FILE *shell;
  shell = popen(cmd, "r");
  if (shell == NULL)
    return -1;
  if (fgets(temp_str, 250, shell) != NULL) {
    temp_str[255] = '\0';
    temp_str[strlen(temp_str) - 1] = '\0';
    strcpy(mac, temp_str);
    pclose(shell);
  } else {
    pclose(shell);
    return -1;
  }
  sprintf(ip, "NotConnected");
  sprintf(netmask, "NotConnected");
  return 0;
}
int ADSysInfo::read_mem_info(char *mem, char *memfree, char *memused) {
  int intmem, intmemfree, intcores, intcload, intaload, intutime;
  char cpu_model[512];
  sprintf(mem, "unknown");
  sprintf(memfree, "unknown");
  sprintf(memused, "unknown");
  if (read_system_status(&intmem, &intmemfree, &intcores, &intcload, &intaload,
                         &intutime, cpu_model) == 0) {
    sprintf(mem, "%d", (intmem / 1024));
    sprintf(memfree, "%d", (intmemfree / 1024));
    sprintf(memused, "%d", ((intmem - intmemfree) / 1024));
  }
  return 0;
}
int ADSysInfo::read_load_info(char *curload, char *avgload, char *uptime) {
  int intmem, intmemfree, intcores, intcload, intaload, intutime;
  char cpu_model[512];
  sprintf(curload, "unknown");
  sprintf(avgload, "unknown");
  sprintf(uptime, "unknown");
  if (read_system_status(&intmem, &intmemfree, &intcores, &intcload, &intaload,
                         &intutime, cpu_model) == 0) {
    sprintf(curload, "%d", intcload);
    sprintf(avgload, "%d", intaload);
    sprintf(uptime, "%d", intutime);
  }
  return 0;
}
int ADSysInfo::read_cpu_info(char *cpumodel, char *cores, char *cpufreq) {
  int intmem, intmemfree, intcores, intcload, intaload, intutime;
  char cpu_model[512];
  sprintf(cpumodel, "unknown");
  sprintf(cores, "unknown");
  sprintf(cpufreq, "unknown");
  if (read_system_status(&intmem, &intmemfree, &intcores, &intcload, &intaload,
                         &intutime, cpu_model) == 0) {
    sprintf(cpumodel, "%s", cpu_model);
    sprintf(cores, "%d", intcores);
  }
  return 0;
}
RPC_SRV_RESULT ADSysInfo::get_total_eth_count(int &count) {
  char command[1024];
  char temp_str[256];
  FILE *shell;
  count = 0;
  sprintf(
      command,
      "ifconfig | grep \"Link encap\" | grep -v \"Local Loopback\" >/dev/null");
  if (system(command) != 0) {
    return RPC_SRV_RESULT_SUCCESS;
  }
  sprintf(command, "ifconfig | grep \"Link encap\" | grep -v \"Local "
                   "Loopback\" | sed -n '$='");
  shell = popen(command, "r");
  if (shell == NULL)
    return RPC_SRV_RESULT_FILE_OPEN_ERR;
  if (fgets(temp_str, 250, shell) != NULL) {
    temp_str[255] = '\0';
    count = atoi(temp_str);
  }
  pclose(shell);
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADSysInfo::get_nth_eth_name(int index, char *name) {
  char command[1024];
  char temp_str[256];
  FILE *shell;
  sprintf(command,
          "ifconfig | grep \"Link encap\" | grep -v \"Local Loopback\" | sed "
          "-n '%d,1p' | awk '{print $1}'",
          index);
  shell = popen(command, "r");
  if (shell == NULL)
    return RPC_SRV_RESULT_FILE_OPEN_ERR;
  if (fgets(temp_str, 250, shell) != NULL) {
    temp_str[255] = '\0';
    temp_str[strlen(temp_str) - 1] = '\0';
    strcpy(name, temp_str);
  }
  pclose(shell);
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADSysInfo::run_shell_script(char *script, bool emulation) {
  if (emulation) {
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    usleep(1000000);
    return RPC_SRV_RESULT_SUCCESS;
  }
  char command[1024];
  sprintf(command, "%s", script);
  int res = system(command);
  res /= 256;
  switch (res) {
  case 0:
    return RPC_SRV_RESULT_SUCCESS;
    break;
  case 1:
    return RPC_SRV_RESULT_FILE_OPEN_ERR;
    break;
  case 2:
    return RPC_SRV_RESULT_FILE_READ_ERR;
    break;
  case 3:
    return RPC_SRV_RESULT_FILE_WRITE_ERR;
    break;
  case 4:
    return RPC_SRV_RESULT_ARG_ERROR;
    break;
  case 5:
    return RPC_SRV_RESULT_ARG_ERROR;
    break;
  case 6:
    return RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR;
    break;
  case 7:
    return RPC_SRV_RESULT_FILE_SAME;
    break;
  case 8:
    return RPC_SRV_RESULT_FILE_DIFFERENT;
    break;
  case 9:
    return RPC_SRV_RESULT_FILE_NOT_FOUND;
    break;
  default:
    return RPC_SRV_RESULT_FAIL;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADSysInfo::run_shell_script(char *cmd, char *ret_val,
                                           bool emulation) {
  char temp_str[256];
  FILE *shell;
  shell = popen(cmd, "r");
  if (shell == NULL)
    return RPC_SRV_RESULT_FILE_OPEN_ERR;
  if (fgets(temp_str, 250, shell) != NULL) {
    temp_str[255] = '\0';
    temp_str[strlen(temp_str)] = '\0';
    strcpy(ret_val, temp_str);
  }
  pclose(shell);
  return RPC_SRV_RESULT_SUCCESS;
}
