#include <SoftPWM_timer.h>
#include <SoftPWM.h>
#include <SPI.h>

#include "mrf24j.h"
#include <avr/sleep.h>
#include <avr/power.h>

/*
 * Wireless Tower Lights controller
 * wirelessTower1
 * CHANNEL 1
 * LEaD Design (Andrew Butler, Adrian Boehner, Paul Martin, Kevin Dorscher) 2018
*/

// CHANNEL can be 0-3, each channel is a group of 16 pixels
// first 16 pixels use the most significant 4 bits of the first 48 bytes
// second 16 pixels use the most significant 4 bits of the last 48 bytes
// third 16 pixels use the least significant 4 bits of the first 48 bytes
// fourth 16 pixels use the least significant 4 bits of the last 48 bytes
int CHANNEL = 1;
int redPin = 3;
int greenPin = 4;
int bluePin = 5;
int pin_reset = 6;
int pin_cs = 8;
int pin_interrupt = 2;
int idVal;
int startId;
int idShif;
volatile int f_timer = 1;
boolean receiving = false;
int red = 0, green = 0, blue = 0;

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

void setup() {
  noInterrupts();
  DDRC = 0x00;
  SoftPWMBegin();
  SoftPWMSet(redPin, 0);
  SoftPWMSet(greenPin, 0);
  SoftPWMSet(bluePin, 0);
  SoftPWMSetFadeTime(ALL, 0, 0);
  // make sure clock is running normally
  TCCR2B = 0x01;
  TCCR0B = 0x01;

  digitalWrite(A0, HIGH);
  digitalWrite(A1, HIGH);
  digitalWrite(A2, HIGH);
  digitalWrite(A3, HIGH);

  setColor(0, 0, 0);

  mrf.reset();
  mrf.init();
  mrf.set_pan(2015);
  mrf.set_channel(0x0C);
  mrf.address16_write(0x4202);
  mrf.set_promiscuous(true);
  mrf.set_bufferPHY(true);

  idVal = ~PINC & 0x0f;

  // if channel is 1 or 3 we're looking at second half of bit stream
  if (CHANNEL == 1 || CHANNEL == 3) {
    idShif = idVal+16;
  }
  else {
    // otherwise first half
    idShif = idVal;
  }
  startId = idShif * 3;

  attachInterrupt(0, interrupt_routine, CHANGE);
  interrupts();
}

// need to reinitialize after waking
void wakeUp() {
  setupCheckingTimer();
  sleep_disable();
  // re-enable everything we shut off before going to sleep
  power_all_enable();
  attachInterrupt(0, interrupt_routine, CHANGE);
  interrupts();
}

 // interrupt vector for the timer overflow interrupt
ISR(TIMER1_OVF_vect)
{
  noInterrupts();
  // check if waking or not
   if (f_timer == 0) {
    f_timer = 1;
   } else if(!receiving) {
    setupSleepTimer();
    sleepNow();
   } else {
    interrupts();
   }
}

// sets up the sleep timer
void setupSleepTimer() {
  // set counter to be normal
  TCCR1A = 0x00;
  // initialize counter at 0 to maximize time asleep
  TCNT1 = 0x0000;
  // set counter to increment every 1024 clock cycles
  TCCR1B = 0x05;
  // enable overflow interrupt
  TIMSK1 = 0x01;
}

// sets up the timer used to stay awake while checking for signal
void setupCheckingTimer() {
  // set counter to be normal
  TCCR1A = 0x00;
  // intialize counter halfway to maximum to reduce time awake
  TCNT1 = 0x8000;
  // set counter to increment every clock cycle
  TCCR1B = 0x01;
  // enable overflow interrupt
  TIMSK1 = 0x01;
}

// initiates sleep mode
void sleepNow() {
  // detach interrupt
  // interrupts on pin 0 are highest priority which can mess up waking routine
  detachInterrupt(0);
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  // power down unused stuff for additional power savings
  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer2_disable();
  power_twi_disable();

  // indicate we're going to sleep
  f_timer = 0;
  // enable interrupts
  interrupts();
  sleep_mode();

  // after waking
  wakeUp();
}

void interrupt_routine()
{
   mrf.interrupt_handler();
}

void loop()
{
  // wait to make sure there's no transmission before going to sleep
  if (!receiving && millis() > 500) {
    setupSleepTimer();
    sleepNow();
    return;
  }
  mrf.check_flags(&handle_rx, &handle_tx);
}

void handle_rx()
{
  // indicate a transmission has been received, prevents sleep
  receiving = true;
  TIMSK1 = 0;
  if (CHANNEL < 2) {
    // shift most significant 4 bits right and multiply by 17 to scale values to 0-255
    red = (mrf.get_rxinfo()->rx_data[startId] >> 4) * 17;
    green = (mrf.get_rxinfo()->rx_data[startId + 1] >> 4) * 17;
    blue = (mrf.get_rxinfo()->rx_data[startId + 2] >> 4) * 17;
  } else {
    // eliminate most significant bits and multiply by 17 to scale values to 0-255
    red = (mrf.get_rxinfo()->rx_data[startId] & 0b00001111) * 17;
    green = (mrf.get_rxinfo()->rx_data[startId + 1] & 0b00001111) * 17;
    blue = (mrf.get_rxinfo()->rx_data[startId + 2] & 0b00001111) * 17;
  }
  mrf.rx_flush();
  setColor(red, green, blue);
}

void handle_tx()
{
}

void setColor(int red, int green, int blue)
{
  SoftPWMSet(redPin, red);
  SoftPWMSet(greenPin, green);
  SoftPWMSet(bluePin, blue);
}
