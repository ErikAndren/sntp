/** 
 *  @file
 *  @brief Wii trace module
 *  @author wplaat
 *
 *  Copyright (C) 2008-2010 PlaatSoft
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <gccore.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogcsys.h>
#include <stdarg.h>
#include <time.h>

#include "trace.h"

#define MAX_BUFFER_SIZE		    8192

// Enable / Disable trace file functionality
bool traceOn = false;

FILE *fp = NULL;

/**
 * Create trace file
 * @author wplaat
 * @param filename  The filename of the trace file
 */
int traceOpen(char *filename)
{
   int returnValue=0;
   
   if (!traceOn) return -1;
         
   if((fp=fopen(filename, "wb"))==NULL) 
   {
      printf("Error: Cannot open trace file.\n");
      returnValue=-2;
   }
   
   return returnValue;
}


/**
 * Close trace file
 * @author wplaat
 */
int traceClose()
{
   int returnValue=0;
   
   if (fp!=NULL)
   {
       fclose(fp);
   }	
   return returnValue;
}


/**
 * Create trace timestamp
 * @author wplaat
 * @return The timestamp in Dutch format.
 */
char *getDate()
{
  struct tm *now = NULL;
  time_t time_value = 0;
  static char buf[ 128 ] ;
  
  // Clear memory
  memset(buf, 0x00, sizeof(buf));
  
  /* Get time value */
  time_value = time(NULL);          
  
  /* Get time and date structure */
  now = localtime(&time_value);     

  // Create time stamp
  sprintf(buf,"%02d-%02d-%04d %02d:%02d:%02d",
	now->tm_mday, now->tm_mon+1, now->tm_year+1900, 
	now->tm_hour,now->tm_min,now->tm_sec);
	
  return buf;
}

/**
 * Save trace event in trace file
 * @author wplaat
 * @param functionName	The functionName
 * @param threadNr		The thread number [0=main thread, 1=network thread]
 * @param event			The event discription
 * @param ...				Optional parameters.
 * @return Zero is succesful.
 */
int traceEvent( char *functionName, int threadNr, char *event, ...)
{
   int returnValue=0;
   
   if (!traceOn) return -1;
   
   // create and start the va_list
   va_list listPointer ;
   va_start( listPointer, event ) ;

   static char buf[ MAX_BUFFER_SIZE ];
   

   // Clear memory  
   memset(buf, 0x00, sizeof(buf));
  
   // Build string
   vsprintf( buf, event, listPointer ) ;
   
   if (fp!=NULL)
   {
      // Save string to file
	  fprintf(fp,"%s [thread%d-%s] %s\n",getDate(), threadNr, functionName, buf); 
	  fflush(fp); 
   }
   
   return returnValue;
}


/**
 * Save trace event in trace file.
 * @author wplaat
 * @param character	The character range between 0...255
 * @return Zero is succesful.
 */ 
int traceEventRaw( char character)
{
   int returnValue=0;
   
   if (!traceOn) return -1;
     
   if (fp!=NULL)
   {
      // Save string to file
	  fprintf(fp,"%c",character); 
	  fflush(fp); 
   }
   
   return returnValue;
}