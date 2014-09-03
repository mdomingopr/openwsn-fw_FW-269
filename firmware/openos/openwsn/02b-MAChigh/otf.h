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
#define OTF_TIMER_PERIOD_MS         10000 //10s

//=========================== typedef =========================================

//=========================== module variables ================================
typedef struct {
   opentimer_id_t  timerId;
} otf_vars_t;

//=========================== prototypes ======================================

// admin
void      otf_init(void);
// notification from sixtop
void      otf_notif_addedCell(void);
void      otf_notif_removedCell(void);

/**
\}
\}
*/

#endif
