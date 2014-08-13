#include "openwsn.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"

//=========================== variables =======================================

#define OTF_MAX_NUMBER_OF_TRACKS 5u


typedef enum {
   OTF_TRACK_NOT_USED   = 0x00,           // The bundle is not used
   OTF_TRACK_ADDING_CELL_TO_PARENT,     // The bundle has been allocated locally, but it has to be executed with your parent
   OTF_TRACK_DELETING_CELL_TO_PARENT,   // The bundle has been removed locally, but it has to be executed with your parend
   OTF_TRACK_IN_USE,                      // The bundle is used

} otf_track_state_t;


typedef struct {
   otf_track_state_t state;
   uint16_t          trackId;
   uint16_t          numAllocatedCells;
} otf_track_t;



otf_track_t   otf_tracks [OTF_MAX_NUMBER_OF_TRACKS];

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);
bool otf_find_track(uint16_t trackId, uint8_t *trackPosition);
bool otf_find_or_create_track(uint16_t trackId, uint8_t *trackPosition);
void otf_addCell_internal(uint16_t trackId, uint16_t numCells);

//=========================== public ==========================================

/**
 * \brief Initializes the On The Fly (OTF) scheduling
 */
void otf_init(void) {
   uint8_t i;
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS; i++) {
      otf_tracks[i].state = OTF_TRACK_NOT_USED;
      otf_tracks[i].trackId = 0;
      otf_tracks[i].numAllocatedCells = 0;
   }
}

/**
 * \brief Internal function that adds cells with your parent
 *
 * \param trackId Id of the track to add
 * \param numCells Num of cell to add
 */
void otf_addCell_internal(uint16_t trackId, uint16_t numCells) {
   uint8_t trackPosition;
   if (TRUE == otf_find_or_create_track(trackId, &trackPosition)) {
      switch (otf_tracks[trackPosition].state) {
         case OTF_TRACK_IN_USE:
               if (numCells > otf_tracks[trackPosition].numAllocatedCells) {
                  otf_tracks[trackPosition].numAllocatedCells = numCells;
                  otf_tracks[trackPosition].state = OTF_TRACK_ADDING_CELL_TO_PARENT;
                  scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
               }
               else { /* Something went wrong. Shouldn't happen */
                  /* \TODO mdomingo: log error. */
               }
            break;
         default:
            /* \TODO mdomingo: log error. */
            break;
      }

   }
   else {   /* All memory alocated for storing tracks is used. */
      /* \TODO mdomingo: log error.*/
   }
}

/**
 * \brief Called from upper layers to start a process of adding cells
 *
 * \param trackId Id of the track to add
 * \param numCells Num of cell to add
 */
void otf_addCell(uint16_t trackId, uint16_t numCells) {
   otf_addCell_internal(trackId, numCells);
}


/**
 * \brief Called by the lower layer (6TOP) when cell(s) has been added
 *
 * \param trackId Id of the track where cells were added
 * \param numCells Num of cell to cells that were added
 */
void otf_notif_addedCell(uint16_t trackId, uint16_t numCells) {
   otf_addCell_internal(trackId, numCells);
}


/**
 * \brief Called by the lower layer (6TOP) when a cell(s) has been removed
 *
 * \param trackId Id of the track where cells were removed
 */
void otf_notif_removedCell(uint16_t trackId, uint16_t numCells) {
   scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);
}

//=========================== private =========================================

void otf_addCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor, foundTrack;
   uint8_t i, trackPosition;
   uint16_t trackId;

   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == foundTrack; i++) {
      if (OTF_TRACK_ADDING_CELL_TO_PARENT == otf_tracks[i].state) {
         foundTrack = TRUE;
         trackPosition = i;
      }
   }

   if (TRUE == foundTrack) {
#if 0      /* \TODO mdomingo: check errors */
      otf_tracks[i].state = OTF_TRACK_ADDING_CELL_TO_PARENT_STEP2;
#else
      otf_tracks[i].state = OTF_TRACK_IN_USE;
      //printf("Num alloc cells: %d\r\n", otf_tracks[i].numAllocatedCells);
#endif

      // get preferred parent
      foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
      if (foundNeighbor==FALSE) {
//printf("----ROOT!!!!!\r\n");
         otf_tracks[i].state = OTF_TRACK_NOT_USED; // \TODO mdomingo: Something wierd happened. What to do?
         return;
      }
   
      // call sixtop
      sixtop_addCells(&neighbor, otf_tracks[i].numAllocatedCells);
   }
}

void otf_removeCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   // call sixtop
   sixtop_removeCell(
      &neighbor
   );
}

/**
 * \brief Search the position of a track locally.
 *
 * \param trackId Id of the track to search
 * \param trackPosition Returns the position of the track inside the vector.
 *
 * \return TRUE if it was found. FALSE otherwise.
 */
bool otf_find_track(uint16_t trackId, uint8_t *trackPosition) {
   uint8_t i;
   bool trackFound = FALSE;
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == trackFound; i++){    /* Search the track */
      if (trackId == otf_tracks[i].trackId) {
         trackFound = TRUE;
         *trackPosition = i;
      }
   }
   return trackFound; 
}

/**
 * \brief Search the position of a track locally. If it doesn't exist it search an empty one.
 *
 * \param trackId Id of the track to search
 * \param trackPosition Returns the position of the track inside the vector
 *
 * \return TRUE if it was found or created. FALSE if it wasn't found and there is no more memory to create it.
 */
bool otf_find_or_create_track(uint16_t trackId, uint8_t *trackPosition) {
   uint8_t i;
   bool trackFound = FALSE;
   
   trackFound = otf_find_track(trackId, trackPosition);
   
   if (FALSE == trackFound) {     /* If not found, search a not used track */
      for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == trackFound; i++) {
         if (OTF_TRACK_NOT_USED == otf_tracks[i].state) {
            trackFound = TRUE;
            *trackPosition = i;
            otf_tracks[i].state = OTF_TRACK_IN_USE;
            otf_tracks[i].trackId = trackId;
            otf_tracks[i].numAllocatedCells = 0;
         }
      }
   }
   return trackFound;
}


