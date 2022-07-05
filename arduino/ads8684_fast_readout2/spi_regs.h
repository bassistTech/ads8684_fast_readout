#define SPI_REGS IMXRT_LPSPI4_S // Teensy 4 SPI register block
#define SPI1_REGS IMXRT_LPSPI3_S // Teensy 4 SPI1 register block
#define SPI2_REGS IMXRT_LPSPI1_S // Teensy 4 SPI2 register block

/* Acknowledgement
Figured out SPI_REGS assignment from https://github.com/hideakitai/TsyDMASPI
Looks like the mappings are:
  LPSPI4 --> Arduino SPI
  LPSPI3 --> Arduino SPI1
  LPSPI1 --> Arduino SPI2
*/
