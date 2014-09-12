#include "openwsn.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
//mdomingo: test

#define PID_WINDUPGUARD_VALUE       2.0 //10.0
#define PID_PROPORTIONAL_GAIN_VALUE 0.5 //4.0
#define PID_INTEGRAL_GAIN_VALUE     0.5 //5.0
#define PID_DERIVATIVE_GAIN_VALUE   0.5 //3.0

//=========================== variables =======================================
otf_vars_t otf_vars;

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);
void otf_timer_cb(void);
void otf_timer_fired_task(void);

void pid_zeroize(PID_t* pid);
void pid_update(PID_t* pid, double curr_error, double dt);
//=========================== public ==========================================

void otf_init(void) {
   uint8_t i;
   for (i=0; i<OTF_MAX_NUM_NEIGHBOR_STATISTICS; i++) {
      otf_vars.neighbor[i].used    = FALSE;
      //otf_vars.neighbor[i].lastAsn;
      //otf_vars.neighbor[i].address;

      pid_zeroize(&(otf_vars.neighbor[i].pid)); 
   }
#if 0
   otf_vars.timerId = opentimers_start(OTF_TIMER_PERIOD_MS,
                                       TIMER_PERIODIC,
                                       TIME_MS,
                                       otf_timer_cb);
#else
{
   open_addr_t* myAddr;
   myAddr = idmanager_getMyID(ADDR_16B);
   if (0x02 == myAddr->addr_16b[1]){
      otf_vars.timerId = opentimers_start(OTF_TIMER_PERIOD_MS,
                                          TIMER_PERIODIC,
                                          TIME_MS,
                                          otf_timer_cb);
   }
}
#endif
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

void     otf_notif_updateStatistics(asn_t* currentAsn, open_addr_t* address, uint8_t numMesgsQueue) {
   uint8_t i;
   int kk;
   bool found = FALSE;
   uint8_t position;

#if 1   
   //otf_printLocalVar();
#endif

#if 1
   printf("%02X ", idmanager_getMyID(ADDR_16B)->addr_16b[1]);
   printf("*****OTF: ");
   printf("ASN: %04X, ", currentAsn->bytes0and1);
   printf("64B addr: ");
#if 0
   for(kk=0;kk<8;kk++) {
      printf("%02X ", address->addr_64b[kk]);
   }
#else
   printf("%02X ", address->addr_64b[7]);
#endif
   printf("NumMsgs: %d, ", numMesgsQueue);
   printf("\n");
#endif

   //Find neighbor
   for (i=0; i<OTF_MAX_NUM_NEIGHBOR_STATISTICS && FALSE == found; i++) {
      if (TRUE == otf_vars.neighbor[i].used && packetfunctions_sameAddress(&otf_vars.neighbor[i].address, address)) {
         position  = i;
         found = TRUE;
//         printf("XXXX-Found %d\n", position);
      }
   }

   //If no found create it
   if(!found) {
      for (i=0; i<OTF_MAX_NUM_NEIGHBOR_STATISTICS && FALSE == found; i++) {
         if (FALSE == otf_vars.neighbor[i].used) {
            otf_vars.neighbor[i].used = TRUE;
            memcpy(&otf_vars.neighbor[i].address, address, sizeof(open_addr_t));
            position = i;
            found = TRUE;
//            printf("XXXX-Created %d\n", position);
         }
      }
   }

   //Add statistics
   if (TRUE == found) {
      pid_update(&(otf_vars.neighbor[position].pid), numMesgsQueue, ieee154e_asnDiff(&otf_vars.neighbor[position].lastAsn));
      memcpy(&otf_vars.neighbor[position].lastAsn, currentAsn, sizeof(asn_t));
   }
   else {
      printf("*****OTF ERROR: could find or create a new neighbor statistics!!!!\n");
   }
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
//   printf("_____________TIMER fires\n");
   scheduler_push_task(otf_timer_fired_task,TASKPRIO_OTF);
}

#if 1
void otf_printLocalVar() {
   uint8_t i;
   for(i=0;i<OTF_MAX_NUM_NEIGHBOR_STATISTICS;i++) {
      printf("%02X ", idmanager_getMyID(ADDR_16B)->addr_16b[1]);
      printf("%d ", otf_vars.neighbor[i].used);
      printf("%02X ", otf_vars.neighbor[i].address.addr_64b[7]);
      printf("%f ", otf_vars.neighbor[i].pid.prev_error);
      printf("%d ", otf_vars.neighbor[i].pid.lastHistoryPosition);
      printf("%f ", otf_vars.neighbor[i].pid.control);
      printf("\n");
   }
}
#endif


/**
 * \brief Task that calculates the bandwidth requirements and increases/decreases radio cells
 */
void otf_timer_fired_task(void) {
   uint8_t i;
   printf("_____________TIMER task reached\n");
   //Checking if more slots have to be added
   for (i=0; i<OTF_MAX_NUM_NEIGHBOR_STATISTICS; i++) {
      if (TRUE == otf_vars.neighbor[i].used) {
         printf("%02X found neighbor %02X %f\n", idmanager_getMyID(ADDR_16B)->addr_16b[1],  otf_vars.neighbor[i].address.addr_64b[7], otf_vars.neighbor[i].pid.control);
         //printPID(&otf_vars.neighbor[i].pid);
         //otf_printLocalVar();
         if (0.4 < otf_vars.neighbor[i].pid.control) {
            printf("<<<<<<<>>>>><<<<>>>>OTFPID: ADDING A CELL to %02X!!!!!<<<<<>>>>>><<<>>\n", otf_vars.neighbor[i].address.addr_64b[7]);
            // call sixtop
            sixtop_addCells(
                  &otf_vars.neighbor[i].address,
                  1
                  );
         }
      }
   }

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

void pid_zeroize(PID_t* pid) {
   int i;
   // set prev and integrated error to zero
   pid->prev_error = 0;
   pid->control    = 0;
   pid->lastHistoryPosition = 0;
   for(i=0;i<PID_NUM_INTEGRAL_HISTORY;i++){
      pid->error_history[i] = 0;
   }
}

void pid_update(PID_t* pid, double curr_error, double dt) {
   double diff;
   double p_term;
   double i_term;
   double d_term;
   double int_error = 0;
   int i;

#if 1
   printf("***PID_UPDATE: ");
   printf("%f ", curr_error);
   printf("%f ", dt);
#endif   
   pid->error_history[pid->lastHistoryPosition] = curr_error * dt;
   pid->lastHistoryPosition++;
   if(PID_NUM_INTEGRAL_HISTORY == pid->lastHistoryPosition) {
      pid->lastHistoryPosition = 0;
   }

   // integration with windup guarding
   for(i=0;i<PID_NUM_INTEGRAL_HISTORY;i++){
      int_error += pid->error_history[i];
   }
   if (int_error < -(PID_WINDUPGUARD_VALUE))
      int_error = -(PID_WINDUPGUARD_VALUE);
   else if (int_error > PID_WINDUPGUARD_VALUE)
      int_error = PID_WINDUPGUARD_VALUE;

   // differentiation
   diff = ((curr_error - pid->prev_error) / dt);

   // scaling
   p_term = (PID_PROPORTIONAL_GAIN_VALUE * curr_error);
   i_term = (PID_INTEGRAL_GAIN_VALUE * int_error);
   d_term = (PID_DERIVATIVE_GAIN_VALUE * diff);

   // summation of terms
   pid->control = p_term + i_term + d_term;
#if 1
   printf("[%f ", p_term);
   printf("%f ", i_term);
   printf("%f] ", d_term);
   printf("%f ", pid->control);
   printf("\n");
#endif
   // save current error as previous error for next iteration
   pid->prev_error = curr_error;
}
