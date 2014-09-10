#include "openwsn.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
//mdomingo: test

//=========================== variables =======================================
otf_vars_t otf_vars;

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);
void otf_timer_cb(void);
void otf_timer_fired_task(void);

//=========================== public ==========================================

void otf_init(void) {
   otf_vars.timerId = opentimers_start(OTF_TIMER_PERIOD_MS,
                                       TIMER_PERIODIC,
                                       TIME_MS,
                                       otf_timer_cb);
}

void otf_notif_addedCell(void) {
#if 0 // mdomingo: don't propagate the addition of cells to your parent
   scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
#endif 
}

void otf_notif_removedCell(void) {
#if 0 // mdomingo: don't propagate the addition of cells to your parent
   scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);
#endif
}

//=========================== private =========================================
void otf_addCell(void) {
   scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
}

void otf_removeCell(void) {
   scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);
}

/**
 * \brief Function called by the otf timer
 */
void otf_timer_cb() {
   scheduler_push_task(otf_timer_fired_task,TASKPRIO_OTF);
}


/**
 * \brief Task that calculates the bandwidth requirements and increases/decreases radio cells
 */
void otf_timer_fired_task(void) {


}

void otf_addCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   // call sixtop
   sixtop_addCells(
      &neighbor,
      1
   );
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
