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
#define OTF_TIMER_PERIOD_MS               150000// 70000 //10s
#define OTF_MAX_NUM_NEIGHBOR_STATISTICS   10

#define PID_NUM_INTEGRAL_HISTORY    10

//=========================== typedef =========================================

//=========================== module variables ================================

typedef struct {
   double prev_error;
   double error_history[PID_NUM_INTEGRAL_HISTORY]; // It is already multiplied by dt
   int    lastHistoryPosition;
   double control;
} PID_t;

typedef struct {
   bool        used;
   asn_t       lastAsn;
   open_addr_t address;    
   PID_t       pid; 
} otf_neighbor_statistics_t;

typedef struct {
   opentimer_id_t             timerId;
   otf_neighbor_statistics_t  neighbor[OTF_MAX_NUM_NEIGHBOR_STATISTICS];
} otf_vars_t;

//=========================== prototypes ======================================

// admin
void      otf_init(void);
// notification from sixtop
void      otf_notif_addedCell(void);
void      otf_notif_removedCell(void);
// notification from IEEE802154E
void     otf_notif_updateStatistics(asn_t* currentAsn, open_addr_t* address, uint8_t numMesgsQueue);
/**
\}
\}
*/

#endif
