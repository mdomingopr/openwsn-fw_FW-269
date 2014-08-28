#include "openwsn.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
#include "idmanager.h"

//=========================== variables =======================================

otf_tracks_t   otf_tracks;

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);
bool otf_find_track(sixtop_trackId_t* trackId, uint8_t* trackPosition, otf_track_state_t state);
bool otf_find_or_create_track(sixtop_trackId_t* trackId, uint8_t *trackPosition);
void otf_addCell_internal(bool rxAdded, sixtop_trackId_t* trackId, uint16_t numCells);
void otf_removeCell_internal(bool rxDeleted, sixtop_trackId_t* trackId, uint16_t numCells);
void printTracks();

//=========================== public ==========================================

/**
 * \brief Initializes the On The Fly (OTF) scheduling
 */
void otf_init(void) {
   uint8_t i;
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS; i++) {
      otf_tracks.info[i].state = OTF_TRACK_NOT_USED;
      otf_tracks.info[i].trackId.ownerInstId = 0;
      otf_tracks.info[i].trackId.trackOwnerAddr_16b[0] = 0x00;
      otf_tracks.info[i].trackId.trackOwnerAddr_16b[1] = 0x00;
      otf_tracks.info[i].rxNumAllocatedCells = 0;
      otf_tracks.info[i].txNumAllocatedCells = 0;
   }
}


/**
 * \brief Called from upper layers to start a process of adding cells
 *
 * \param trackId id of the track 
 * \param numCells Num of cell to add
 */
void otf_addCell(sixtop_trackId_t* trackId, uint16_t numCells) {
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("--OTF_ADDCELL\n");
   printTracks();
   otf_addCell_internal(FALSE, trackId, numCells);
}


/**
 * \brief Called by the lower layer (6TOP) when cell(s) has been added
 *
 * \param trackId id of the track 
 * \param numCells Num of cells that were added
 */
void otf_notif_addedCell(sixtop_trackId_t* trackId, uint16_t numCells) {
   printTracks();
   otf_addCell_internal(TRUE, trackId, numCells);
}




/**
 * \brief Called from upper layers to start a process of removing cells
 *
 * \param trackId id of the track 
 * \param numCells Num of cell to remove
 */
void otf_removeCell(sixtop_trackId_t* trackId, uint16_t numCells) {
   otf_removeCell_internal(FALSE, trackId, numCells);
}
/**
 * \brief Called by the lower layer (6TOP) when a cell(s) has been removed
 *
 * \param trackId id of the track 
 * \param numCells Num of cells taht were removed
 */
void otf_notif_removedCell(sixtop_trackId_t* trackId, uint16_t numCells) {
   otf_removeCell_internal(TRUE, trackId, numCells);
}


void otf_notif_removeCellReqSent(sixtop_trackId_t* trackId, uint16_t numCells) {
   uint8_t trackPosition;
   bool found;
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("---_BB\n");   
   found = otf_find_track(trackId, &trackPosition, OTF_TRACK_ADDING_CELL_REMOVING_TRACK);
   printf("found %d\n", found);
   if (TRUE == found) {
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---BB\n");   

      otf_tracks.info[trackPosition].tmpCells += otf_tracks.info[trackPosition].txNumAllocatedCells;
      otf_tracks.info[trackPosition].txNumAllocatedCells = 0;
      otf_tracks.info[trackPosition].state = OTF_TRACK_ADDING_CELL_ADDING_TRACK;
      scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
   }
   else {
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---ERROR!!! TrackId not Found.\n");   
   }

}

//=========================== private =========================================

void otf_addCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor, foundTrack;
   uint8_t i, trackPosition;
   uint16_t trackId;

   foundTrack = FALSE;
#ifdef OTF_REM_ADD_STRATEGY
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == foundTrack; i++) {
      if (OTF_TRACK_ADDING_CELL_ADDING_TRACK == otf_tracks.info[i].state) {
         foundTrack = TRUE;
         trackPosition = i;
      }
   }
