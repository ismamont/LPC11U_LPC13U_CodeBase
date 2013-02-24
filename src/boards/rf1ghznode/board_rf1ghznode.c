/**************************************************************************/
/*!
    @file     board_rf1ghznode.c
    @author   K. Townsend (microBuilder.eu)

    @section DESCRIPTION

    Common, board-specific files for the AT86RF2xx wireless boards

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2012, K. Townsend
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#include "projectconfig.h"

#if defined CFG_BRD_RF1GHZNODE

#include "boards/board.h"
#include "board_rf1ghznode.h"
#include "core/gpio/gpio.h"
#include "core/systick/systick.h"
#include "core/eeprom/eeprom.h"
#include "core/pmu/pmu.h"
#include "core/adc/adc.h"

#ifdef CFG_CHIBI
  #include "drivers/rf/chibi/chb.h"
  #include "drivers/rf/chibi/chb_drvr.h"
#endif

#ifdef CFG_USB
  #include "core/usb/usbd.h"
  #ifdef CFG_USB_CDC
    #include "core/usb/usb_cdc.h"
  #endif
#endif

#ifdef CFG_TFTLCD
  #include "drivers/displays/graphic/lcd.h"
#endif

#ifdef CFG_SDCARD
  #include "drivers/storage/fatfs/diskio.h"
  #include "drivers/storage/fatfs/ff.h"
#endif

#ifdef CFG_INTERFACE
  #include "cli/cli.h"
#endif

#ifdef CFG_RTC
  #include "drivers/rtc/pcf2129/pcf2129.h"
#endif

#define PINS_VREGVSEL_PORT      (1)
#define PINS_VREGVSEL_PIN       (16)
#define PINS_VINADC_EN_PORT     (0)
#define PINS_VINADC_EN_PIN      (20)
#define PINS_VINADC_INPUT_PORT  (0)
#define PINS_VINADC_INPUT_PIN   (11)
#define PINS_I2C_PULLUPS_PORT   (0)
#define PINS_I2C_PULLUPS_PIN    (23)

/* VIN resistor divider multplier is 3.12766 */
#define RF1GHZNODE_VINADC_MULTIPLIER_FIXED10K (31277)

