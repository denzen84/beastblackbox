//  BEAST black box utility
//  Utility to read binary BEAST traffic from the file
//  Log file could be generated using netcat:
//  nc 127.0.0.1 30005 > radar.log
//	For more information please visit https://github.com/denzen84/beastblackbox/blob/master/README.md
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
#include "beastblackbox.h"


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
void blackboxInitConfig(void) {
    // Default everything to zero/NULL

    memset(&Modes,    0, sizeof(Modes));
    Modes.mlat_decoder			  = MLAT_NONE;
	Modes.MLATtimefunc 			  = &MLATtime_none;
    Modes.check_crc               = 1;
}

//
// ================================ Main ====================================
//
void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"| BEAST: black box utility                                            v0.999a|\n"
"-----------------------------------------------------------------------------\n"
"Build: %s\n\n"

  "--filename <file>        Source file to proceed\n"
  "--extract <file>         Extract BEAST data to new file (if no ICAO filter specified it just copies the source)\n"
  "--only-find-icaos        Find all unique ICAOs in the file and print ICAOs list (WARNING: also shows non-ICAO!)\n"
  "--export-kml <file>      Export coordinates and height to KML (WARNING: works only with --filter-icao)\n"
  "--mlat-time <type>       Decode MLAT timestamps in specified manner. Types are: none (default), beast, dump1090\n"
  "--init-time-unix <sec>   Start time (UNIX epoch, format: ss.ms) to calculate realtime using MLAT timestamps\n"
  "--localtime              Decode time as local time (default: UTC)\n"
  "--sbs-output             Show messages in SBS format (default: dump1090 style)\n"
  "--filter-icao <addr>     Show only messages from the given ICAO\n"
  "--max-messages <count>   Limit messages count from the start of the file (default: all)\n"
  "--show-progress          Show progress during file operation\n"
  "--quiet                  Do not output decoded messages to stdout (useful for --extract and --export-kml)\n\n"
  "Additional BEAST options:\n"
  "--modeac                 Enable decoding of SSR modes 3/A & 3/C\n"
  "--gnss                   Show altitudes as HAE/GNSS (with H suffix) when available\n"
  "--no-crc-check           Disable messages with broken CRC (discouraged)\n"
  "--no-fix                 Disable single-bits error correction using CRC\n"
  "--fix                    Enable single-bits error correction using CRC\n"
  "--aggressive             More CPU for more messages (two bits fixes, ...)\n"
  "--help                   Show this help\n",
  MODES_DUMP1090_VERSION
    );
}