#endif

   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("---ADDCELL_TASK\n");
   if (FALSE == foundTrack) {
      for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == foundTrack; i++) {
         if (OTF_TRACK_ADDING_CELL_TO_PARENT == otf_tracks.info[i].state) {
            foundTrack = TRUE;
            trackPosition = i;
         }
      }
   }
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("ADDCELLTASK: foundTrack %d at %d\n", foundTrack, trackPosition);

   if (TRUE == foundTrack) {
#if 0      /* \TODO mdomingo: check errors */
      otf_tracks.info[trackPosition].state = OTF_TRACK_ADDING_CELL_TO_PARENT_STEP2;
#else
      //printf("Num alloc cells: %d\r\n", otf_tracks.info[trackPosition].txNumAllocatedCells);
#endif

      // get preferred parent
      foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
      if (foundNeighbor==FALSE) {
         printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
         printf("----ROOT!!!!!\r\n");
         otf_tracks.info[trackPosition].state = OTF_TRACK_NOT_USED; // \TODO mdomingo: Something wierd happened. What to do?
         return;
      }
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---CALL SIXTOP_ADDCELLS\n");   
      // call sixtop
      /* \TODO mdomingo: parameters */
      sixtop_addCells(
            1, /* slot frame id */
            otf_tracks.info[trackPosition].tmpCells, 
            0, /* link option*/
            &neighbor, 
            &otf_tracks.info[trackPosition].trackId,
            0); /* QoS*/
      /* \TODO mdomingo: set tmpCells to 0 */

      otf_tracks.info[trackPosition].txNumAllocatedCells += otf_tracks.info[trackPosition].tmpCells; /* \TODO mdomingo: probably it should be done after, when it is confirmed by the parent. */
      otf_tracks.info[trackPosition].tmpCells = 0;
      switch (otf_tracks.info[trackPosition].state) {
         case OTF_TRACK_ADDING_CELL_TO_PARENT:
            otf_tracks.info[trackPosition].state = OTF_TRACK_IN_USE;
            break;
         case OTF_TRACK_ADDING_CELL_ADDING_TRACK:
            otf_tracks.info[trackPosition].state = OTF_TRACK_IN_USE;
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---CC\n");   
            break;
         default:
            /* Shouldn't happend */
            otf_tracks.info[trackPosition].state = OTF_TRACK_NOT_USED;
            break;
      }
   }
}

void otf_removeCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor, foundTrack;
   uint8_t i, trackPosition;
   uint16_t trackId, cellsToRemove;

   foundTrack = FALSE;
#ifdef OTF_REM_ADD_STRATEGY
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == foundTrack; i++) {
      if (OTF_TRACK_ADDING_CELL_REMOVING_TRACK == otf_tracks.info[i].state) {
         foundTrack = TRUE;
         trackPosition = i;
      }
   }
#endif

   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("---REMOVECELL_TASK\n");
   if (FALSE == foundTrack) {
      for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == foundTrack; i++) {
         if (OTF_TRACK_REMOVING_CELL_TO_PARENT == otf_tracks.info[i].state) {
            foundTrack = TRUE;
            trackPosition = i;
         }
      }
   }
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("REMOVECELLTASK: foundTrack %d at %d\n", foundTrack, trackPosition);

   if (TRUE == foundTrack) {

      // get preferred parent
      foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
      if (foundNeighbor==FALSE) {
         printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
         printf("----ROOT!!!!!\r\n");
         otf_tracks.info[trackPosition].state = OTF_TRACK_NOT_USED; // \TODO mdomingo: Something wierd happened. What to do?
         return;
      }



      switch (otf_tracks.info[trackPosition].state) { /* \TODO mdomingo treat errors */
         case OTF_TRACK_REMOVING_CELL_TO_PARENT:
            cellsToRemove = otf_tracks.info[trackPosition].tmpCells;
            break;
         case OTF_TRACK_ADDING_CELL_REMOVING_TRACK:
            cellsToRemove = otf_tracks.info[trackPosition].txNumAllocatedCells;
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---AA\n");   
            break;
         default:
            /* Shouldn't happen */
            cellsToRemove = 0;
            break;
      }


      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---CALL SIXTOP_REMOVECELLS\n");   
      // call sixtop
      /* \TODO mdomingo: parameters */
      sixtop_removeCells(
            1, /* slot frame id */
            cellsToRemove, 
            0, /* link option*/
            &neighbor, 
            &otf_tracks.info[trackPosition].trackId);
      /* \TODO mdomingo: set tmpCells to 0 */
      
      switch (otf_tracks.info[trackPosition].state) { /* \TODO mdomingo treat errors */
         case OTF_TRACK_REMOVING_CELL_TO_PARENT:
            otf_tracks.info[trackPosition].txNumAllocatedCells -= otf_tracks.info[trackPosition].tmpCells; /* \TODO mdomingo: protect */
            otf_tracks.info[trackPosition].tmpCells = 0;
            otf_tracks.info[trackPosition].state = OTF_TRACK_IN_USE; 
            break;
         case OTF_TRACK_ADDING_CELL_REMOVING_TRACK:
#if 0
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---BB\n");   
            otf_tracks.info[trackPosition].tmpCells += otf_tracks.info[trackPosition].txNumAllocatedCells;
            otf_tracks.info[trackPosition].txNumAllocatedCells = 0;
            otf_tracks.info[trackPosition].state = OTF_TRACK_ADDING_CELL_ADDING_TRACK;
            scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
#endif            
            break;
         default:
            /* Something whent wrong. */
            otf_tracks.info[trackPosition].state = OTF_TRACK_IN_USE;
            break;
      }
   }
}