/**************************************************************************/
/*!
    @brief Board-specific initialisation function
*/
/**************************************************************************/
void boardInit(void)
{
  SystemCoreClockUpdate();
  systickInit(CFG_SYSTICK_DELAY_IN_MS);
  GPIOInit();

  #ifdef CFG_PRINTF_UART
    uartInit(CFG_UART_BAUDRATE);
  #endif

  /* Power down unused blocks */
  uint32_t pdrunconfig = ((0x1<<0)  |  // IRCOUT powered down
                          (0x1<<1)  |  // IRC powered down
                          (0x0<<2)  |  // FLASH powered up
                          (0x1<<3)  |  // BOD powered down
                          (0x1<<4)  |  // ADC powered down
                          (0x0<<5)  |  // SYSOSC powered up
                          (0x1<<6)  |  // WDTOSC powered down
                          (0x0<<7)  |  // SYSPLL powered up
                          (0x0<<8)  |  // USBPLL powered up
                          (0x0<<9)  |  // RESERVED ... set as 0
                          (0x0<<10) |  // USBPAD powered up
                          (0x1<<11) |  // RESERVED ... set as 1
                          (0x0<<12) |  // RESERVED ... set as 0
                          (0x0<<13) |  // RESERVED ... set as 1
                          (0x0<<14) |  // RESERVED ... set as 1
                          (0x0<<15));  // RESERVED ... set as 1
  LPC_SYSCON->PDRUNCFG = pdrunconfig;

  /* Explicitly set all pin functions to a known state and disable any
     unnecessary pull-ups to save some power.  It's important to setup
     unbonded pins as well since they are still available on the die,
     and can save some power setting them to output low, and disabling
     their internal resistors (this cut ~350µA off the sleep current
     in initial testing)

  Pin                       Pin Config Bits               Selected Function     Notes
  =======================   ==========================    ===================   ============== */
  LPC_IOCON->RESET_PIO0_0   = (0<<0) | (0<<3);            // RESET              no pull-up/down
  LPC_IOCON->PIO0_1         = (0<<0) | (2<<3);            // GPIO               pull-up (ISP)
  LPC_IOCON->PIO0_2         = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_3         = (1<<0) | (0<<3);            // USBVBUS            no pull-up/down
  LPC_IOCON->PIO0_4         = (1<<0);                     // I2C-SCL            I2C = standard mode
  LPC_IOCON->PIO0_5         = (1<<0);                     // I2C-SDA            I2C = standard mode
  LPC_IOCON->PIO0_6         = (1<<0) | (0<<3);            // USBConnect         no pull-up/down
  LPC_IOCON->PIO0_7         = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_8         = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_9         = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->SWCLK_PIO0_10  = (0<<0) | (2<<3);            // SWCLK              pull-up (SWCLK)
  LPC_IOCON->TDI_PIO0_11    = (1<<0) | (0<<3) | (1<<7);   // GPIO (not TDI)     no pull-up/down, ADMODE = digital
  LPC_IOCON->TMS_PIO0_12    = (1<<0) | (0<<3) | (1<<7);   // GPIO (not TMS)     no pull-up/down, ADMODE = digital
  LPC_IOCON->TDO_PIO0_13    = (1<<0) | (0<<3) | (1<<7);   // GPIO (not TDO)     no pull-up/down, ADMODE = digital
  LPC_IOCON->TRST_PIO0_14   = (1<<0) | (0<<3) | (1<<7);   // GPIO (not TRST)    no pull-up/down, ADMODE = digital
  LPC_IOCON->SWDIO_PIO0_15  = (0<<0) | (2<<3) | (1<<7);   // SWDIO              pull-up (SWDIO), ADMODE = digital
  LPC_IOCON->PIO0_16        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital
  LPC_IOCON->PIO0_17        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  #if defined CFG_PRINTF_UART
  LPC_IOCON->PIO0_18        = (1<<0) | (0<<3);            // RXD                no pull-up/down
  LPC_IOCON->PIO0_19        = (1<<0) | (0<<3);            // TXD                no pull-up/down
  #else
  LPC_IOCON->PIO0_18        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_19        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  #endif
  LPC_IOCON->PIO0_20        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_21        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO0_22        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital
  LPC_IOCON->PIO0_23        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital

  LPC_IOCON->PIO1_0         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_1         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_2         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_3         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_4         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_5         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_6         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_7         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_8         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_9         = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_10        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_11        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_12        = (0<<0) | (0<<3) | (1<<7);   // GPIO               no pull-up/down, ADMODE = digital (pin not present on QFP48)
  LPC_IOCON->PIO1_13        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_14        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_15        = (0<<0) | (2<<3);            // GPIO               pull-up (SLP_TR)
  LPC_IOCON->PIO1_16        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_17        = (0<<0) | (0<<3);            // GPIO               no pull-up/down (pin not present on QFP48)
  LPC_IOCON->PIO1_18        = (0<<0) | (0<<3);            // GPIO               no pull-up/down (pin not present on QFP48)
  LPC_IOCON->PIO1_19        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_20        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_21        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_22        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_23        = (0<<0) | (2<<3);            // GPIO               pull-up (SD_DETECT)
  LPC_IOCON->PIO1_24        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_25        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_26        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_27        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_28        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_29        = (0<<0) | (0<<3);            // GPIO               no pull-up/down
  LPC_IOCON->PIO1_30        = (0<<0) | (0<<3);            // GPIO               no pull-up/down (pin not present on QFP48)
  LPC_IOCON->PIO1_31        = (0<<0) | (0<<3);            // GPIO               no pull-up/down

  /* Set pins to output where possible */
  LPC_GPIO->DIR[0] = ~( (1<<1) );         /* 0.1  = ISP (used as wakeup pin) */
  LPC_GPIO->DIR[1] = 0xFFFFFFFF;

  /* Set user LED pin to output and disable it */
  GPIOSetDir(CFG_LED_PORT, CFG_LED_PIN, 1);
  boardLED(CFG_LED_OFF);

  /* Enable I2C pullups by default */
  GPIOSetDir(PINS_I2C_PULLUPS_PORT, PINS_I2C_PULLUPS_PIN, 1);
  LPC_GPIO->CLR[PINS_I2C_PULLUPS_PORT] = (1 << PINS_I2C_PULLUPS_PIN);

  /* Set VREG voltage selection pin to output and switch to 3.3V */
  GPIOSetDir(PINS_VREGVSEL_PORT, PINS_VREGVSEL_PIN, 1);
  boardSetVREG3V3();

  /* Turn the SD card off by default */
  GPIOSetDir(CFG_SDCARD_ENBLPORT, CFG_SDCARD_ENBLPIN, 1);
  LPC_GPIO->SET[CFG_SDCARD_ENBLPORT] = (1 << CFG_SDCARD_ENBLPIN);

  /* Turn the VIN ADC off by default */
  GPIOSetDir(PINS_VINADC_EN_PORT, PINS_VINADC_EN_PIN, 1);
  LPC_GPIO->SET[PINS_VINADC_EN_PORT] = (1 << PINS_VINADC_EN_PIN);
  GPIOSetDir(PINS_VINADC_INPUT_PORT, PINS_VINADC_INPUT_PIN, 1);
  LPC_GPIO->CLR[PINS_VINADC_INPUT_PORT] = (1 << PINS_VINADC_INPUT_PIN);

  /* Start Chibi */
  #ifdef CFG_CHIBI
    /* You may need to write a new address to EEPROM if it doesn't exist */
    // uint16_t nodeaddr = 0xDADA;
    // uint64_t ieeeaddr = 0x123456780000DADA;
    // writeEEPROM((uint8_t*)CFG_EEPROM_CHIBI_NODEADDR, (uint8_t*)&nodeaddr, sizeof(nodeaddr));
    // writeEEPROM((uint8_t*)CFG_EEPROM_CHIBI_IEEEADDR, (uint8_t*)&ieeeaddr, sizeof(ieeeaddr));
    chb_init();
  #endif

  /* Initialise the SD Card? */
  #ifdef CFG_SDCARD
    // DSTATUS stat = disk_initialize(0);
  #endif

  /* Initialise USB */
  #ifdef CFG_USB
    systickDelay(500);
    usb_init();
  #endif

  /* Initialise the LCD */
  #ifdef CFG_TFTLCD
    lcdInit();
  #endif

  /* Initialise the RTC */
  #ifdef CFG_RTC
    /* ToDo: check if this throws an error! */
    pcf2129Init();
  #endif

  /* Start the command line interface */
  #ifdef CFG_INTERFACE
    cliInit();
  #endif

  /* Turn the user LED on after init to indicate that everything is OK */
  boardLED(CFG_LED_ON);
}

