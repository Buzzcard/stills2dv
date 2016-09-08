/***********************************************************************************************
 *
 * s2d_smoothing.c
 *
 * Author: Denis-Carl Robidoux
 *
 * Copyrights and license: GPL v3 (see file named gpl-3.0.txt for details
 *
 *
 *
 *
 ***********************************************************************************************/


#include <ctype.h>
#include <math.h>


#include "stills2dv.h"


double smoothed_step (double start, double end, int count, double total, double smoothratio)
{
  double virtual_step, f_count, low_limit, hi_limit;
/*   static double trace_virtual=0.0; */
  int integer_xfer;
  if(smoothratio<0.0)smoothratio=0.0;
  if(smoothratio>0.5)smoothratio=0.5;
  if(total <= 1.0)return (end - start);
  f_count=count;
  virtual_step=(end - start) / ((1.0 - smoothratio) * total );
/*   if((virtual_step != trace_virtual) && (virtual_step != 0.0)) */
/*     { */
/*       printf("New virtual step of %12.6f\n", virtual_step); */
/*       trace_virtual=virtual_step; */
/*     } */
  if(smoothratio == 0.0)
    {
      return virtual_step;
    }
  if(virtual_step == 0.0) return 0.0;
  low_limit=smoothratio * total ;
  integer_xfer=low_limit; 
  if(integer_xfer < 1) return virtual_step;
/*   low_limit=integer_xfer; */
  hi_limit=total - low_limit;
  if(f_count<low_limit)
    {
      return (virtual_step * (f_count) / low_limit);
    }
  if(f_count>hi_limit)
    {
      return (virtual_step * (total - f_count) / low_limit);
    }
  return virtual_step;
}