/**
 * \brief Internal function that remove cells with your parent
 *
 * \param rxAdded were rx slots deleted?
 * \param trackId id of the track 
 * \param numCells Num of cell to remove
 */
void otf_removeCell_internal(bool rxDeleted, sixtop_trackId_t* trackId, uint16_t numCells) {
   uint8_t trackPosition;

   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("RemoveCellInternal\n");

   if (TRUE == otf_find_track(trackId, &trackPosition, OTF_TRACK_IN_USE)) {
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("TrackFound\n");

      if (TRUE == rxDeleted) {
         otf_tracks.info[trackPosition].rxNumAllocatedCells -= numCells; /* \TODO mdomingo: check */
      }
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("--1. Numcells: %d, RxCells:%d, TxCells:%d\n", numCells, otf_tracks.info[trackPosition].rxNumAllocatedCells, otf_tracks.info[trackPosition].txNumAllocatedCells);

      otf_tracks.info[trackPosition].tmpCells = numCells;
      otf_tracks.info[trackPosition].state = OTF_TRACK_REMOVING_CELL_TO_PARENT;
      scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);

      printf("--2. Numcells: %d, RxCells:%d, TxCells:%d\n", numCells, otf_tracks.info[trackPosition].rxNumAllocatedCells, otf_tracks.info[trackPosition].txNumAllocatedCells);
   }
   else {   /* The track is not in the list. */
      /* \TODO mdomingo: log error.*/
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("--ERROR!!! No track to delete with that id, or invalid state\n");
   }
}

/**
 * \brief Internal function that adds cells with your parent
 *
 * \param rxAdded were rx slots added?
 * \param trackId id of the track 
 * \param numCells Num of cell to add
 */
void otf_addCell_internal(bool rxAdded, sixtop_trackId_t* trackId, uint16_t numCells) {
   uint8_t trackPosition;
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("--OTF_ADDCELL_INTERNAL. Id: %d, NumCells: %d\n", trackId->ownerInstId, numCells);

#ifdef OTF_REM_ADD_STRATEGY
   /* If the track already exist, delete it first */
   if (FALSE == rxAdded && TRUE == otf_find_track(trackId, &trackPosition, OTF_TRACK_IN_USE)) {
      otf_tracks.info[trackPosition].tmpCells = numCells;
      otf_tracks.info[trackPosition].state = OTF_TRACK_ADDING_CELL_REMOVING_TRACK;
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("---_AA\n");   
      scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);
      return;
   }
