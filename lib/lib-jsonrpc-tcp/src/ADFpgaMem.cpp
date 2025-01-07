#include "ADFpgaMem.hpp"
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
using namespace std;
ADFpgaMem::ADFpgaMem() {
  emulation_mode = 0;
  access_sts = ADFPGA_MEM_ACC_OFF;
  fpga_memsize = 0;
  fpga_memaddr = 0;
}
ADFpgaMem::~ADFpgaMem() { disconnect(); }
RPC_SRV_RESULT ADFpgaMem::read_fpga_register(unsigned int addr,
                                             unsigned short *data) {
  if (access_sts == ADFPGA_MEM_ACC_OFF)
    return RPC_SRV_RESULT_DEV_NOT_ACCESSIBLE;
  *data = pFpga[addr];
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::write_fpga_register(unsigned int addr,
                                              unsigned short data) {
  if (access_sts == ADFPGA_MEM_ACC_OFF)
    return RPC_SRV_RESULT_DEV_NOT_ACCESSIBLE;
  pFpga[addr] = data;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::read_fpga_register_array(unsigned int addr, int size,
                                                   unsigned short **data) {
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::write_fpga_register_array(unsigned int addr, int size,
                                                    unsigned short **data) {
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::write_fpga_register_bit(unsigned int addr, int bitpos,
                                                  int bitval) {
  unsigned short data, mask = 1;
  if (read_fpga_register(addr, &data) != RPC_SRV_RESULT_SUCCESS)
    return RPC_SRV_RESULT_FAIL;
  mask = mask << bitpos;
  if (bitval)
    data |= mask;
  else
    data &= ~mask;
  return write_fpga_register(addr, data);
}
RPC_SRV_RESULT ADFpgaMem::read_fpga_register_bit(unsigned int addr, int bitpos,
                                                 int *bitval) {
  unsigned short data, mask = 1;
  if (read_fpga_register(addr, &data) != RPC_SRV_RESULT_SUCCESS)
    return RPC_SRV_RESULT_FAIL;
  data &= mask << bitpos;
  if (data == 0)
    *bitval = 0;
  else
    *bitval = 1;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::connect(char *fpga_mem_device,
                                  unsigned long fpga_mem_addr,
                                  unsigned long fpga_mem_size) {
  if (access_sts == ADFPGA_MEM_ACC_ON)
    return RPC_SRV_RESULT_SUCCESS;
  if (emulation_mode == 0) {
    if ((fpga_mem_node = open(fpga_mem_device, O_RDWR)) < 0)
      return RPC_SRV_RESULT_DEV_NOT_ACCESSIBLE;
    void *pTemp = mmap(NULL, fpga_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                       fpga_mem_node, fpga_mem_addr);
    if (pTemp < MAP_FAILED) {
      close(fpga_mem_node);
      return RPC_SRV_RESULT_DEV_NOT_ACCESSIBLE;
    }
    pFpga = (unsigned short *)pTemp;
    fpga_memsize = fpga_mem_size;
    fpga_memaddr = fpga_mem_addr;
  } else {
    pFpga = new unsigned short[fpga_mem_size];
  }
  access_sts = ADFPGA_MEM_ACC_ON;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::disconnect() {
  if (access_sts == ADFPGA_MEM_ACC_OFF)
    return RPC_SRV_RESULT_SUCCESS;
  if (emulation_mode == 0) {
    munmap(pFpga, fpga_memsize);
    close(fpga_mem_node);
    fpga_memsize = 0;
    fpga_memaddr = 0;
  } else {
    delete[] pFpga;
  }
  access_sts = ADFPGA_MEM_ACC_OFF;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::set_emulation_mode(int mode) {
  emulation_mode = mode;
  return RPC_SRV_RESULT_SUCCESS;
}
RPC_SRV_RESULT ADFpgaMem::get_emulation_mode(int *mode) {
  *mode = emulation_mode;
  return RPC_SRV_RESULT_SUCCESS;
}
ADFPGA_MEM_ACC ADFpgaMem::get_connection_status() { return access_sts; }
