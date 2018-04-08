// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// util.c: misc utilities
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it  
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your  
// option) any later version.  
//
// This file is distributed in the hope that it will be useful, but  
// WITHOUT ANY WARRANTY; without even the implied warranty of  
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License  
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// This file incorporates work covered by the following copyright and  
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "util.h"

#include <stdlib.h>
#include <sys/time.h>

uint64_t mstime(void)
{
    struct timeval tv;
    uint64_t mst;

    gettimeofday(&tv, NULL);
    mst = ((uint64_t)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}

int64_t receiveclock_ns_elapsed(uint64_t t1, uint64_t t2)
{
    return (t2 - t1) * 1000U / 12U;
}

void normalize_timespec(struct timespec *ts)
{
    if (ts->tv_nsec > 1000000000) {
        ts->tv_sec += ts->tv_nsec / 1000000000;
        ts->tv_nsec = ts->tv_nsec % 1000000000;
    } else if (ts->tv_nsec < 0) {
        long adjust = ts->tv_nsec / 1000000000 + 1;
        ts->tv_sec -= adjust;
        ts->tv_nsec = (ts->tv_nsec + 1000000000 * adjust) % 1000000000;
    }
}

// Function from stackoverflow.com: https://stackoverflow.com/questions/13804095/get-the-time-zone-gmt-offset-in-c
int time_offset()
{
    time_t gmt, rawtime = time(NULL);
    struct tm *ptm;

#if !defined(WIN32)
    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);
#else
    ptm = gmtime(&rawtime);
#endif
    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    return (int)difftime(rawtime, gmt);
}


void MLATtime_none(struct timespec *msgTime, uint64_t mlatTimestamp) {

	msgTime->tv_nsec = mlatTimestamp; // Just to avoid compiler warning that "mlatTimestamp" never used
    // Find current system time
    clock_gettime(CLOCK_REALTIME, msgTime);

}


/*	The GPS timestamp is completely handled in the FPGA (hardware) and does not require any interactions on the Linux side.
	This is essential to meet the required accuracy. The local clock in the FPGA (64MHz or 96MHz) is stretched or compressed
	to meet 1e9 counts in between two pulses by a linear algorithm, in order to avoid bigger jumps in the timestamp.
	Rollover from 999999999 to 0 occurs synchronously to the 1PPS leading edge. In parallel, the Second-of-Day information is read
	from the TSIP serial data stream and also aligned to the 1PPS pulse. Both parts are then mapped into the 48bits that are available
	for the timestamp and transmitted with each Mode-S or Mode-A/C message.

	SecondsOfDay are using the upper 18 bits of the timestamp
	Nanoseconds are using the lower 30 bits. The value there directly converts into a 1ns based value and does not need to be converted by sample rate
*/


void MLATtime_beast(struct timespec *msgTime, uint64_t mlatTimestamp) {

	msgTime->tv_sec = Modes.baseTime.tv_sec + (mlatTimestamp >> 30);
	msgTime->tv_nsec = mlatTimestamp & BEAST_DROP_UPPER_34_BITS;
}


void MLATtime_dump1090(struct timespec *msgTime, uint64_t mlatTimestamp) {

	uint64_t mlat_realtime;


	msgTime->tv_nsec = Modes.baseTime.tv_nsec;
	msgTime->tv_sec = Modes.baseTime.tv_sec;

	mlat_realtime = (mlatTimestamp - Modes.firsttimestampMsg) / 12;
	msgTime->tv_nsec += 1000 * (mlat_realtime % 1000000);

	if (msgTime->tv_nsec > 999999999) {
		msgTime->tv_sec++;
		msgTime->tv_nsec -= 999999999;
	}
	msgTime->tv_sec += mlat_realtime / 1000000;
}

/*
addMLATtime(&mm.sysTimestampMsg, (mm.timestampMsg - Modes.firsttimestampMsg));
*/

