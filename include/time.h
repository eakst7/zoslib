///////////////////////////////////////////////////////////////////////////////
// Licensed Materials - Property of IBM
// ZOSLIB
// (C) Copyright IBM Corp. 2020. All Rights Reserved.
// US Government Users Restricted Rights - Use, duplication
// or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
///////////////////////////////////////////////////////////////////////////////

#ifndef ZOS_TIME_H_
#define ZOS_TIME_H_

#if (__EDC_TARGET < 0x42050000)
typedef enum {
  CLOCK_REALTIME,
  CLOCK_MONOTONIC,
  CLOCK_HIGHRES,
  CLOCK_THREAD_CPUTIME_ID
} clockid_t;

/**
 * Retrieves the time of the specified clock id
 * \param [in] clk_id clock id.
 * \param [out] tp structure to store the current time to.  
 * \return return 0 for success, or -1 for failure.
 */
extern int (*clock_gettime)(clockid_t cld_id, struct timespec * tp);

#include_next <time.h>
#else
#include_next <time.h>
#endif

#endif