#include <Wire.h>
#define SLAVE_ADDRESS2 8  //DAC address




// Address of all DAC

#define DAC_WRITE_TO_INPUT_REG 0b00000111

#define DAC_UPDATE_DAC_REG 0b00001111

#define DAC_WRITE_TO_INPUT_REG_UPDATE_ALL 0b00010111

#define DAC_WRITE_TO_AND_UPDATE_CHANNEL 0b00011111

#define DAC_POWER_UP_OR_DOWN 0b00100111

#define DAC_RESET 0b00101111

#define DAC_LDAC_REGISTER_SET 0b00110111

#define DAC_INTERNAL_REF_SET 0b00111111



//Address DAC A

#define DAC_A_WRITE_TO_INPUT_REG 0b00000000

#define DAC_A_UPDATE_DAC_REG 0b00001000

#define DAC_A_WRITE_TO_INPUT_REG_UPDATE_ALL 0b00010000

#define DAC_A_WRITE_TO_AND_UPDATE_CHANNEL 0b00011000

#define DAC_A_POWER_UP_OR_DOWN 0b00100000

#define DAC_A_RESET 0b00101000

#define DAC_A_LDAC_REGISTER_SET 0b00110000

#define DAC_A_INTERNAL_REF_SET 0b00111000



//Address DAC B

#define DAC_B_WRITE_TO_INPUT_REG 0b00000001

#define DAC_B_UPDATE_DAC_REG 0b00001001

#define DAC_B_WRITE_TO_INPUT_REG_UPDATE_ALL 0b00010001

#define DAC_B_WRITE_TO_AND_UPDATE_CHANNEL 0b00011001

#define DAC_B_POWER_UP_OR_DOWN 0b00100001

#define DAC_B_RESET 0b00101001

#define DAC_B_LDAC_REGISTER_SET 0b00110001

#define DAC_B_INTERNAL_REF_SET 0b00111001



//Address DAC C

#define DAC_C_WRITE_TO_AND_UPDATE_CHANNEL 0b00011010

#define DAC_C_WRITE_TO_INPUT_REG 0b00000010

#define DAC_C_UPDATE_DAC_REG 0b00001010

#define DAC_C_WRITE_TO_INPUT_REG_UPDATE_ALL 0b00010010

#define DAC_C_POWER_UP_OR_DOWN 0b00100010

#define DAC_C_RESET 0b00101010

#define DAC_C_LDAC_REGISTER_SET 0b00110010

#define DAC_C_INTERNAL_REF_SET 0b00111010



//Address DAC D

#define DAC_D_WRITE_TO_AND_UPDATE_CHANNEL 0b00011011

#define DAC_D_WRITE_TO_INPUT_REG 0b00000011

#define DAC_D_UPDATE_DAC_REG 0b00001011

#define DAC_D_WRITE_TO_INPUT_REG_UPDATE_ALL 0b00010011

#define DAC_D_POWER_UP_OR_DOWN 0b00100011

#define DAC_D_RESET 0b00101011

#define DAC_D_LDAC_REGISTER_SET 0b00110011

#define DAC_D_INTERNAL_REF_SET 0b00111011


void DAC_command(uint8_t command, uint16_t val);