#endif

   if (TRUE == otf_find_or_create_track(trackId, &trackPosition)) {
      if (TRUE == rxAdded) {
         otf_tracks.info[trackPosition].rxNumAllocatedCells = numCells;
      }
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("--1. Numcells: %d, RxCells:%d, TxCells:%d\n", numCells, otf_tracks.info[trackPosition].rxNumAllocatedCells, otf_tracks.info[trackPosition].txNumAllocatedCells);

      otf_tracks.info[trackPosition].tmpCells = numCells;
      otf_tracks.info[trackPosition].state = OTF_TRACK_ADDING_CELL_TO_PARENT;
      scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);

      printf("--2. Numcells: %d, RxCells:%d, TxCells:%d\n", numCells, otf_tracks.info[trackPosition].rxNumAllocatedCells, otf_tracks.info[trackPosition].txNumAllocatedCells);

   }
   else {   /* All memory alocated for storing tracks is used. */
      /* \TODO mdomingo: log error.*/
      printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("--Error creating tracks\n");
   }
}


/**
 * \brief Search the position of a track locally.
 *
 * \param trackId id of the track 
 * \param trackPosition Returns the position of the track inside the vector.
 * \param state State to search the track. If OTF_TRACK_ANY_STATE, this parameter is not used.
 *
 * \return TRUE if it was found. FALSE otherwise.
 */
bool otf_find_track(sixtop_trackId_t* trackId, uint8_t* trackPosition, otf_track_state_t state) {
   uint8_t i;
   bool trackFound = FALSE;
   bool ret = FALSE;
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == trackFound; i++){    /* Search the track */
      printf("%d\n", i);
      if (sixtop_is_same_trackId(&otf_tracks.info[i].trackId, trackId)) {
         trackFound = TRUE;
         if (OTF_TRACK_ANY_STATE != state) {
            if (state == otf_tracks.info[i].state) {
               ret = TRUE;
               *trackPosition = i;
            }
         }
         else {
            ret = TRUE;
         }
      }
   }
   return ret; 
}

/**
 * \brief Search the position of a track locally. If it doesn't exist it search an empty one.
 *
 * \param trackId id of the track 
 * \param trackPosition Returns the position of the track inside the vector
 *
 * \return TRUE if it was found or created. FALSE if it wasn't found and there is no more memory to create it.
*/
bool otf_find_or_create_track(sixtop_trackId_t* trackId, uint8_t* trackPosition) {
   uint8_t i;
   bool trackFound = FALSE;
   bool ret = FALSE;
   printTracks(); 
   trackFound = otf_find_track(trackId, trackPosition, OTF_TRACK_ANY_STATE);
   //debugPrint_id();
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("FINDORCREATETRACK: FIND: trackFound %d at %d\n", trackFound, *trackPosition);
   
   if (TRUE == trackFound) {
      if (OTF_TRACK_IN_USE == otf_tracks.info[*trackPosition].state) {
         ret = TRUE;
      }
   }
   else {/* If not found, search search a not used track */
      for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS && FALSE == trackFound; i++) {
         if (OTF_TRACK_NOT_USED == otf_tracks.info[i].state) {
            trackFound = TRUE;
            ret = TRUE;
            *trackPosition = i;
            otf_tracks.info[i].state = OTF_TRACK_IN_USE;
            otf_tracks.info[i].trackId.ownerInstId = trackId->ownerInstId;
            otf_tracks.info[i].trackId.trackOwnerAddr_16b[1] = trackId->trackOwnerAddr_16b[1]; 
            otf_tracks.info[i].trackId.trackOwnerAddr_16b[0] = trackId->trackOwnerAddr_16b[0]; 
            otf_tracks.info[i].rxNumAllocatedCells = 0;
            otf_tracks.info[i].txNumAllocatedCells = 0;
         }
      }
   }
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("FINDORCREATETRACK: trackFound %d at %d\n", trackFound, *trackPosition);
   return ret;
}


void printTracks() {
   uint8_t i;
   printf("M%02X%02X: ", idmanager_getMyID(ADDR_16B)->addr_16b[0], idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf(" Tracks: ");
   for (i=0; i<OTF_MAX_NUMBER_OF_TRACKS; i++) {
      printf("Position %d, ", i);
      printf("State%d, ", otf_tracks.info[i].state);
      printf("trackId%d, ", otf_tracks.info[i].trackId.ownerInstId, otf_tracks.info[i].trackId.trackOwnerAddr_16b);
      printf("rxNumAllocatedCells%d, ", otf_tracks.info[i].rxNumAllocatedCells);
      printf("txNumAllocatedCells%d, ", otf_tracks.info[i].txNumAllocatedCells);
   }
}