//
//=========================================================================
//
void blackboxInit(void) {

	struct tm stTime_init;

    // Prepare error correction tables
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();

    Modes.input_bb = -1;
    Modes.output_bb = -1;
	Modes.output_kml = NULL;

	if (Modes.filename == NULL) {
			showHelp();
			fprintf(stderr, "\nERROR: no file specified. Nothing to do. Use --filename option or --help for more info.\n\n");
            exit(1);
	}

	if((Modes.filename_kml != NULL) && (!Modes.show_only)) {
		    showHelp();
			fprintf(stderr, "\nERROR: no filter ICAO specified. Option --export-kml works only with --filter-icao. Use --help for more info\n\n");
			exit(1);
	}

   // Init the files
	Modes.input_bb = open(Modes.filename, O_RDONLY);
    if (Modes.input_bb == -1) {
            fprintf(stderr, "Error. Unable to open for read BEAST file %s\n",Modes.filename);
            exit(1);
    }

	if (Modes.filename_extract != NULL) {
		Modes.output_bb = open(Modes.filename_extract, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (Modes.output_bb == -1) {
            fprintf(stderr, "Error. Unable to open for write BEAST file %s\n",Modes.filename_extract);
            exit(1);
		}
	}

	if (Modes.filename_kml != NULL) {
        Modes.output_kml = fopen(Modes.filename_kml, "w");
		if (Modes.output_kml == NULL) {
            fprintf(stderr, "Error. Unable to open for write KML file %s\n",Modes.filename_extract);
            exit(1);
		}
		writeKMLpreamble(Modes.output_kml, Modes.show_only);
	}

	if((Modes.mlat_decoder == MLAT_BEAST) && Modes.baseTime.tv_sec) {
		gmtime_r(&Modes.baseTime.tv_sec, &stTime_init);
		stTime_init.tm_hour = 0;
		stTime_init.tm_min = 0;
		stTime_init.tm_sec = 0;
		Modes.baseTime.tv_sec = mktime(&stTime_init) + time_offset();
	}
}

int main(int argc, char **argv) {
    // Initialization
    int j;
	double t;

	blackboxInitConfig();


    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)


    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments

		if (!strcmp(argv[j],"--modeac")) {
            Modes.mode_ac = 1;
		} else if (!strcmp(argv[j],"--localtime")) {
            Modes.useLocaltime = 1;
		} else if (!strcmp(argv[j],"--only-find-icaos")) {
			Modes.find_icao = 1;
        } else if (!strcmp(argv[j],"--init-time-unix") && more) {
			Modes.baseTime.tv_nsec = (int) (1000000000 * modf(atof(argv[++j]),&t));
			Modes.baseTime.tv_sec = (int) t;
		} else if (!strcmp(argv[j],"--export-kml") && more) {
			Modes.filename_kml = strdup(argv[++j]);
	    } else if (!strcmp(argv[j],"--filename") && more) {
		    Modes.filename = strdup(argv[++j]);
	    } else if (!strcmp(argv[j],"--mlat-time") && more) {
	    	++j;
	    	if (!strcmp(argv[j],"beast")) {
	    		Modes.mlat_decoder = MLAT_BEAST;
	    		Modes.MLATtimefunc = &MLATtime_beast;
	    	} else if (!strcmp(argv[j],"dump1090")) {
	    		Modes.mlat_decoder = MLAT_DUMP1090;
	    		Modes.MLATtimefunc = &MLATtime_dump1090;
	    	} else if (!strcmp(argv[j],"none")) {
	    		Modes.mlat_decoder = MLAT_NONE;
	    		Modes.MLATtimefunc = &MLATtime_none;
	    	} else {
	    		fprintf(stderr, "Unknown argument for option --mlat-time: '%s'.\n\n", argv[j]);
	    		exit(1);
	    	}
		} else if (!strcmp(argv[j],"--extract") && more) {
		    Modes.filename_extract = strdup(argv[++j]);
		} else if (!strcmp(argv[j],"--filter-icao") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
        } else if (!strcmp(argv[j],"--max-messages") && more) {
            Modes.max_messages = strtoul(argv[++j],NULL, 10);
        } else if (!strcmp(argv[j],"--sbs-output")) {
            Modes.sbs_output = 1;
        } else if (!strcmp(argv[j],"--quiet")) {
            Modes.quiet = 1;
		} else if (!strcmp(argv[j],"--show-progress")) {
            Modes.show_progress = 1;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--fix")) {
            Modes.nfix_crc = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.nfix_crc = MODES_MAX_BITERRORS;
        } else if (!strcmp(argv[j],"--hae") || !strcmp(argv[j],"--gnss")) {
            Modes.use_gnss = 1;
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            showHelp();
            exit(1);
        }
    }

    blackboxInit();

	// Main routine
    readbeastfile();

	printf("\n");
	if(Modes.find_icao) {
		icaoPrintDB();
	} else {

	if (Modes.msg_extracted) printf("Extracted %llu messages\n", Modes.msg_extracted);
	printf("Total processed %llu messages\n", Modes.msg_processed);

	if(Modes.err_bad_crc) printf("WARNING! Found %d messages with bad CRC\n", Modes.err_bad_crc);
	if(Modes.err_not_known_ICAO) printf("WARNING! Found %d messages that might be valid, but we couldn't validate the CRC against a known ICAO\n", Modes.err_not_known_ICAO);
	}

    // Close all files
    close (Modes.input_bb);
	close (Modes.output_bb);
	if(Modes.output_kml != NULL) {
    writeKMLend(Modes.output_kml);
    fclose(Modes.output_kml);
	}


    return (0);
}
//
//=========================================================================
//
