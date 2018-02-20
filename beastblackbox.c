// view1090, a Mode S messages viewer for dump1090 devices.
//
// Copyright (C) 2014 by Malcolm Robb <Support@ATTAvionics.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#include "dump1090.h"

#define BUF_SIZE 4096
#define MAX_MSG_LEN 64

//
// ============================= Utility functions ==========================
//
void sigintHandler(int dummy) {
    MODES_NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
}
//
//

// =============================== Initialization ===========================
//
void view1090InitConfig(void) {
    // Default everything to zero/NULL

    memset(&Modes,    0, sizeof(Modes));

    Modes.check_crc               = 1;
    Modes.interactive_rows        = 40;
    Modes.interactive_display_ttl = 0;
    Modes.interactive             = 0;
    Modes.quiet                   = 0;
    Modes.maxRange                = 1852 * 300; // 300NM default max range
    Modes.mode_ac = 1;
    
}
//
//=========================================================================
//
void view1090Init(void) {

//    pthread_mutex_init(&Modes.data_mutex,NULL);
//    pthread_cond_init(&Modes.data_cond,NULL);

    // Validate the users Lat/Lon home location inputs
    if ( (Modes.fUserLat >   90.0)  // Latitude must be -90 to +90
      || (Modes.fUserLat <  -90.0)  // and 
      || (Modes.fUserLon >  360.0)  // Longitude must be -180 to +360
      || (Modes.fUserLon < -180.0) ) {
        Modes.fUserLat = Modes.fUserLon = 0.0;
    } else if (Modes.fUserLon > 180.0) { // If Longitude is +180 to +360, make it -180 to 0
        Modes.fUserLon -= 360.0;
    }
    // If both Lat and Lon are 0.0 then the users location is either invalid/not-set, or (s)he's in the 
    // Atlantic ocean off the west coast of Africa. This is unlikely to be correct. 
    // Set the user LatLon valid flag only if either Lat or Lon are non zero. Note the Greenwich meridian 
    // is at 0.0 Lon,so we must check for either fLat or fLon being non zero not both. 
    // Testing the flag at runtime will be much quicker than ((fLon != 0.0) || (fLat != 0.0))
    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) {
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    }


    // Prepare error correction tables
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
  
}

//
// ================================ Main ====================================
//
void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"| BEAST black box utility     %45s |\n"
"-----------------------------------------------------------------------------\n"
  "--no-interactive         Disable interactive mode, print messages to stdout\n"
  "--interactive-rows <num> Max number of rows in interactive mode (default: 15)\n"
  "--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60)\n"
  "--interactive-rtl1090    Display flight table in RTL1090 format\n"
  "--modeac                 Enable decoding of SSR modes 3/A & 3/C\n"
  "--net-bo-ipaddr <IPv4>   TCP Beast output listen IPv4 (default: 127.0.0.1)\n"
  "--net-bo-port <port>     TCP Beast output listen port (default: 30005)\n"
  "--lat <latitude>         Reference/receiver latitide for surface posn (opt)\n"
  "--lon <longitude>        Reference/receiver longitude for surface posn (opt)\n"
  "--max-range <distance>   Absolute maximum range for position decoding (in nm, default: 300)\n"
  "--no-crc-check           Disable messages with broken CRC (discouraged)\n"
  "--no-fix                 Disable single-bits error correction using CRC\n"
  "--fix                    Enable single-bits error correction using CRC\n"
  "--aggressive             More CPU for more messages (two bits fixes, ...)\n"
  "--metric                 Use metric units (meters, km/h, ...)\n"
  "--show-only <addr>       Show only messages from the given ICAO on stdout\n"
  "--help                   Show this help\n",
  MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION
    );
}

//
//=========================================================================
//
// This function decodes a Beast binary format message
//
// The message is passed to the higher level layers, so it feeds
// the selected screen output, the network output and so forth.
//
// If the message looks invalid it is silently discarded.
//
// The function always returns 0 (success) to the caller as there is no
// case where we want broken messages here to close the client connection.
//
int decodeBinMessage(char *p) {
    int msgLen = 0;
    int  j;
    char ch;
    unsigned char msg[MODES_LONG_MSG_BYTES];
    static struct modesMessage zeroMessage;
    struct modesMessage mm;
    memset(&mm, 0, sizeof(mm));
                               

    ch = *p++; /// Get the message type
    if (0x1A == ch) {ch=*p++;}   
    if       ((ch == '1') && (Modes.mode_ac)) { // skip ModeA/C unless user enables --modes-ac
        msgLen = MODEAC_MSG_BYTES;
    } else if (ch == '2') {
        msgLen = MODES_SHORT_MSG_BYTES;
    } else if (ch == '3') {
        msgLen = MODES_LONG_MSG_BYTES;
    }
    
    if (msgLen) {
        mm = zeroMessage;

        // Mark messages received over the internet as remote so that we don't try to
        // pass them off as being received by this instance when forwarding them
        mm.remote      =    0;

        // Grab the timestamp (big endian format)
        mm.timestampMsg = 0;
        for (j = 0; j < 6; j++) {
            ch = *p++;
            mm.timestampMsg = mm.timestampMsg << 8 | (ch & 255);
            if (0x1A == ch) {p++;}
        }

        // record reception time as the time we read it.
        clock_gettime(CLOCK_REALTIME, &mm.sysTimestampMsg);

        ch = *p++;  // Grab the signal level
        mm.signalLevel = ((unsigned char)ch / 255.0);
        mm.signalLevel = mm.signalLevel * mm.signalLevel;
        if (0x1A == ch) {p++;}

        for (j = 0; j < msgLen; j++) { // and the data
            msg[j] = ch = *p++;
            if (0x1A == ch) {p++;}
        }

        if (msgLen == MODEAC_MSG_BYTES) { // ModeA or ModeC
            Modes.stats_current.remote_received_modeac++;
            decodeModeAMessage(&mm, ((msg[0] << 8) | msg[1]));
        } else {
            int result;

            Modes.stats_current.remote_received_modes++;
            result = decodeModesMessage(&mm, msg);
            if (result < 0) {
                if (result == -1)
                    Modes.stats_current.remote_rejected_unknown_icao++;
                else
                    Modes.stats_current.remote_rejected_bad++;
                return 0;
            } else {
                Modes.stats_current.remote_accepted[mm.correctedbits]++;
            }
        }

        useModesMessage(&mm);
    }
    return (0);
}



