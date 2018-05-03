#include "mrf24j.h"
#include <avr/sleep.h>
#include <avr/power.h>

/*
  uiGC0
  Goofy Controller 0
  common cathode LEDs +
  Mrf24j40

  Benjamin Jeffery
  University of Idaho
  10/09/2015

*/

int redPin = 3;
int greenPin = 4;
int bluePin = 5;
int pin_reset = 6;
int pin_cs = 8;
int pin_interrupt = 2;
int idVal;
int startId;
volatile int f_timer=0;
boolean receiving = false;
boolean readyToReceive = false;

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

void setup() {
  noInterrupts();
  DDRC = 0x00;
  pinMode(bluePin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);

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
  startId = idVal * 3;
  
  setupSleepTimer();
  sleepNow();
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
  if (idVal == 15)
  {
    setColor(255, 0, 0);
    delay(250);
    setColor(0, 0, 0);
    delay(45);
    setColor(0, 255, 0);
    delay(250);
    setColor(0, 0, 0);
    delay(45);
    setColor(0, 0, 255);
    delay(250);
    setColor(0, 0, 0);
    delay(45);
    setColor(255, 215, 0);
    delay(250);
    setColor(0, 0, 0);
    delay(50);
    setColor(255, 255, 215);
    delay(250);
    setColor(0, 0, 0);
    delay(50);
  }
  else {
    mrf.check_flags(&handle_rx, &handle_tx);
  }
}

void handle_rx()
{
  receiving = true;
  setColor(mrf.get_rxinfo()->rx_data[startId], mrf.get_rxinfo()->rx_data[startId + 1], mrf.get_rxinfo()->rx_data[startId + 2]);
  mrf.rx_flush();
}

void handle_tx()
{
}

void setColor(int red, int green, int blue)
{
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}
