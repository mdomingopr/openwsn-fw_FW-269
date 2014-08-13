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

//=========================== typedef =========================================

//=========================== module variables ================================

//=========================== prototypes ======================================

// admin
void     otf_init(void);
// notification from sixtop
void     otf_notif_addedCell(uint16_t trackId, uint16_t numCells);
void     otf_notif_removedCell(uint16_t trackId, uint16_t numCells);



// COAP
void     otf_addCell(uint16_t trackId, uint16_t numCells);
/**
\}
\}
*/

#endif