// Experimental

static int copyBinMessageSafe(char *p, int limit, char *out) {
    int msgLen = 0;
    int  j = 2;
    char ch;
	if (limit < 11) return -1; // Nothing to do
    ch = *p;
    if (0x1A == ch) {
		p++;
		*out = 0x1A;
		out++;
		} else return 0;
		
	ch = *p;	
    if (ch == '1')  {
        msgLen = MODEAC_MSG_BYTES;
		*out = '1';
		out++;
    } else if (ch == '2') {
        msgLen = MODES_SHORT_MSG_BYTES;
		*out = '2';
		out++;
    } else if (ch == '3') {
        msgLen = MODES_LONG_MSG_BYTES;
		*out = '3';
		out++;
    } else return 0;
	
	
	
    if (msgLen) {
	   msgLen += 9;
	   if (msgLen  > limit)   return -1;
	   p++;
        while ((j < msgLen)) {

			if (msgLen > limit)   return -2;
			*out = ch = *p++;
			j++;
			out++;
			if (0x1A == ch) { msgLen++; *out = ch = *p++; out++; j++; }
        }
    }
	if (msgLen > limit) return -3;
	out -= msgLen;
    return msgLen;
}



int readbeastfile() {

    int input_bb;//, output_bb;   
    
	ssize_t i,k,ret_in,seek;
    char buffer[BUF_SIZE];
	struct stat sb;
	uint64_t  j, global;

	
	char beastmessage[MAX_MSG_LEN];
		
 
    input_bb = open("2017-03-15--14-28-17-ulss7-beast-bin.log", O_RDONLY);
    if (input_bb == -1) {
            perror ("open");
            return 2;
    }
	/*
    output_bb = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_bb == -1) {
            perror ("open write");
            return 2;
    }
	*/
	
   fstat(input_bb, &sb);
   printf("Size: %llu\n", (uint64_t)sb.st_size);

    seek = 0;
	global = 0;
	j = 0;
	
		
    ret_in = read (input_bb, &buffer[0], BUF_SIZE);

	
	while(ret_in > 0) {
	
	k = 0;
	
	while(k < (ret_in + seek)) {
	
	i = copyBinMessageSafe(&buffer[k], ret_in + seek - k, &beastmessage[0]);
	if(i > 0) {
	j++;
	/* printf("%16llX %08X lim = %d len = %d, HEX = ", global, k, ret_in + seek - k, i);
	for(j=0;j<i;j++) {
		printf("%02X ",beastmessage[j]);
	}
	printf("\n");
	*/
	
	//if ( j % 0xFFFFF  == 0) 

        {printf("\nFile offset 0x%llX (%llu%%), message #%llu:\n", global, ((100*global)/(uint64_t)sb.st_size),j); fflush(stdout);}
	decodeBinMessage(&beastmessage[0]);
	if (Modes.interactive_display_ttl && (j==Modes.interactive_display_ttl)) return 0;
	/*
    ret_out = write (output_bb, &beastmessage[0], (ssize_t) i);
    if(ret_out != i){
    return 4;
	}
	*/
	
	k+=i;
	global+=i;
	} else
	   if(i == 0) {
		//printf("\nExit with 0, global=%llX k=%d, bufsize=%d, seek = %d\n",global, k, BUF_SIZE, seek);   
		if ((ret_in + seek - k) == 1) {
			printf("break\n");
			break; 
			
		} 
		else { k++; global++; }
		}
		else { break;}
	
	
	}
	seek = BUF_SIZE - k;
	if (seek > 0 ) memcpy(&buffer[0],&buffer[k], seek);
	//printf("\nExit, k=%d, bufsize=%d, seek = %d\n", k, BUF_SIZE, seek);
	

	ret_in = read (input_bb, &buffer[seek], k);
	}
	
	printf("Total processed %llu messages\n", j);
   

   
    close (input_bb);
	//close (output_bb);

    return 0;
}


//
//=========================================================================
//
int main(int argc, char **argv) {
    // Initialization
    int j;

    view1090InitConfig();
    view1090Init();

    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)


    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments

        if (!strcmp(argv[j],"--show-only") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
            Modes.interactive = 0;
        } else if (!strcmp(argv[j],"--max-messages") && more) {
            Modes.interactive_display_ttl = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--interactive-rtl1090")) {
            Modes.interactive = 1;
            Modes.interactive_rtl1090 = 1;
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--fix")) {
            Modes.nfix_crc = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.nfix_crc = MODES_MAX_BITERRORS;
        } else if (!strcmp(argv[j],"--max-range") && more) {
            Modes.maxRange = atof(argv[++j]) * 1852.0; // convert to metres
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            showHelp();
            exit(1);
        }
    }

    
    readbeastfile();
    	
    return (0);
}
//
//=========================================================================
//