/**************************************************************************/
/*!
    @brief Turns the LED(s) on or off
*/
/**************************************************************************/
void boardLED(uint8_t state)
{
  if (state)
  {
    LPC_GPIO->SET[CFG_LED_PORT] = (1 << CFG_LED_PIN);
  }
  else
  {
    LPC_GPIO->CLR[CFG_LED_PORT] = (1 << CFG_LED_PIN);
  }
}

/**************************************************************************/
/*!
    @brief  Configure the board for low power and enter sleep mode
*/
/**************************************************************************/
void boardSleep(void)
{
  /* Configure ISP switch as a wakeup source (pin 0.1)  */
  LPC_IOCON->PIO0_1 = (0<<0) | (2<<3);
  GPIOSetDir(0, 1, 0);
  GPIOSetPinInterrupt(CHANNEL0, 0, 1, 0, 0);
  LPC_SYSCON->STARTERP0 = 0x1<<0;

  #ifdef CFG_CHIBI
    /* What about leaving the AT86RF212 awake, and setting up the the IRQ
       pin to wake the MCU up on incoming message? */
    chb_sleep(TRUE);
  #endif

  #ifdef CFG_SDCARD
    /* Note: The pins on the SD card will cause phantom current
       to be drawn through to the SD card, and need to be set
       to GPIO output and low to prevent this from happening.
       Can save up to 1mA of current when a card is inserted. */
    GPIOSetDir(CFG_SDCARD_ENBLPORT, CFG_SDCARD_ENBLPIN, 1);           // SD_EN
    LPC_GPIO->SET[CFG_SDCARD_ENBLPORT] = (1 << CFG_SDCARD_ENBLPIN);
    GPIOSetDir(CFG_SDCARD_CDPORT, CFG_SDCARD_CDPIN, 1);               // SD_DETECT
    LPC_GPIO->CLR[CFG_SDCARD_CDPORT] = (1 << CFG_SDCARD_CDPIN);
    GPIOSetDir(CFG_SDCARD_SSELPORT, CFG_SDCARD_SSELPIN, 1);           // SD_CS
    LPC_GPIO->CLR[CFG_SDCARD_SSELPORT] = (1 << CFG_SDCARD_SSELPIN);
    LPC_IOCON->PIO1_20 = (0<<0) | (0<<3);                             // SCK1  -> GPIO
    LPC_IOCON->PIO1_21 = (0<<0) | (0<<3);                             // MISO1 -> GPIO
    LPC_IOCON->PIO1_22 = (0<<0) | (0<<3);                             // MOSI1 -> GPIO
    GPIOSetDir(1, 20, 1);
    LPC_GPIO->CLR[1] = (1 << 20);
    GPIOSetDir(1, 21, 1);
    LPC_GPIO->CLR[1] = (1 << 21);
    GPIOSetDir(1, 22, 1);
    LPC_GPIO->CLR[1] = (1 << 22);
  #endif

  /* Turn the VIN voltage divider off */
  LPC_GPIO->SET[PINS_VINADC_EN_PORT] = (1 << PINS_VINADC_EN_PIN);
  GPIOSetDir(PINS_VINADC_INPUT_PORT, PINS_VINADC_INPUT_PIN, 1);
  LPC_GPIO->CLR[PINS_VINADC_INPUT_PORT] = (1 << PINS_VINADC_INPUT_PIN);

  /* Disable I2C pullups */
  LPC_GPIO->SET[PINS_I2C_PULLUPS_PORT] = (1 << PINS_I2C_PULLUPS_PIN);

  /* Turn the user LED off */
  boardLED(CFG_LED_OFF);

  /* Set SWCLK to GPIO (comment out to maintain SWD connection) */
  LPC_IOCON->SWCLK_PIO0_10 = (1<<0) | (0<<3);
  GPIOSetDir(0, 10, 1);
  LPC_GPIO->CLR[0] = (1 << 10);

  /* Set SWDIO to GPIO (comment out to maintain SWD connection) */
  LPC_IOCON->SWDIO_PIO0_15 = (1<<0) | (0<<3) | (1<<7);
  GPIOSetDir(0, 15, 1);
  LPC_GPIO->CLR[0] = (1 << 15);

  /* Set USBConnect to GPIO output low */
  LPC_IOCON->PIO0_6 = (0<<0) | (0<<3);
  GPIOSetDir(0, 6, 1);
  LPC_GPIO->CLR[0] = (1 << 6);

  /* Switch to 2.2V supply (less leakage current, etc.) */
  boardSetVREG2V2();

  /* Enter power down mode (Disable BOD and WDT oscillator) */
  PMU_Sleep(MCU_POWER_DOWN, BOD_PD | WDT_OSC_PD);

  /* Reconfigure the board post-wakeup */
  boardWakeup();
}

