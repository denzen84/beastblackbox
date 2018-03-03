//  BEAST black box utility
//  Utility to read benary beast traffic from the file
//  File could be generated using netcat:
//  nc 127.0.0.1 30005 > radar.log
//
//  (c) 2018 Denis G Dugushkin
//  dentall@mail.ru
//
//  based on sources of dump1090 and view1090
//
//
//  Previous copyrights
//----------------------------------------------------------------------
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
	Modes.throttle                = 0;
	Modes.filename                = NULL;
	Modes.filename_extract        = NULL;
    Modes.interactive_rows        = 60;
    Modes.interactive_display_ttl = 0;
    Modes.interactive             = 0;
    Modes.quiet                   = 0;
    Modes.maxRange                = 1852 * 450; // 300NM default max range
    Modes.mode_ac 				  = 0;
	Modes.baseTime.tv_sec         = 0;
	Modes.baseTime.tv_nsec        = 0;
	Modes.useLocaltime            = 0;   
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
"| BEAST: black box utility                                            v0.996a|\n"
"-----------------------------------------------------------------------------\n"
 
  "--filename <file>        Source file to proceed\n"
  "--extract <file>         Extract BEAST data to new file (if no filter specified it just copies source)\n"
  "--init-time-unix <sec>   Start time (UNIX format) to calculate message realtime using MLAT timestamps\n"
  "--localtime              Decode time as local time (default is UTC)\n"
  "--filter-icao <addr>     Show only messages from the given ICAO\n"
  "--max-messages <count>   Limit messages count (from the start of the file)\n"
  "--sbs-output             Show messages in SBS format\n"
  "--show-progress          Show progress during file operation\n\n"
  "Additional BEAST options:\n"
  "--modeac                 Enable decoding of SSR modes 3/A & 3/C\n"
  "--no-crc-check           Disable messages with broken CRC (discouraged)\n"
  "--no-fix                 Disable single-bits error correction using CRC\n"
  "--fix                    Enable single-bits error correction using CRC\n"
  "--aggressive             More CPU for more messages (two bits fixes, ...)\n"
  "--metric                 Use metric units (meters, km/h, ...)\n\n"
  "--help                   Show this help\n"
    );
}

static void addMLATtime(struct timespec *msgTime, uint64_t mlatTimestamp) {
	
	uint64_t mlat_realtime;
	
	mlat_realtime = mlatTimestamp / 12;
	//printf("%llu\n",(unsigned long long) mlat_realtime);
	
	msgTime->tv_nsec += 1000 * (mlat_realtime % 1000000);
	
	if (msgTime->tv_nsec > 999999999) {
		msgTime->tv_sec++;
		msgTime->tv_nsec -= 999999999;
	}
	
	msgTime->tv_sec += mlat_realtime / 1000000;	
}

