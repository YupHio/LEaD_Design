// Towerplayer for WIRELESS tower lights project -P

// (fixed -P) Compile:  g++ towerplayer.cpp yswavfile.cpp -I. -lpulse -lftdi

// (fixed -P) Usage:    ./a.out <wavefile.wav> <tanfile.tan>



// (old) g++ pulse_async_test.cpp yswavfile.cpp -I. -lpulse
// (old) Usage: ./a.out <wavefile.wav>

// This program plays a WAV file

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <cstring>

#include <pulse/mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/timeval.h>
#include <pulse/error.h>

#include <yswavfile.h>

#include <ftdi.h>

struct RGB
{
        uint8_t red;
        uint8_t green;
        uint8_t blue;
};

struct tannode
{
        int startus;     // microsecond time stamp when this frame starts
        RGB rgb[40];     // raw frame data
        uint16_t data16[80];
        uint8_t data8[40];
        uint8_t data12[60];
        struct tannode *next;
};

uint8_t *Convert_To_Individual_RGB32(tannode *tanll);
tannode *readtan(char tanfilename[]);   //  reads a tan file into a linked list
void writetan(char tanfilename[], tannode *tanlli, char vnum, int dummy[]); // writes a tan file from a llist
void printtan(tannode *tanll);
int cvt2us(char timestring[]);
tannode *getv2frame(FILE *in);
tannode *getv3frame(FILE *in);
void cvt2data16(tannode *p);
void cvt2data12(tannode *p);
void cvt2data8(tannode *p);
void printdata16(tannode *tanll);
void printdata12(tannode *tanll);
void printdata8(tannode *tanll);
void printtanstats(tannode *tanll);
void cvtfromdata12(tannode *tanll);
void printtime(int time, char str[]);
const int MINTIME = 50000; // minimum allowable time between frames

int xbee_open(struct ftdi_context *ftdic);

static size_t getEncodedBufferSize(size_t sourceSize)
{
        size_t s;
        s = sourceSize + sourceSize / 254 + 1;
        printf("buffer size is : %zd.\n", s);
        return s;
}

static YSRESULT YsPulseAudioWaitForConnectionEstablished(pa_context *paContext,pa_mainloop *paMainLoop,time_t timeOut)
{
        time_t timeLimit=time(NULL)+timeOut;
        while(timeLimit>=time(NULL))
        {
                pa_mainloop_iterate(paMainLoop,0,NULL);
                if(PA_CONTEXT_READY==pa_context_get_state(paContext))
                {
                        return YSOK;
                }
        }
        return YSERR;
}

