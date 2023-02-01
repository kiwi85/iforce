#include "DAC_cmd.h"
void DAC_command(uint8_t command,uint16_t val)
{
  Wire.beginTransmission(SLAVE_ADDRESS2);
  Wire.write(command);
  Wire.write((uint8_t)(val>>4));
  Wire.write((uint8_t)(val<<4));
  //Wire.write(val,2);
  Wire.endTransmission();
}


