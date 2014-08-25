#ifndef __OTF_H
#define __OTF_H

/**
\addtogroup MAChigh
\{
\addtogroup otf
\{
*/

#include "openwsn.h"

//=========================== define ==========================================

#define OTF_MAX_NUMBER_OF_TRACKS 5u

//=========================== typedef =========================================

typedef enum {
   OTF_TRACK_NOT_USED   = 0x00,           // The bundle is not used
   OTF_TRACK_ADDING_CELL_TO_PARENT,     // The bundle has been allocated locally, but it has to be executed with your parent
   OTF_TRACK_DELETING_CELL_TO_PARENT,   // The bundle has been removed locally, but it has to be executed with your parend
   OTF_TRACK_IN_USE,                      // The bundle is used

} otf_track_state_t;

typedef struct {
   otf_track_state_t state;
   sixtop_trackId_t  trackId;
   uint16_t          rxNumAllocatedCells;
   uint16_t          txNumAllocatedCells;
} otf_track_t;

typedef struct {
   otf_track_t info [OTF_MAX_NUMBER_OF_TRACKS];
} otf_tracks_t;

//=========================== module variables ================================

//=========================== prototypes ======================================

// admin
void     otf_init(void);
// notification from sixtop
void     otf_notif_addedCell(sixtop_trackId_t* trackId, uint16_t numCells);
void     otf_notif_removedCell(sixtop_trackId_t* trackId, uint16_t numCells);


// COAP
void     otf_addCell(sixtop_trackId_t* trackId, uint16_t numCells);
void     otf_removeCell(sixtop_trackId_t* trackId, uint16_t numCells);

/**
\}
\}
*/

#endif