//
//=========================================================================
//
// Write SBS output
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
static int decodeBinMessage(char *p) {
    int msgLen = 0;
    int msgrealLen = 0;
    int  j;
    char ch;
    unsigned char msg[MODES_LONG_MSG_BYTES];
    static struct modesMessage zeroMessage;
    struct modesMessage mm;

    memset(&mm, 0, sizeof(mm));



    ch = *p++; /// Get the message type
    if (0x1A == ch) {ch=*p++;}
    if (ch == '1' && Modes.mode_ac) { // skip ModeA/C unless user enables --modes-ac
    	msgLen = MODEAC_MSG_BYTES;

    } else if (ch == '2') {
        msgLen = MODES_SHORT_MSG_BYTES;
    } else if (ch == '3') {
        msgLen = MODES_LONG_MSG_BYTES;
    };

    if (msgLen) {
        mm = zeroMessage;

        // Mark messages received over the internet as remote so that we don't try to
        // pass them off as being received by this instance when forwarding them
        mm.remote      =    0;

        // Grab the timestamp (big endian format)
        mm.timestampMsg = 0;
        for (j = 0; j < 6; j++) {
            ch = *p++;
            msgrealLen++;
            mm.timestampMsg = mm.timestampMsg << 8 | (ch & 255);
            if (0x1A == ch) {p++; msgrealLen++;}
        }


	    Modes.MLATtimefunc(&mm.sysTimestampMsg, mm.timestampMsg);


	    msgrealLen++;
        ch = *p++;  // Grab the signal level
        mm.signalLevel = ((unsigned char)ch / 255.0);
        mm.signalLevel = mm.signalLevel * mm.signalLevel;
        if (0x1A == ch) {p++; msgrealLen++;}

        for (j = 0; j < msgLen; j++) { // and the data
            msg[j] = ch = *p++;
            msgrealLen++;
            if (0x1A == ch) {p++; msgrealLen++;}
        }

        if (msgLen == MODEAC_MSG_BYTES) { // ModeA or ModeC
            decodeModeAMessage(&mm, ((msg[0] << 8) | msg[1]));
        } else {
            int result;

            result = decodeModesMessage(&mm, msg);
            if (result < 0) {
            	if(result == -1) Modes.err_not_known_ICAO++;
            	if(result == -2) Modes.err_bad_crc++;
            	return 0;}

        }

        if(Modes.find_icao) {
        	icaoAddtoDB(mm.addr);
        }
        else {
    	if ((!Modes.show_only || mm.addr == Modes.show_only) || (Modes.show_only==0)) {
            if (Modes.output_bb != -1) {
            	msgrealLen += 2; // HEADER
            	p-=msgrealLen;
                j = write (Modes.output_bb, p, (ssize_t) (msgrealLen));
    		if (j != msgrealLen) {
    		fprintf(stderr, "Error. Write error in file %s\n",Modes.filename_extract);
            return 2;
    		}
    		Modes.msg_extracted++;
            }
    	}

		useModesMessage(&mm);
		if(!Modes.quiet && Modes.sbs_output) modesSendSBSOutput(&mm);

        if((Modes.output_kml != NULL) && (Modes.show_only == mm.addr)) {
            writeKMLcoordinates(Modes.output_kml, &mm);
        }
        }
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
    if ((ch == '1')) {
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


// Scan for header here -----------------------------------------------------
// Now we only read first beast message and get it MLAT timestamp to relative
// time calculations.

static void initMLATtime_dump(char *p, ssize_t size) {

	char beastmessage[MAX_MSG_LEN];
	ssize_t i, j, k = 0;

	while(k < size) {

	i = copyBinMessageSafe(p, BUF_SIZE - k, &beastmessage[0]);

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
	} else {p++; k++;}

	} else {p++; k++;}

	}
}
// ------------------------------------------------------------------------


int readbeastfile(void) {

	ssize_t i,k,ret_in, seek;
    char buffer[BUF_SIZE];
	struct stat sb;
	long long unsigned global;
	char beastmessage[MAX_MSG_LEN];

   fstat(Modes.input_bb, &sb);

   ret_in = read (Modes.input_bb, &buffer[0], BUF_SIZE);

   if (Modes.mlat_decoder == MLAT_DUMP1090) {
   initMLATtime_dump(&buffer[0], ret_in);
   }



	seek = 0;
	global = 0;

	while((ret_in > 0) && (!Modes.exit)) {

	k = 0;

	while(k < (ret_in + seek)) {

	icaoFilterExpire();
    trackPeriodicUpdate();

	i = copyBinMessageSafe(&buffer[k], ret_in + seek - k, &beastmessage[0]);
	if(i > 0) {
	Modes.msg_processed++;

	if (Modes.show_progress && (Modes.msg_processed % 0xFFF  == 0)) {printf("Processing... File offset 0x%llX (%llu%%), message #%llu\r", global, ((100*global)/(uint64_t)sb.st_size), Modes.msg_processed); }
	decodeBinMessage(&beastmessage[0]);

	if (Modes.max_messages && (Modes.msg_processed == Modes.max_messages)) {return 0; }


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

	ret_in = read (Modes.input_bb, &buffer[seek], k);
	}


    return 0;
}

//
//=========================================================================
//
