#include "I2CBusAccess.h"
#include "ADJsonRpcMgr.hpp"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
using namespace std;
I2CBusAccess::I2CBusAccess(std::string DevNode) {
  node = DevNode;
  DevOpened = RPC_SRV_RESULT_FAIL;
  if (is_it_device_node(DevNode) == RPC_SRV_RESULT_SUCCESS) {
    fd = open(DevNode.c_str(), O_RDWR);
    switch (errno) {
    case ENOENT:
      DevOpened = RPC_SRV_RESULT_FILE_NOT_FOUND;
      break;
    case ENOTDIR:
      DevOpened = RPC_SRV_RESULT_FILE_NOT_FOUND;
      break;
    case EACCES:
      DevOpened = RPC_SRV_RESULT_DEVNODE_ACCERR;
      break;
    default:
      DevOpened = RPC_SRV_RESULT_SUCCESS;
      ConnType = ADLIB_DEV_CONN_TYPE_DEVNODE;
      break;
    }
  } else if (is_it_network_node(DevNode, Port, IpAddr) ==
             RPC_SRV_RESULT_SUCCESS) {
    if (Client.rpc_server_connect(IpAddr.c_str(), Port) != 0)
      DevOpened = RPC_SRV_RESULT_HOST_NOT_REACHABLE_ERR;
    else {
      DevOpened = RPC_SRV_RESULT_SUCCESS;
      ConnType = ADLIB_DEV_CONN_TYPE_NETWORK;
    }
  }
}
I2CBusAccess::~I2CBusAccess() {
  if (DevOpened == RPC_SRV_RESULT_SUCCESS &&
      ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE)
    close(fd);
  else if (DevOpened == RPC_SRV_RESULT_SUCCESS &&
           ConnType == ADLIB_DEV_CONN_TYPE_NETWORK)
    Client.rpc_server_disconnect();
}
bool I2CBusAccess::is_number(const std::string &s) {
  std::string::const_iterator it = s.begin();
  while (it != s.end() && std::isdigit(*it))
    ++it;
  return !s.empty() && it == s.end();
}
bool I2CBusAccess::validateIpAddress(const string &ipAddress) {
  struct sockaddr_in sa;
  int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
  return result != 0;
}
RPC_SRV_RESULT I2CBusAccess::is_it_network_node(std::string devnode, int &port,
                                                std::string &ip) {
  vector<string> strings;
  istringstream f(devnode);
  string s, str1, str2;
  ip = "none";
  port = -1;
  while (getline(f, s, ':')) {
    strings.push_back(s);
  }
  if (strings.size() <= 0 || strings.size() > 2) {
    return RPC_SRV_RESULT_FAIL;
  }
  if (strings.size() == 2) {
    str1 = strings[0];
    str2 = strings[1];
  } else {
    str1 = strings[0];
    str2 = "127.0.0.1";
  }
  if (is_number(str1)) {
    port = atoi(str1.c_str());
  } else if (is_number(str2)) {
    port = atoi(str2.c_str());
  } else
    return RPC_SRV_RESULT_FAIL;
  if (validateIpAddress(str1) == true)
    ip = str1;
  else if (validateIpAddress(str2) == true)
    ip = str2;
  else
    return RPC_SRV_RESULT_FAIL;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CBusAccess::is_it_device_node(std::string devnode) {
  struct stat buf;
  stat(devnode.c_str(), &buf);
  if (S_ISCHR(buf.st_mode))
    return RPC_SRV_RESULT_SUCCESS;
  return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::SetSlaveAddr(uint8_t addr) {
  if (ioctl(fd, I2C_SLAVE, addr) < 0) {
    return RPC_SRV_RESULT_BUS_ERROR;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CBusAccess::read_byte(uint32_t addr, uint8_t *data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    int32_t smret = i2c_smbus_read_byte(fd);
    if (smret < 0)
      return RPC_SRV_RESULT_FILE_READ_ERR;
    else
      *data = (uint8_t)smret;
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    char tmpbuf[64];
    RPC_SRV_RESULT retval = Client.get_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_BYTE_GET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, tmpbuf);
    if (retval == RPC_SRV_RESULT_SUCCESS)
      *data = (uint8_t)atoi(tmpbuf);
    return retval;
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::write_byte(uint32_t addr, uint8_t data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    int32_t smret = i2c_smbus_write_byte(fd, data);
    if (smret != 0)
      return RPC_SRV_RESULT_FILE_WRITE_ERR;
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    return Client.set_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_BYTE_SET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, data);
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::read_word(uint32_t addr, uint16_t *data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    uint8_t buff[16];
    if (I2C_READ(fd, buff, 2) != 2)
      return RPC_SRV_RESULT_FILE_READ_ERR;
    *data = (uint16_t)(buff[0] << 8) | (uint16_t)buff[1];
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    char tmpbuf[64];
    RPC_SRV_RESULT retval = Client.get_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_WORD_GET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, tmpbuf);
    if (retval == RPC_SRV_RESULT_SUCCESS)
      *data = (uint16_t)atoi(tmpbuf);
    return retval;
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::write_word(uint32_t addr, uint16_t data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    uint8_t buff[16];
    buff[0] = (uint8_t)(data >> 8);
    buff[1] = data & 0x00FF;
    if (I2C_WRITE(fd, buff, 2) != 2)
      return RPC_SRV_RESULT_FILE_WRITE_ERR;
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    return Client.set_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_WORD_SET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, data);
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::read_dword(uint32_t addr, uint32_t *data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    uint8_t buff[16];
    if (I2C_READ(fd, buff, 4) != 4)
      return RPC_SRV_RESULT_FILE_READ_ERR;
    *data = (uint32_t)(buff[0] << 24) | (uint32_t)(buff[1] << 16) |
            (uint32_t)(buff[2] << 8) | (uint32_t)buff[3];
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    char tmpbuf[64];
    RPC_SRV_RESULT retval = Client.get_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_DWORD_GET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, tmpbuf);
    if (retval == RPC_SRV_RESULT_SUCCESS)
      *data = (uint32_t)atoi(tmpbuf);
    return retval;
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::write_dword(uint32_t addr, uint32_t data) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  if (ConnType == ADLIB_DEV_CONN_TYPE_DEVNODE) {
    RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
    if (ret != RPC_SRV_RESULT_SUCCESS)
      return ret;
    uint8_t buff[16];
    buff[0] = (uint8_t)(data >> 24);
    buff[1] = (uint8_t)(data >> 16);
    buff[2] = (uint8_t)(data >> 8);
    buff[3] = data & 0x000000FF;
    if (I2C_WRITE(fd, buff, 4) != 4)
      return RPC_SRV_RESULT_FILE_WRITE_ERR;
    return RPC_SRV_RESULT_SUCCESS;
  } else if (ConnType == ADLIB_DEV_CONN_TYPE_NETWORK) {
    return Client.set_integer_type_with_addr_para(
        (char *)RPCMGR_RPC_MW_DWORD_SET, (char *)RPCMGR_RPC_MW_ARGADDR, addr,
        (char *)RPCMGR_RPC_MW_ARGDATA, data);
  } else
    return RPC_SRV_RESULT_FAIL;
}
RPC_SRV_RESULT I2CBusAccess::write_array(uint32_t addr, uint8_t *data,
                                         uint32_t len) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
  if (ret != RPC_SRV_RESULT_SUCCESS)
    return ret;
  if (I2C_WRITE(fd, data, len) != len)
    return RPC_SRV_RESULT_FILE_WRITE_ERR;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CBusAccess::read_array(uint32_t addr, uint8_t *data,
                                        uint32_t len) {
  if (DevOpened != RPC_SRV_RESULT_SUCCESS)
    return DevOpened;
  RPC_SRV_RESULT ret = SetSlaveAddr((uint8_t)addr);
  if (ret != RPC_SRV_RESULT_SUCCESS)
    return ret;
  if (I2C_READ(fd, data, len) != len)
    return RPC_SRV_RESULT_FILE_READ_ERR;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT I2CBusAccess::test_write_byte(char *dev, uint8_t addr,
                                             uint8_t data) {
  int myfd;
  myfd = open(dev, O_RDWR);
  if (myfd < 0) {
    switch (errno) {
    case ENOENT:
      return RPC_SRV_RESULT_FILE_NOT_FOUND;
    case ENOTDIR:
      return RPC_SRV_RESULT_FAIL;
    case EACCES:
      return RPC_SRV_RESULT_DEVNODE_ACCERR;
    default:
      break;
    }
  }
  if (ioctl(myfd, I2C_SLAVE, addr) < 0) {
    return RPC_SRV_RESULT_FILE_WRITE_ERR;
  }
  uint8_t buff[16];
  buff[0] = data;
  unsigned int tst = data;
  int sz = I2C_WRITE(myfd, &tst, 1);
  if (sz != 1) {
    return RPC_SRV_RESULT_FILE_WRITE_ERR;
  }
  return RPC_SRV_RESULT_SUCCESS;
}
int32_t I2CBusAccess::i2c_smbus_access(int file, char read_write,
                                       uint8_t command, int size,
                                       union i2c_smbus_data *data) {
  struct i2c_smbus_ioctl_data args;
  args.read_write = read_write;
  args.command = command;
  args.size = size;
  args.data = data;
  return ioctl(file, I2C_SMBUS, &args);
}
int32_t I2CBusAccess::i2c_smbus_read_byte(int file) {
#ifdef I2C_ACCESS_TYPE_SMBUS
  union i2c_smbus_data data;
  if (i2c_smbus_access(file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))
    return -1;
  else
    return 0x0FF & data.byte;
  return 0;
#else
  uint8_t buff[16];
  return read(file, buff, 1);
#endif
}
int32_t I2CBusAccess::i2c_smbus_write_byte(int file, uint8_t value) {
#ifdef I2C_ACCESS_TYPE_SMBUS
  size_t sz =
      i2c_smbus_access(file, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
  return sz;
#else
  uint8_t buff[16];
  buff[0] = value;
  size_t sz = write(file, buff, 1);
  return sz;
#endif
}
