#ifndef __AD_FPGA_MEM_H_
#define __AD_FPGA_MEM_H_
#include "ADCommon.hpp"
typedef enum ADFPGA_MEM_ACC_T {
  ADFPGA_MEM_ACC_OFF = 0,
  ADFPGA_MEM_ACC_ON,
  ADFPGA_MEM_ACC_UNKNOWN,
  ADFPGA_MEM_ACC_NONE
} ADFPGA_MEM_ACC;
class ADFpgaMem {
  ADFPGA_MEM_ACC access_sts;
  int emulation_mode;
  int fpga_mem_node;
  unsigned short *pFpga;
  unsigned long fpga_memsize;
  unsigned long fpga_memaddr;

public:
  ADFpgaMem();
  ~ADFpgaMem();
  RPC_SRV_RESULT connect(char *fpga_mem_device, unsigned long fpga_mem_addr,
                         unsigned long fpga_mem_size);
  RPC_SRV_RESULT disconnect();
  ADFPGA_MEM_ACC get_connection_status();
  RPC_SRV_RESULT set_emulation_mode(int mode);
  RPC_SRV_RESULT get_emulation_mode(int *mode);
  RPC_SRV_RESULT read_fpga_register(unsigned int addr, unsigned short *data);
  RPC_SRV_RESULT write_fpga_register(unsigned int addr, unsigned short data);
  RPC_SRV_RESULT read_fpga_register_array(unsigned int addr, int size,
                                          unsigned short **data);
  RPC_SRV_RESULT write_fpga_register_array(unsigned int addr, int size,
                                           unsigned short **data);
  RPC_SRV_RESULT write_fpga_register_bit(unsigned int addr, int bitpos,
                                         int bitval);
  RPC_SRV_RESULT read_fpga_register_bit(unsigned int addr, int bitpos,
                                        int *bitval);
};
#endif
