#include <SPI.h>

#include <twrlightsavr.h>
// #include <avr/interrupt.h>

//#include "twrlights.h"

int inByte = 0;   // for incoming serial data

// Buffer info - packet looks like $data%, where data is BUFFSZ long
#define STARTBYTE '$'
#define ENDBYTE '%'
#define BUFFSZ 60
uint8_t inbuff[BUFFSZ];
uint8_t gotfirst;
uint8_t gotlast;
int nbytesread;

#define TEST 1
   // The following is 60 bytes for testing sending integrity
   char testdata[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuzwx";

// TowerLights "standard" SPI pins

#define DATAPIN 6
#define CLOCKPIN 7
#define LATCHPIN 8
#define GOODLED 13
#define BADLED  12
 
void setup() 
  {
        cli(); // Turn off interrupts
        Serial.begin(57600); // opens serial port, sets data rate to 57600 bps
        gotfirst = 0;
        gotlast = 0;
        nbytesread = 0;
        // initialize pins for output to TowerLights hardware
        pinMode(DATAPIN, OUTPUT);
        pinMode(CLOCKPIN, OUTPUT);
        pinMode(LATCHPIN, OUTPUT);
        pinMode(GOODLED, OUTPUT);    // Good LED pin
        pinMode(BADLED, OUTPUT);     // Error LED pin
        digitalWrite(GOODLED, HIGH); // turn on both LEDS to test them for .5 secs
        digitalWrite(BADLED, HIGH);
        delay(500);
        digitalWrite(GOODLED, LOW);  // start with LEDs off
        digitalWrite(BADLED, LOW);
        digitalWrite(LATCHPIN, LOW); // start with 595 latch 0

        // Set up timer 0 for a 2mS interr upt
        

//set timer0 interrupt at 2kHz
  TCCR0A = 0;// set entire TCCR0A register to 0
  TCCR0B = 0;// same for TCCR0B
  TCNT0  = 0;//initialize counter value to 0
  // set compare match register for 2khz increments
  OCR0A = 124;// = (16*10^6) / (2000*64) - 1 (must be <256)
  // turn on CTC mode
  TCCR0A |= (1 << WGM01);
  // Set CS01 and CS00 bits for 256 prescaler
  TCCR0B |= (1 << CS02);   
  // enable timer compare interrupt
  TIMSK0 |= (1 << OCIE0A);

         // OK - ready for interrupts. Turn em on!
        sei();
        
  }  // END setup


void loop() 
  {
        // send data only when you receive data:
        if(Serial.available() > 0) 
          {
                // read the incoming byte:
           inByte = Serial.read();
           if(gotlast)  // set after reading BUFFSZ bytes. we are supposed to
                        // be at end of frame, so reset, and check packet
             {
              // reset buffer so it can accept a new frame of data
              gotfirst = 0;
              gotlast = 0;
              nbytesread = 0;
              digitalWrite(GOODLED, LOW); //Turn off OK LED

              if(inByte == ENDBYTE) // Good! Packet looks good, so display it
                {
                 digitalWrite(BADLED, LOW); // turn off error LED
                 digitalWrite(GOODLED, HIGH);  // turn on OK LED
                 if(TEST)
                   {
                    cvtfromdata12(inbuff); // convert to 8-bit data
                    newframe();                     // display the frame
                   }
                 else
                   {
                    dataOK = 1;
                    for(i =0; i < 60; i++)
                      {
                       if(inbuff[i] != testdata[i]) dataOK = 0;
                      }
                    if(!dataOK)  // something wrong with test data
                      {
                       digitalWrite(BADLED, HIGH); // turn on bad LED to 
                                        // indicate bad data, OK stays on also
                      } 
                   }
                }
              else       // OhOh - something happened. Frame error. Ignore
                         // this packet and turn on error LED
                 digitalWrite(BADLED, HIGH); // turn on error LED
                 
             } // END if gotlast
 
           if(gotfirst)  // we are in the middle of a packet, so store byte
             {
              digitalWrite(GOODLED, HIGH);// turn LED on while receiving packet
              inbuff[nbytesread++] = inByte;
              if(nbytesread >= BUFFSZ) // full buffer
                {
                gotlast = 1; // set flag indicating last data byte has been
                              // received
                }
             }   
           else if(inByte == STARTBYTE)  // start of a new packet
             {
              gotfirst = 1;  // set flag indicating start of new packet
              gotlast = 0;   // should already be clear, but just in case...
             }  // END if gotfirst

          } // END if serial.available()

  } // END loop()
  
/*
 * Common routines for the towerlights project. These routines are used for
 * both the AVR and PC versions. 
 *
 * This test program sends data to all 120 LEDs (15 bytes). The first byte
 * of data is for RED, the next is for GREEN, the next is BLUE.  
 *
 * To set up the board for this test, place a jumper in any position evenly
 * divisible by 3 (0, 3, 6, 9) in the RED column, place a jumper in position
 * (1, 4, 7, 10, etc) in the GREEN column, and place a jumper in position
 * (2, 5, 8, 11, etc) in the BLUE column.
 *
 */

#include <inttypes.h>
// #include <avr/pgmspace.h>
// #include <twrlightsavr.h>

// Types used by towerlights functions
/*
typedef struct RGB RGB;
struct RGB
  {
   uint8_t red;
   uint8_t green;
   uint8_t blue;
  };

typedef union BUFF BUFF;
union BUFF
  {
   struct RGB rgb[40];
   uint8_t linear[120];
  };

*/

// Global values (they are global for efficiency purposes. 

union BUFF inarray;  // input array - this should get filled by uart routines
                     // in AVR code
                     
union BUFF bitarray; // array of bits to send to towerlights board, one bit 
                     // at a time

uint8_t outarray[15][7]; // array holding the actual bits to be sent to 
                         // the towerlights boards. The first subscript 
                         // specifies the byte (120 bits requires 15 bytes),
                         // while the second subscript specifies the PWM
                         // clock cycle.

uint8_t outbits[8] // bits to send each cycle for 1 of 8 brightness
                   // levels. Bits are permuted to even out total
                   // power needed at any one instant
                   = { 0x00,  // dark -     level 0  000 0000
                       0x01,  // brightness level 1  000 0001
                       0x60,  // brightness level 2  110 0000
                       0x07,  // brightness level 3  000 0111
                       0x78,  // brightness level 4  111 1000
                       0x1f,  // brightness level 5  001 1111
                       0x7e,  // brightness level 6  111 1110
                       0x7f}; // brightness level 7  111 1111

// uint8_t cyclecnt = 0;  // counts 1000's of iterations
// int cntr = 0;          // counts each cycle


// Functions that are common to both the "real" (AVR) version and the 
// "simulated" (PC) version of the towerlights software.


/* The following function converts an array of values 0-255 into an array
 * holding bits to send out to the towerlights board. These bits implement
 * 8 levels of intensity for each LED (lower 5 bits of each input byte are
 * ignored). This routine should be called whenever the input data (from
 * outroom or eventually from the uart) changes.
 */

void tobits()
  {
    uint8_t i;
    
    for(i = 0; i < 120; i++)
      {
//     if(inarray.linear[i] != 0)
//     printf("Setting bitarray.linear[%d] to %02x\n", 
//                                      i, outbits[inarray.linear[i] >> 5]);
//     bitarray.linear[i] = outbits[inarray.linear[i] >> 5]; 
       bitarray.linear[i] = outbits[inarray.linear[i]]; 
      }    
  } // END tobits 



/* This function determines the actual bytes that need to be sent to the
 * towerlights boards. It accepts the array of bits to be sent out and the
 * clock cycle number of the PWM (a value 0-6), and assembles the bits to be 
 * sent into fifteen bytes that need to be sent to the towerlights boards. The
 * bytes are arranged in the order: first byte of RED, then first byte of 
 * GREEN, first byte of BLUE, then second byte of RED, etc.
 *
 */

uint8_t bits[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

void makeoutdata (uint8_t clknum)
  {
   uint8_t i,j,k;
   uint8_t mask;

   mask = bits[clknum];

// printf("Makeoutdata: clknum=%d\n", clknum);
   
   for(i = 0, k = 0; i < 15; i+=3, k++)
     {

      j = k * 8;
//    printf("  outarray[%2d]= %02x | %02x | %02x | %02x | %02x | %02x | %02x |%02x\n", i, 
//                   (bitarray.rgb[j+0].red   >> clknum) & 0x01,       
//                  ((bitarray.rgb[j+1].red   >> clknum) & 0x01) << 1, 
//                  ((bitarray.rgb[j+2].red   >> clknum) & 0x01) << 2, 
//                  ((bitarray.rgb[j+3].red   >> clknum) & 0x01) << 3, 
//                  ((bitarray.rgb[j+4].red   >> clknum) & 0x01) << 4, 
//                  ((bitarray.rgb[j+5].red   >> clknum) & 0x01) << 5, 
//                  ((bitarray.rgb[j+6].red   >> clknum) & 0x01) << 6, 
//                  ((bitarray.rgb[j+7].red   >> clknum) & 0x01) << 7);

//    outarray[i] =    (bitarray.rgb[j+0].red   >> clknum) & 0x01       |
//                    ((bitarray.rgb[j+1].red   >> clknum) & 0x01) << 1 |
//                    ((bitarray.rgb[j+2].red   >> clknum) & 0x01) << 2 |
//                    ((bitarray.rgb[j+3].red   >> clknum) & 0x01) << 3 |
//                    ((bitarray.rgb[j+4].red   >> clknum) & 0x01) << 4 |
//                    ((bitarray.rgb[j+5].red   >> clknum) & 0x01) << 5 |
//                    ((bitarray.rgb[j+6].red   >> clknum) & 0x01) << 6 |
//                    ((bitarray.rgb[j+7].red   >> clknum) & 0x01) << 7;
      outarray[i][clknum] =   (bitarray.rgb[j+0].red & mask ? 0x01: 0x00) |
                      (bitarray.rgb[j+1].red & mask ? 0x02: 0x00) | 
                      (bitarray.rgb[j+2].red & mask ? 0x04: 0x00) |
                      (bitarray.rgb[j+3].red & mask ? 0x08: 0x00) |
                      (bitarray.rgb[j+4].red & mask ? 0x10: 0x00) |
                      (bitarray.rgb[j+5].red & mask ? 0x20: 0x00) |
                      (bitarray.rgb[j+6].red & mask ? 0x40: 0x00) |
                      (bitarray.rgb[j+7].red & mask ? 0x80: 0x00);

//    outarray[i+1] =  (bitarray.rgb[j+0].green >> clknum) & 0x01       |
//                    ((bitarray.rgb[j+1].green >> clknum) & 0x01) << 1 |
//                    ((bitarray.rgb[j+2].green >> clknum) & 0x01) << 2 |
//                    ((bitarray.rgb[j+3].green >> clknum) & 0x01) << 3 |
//                    ((bitarray.rgb[j+4].green >> clknum) & 0x01) << 4 |
//                    ((bitarray.rgb[j+5].green >> clknum) & 0x01) << 5 |
//                    ((bitarray.rgb[j+6].green >> clknum) & 0x01) << 6 |
//                    ((bitarray.rgb[j+7].green >> clknum) & 0x01) << 7;

      outarray[i+1][clknum] = (bitarray.rgb[j+0].green & mask ? 0x01: 0x00) |
                      (bitarray.rgb[j+1].green & mask ? 0x02: 0x00) | 
                      (bitarray.rgb[j+2].green & mask ? 0x04: 0x00) |
                      (bitarray.rgb[j+3].green & mask ? 0x08: 0x00) |
                      (bitarray.rgb[j+4].green & mask ? 0x10: 0x00) |
                      (bitarray.rgb[j+5].green & mask ? 0x20: 0x00) |
                      (bitarray.rgb[j+6].green & mask ? 0x40: 0x00) |
                      (bitarray.rgb[j+7].green & mask ? 0x80: 0x00);

//    outarray[i+2] =  (bitarray.rgb[j+0].blue  >> clknum) & 0x01       |
//                    ((bitarray.rgb[j+1].blue  >> clknum) & 0x01) << 1 |
//                    ((bitarray.rgb[j+2].blue  >> clknum) & 0x01) << 2 |
//                    ((bitarray.rgb[j+3].blue  >> clknum) & 0x01) << 3 |
//                    ((bitarray.rgb[j+4].blue  >> clknum) & 0x01) << 4 |
//                    ((bitarray.rgb[j+5].blue  >> clknum) & 0x01) << 5 |
//                    ((bitarray.rgb[j+6].blue  >> clknum) & 0x01) << 6 |
//                    ((bitarray.rgb[j+7].blue  >> clknum) & 0x01) << 7;
      outarray[i+2][clknum] = (bitarray.rgb[j+0].blue & mask ? 0x01: 0x00) |
                      (bitarray.rgb[j+1].blue & mask ? 0x02: 0x00) |
                      (bitarray.rgb[j+2].blue & mask ? 0x04: 0x00) |
                      (bitarray.rgb[j+3].blue & mask ? 0x08: 0x00) |
                      (bitarray.rgb[j+4].blue & mask ? 0x10: 0x00) |
                      (bitarray.rgb[j+5].blue & mask ? 0x20: 0x00) |
                      (bitarray.rgb[j+6].blue & mask ? 0x40: 0x00) |
                      (bitarray.rgb[j+7].blue & mask ? 0x80: 0x00);


     }

//   printf("Makeoutdata - done\n");

  } // END makeoutdata


/* This routine determines all of the LED control bits that will need to
 * be output whenever a new frame is received. It should be called whenever
 * a new frame comes in from the uart or from the test routine outroom.
 */
 
void newframe()
   {
    uint8_t i;

    tobits();    

    for(i = 0; i < 7; i++)
       makeoutdata(i);
   } // END newframe
 

/* This routine outputs one complete PWM cycle. It goes through 7 times to 
 * complete one full cycle of data. 
 */

void docycle()
  {
   uint8_t i;

   for(i = 0; i < 7; i++)
     {
//      makeoutdata(i);
      outdata(i);
     }
 
  } // END docycle


/* This routine is used during testing to set values for each room. Normally,
 * each room's color and intensity is determined by the packet coming in on
 * the serial port of the AVR. However, this function is used during testing
 * to do the equivalent operation, on a room by room basis.
 */

void outroom( uint8_t rmno, uint8_t red, uint8_t green, uint8_t blue)
  {
   inarray.rgb[rmno].red   = red;
   inarray.rgb[rmno].green = green;
   inarray.rgb[rmno].blue  = blue;

  } // END outroom

/*
 * This routine converts from the two values per byte format used to transmit data across
 * the xbees to the three values per byte (RGB) used by the rest of the system.
 */

void cvtfromdata12( uint8_t inbuff[])
  {
   // Transmit data format is 01rrrggg 01bbbrrr 01gggbbb
   // Two (RGB) pixels are sent in 2 bytes, then unpacked to three bytes
   
   int i, j;

   for(i = 0, j = 0; i < 60; i+=3, j+=2)
     {
      inarray.rgb[j].red     = inbuff[i]   >> 3 & 0x07;
      inarray.rgb[j].green   = inbuff[i]        & 0x07;
      inarray.rgb[j].blue    = inbuff[i+1] >> 3 & 0x07;
      inarray.rgb[j+1].red   = inbuff[i+1]      & 0x07;
      inarray.rgb[j+1].green = inbuff[i+2] >> 3 & 0x07;
      inarray.rgb[j+1].blue  = inbuff[i+2]      & 0x07;
     } // END for

  }  // END cvtfromdata12


/* 
 *  * Arduino controller - the Arduino doesn't have enough pins to control
 *   * the old-style boards directly, so it outputs to three 595 chips instead
 *    */

void outdata(uint8_t clknum)
   {
    uint8_t select;
    uint8_t buffer[3];
    uint8_t i;

#define MSBFIRST 0
    select = 1;
    for(i = 0; i < 8; i++)
      {
       digitalWrite(LATCHPIN, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, outarray[i][clknum]); // Data byte
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, select); // toggle select bit
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0);
       digitalWrite(LATCHPIN, 1);
       digitalWrite(LATCHPIN, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, outarray[i][clknum]); // Data byte
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0); // select bit back to zero
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0);
       digitalWrite(LATCHPIN, 1);
       select = select << 1;
      }

    select = 1;
    for(i = 8; i < 15; i++)
      {
       digitalWrite(LATCHPIN, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, outarray[i][clknum]); // Data byte
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, select); // toggle select bit
       digitalWrite(LATCHPIN, 1);
       digitalWrite(LATCHPIN, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, outarray[i][clknum]); // Data byte
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0);
       shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0); // select bit back to zero
       digitalWrite(LATCHPIN, 1);
       select = select << 1;
      }
   } // END outdata