/**************************************************************************/
/*!
    @brief  Restores parts and system peripherals to an appropriate
            state after waking up from sleep mode
*/
/**************************************************************************/
void boardWakeup(void)
{
  /* Set power back to main voltage if dual output supply is being used */
  boardSetVREG3V3();

  /* Switch I2C pullups back on */
  LPC_GPIO->CLR[PINS_I2C_PULLUPS_PORT] = (1 << PINS_I2C_PULLUPS_PIN);

  /* ToDo: Reconfigure SPI pins or reinitialize SPI, etc.? */

  #ifdef CFG_CHIBI
    /* Wakeup Chibi/Transceiver */
    chb_sleep(FALSE);
  #endif
}

/**************************************************************************/
/*!
    @brief Sets the output of the TPS780 to 3.3V
*/
/**************************************************************************/
void boardSetVREG3V3(void)
{
  LPC_GPIO->CLR[PINS_VREGVSEL_PORT] = (1 << PINS_VREGVSEL_PIN);
}

/**************************************************************************/
/*!
    @brief Sets the output of the TPS780 to 2.2V
*/
/**************************************************************************/
void boardSetVREG2V2(void)
{
  LPC_GPIO->SET[PINS_VREGVSEL_PORT] = (1 << PINS_VREGVSEL_PIN);
}

/**************************************************************************/
/*!
    @brief Reads the current VIN level using the on-chip ADC

    @returns A fixed point voltage level where 3176 = 3.176V
*/
/**************************************************************************/
uint32_t boardGetVIN(void)
{
  uint32_t vraw, vcomp, vref, i;

  /* Indicate the ADC range */
  #if ADC_MODE_10BIT
  uint32_t adcRange = 1024;
  #else
  uint32_t adcRange = 4096;
  #endif

  /* Delay ADC init until now to save power */
  adcInit();

  /* Enable the VIN ADC resistor-divider and setup the ADC pin */
  LPC_GPIO->CLR[PINS_VINADC_EN_PORT] = (1 << PINS_VINADC_EN_PIN);
  LPC_IOCON->TDI_PIO0_11 = (2<<0) | (0<<7);

  /* We need a small delay for everything to stabilise with the FET + R/D */
  /* ToDo: Check this on a scope to see how long stabilisation takes */
  for (i=0;i<1000;i++)
  {
    __NOP();
  }

  /* Get the raw ADC value taking into account range, adc ref, and r/d multiplier */
  vref = GPIOGetPinValue(PINS_VREGVSEL_PORT, PINS_VREGVSEL_PIN) ? 2200 : 3300;
  vraw = (((adcRead(0) * 1000) / adcRange) * vref) / 1000;
  vcomp = (vraw * RF1GHZNODE_VINADC_MULTIPLIER_FIXED10K) / 10000;

  /* Turn everything back off to save power */
  LPC_GPIO->SET[PINS_VINADC_EN_PORT] = (1 << PINS_VINADC_EN_PIN);
  GPIOSetDir(PINS_VINADC_INPUT_PORT, PINS_VINADC_INPUT_PIN, 1);
  LPC_GPIO->CLR[PINS_VINADC_INPUT_PORT] = (1 << PINS_VINADC_INPUT_PIN);
  LPC_IOCON->TDI_PIO0_11 = (1<<0) | (0<<3) | (1<<7);

  /* Return fixed point value (3157 = 3.157V) */
  return vcomp;
}

#endif