//
//=========================================================================
//
// Write SBS output to TCP clients
//
static void modesSendSBSOutput(struct modesMessage *mm) {
    char *p;
	char buffer[200];
    struct timespec now;
    struct tm    stTime_receive, stTime_now;
    int          msgType;
	p = &buffer[0];
	struct aircraft *a = Modes.aircrafts;
	
    // For now, suppress non-ICAO addresses
    if ((mm->addr & MODES_NON_ICAO_ADDRESS) || !(!Modes.show_only || mm->addr == Modes.show_only))
        return;

    //
    // SBS BS style output checked against the following reference
    // http://www.homepages.mcb.net/bones/SBS/Article/Barebones42_Socket_Data.htm - seems comprehensive
    //

    // Decide on the basic SBS Message Type
    switch (mm->msgtype) {
    case 4:
    case 20:
        msgType = 5;
        break;
        break;

    case 5:
    case 21:
        msgType = 6;
        break;

    case 0:
    case 16:
        msgType = 7;
        break;

    case 11:
        msgType = 8;
        break;

    case 17:
    case 18:
        if (mm->metype >= 1 && mm->metype <= 4) {
            msgType = 1;
        } else if (mm->metype >= 5 && mm->metype <=  8) {
            msgType = 2;
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            msgType = 3;
        } else if (mm->metype == 19) {
            msgType = 4;
        } else {
            return;
        }
        break;

    default:
        return;
    }

    // Fields 1 to 6 : SBS message type and ICAO address of the aircraft and some other stuff
    p += sprintf(p, "MSG,%d,1,1,%06X,1,", msgType, mm->addr);

    // Find current system time
    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &stTime_now);

    // Find message reception time
	if (Modes.useLocaltime) localtime_r(&mm->sysTimestampMsg.tv_sec, &stTime_receive); 
	else gmtime_r(&mm->sysTimestampMsg.tv_sec, &stTime_receive);

    // Fields 7 & 8 are the message reception time and date
    p += sprintf(p, "%04d/%02d/%02d,", (stTime_receive.tm_year+1900),(stTime_receive.tm_mon+1), stTime_receive.tm_mday);
    p += sprintf(p, "%02d:%02d:%02d.%03u,", stTime_receive.tm_hour, stTime_receive.tm_min, stTime_receive.tm_sec, (unsigned) (mm->sysTimestampMsg.tv_nsec / 1000000U));

    // Fields 9 & 10 are the current time and date
    p += sprintf(p, "%04d/%02d/%02d,", (stTime_now.tm_year+1900),(stTime_now.tm_mon+1), stTime_now.tm_mday);
    p += sprintf(p, "%02d:%02d:%02d.%03u", stTime_now.tm_hour, stTime_now.tm_min, stTime_now.tm_sec, (unsigned) (now.tv_nsec / 1000000U));

    // Field 11 is the callsign (if we have it)
    if (mm->callsign_valid) {p += sprintf(p, ",%s", mm->callsign);}
    else                    {p += sprintf(p, ",");}

    // Field 12 is the altitude (if we have it)
    if (mm->altitude_valid) {
        if (Modes.use_gnss) {
            if (mm->altitude_source == ALTITUDE_GNSS) {
                p += sprintf(p, ",%dH", mm->altitude);
            } else if (trackDataValid(&a->gnss_delta_valid)) {
                p += sprintf(p, ",%dH", mm->altitude + a->gnss_delta);
            } else {
                p += sprintf(p, ",%d", mm->altitude);
            }
        } else {
            if (mm->altitude_source == ALTITUDE_BARO) {
                p += sprintf(p, ",%d", mm->altitude);
            } else if (trackDataValid(&a->gnss_delta_valid)) {
                p += sprintf(p, ",%d", mm->altitude - a->gnss_delta);
            } else {
                p += sprintf(p, ",");
            }
        }
    } else {
        p += sprintf(p, ",");
    }

    // Field 13 is the ground Speed (if we have it)
    if (mm->speed_valid && mm->speed_source == SPEED_GROUNDSPEED) {
        p += sprintf(p, ",%d", mm->speed);
    } else {
        p += sprintf(p, ","); 
    }

    // Field 14 is the ground Heading (if we have it)       
    if (mm->heading_valid && mm->heading_source == HEADING_TRUE) {
        p += sprintf(p, ",%d", mm->heading);
    } else {
        p += sprintf(p, ",");
    }

    // Fields 15 and 16 are the Lat/Lon (if we have it)
    if (mm->cpr_decoded) {
        p += sprintf(p, ",%1.5f,%1.5f", mm->decoded_lat, mm->decoded_lon);
    } else {
        p += sprintf(p, ",,");
    }

    // Field 17 is the VerticalRate (if we have it)
    if (mm->vert_rate_valid) {
        p += sprintf(p, ",%d", mm->vert_rate);
    } else {
        p += sprintf(p, ",");
    }

    // Field 18 is  the Squawk (if we have it)
    if (mm->squawk_valid) {
        p += sprintf(p, ",%04x", mm->squawk);
    } else {
        p += sprintf(p, ",");
    }

    // Field 19 is the Squawk Changing Alert flag (if we have it)
    if (mm->alert_valid) {
        if (mm->alert) {
            p += sprintf(p, ",-1");
        } else {
            p += sprintf(p, ",0");
        }
    } else {
        p += sprintf(p, ",");
    }

    // Field 20 is the Squawk Emergency flag (if we have it)
    if (mm->squawk_valid) {
        if ((mm->squawk == 0x7500) || (mm->squawk == 0x7600) || (mm->squawk == 0x7700)) {
            p += sprintf(p, ",-1");
        } else {
            p += sprintf(p, ",0");
        }
    } else {
        p += sprintf(p, ",");
    }

    // Field 21 is the Squawk Ident flag (if we have it)
    if (mm->spi_valid) {
        if (mm->spi) {
            p += sprintf(p, ",-1");
        } else {
            p += sprintf(p, ",0");
        }
    } else {
        p += sprintf(p, ",");
    }

    // Field 22 is the OnTheGround flag (if we have it)
    switch (mm->airground) {
    case AG_GROUND:
        p += sprintf(p, ",-1");
        break;
    case AG_AIRBORNE:
        p += sprintf(p, ",0");
        break;
    default:
        p += sprintf(p, ",");
        break;
    }

    p += sprintf(p, "\r\n");

    printf("%s", buffer);
	
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
		
		mm.sysTimestampMsg.tv_nsec = Modes.baseTime.tv_nsec;
		mm.sysTimestampMsg.tv_sec = Modes.baseTime.tv_sec;
	    addMLATtime(&mm.sysTimestampMsg, (mm.timestampMsg - Modes.firsttimestampMsg));
		


        // record reception time as the time we read it.
        //clock_gettime(CLOCK_REALTIME, &mm.sysTimestampMsg);

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

		//if (Modes.interactive_rtl1090) 
		Modes.last_addr = mm.addr;
		useModesMessage(&mm);
		if(Modes.quiet) modesSendSBSOutput(&mm);
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

    int input_bb, output_bb;   
	ssize_t i,k,ret_in, ret_out, seek;
    char buffer[BUF_SIZE];
	struct stat sb;
	char *p;
	long long unsigned  j, js, global;
	char beastmessage[MAX_MSG_LEN];
		
	output_bb = -1;
	
    input_bb = open(Modes.filename, O_RDONLY);
    if (input_bb == -1) {
            fprintf(stderr, "Error. Unable to open file %s\n",Modes.filename);
            return 2;
    }
	
	if (Modes.filename_extract != NULL) {
	output_bb = open(Modes.filename_extract, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (output_bb == -1) {
            fprintf(stderr, "Error. Unable to open for write file %s\n",Modes.filename_extract);
            return 2;
		}
	}
	
   fstat(input_bb, &sb);
   ret_in = read (input_bb, &buffer[0], BUF_SIZE);
	
	// Scan for header here ---------------------------------------------------
	// Now we only read first beast message and get it MLAT timestamp to relative
	// time calculations.
	k = 0;
	
	while(k < ret_in) {
	
	i = copyBinMessageSafe(&buffer[k], BUF_SIZE - k, &beastmessage[0]);
	
	if(i > 0) {
	Modes.firsttimestampMsg = 0;
	p = &beastmessage[2];
	
	for (j = 0; j < 6; j++) {   
	
            Modes.firsttimestampMsg = Modes.firsttimestampMsg << 8 | (*p & 255);
            if (0x1A == *p) {p++;}
			p++;
        }
	if (Modes.firsttimestampMsg) {
		Modes.previoustimestampMsg = Modes.firsttimestampMsg;
		break;
	} else k++;
	
	} else k++;
	
	}
	// ------------------------------------------------------------------------
	
	j = 0;
	js = 0;
	seek = 0;
	global = 0;
	
	while((ret_in > 0) && (!Modes.exit)) {
	
	k = 0;
	
	while(k < (ret_in + seek)) {
	icaoFilterExpire();
    trackPeriodicUpdate();
	i = copyBinMessageSafe(&buffer[k], ret_in + seek - k, &beastmessage[0]);
	if(i > 0) {
	j++;
	/* printf("%16llX %08X lim = %d len = %d, HEX = ", global, k, ret_in + seek - k, i);
	for(j=0;j<i;j++) {
		printf("%02X ",beastmessage[j]);
	}
	printf("\n");
	*/
	
	if (Modes.throttle && (j % 0xFFF  == 0)) {printf("Processing... File offset 0x%llX (%llu%%), message #%llu\r", global, ((100*global)/(uint64_t)sb.st_size),j); }
	decodeBinMessage(&beastmessage[0]);
	
	if (Modes.interactive_display_ttl && (j==Modes.interactive_display_ttl)) {goto EXIT_LIMIT; }
	
	if ((output_bb != -1) && ((!Modes.show_only || Modes.last_addr == Modes.show_only) || (Modes.show_only==0))) {
		ret_out = write (output_bb, &beastmessage[0], (ssize_t) i);
		
		if (ret_out != i) {
		fprintf(stderr, "Error. Write error in file %s\n",Modes.filename_extract);
        return 2;	
		}
		js++;
	}
	
	k+=i;
	global+=i;
	} else
	   if(i == 0) {
		if ((ret_in + seek - k) == 1) {

			break; 
			
		} 
		else { k++; global++; }
		}
		else { break;}
	
	
	}
	seek = BUF_SIZE - k;
	if (seek > 0 ) memcpy(&buffer[0],&buffer[k], seek);
	
	ret_in = read (input_bb, &buffer[seek], k);
	}
	
	EXIT_LIMIT:
	printf("\n");
	if (js) printf("Extracted %llu messages\n", js);
	printf("Total processed %llu messages\n", j);
    

   
    close (input_bb);
	close (output_bb);

    return 0;
}

