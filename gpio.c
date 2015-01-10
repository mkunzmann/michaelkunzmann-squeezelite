
/*
  * gpio_relay.c - example of driving a relay using the GPIO peripheral on a BCM2835 (Raspberry Pi)
  *
  * Copyright 2012 Kevin Sangeelee.
  * Released as GPLv2, see <http://www.gnu.org/licenses/>
  *
  * This is intended as an example of using Raspberry Pi hardware registers to drive a relay using GPIO. Use at your own
   * risk or not at all. As far as possible, I've omitted anything that doesn't relate to the Raspi registers. There are more
  * conventional ways of doing this using kernel drivers.
  */

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "squeezelite.h"

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

void setup_io();

int gpio_state = -1;
int initialized = -1;

void relay( int state) {
    gpio_state = state;

  // Set up gpi pointer for direct register access
  if (initialized == -1){
	setup_io();
	initialized = 1;
	INP_GPIO(18); // must use INP_GPIO before we can use OUT_GPIO
     	OUT_GPIO(18);
//    	INP_GPIO(23); // must use INP_GPIO before we can use OUT_GPIO
//    	OUT_GPIO(23);
//    	GPIO_SET =  1<<23;    //This is spare wire for now.
  }

  // Set GPIO pin 18 to output

    if(gpio_state == 1)
        GPIO_CLR = 1<<18;
    else if(gpio_state == 0)
	GPIO_SET = 1<<18;

    usleep(1);    // Delay to allow any change in state to be reflected in the LEVn, register bit.

//    printf("GPIO 17 is %s\n", (GPLEV0 & BIT_17) ? "high" : "low");

    // Done!
}

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io

