#ifndef __HSOFT_GPIO_H__
#define __HSOFT_GPIO_H__

// GPIOs
#define BUTTON  4
#define RELAY 5
#define MUX_S2 12
#define MUX_S1 13
#define MUX_S0 14

// Device numbering
#define EN_VOLT 0
#define EN_AMP 1
#define EN_TEMP 2
#define EN_HUMI 3

// delay in usecs between mux settings
#define MUX_UDELAY 5

// Temp and Humidity delay
#define TH_DELAY 1

//Volt and Amp delay
#define VA_DELAY 10

void select_analog_input(int device);

#endif //__HSOFT_GPIO_H__
