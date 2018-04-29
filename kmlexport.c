// Part of BEAST black box utility, a Mode S message BEAST decoder from file
//
// kmlexport.c: routines to write KML format
//
// Copyright (c) 2018 Denis G Dugushkin <dentall@mail.ru>
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

#include "kmlexport.h"

const char kml_head[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<kml xmlns=\"http://www.opengis.net/kml/2.2\"> <Document>"
 "<name>KML Flight reconstruction of ICAO %X</name>"
 "<description>Produced by BEAST black box https://github.com/denzen84/beastblackbox/blob/master/README.md</description> <Style id=\"yellowLineGreenPoly\">"
 "<LineStyle>"
 "<color>7fffae1f</color>"
 "<width>4</width>"
 "</LineStyle>"
 "<PolyStyle>"
 "<color>7fb8581b</color>"
 "</PolyStyle>"
 "</Style> <Placemark>"
 "<name>ICAO %X</name>"
 "<description>Flight</description>"
 "<styleUrl>#yellowLineGreenPoly</styleUrl>"
 "<LineString>"
 "<extrude>1</extrude>"
 "<tessellate>1</tessellate>"
 "<altitudeMode>absolute</altitudeMode>"
 "<coordinates>\r\n";

 const char kml_end[] = "</coordinates>"
 "</LineString> </Placemark>"
 "</Document> </kml>\r\n";

 void writeKMLpreamble(FILE *f, uint32_t icao) {
	fprintf(f, kml_head, icao, icao);
 }

 void writeKMLcoordinates(FILE *f, struct modesMessage *mm) {
	 int alt = 0;
	 struct aircraft *a = Modes.aircrafts;
	 if (mm->msgtype == 17 || mm->msgtype == 18) {
		if (mm->metype >= 9 && mm->metype <= 18) {
            // It's what we need
			// Fields 15 and 16 are the Lat/Lon (if we have it)
			if (mm->altitude_valid && mm->cpr_decoded) {
			if (mm->altitude_source == ALTITUDE_BARO) {
                alt = mm->altitude;
            } else if (trackDataValid(&a->gnss_delta_valid)) {
                alt = mm->altitude - a->gnss_delta;
            } else return;

			fprintf(f, "%1.5f,%1.5f,%.1f\r\n", mm->decoded_lon,mm->decoded_lat,(float) alt*0.3048);
			}
		}
     }
}

void writeKMLend(FILE *f) {
    fprintf(f, kml_end);
 }