//
//=========================================================================
//
int main(int argc, char **argv) {
    // Initialization
    int j;
	double t;

    view1090InitConfig();
    view1090Init();

    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)


    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments
		
		if (!strcmp(argv[j],"--modeac")) {
            Modes.mode_ac = 1;
		} else if (!strcmp(argv[j],"--localtime")) {
            Modes.useLocaltime = 1; 
		} else if (!strcmp(argv[j],"--init-time-unix") && more) {
			Modes.baseTime.tv_nsec = (int) (1000000000 * modf(atof(argv[++j]),&t));
			Modes.baseTime.tv_sec = (int) t;
	    } else if (!strcmp(argv[j],"--filename") && more) {
		    Modes.filename = strdup(argv[++j]);
		} else if (!strcmp(argv[j],"--extract") && more) {
		    Modes.filename_extract = strdup(argv[++j]);			
		} else if (!strcmp(argv[j],"--filter-icao") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
            Modes.interactive = 0;
        } else if (!strcmp(argv[j],"--max-messages") && more) {
            Modes.interactive_display_ttl = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--sbs-output")) {
            Modes.quiet = 1;
            Modes.interactive_rtl1090 = 1;
		} else if (!strcmp(argv[j],"--show-progress")) {
            Modes.throttle = 1;
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
	
	if (Modes.filename == NULL) {
            showHelp();
			fprintf(stderr, "\nERROR: no file specified. Nothing to do. Use --filename option.\n\n");
            exit(1);		
	}
	
    readbeastfile();
	
    	
    return (0);
}
//
//=========================================================================
//