int main(int ac,char *av[])
{
        int loopcount = 0;
        int oldloopcnt = 0;
        int countsincelast;
        tannode *tanll, *p;
        int ans;
        char tanfilename[50];
        // The following is 60 bytes for testing sending integrity
        char testdata[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuzwx";
        // turn on the following flag to do testing
#define TEST 0

        int ret;
        struct ftdi_context ftdic;
        int xbee_stat;

        int nbytes, bufcnt;
        unsigned char buf[80];
        int i,j=0;

        // Open wav file
        YsWavFile wavFile;
        if(2>ac || YSOK!=wavFile.LoadWav(av[1]))
        {
                fprintf(stderr,"Cannot open wave file.\n");
                return 1;
        }

        // Open tan file
        tanll = readtan(av[2]);
        if(tanll == NULL) return 1;
        p = tanll;    // used to tranverse tan file linked list

        // Open usb (xbee) port
        xbee_stat = xbee_open(&ftdic);
        if(xbee_stat < 0) return EXIT_FAILURE;
        //xbee_stat = ftdi_write_data_set_chunksize(&ftdic, 60); // set chunk (buffer) size

        // Set up pulse audio loop and context
        pa_mainloop *paMainLoop=pa_mainloop_new(); // Pennsylvania Main loop?
        if(NULL==paMainLoop)
        {
                fprintf(stderr,"Cannot create main loop.\n");
                return 1;
        }

        pa_context *paContext=pa_context_new(pa_mainloop_get_api(paMainLoop),"YsPulseAudioCon");
        if(NULL==paContext)
        {
                fprintf(stderr,"Cannot create context.\n");
                return 1;
        }

        printf("Mainloop and Context Created.\n");

        // pa_context_set_state_callback(paContext,YsPulseAudioConnectionCallBack,NULL);
        pa_context_connect(paContext,NULL,(pa_context_flags_t)0,NULL);

        // I seem to be able to either wait for call back or poll context state until it is ready.
        YsPulseAudioWaitForConnectionEstablished(paContext,paMainLoop,5);

        printf("I hope it is connected.\n");




        pa_sample_format_t format;
        switch(wavFile.BitPerSample())
        {
        case 8:
                if(YSTRUE==wavFile.IsSigned())
                {
                        wavFile.ConvertToUnsigned();
                }
                format=PA_SAMPLE_U8;
                break;
        case 16:
                format=PA_SAMPLE_S16LE;
                break;
        }
        const int rate=wavFile.PlayBackRate();
        const int nChannel=(YSTRUE==wavFile.Stereo() ? 2 : 1);

        const pa_sample_spec ss = {
                format,
                rate,
                nChannel
        };

        pa_stream *paStream=pa_stream_new(paContext,"YsStream",&ss,NULL);

        if(NULL!=paStream)
        {
                printf("Stream created!  Getting there!\n");
        }

        pa_stream_connect_playback(paStream,NULL,NULL,(pa_stream_flags_t)0,NULL,NULL);



        printf("Entering main loop.\n");

        unsigned int playBackPtr=0;
        YSBOOL checkForUnderflow=YSTRUE;

        struct timeval start, now;
        pa_usec_t elaptime;
        const time_t t0=time(NULL);
        time_t prevT=time(NULL)-1;
        gettimeofday(&start, NULL);

        for(;; )
        {
                gettimeofday(&now, NULL);
                elaptime = pa_timeval_diff(&now, &start);

                if(p != NULL) //not at end of tan file yet
                        if(p->startus <= elaptime) // time to send another frame to xbee
                        {

                                /*bufcnt = 0;
                                   buf[bufcnt++] = '$';  // Form transmission packet - start with '$'
                                   for(i = 0; i < 60; i++)  // copy frame into buffer
                                   if(TEST) buf[bufcnt++] = testdata[i];
                                   else     buf[bufcnt++] = p->data12[i];
                                   j++;
                                   if(TEST) // Introduce error every tenth packet, for testing
                                   if(j%10 == 0) buf[3] = '4';
                                   buf[bufcnt++] = '%';  // end of packet*/
                                uint8_t * thisFrame = Convert_To_Individual_RGB32(p);
                                size_t m = 96;
                                nbytes = ftdi_write_data(&ftdic, thisFrame, m);
                                if(nbytes < 0)
                                {
                                        fprintf(stderr, "Error writing bytes to xbee - error code %d\n", nbytes);
                                        return EXIT_FAILURE;
                                }
                                //else
                                //  {
                                //   printf("%d bytes written %d\r", nbytes, i);
                                //   fflush(stdout);
                                //  }
                                p = p->next; // advance to next frame for next time around
                        } // END if


                if(time(NULL)!=prevT)
                {
                        countsincelast = loopcount-oldloopcnt;
                        //printf("Ping...%d %d %d\n", elaptime, loopcount, countsincelast);
                        printf("Ping...%d\r", elaptime);
                        fflush(stdout);
                        prevT=time(NULL);
                        oldloopcnt = loopcount;
                }

                if(PA_STREAM_READY==pa_stream_get_state(paStream))
                {
                        const size_t writableSize=pa_stream_writable_size(paStream);
                        const size_t sizeRemain=wavFile.SizeInByte()-playBackPtr;
                        const size_t writeSize=(sizeRemain<writableSize ? sizeRemain : writableSize);

                        if(0<writeSize)
                        {
                                loopcount++;
                                //printf("Write %d\n",writeSize);
                                pa_stream_write(paStream,wavFile.DataPointer()+playBackPtr,writeSize,NULL,0,PA_SEEK_RELATIVE);
                                playBackPtr+=writeSize;
                        }
                }

                if(wavFile.SizeInByte()<=playBackPtr &&
                   0<=pa_stream_get_underflow_index(paStream) &&
                   YSTRUE==checkForUnderflow)
                {
                        printf("Underflow detected. (Probably the playback is done.)\n");
                        checkForUnderflow=YSFALSE;
                        break;
                }

                pa_mainloop_iterate(paMainLoop,0,NULL);

//		if(t0+50<=time(NULL)) // This code stops after 50 secs
//		{
//			break;
//		}
//                if(checkForUnderflow == YSFALSE) // is this really the best way to stop?
//                  {
//                     break;
//                  }
        }

        pa_stream_disconnect(paStream);
        pa_stream_unref(paStream);
        pa_context_disconnect(paContext);
        pa_context_unref(paContext);
        pa_mainloop_free(paMainLoop);

        if ((ret = ftdi_usb_close(&ftdic)) < 0)
        {
                fprintf(stderr, "unable to close ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
                return EXIT_FAILURE;
        }
        ftdi_deinit(&ftdic);

        printf("End of program.\n");

        return 0;
} // END main


/*
   int main()
   {
   tannode *tanll;
   int ans;
   char tanfilename[50];

   printf("Tan file test program\n");
   printf("Enter a tan file name: ");
   scanf("%s", tanfilename);

   tanll = readtan(tanfilename);
   if(tanll == NULL) return -1;

   printf("Tan file input: what next?\n");
   printf("  1- print rgb\n  2- print 16\n  3- print 12 \n  4- print 8\n  5- print tan stats\n");
   printf("Enter choice :");
   scanf("%d", &ans);
   switch(ans)
     {
      case 1:  printtan(tanll);
               break;
      case 2:  printdata16(tanll);
               break;
      case 3:  printdata12(tanll);
               break;
      case 4:  printdata8(tanll);
               break;
      case 5:  printtanstats(tanll);
               break;
     }

   } // END main
 */

/*
 * Input: tannode struct
 * Output: Pointer to a 96 element uint8_t array
 */
uint8_t* Convert_To_Individual_RGB32(tannode *tanll)
{
        tannode *p;
        int i;
        int j;

        // Array of 96 individual RGB values (from 1st 32 RGB structs with 3 rgb values)
        uint8_t *indivRBG32 = new uint8_t[96];

        p = tanll;

        // 40 total, but only wanting 32 RGB structs
        for(i = 0; i < 32; i++)
        {
                j = i*3;
                indivRBG32[j] = p->rgb[i].red;
                indivRBG32[j+1] = p->rgb[i].green;
                indivRBG32[j+2] = p->rgb[i].blue;
        }
        int x;
        return indivRBG32;
}

tannode *readtan(char tanfilename[])
{
        // inputs a tan file, creates a linked list of the frames

        FILE *in;
        char version[10]; // holds version number
        char vnum;    // holds minor version number
        int dummy[117];
        char timestr[20];
        char tanoutname[] = "testfile.tan";
        int nframes, row, col;
        int changed, passnum, timediff;

        int i;


        tannode *tanll = NULL, *p, *last;
        int debug = 0;

        in = fopen(tanfilename, "r");

        if(in == NULL)
        {
                printf("Something happened - tan file didn't open\n");
                return (tannode *)NULL;
        }

        fscanf(in, "%s", version); // get version number: "0.2" is BW version
                                   //                     "0.3" is Color
        if(debug) printf("Version is %s\n", version);

        vnum = version[2];
        if(vnum != '2' && vnum != '3') // something wrong with version number
        {
                printf("Illegal version number in tan file - bye!\n");
                return (tannode *)NULL;
        }

// For 0.3, next thing is color palette info. We will ignore here.

        if(vnum == '3')
        {
                for(i = 0; i < 9; i++) // current colors
                        fscanf(in, "%d", &dummy[i]);
                for(i = 9; i < 117; i++) // color palette - 36 colors, each with rgb value
                        fscanf(in, "%d", &dummy[i]);
        }


// for all versions, next thing is number of frames, followed by row x col

        fscanf(in, "%d %d %d", &nframes, &row, &col);
        if(debug) printf("nframes = %d, rows = %d, cols = %d\n", nframes, row, col);

// Now start reading frames.
//   For 0.2, time is in secs since last frame, followed by 40 pixel values (0 or 1)
//   For 0.3, time is time since start, followed by RGB values for each of 40 frames.

        for(i = 0; i < nframes; i++)
        {
                switch(vnum)
                {
                case '2': p = getv2frame(in);
                        break;
                case '3': p = getv3frame(in);
                } // END frame

                cvt2data16(p); // compress RGB to a single 16 bit value - 0rrrrrgggggbbbbb
                cvt2data12(p); // compress 2 RGB's to 3 bytes - 00rrrggg 00bbbrrr 00gggbbb
                cvt2data8(p); // compress RGB to 8 bit - rrrgggbb

// at this point, frame has been input, is same for either version. So now link it in.

                if(tanll == NULL) // this is first node
                        tanll = p;
                else
                        last->next = p;
                last = p;
        } // END for


        printf("Done reading tan file!\n");

// Now we iterate over the linked list, and remove any frames that are
// too fast. We iterate to handle the case where deleting one node doesn't solve
// the problem. We keep going until we do a pass when no more nodes are deleted.
//
// This is a little tricky - if we find a frame that comes too fast, we delete the
// node previous to the fast one, so that we will end up in the right place.
//
        printtanstats(tanll);

        printf("Checking for fast nodes\n");

        changed = 1;
        passnum = 0;
        while(changed)
        {
                if(debug) printf("Pass number %d\n", passnum);
                p = tanll;
                last = NULL;
                changed = 0;
                while(p->next != NULL)
                {
                        timediff = (p->next)->startus - p->startus;
                        if(timediff < MINTIME) // next node is coming too fast, so delete this node
                        {
                                if(last == NULL) tanll = p->next;  // first node
                                else last->next = p->next;  // remove this node
                                if(debug) printf("Removing node at time ");
                                printtime(p->startus, timestr);
                                if(debug) printf("%s\n", timestr);
                                changed = 1;
                        }
                        last = p;
                        p = p->next;
                } // END while(p->next !=NULL)
                passnum++;
        } // END while(changed)

// Check to see if tan file still looks ok - the following routine outputs a new
// tan file containing the changes we just made, if any.

        //printf("Writing tan file\n");

        //writetan(tanoutname, tanll, vnum, dummy);

        return tanll;

}   // END readtan


/*
   int main()      // Tester for cvt2data16, cvt2data12 and cvt2data8
   {
   tannode node;
   int i;

   for(i = 0; i < 40; i++)
     {
      node.rgb[i].red = 0;
      node.rgb[i].green = 0;
      node.rgb[i].blue = 0;
     }
   while(1)
     {
      printf("Enter two rgb values: ");
      scanf("%x %x %x %x %x %x",
             &node.rgb[0].red, &node.rgb[0].green, &node.rgb[0].blue,
             &node.rgb[1].red, &node.rgb[1].green, &node.rgb[1].blue );
      cvt2data16(&node);
      cvt2data12(&node);
      cvt2data8(&node);
      printf("Orig: %d %d %d, 16 bit: %x, 12 bit: %x %x %x, 8 bit: %x\n",
              node.rgb[0].red, node.rgb[0].green, node.rgb[0].blue,
              node.data16[0],
              node.data12[0], node.data12[1], node.data12[2],
              node.data8[0]);
      cvtfromdata12(&node);
     } //END while

   } // END main tester for cvt2data16, cvt2data8
 */


// writes a tan file from a llist
void writetan(char tanname[], tannode *tanll, char vnum, int dummy[])
{
        FILE *out;
        int i;
        int nodecount;
        tannode *p;
        char timestr[20];
        int oldtime;
        float timediff;

        out = fopen(tanname, "w");
        if(out == NULL)
        {
                printf("Something happened opening %s for writing - no file created\n");
                return;
        }

        fprintf(out,"0.%c\r\n", vnum); // output version number
        if(vnum == '3')
        {
                for(i = 0; i < 9; i++)
                {
                        fprintf(out, "%d ", dummy[i]); // print out color palette
                        // if(i == 8)
                        // else       fprintf(out," ");
                }
                fprintf(out,"\r\n");
                for(i = 9; i < 63; i++)
                {
                        fprintf(out, "%d ", dummy[i]);
                        //if (i == 62)
                        // else         fprintf(out, " ");
                }
                fprintf(out, "\r\n");
                for(i = 63; i < 117; i++)
                {
                        fprintf(out, "%d ", dummy[i]);
                        // if(i == 116)
                        // else         fprintf(out, " ");
                }
                fprintf(out, "\r\n");
        }

        // count number of nodes

        nodecount = 0;
        p = tanll;
        while(p != NULL)
        {
                nodecount++;
                p = p->next;
        }
        // output nodecount and matrix dimensions

        fprintf(out, "%d 10 4\r\n", nodecount);

        // now traverse linked list again and output frames
        p = tanll;
        oldtime = 0;

        while(p != NULL)
        {
                switch(vnum)
                {
                case '2': timediff = (p->startus - oldtime)/1000000.;
                        fprintf(out, "%f\r\n", timediff);
                        oldtime = p->startus;
                        break;
                case '3': printtime(p->startus, timestr);
                        fprintf(out, "%s\r\n", timestr);
                        break;
                } // END switch

                for(i = 0; i < 40; i++)
                {
                        switch(vnum)
                        {
                        case '2': if(p->rgb[i].red == 255) fprintf(out,"1 ");
                                else fprintf(out,"0 ");
                                break;
                        case '3': fprintf(out, "%d %d %d ",
                                          p->rgb[i].red, p->rgb[i].green, p->rgb[i].blue);
                                break;
                        }
                        if(i % 4 == 3) fprintf(out,"\r\n");
                        // else           fprintf(out," ");

                } // END for i

                p = p->next;
        } // END while

        fclose(out);
}   // END writetan


void cvt2data16(tannode *p)
{
        uint8_t red, green, blue;
        int i;

        for(i = 0; i < 40; i++)
        {
                red = p->rgb[i].red >> 3; // truncate each color to 5 bits
                green = p->rgb[i].green >> 3;
                blue = p->rgb[i].blue >> 3;
                p->data16[i] = red << 10 | green << 5 | blue; // put everything together
        }
}   // END cvt2data16


void cvt2data12(tannode *p)
{
        uint8_t red, green, blue;
        int i;
        int ctr;

        for(i = 0, ctr = 0; i < 60; i+=3, ctr+=2)
        {
                red   = p->rgb[ ctr ].red   >> 5; // throw away all but 3 bits
                green = p->rgb[ ctr ].green >> 5;
                blue  = p->rgb[ ctr ].blue  >> 5;
                // we add 0x40, so all the chars (bytes) fall in the legal ASCII range,
                // in case some component responds to control chars, and to avoid the
                // possibility that the values create a "+++" sequence, which would
                // freak out the xbees (put them into command mode).
                p->data12[ i ] = (red << 3 | green) | 0x40;
                red   = p->rgb[ctr+1].red   >> 5;
                green = p->rgb[ctr+1].green >> 5;
                p->data12[i+1] = (blue << 3 | red)   | 0x40;
                blue  = p->rgb[ctr+1].blue  >> 5;
                p->data12[i+2] = (green << 3 | blue)  | 0x40;
        }
}   // END cvt2data12


void cvt2data8(tannode *p)
{
        uint8_t red, green, blue;
        int i;

        for(i = 0; i < 40; i++)
        {
                red = p->rgb[i].red >> 5; // truncate red to 3 bits
                green = p->rgb[i].green >> 5; // truncate green to 3 bits
                blue = p->rgb[i].blue >> 6; // truncate blue to 2 bits
                p->data8[i] = red << 5 | green << 2 | blue; // put everything together
        }
}   // END cvt2data8


void printtan(tannode *tanll)
{
        tannode *p;
        int i;

        p = tanll;

        while(p != NULL)
        {
                printf("Time(us): %d\n", p->startus);
                for(i = 0; i < 40; i++)
                {
                        printf(" %3d %3d %3d ", p->rgb[i].red, p->rgb[i].green, p->rgb[i].blue);
                        if(i%4 == 3) printf("\n");
                }
                p = p->next;
        } // END while
}   // END printtan


void printdata16(tannode *tanll)
{
        tannode *p;
        uint8_t high, low;
        int i;

        p = tanll;

        while(p != NULL)
        {
                printf("Time(us): %d\n", p->startus);
                for(i = 0; i < 40; i++)
                {
                        high = p->data16[i] >> 8;
                        low =  p->data16[i] & 0x00ff;
                        printf(" %2x %2x ", high, low);
                        if(i%4 == 3) printf("\n");
                }
                p = p->next;
        } // END while
}   // END printdata16


void printdata12(tannode *tanll)
{
        tannode *p;
        int i, j = 0;

        p = tanll;

        while(p != NULL)
        {
                printf("Time(us): %d\n", p->startus);
                for(i = 0; i < 60; i+=3)
                {
                        printf(" %2x %2x %2x  ", p->data12[i], p->data12[i+1], p->data12[i+2]);
                        if(j%4 == 3) printf("\n");
                        j++;
                }
                p = p->next;
        } // END while
}   // END printdata8


void cvtfromdata12(tannode *tanll)
{
        RGB rgb[40];
        tannode *p;

        p = tanll;

        rgb[0].red   = ((p->data12[0]) >> 3 & 0x07) << 5;
        rgb[0].green = ((p->data12[0])      & 0x07) << 5;
        rgb[0].blue  = ((p->data12[1]) >> 3 & 0x07) << 5;
        rgb[1].red   = ((p->data12[1])      & 0x07) << 5;
        rgb[1].green = ((p->data12[2]) >> 3 & 0x07) << 5;
        rgb[1].blue  = ((p->data12[2])      & 0x07) << 5;
        printf("Values are r1:%x g1:%x b1:%x, r2:%x g2:%x b2:%x\n",
               rgb[0].red, rgb[0].green, rgb[0].blue,
               rgb[1].red, rgb[1].green, rgb[1].blue);
}    // END cvtfromdata12


void printdata8(tannode *tanll)
{
        tannode *p;
        int i;

        p = tanll;

        while(p != NULL)
        {
                printf("Time(us): %d\n", p->startus);
                for(i = 0; i < 40; i++)
                {
                        printf(" %2x ", p->data8[i]);
                        if(i%4 == 3) printf("\n");
                }
                p = p->next;
        } // END while
}   // END printdata8


tannode *getv2frame(FILE *in)
{
        static int timeus = 0;
        float elaptime;
        tannode *p;
        int val;
        int i;
        int debug = 1;

        fscanf(in, "%f", &elaptime); // for version 0.2, time is elapsed time since last frame
        timeus += elaptime*1000000;

        p = new tannode;
        p->startus = timeus;

        for(i = 0; i < 40; i++) // frames are specified as 0 or per pixel.
        {
                fscanf(in, "%d", &val);
                if(val == 1) // convert BW to RGB
                {
                        p->rgb[i].red = 255;
                        p->rgb[i].green = 255;
                        p->rgb[i].blue = 255;
                }
                else
                {
                        p->rgb[i].red = 0;
                        p->rgb[i].green = 0;
                        p->rgb[i].blue = 0;
                }
        } // END for

        return p;
}   // END getv2frame


tannode *getv3frame(FILE *in)
{
        int timeus;
        char timestring[11];
        tannode *p;
        int red, green, blue;
        int i;

        fscanf(in, "%s", timestring); // for version 0.3, time is absolute, mm:ss.sss
        timeus = cvt2us(timestring);

        p = new tannode;
        p->startus = timeus;

        for(i = 0; i < 40; i++) // frames are specified as 0 or per pixel.
        {
                fscanf(in, "%d %d %d", &red, &green, &blue);
                p->rgb[i].red = red;
                p->rgb[i].green = green;
                p->rgb[i].blue = blue;
        } // END for

        return p;
}   // END getv3frame

/*
   int main()  // tester for cvt2us
   {
    char timestring[20];

    while(1)
      {
       scanf("%s", timestring);
       printf("time: %d\n", cvt2us(timestring));
      }
   } // END tester fof cvt2us
 */

int cvt2us(char timestring[])
{
        int i = 0;
        int mins = 0, secs = 0, fracsecs = 0;
        int divisor = 1;
        int timeus;

        while(timestring[i] != ':')
        {
                mins = mins * 10 + timestring[i] - '0';
                i++;
        }
        i++;
        while(timestring[i] != '.')
        {
                secs = secs * 10 + timestring[i] - '0';
                i++;
        }
        i++;
        while(timestring[i] != '\0')
        {
                fracsecs = fracsecs * 10 + timestring[i] - '0';
                divisor = divisor*10;
                i++;
        }
        while(divisor < 1000000) // convert seconds fraction to microseconds.
        {
                fracsecs = fracsecs * 10;
                divisor = divisor*10;
        }

        timeus = (mins * 60 + secs) * 1000000 + fracsecs;

        return timeus;
}   // END cvt2us


void printtanstats(tannode *tanll)  // determine minimum time between frames in
                                    // this tan file, and calculates and prints the
                                    // distribution of RGB values
{
        tannode *p;
        int reddist[255], greendist[255], bluedist[255];
        int mintime = 10000000;
        int lastus = -10000000; // bogus value, so first node won't get counted
        int timediff;
        char timestr[20];
        int smalltimecnt = 0; // number of time frames are less than MINTIME apart
        int absmintime;
        int framecount = 0;
        int i;
        int debug = 0;

        for(i = 0; i <255; i++)
        {
                reddist[i] = 0;
                greendist[i] = 0;
                bluedist[i] = 0;
        }

        p = tanll;

        while(p != NULL)
        {
//      printf("Time(us): %d\n", p->startus);
                // calculate time since last frame, save result if smallest
                timediff = p->startus - lastus;
                if(timediff <= 0)
                {
                        //printf("Time between frame %d and %d is %d\n",
                        //                           framecount-1, framecount, timediff);
                        //printf("Absolute time is: ");
                        //printtime(p->startus, timestr);
                        //printf("%s\n", timestr);
                }
                else
                {
                        if(timediff < mintime)
                        {
                                mintime = timediff;
                                absmintime = p->startus;
                        }
                        if(timediff < MINTIME)
                        {
                                smalltimecnt++;
                                //printf("Small time %d at ", timediff);
                                //printtime(p->startus, timestr);
                                //printf("%s\n", timestr);
                        }
                }

                lastus = p->startus; // save this frame start time
                framecount++;

                for(i = 0; i < 40; i++)
                {
                        // create histogram of color distribution
                        reddist[p->rgb[i].red]++;
                        greendist[p->rgb[i].green]++;
                        bluedist[p->rgb[i].blue]++;
                }
                p = p->next;
        } // END while

        // we've collected all the stats, so print them out
        //printf("Minimum time between frames (us): %d, first happens at ", mintime);
        //printtime(absmintime, timestr);
        //printf("%s\nNumber of times frames are less than %d ms apart: %d\n\n",
        //        timestr, MINTIME/1000, smalltimecnt);

/*
     printf("Red pixel intensity distribution\n");
     for(i = 0; i < 255; i++)
        if(reddist[i] != 0) printf("Value: %d Frequency: %d\n", i, reddist[i]);

     printf("Green pixel intensity distribution\n");
     for(i = 0; i < 255; i++)
        if(greendist[i] != 0) printf("Value: %d Frequency: %d\n", i, greendist[i]);

     printf("Blue  pixel intensity distribution\n");
     for(i = 0; i < 255; i++)
        if(bluedist[i] != 0) printf("Value: %d Frequency: %d\n", i, bluedist[i]);
 */

}   // END printtanstats


void printtime(int time, char timestr[])
{
        int mins, secs, us;
        us = time%1000000/1000;
        secs = time/1000000;
        mins = secs/60;
        secs = secs%60;
        sprintf(timestr, "%02d:%02d.%03d", mins, secs, us);
}   // END printtime






/*
    i = 0;
    while(1)
      {
       nbytes = ftdi_write_data(&ftdic, buf, 62);
       i++;
       if(nbytes < 0)
         {
          fprintf(stderr, "Error writing bytes - error code %d\n", nbytes);
          return EXIT_FAILURE;
         }
       else
         {
          printf("%d bytes written %d\r", nbytes, i);
          fflush(stdout);
         }
       usleep(500000);   // Sleep for 1/2 second (500000 usecs)

      } //END while 1
 */


int xbee_open(struct ftdi_context *ftdic)
{
        int ret;

        if ((ret = ftdi_init(ftdic)) < 0)
        {
                fprintf(stderr, "ftdi_init failed\n");
                return ret;
        }
        if ((ret = ftdi_usb_open(ftdic, 0x0403, 0x6001)) < 0)
        {
                fprintf(stderr, "unable to open ftdi (xbee) device: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
                return ret;
        }
        // Read out FTDIChip-ID of R type chips
//    if (ftdic.type == TYPE_R)
//    {
//        unsigned int chipid;
//        printf("ftdi_read_chipid: %d\n", ftdi_read_chipid(&ftdic, &chipid));
//        printf("FTDI chipid: %X\n", chipid);
//    }

        ret = ftdi_set_baudrate(ftdic, 57600);
        if (ret < 0) {
                fprintf(stderr, "unable to set baud rate: %d (%s).\n", ret, ftdi_get_error_string(ftdic));
        } else {
                printf("baudrate set.\n");
        }

        ret = ftdi_set_line_property(ftdic, BITS_8, STOP_BIT_1, NONE);
        if(ret < 0) {
                fprintf(stderr, "unable to set line parameters: %d (%s).\n", ret, ftdi_get_error_string(ftdic));
        } else {
                printf("line parameters set.\n");
        }

        return 0;

}   // END xbee_open
