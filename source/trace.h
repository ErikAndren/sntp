/** 
 *  @file
 *  @brief Wii trace include file
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

#ifndef __TRACE_H__
#define __TRACE_H__

/**
 * Create trace file
 * @param filename  The filename of the trace file
 */
int traceOpen(char *filename);


/**
 * Close trace file
 */
int traceClose();
 
/**
 * Save trace event in trace file
 * @param functionName	The functionName
 * @param threadNr		The thread number [0=main thread, 1=network thread]
 * @param event			The event discription
 * @param ...				Optional parameters.
 * @return Zero is succesful.
 */ 
int traceEvent( char *functionName, int threadNr, char *event, ...);


/**
 * Save trace event in trace file.
 * @param character	The character range between 0...255
 * @return Zero is succesful.
 */ 
int traceEventRaw( char character);


#endif
