#ifndef KMLEXPORT_H_INCLUDED
#define KMLEXPORT_H_INCLUDED

#include <stdio.h>
#include "beastblackbox.h"
struct modesMessage;

void writeKMLpreamble(FILE *f, uint32_t icao);
void writeKMLcoordinates(FILE *f, struct modesMessage *mm);
void writeKMLend(FILE *f);


#endif // KMLEXPORT_H_INCLUDED

