/** 
 *  @file
 *  @brief Wii network include file
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

#ifndef _HTTP_H_
#define _HTTP_H_

enum
{
    TCP_INIT=0,
	TCP_REQUEST1a=1,
	TCP_REQUEST1b=2,
	TCP_REQUEST2a=3,
	TCP_REQUEST2b=4,
	TCP_REQUEST3a=5,
	TCP_REQUEST3b=6,
	TCP_REQUEST4a=7,
	TCP_REQUEST4b=8,
	TCP_ERROR=9,
	TCP_RETRY=10,
	TCP_IDLE=11,
	TCP_END=12
};

// Start HTTP thread
extern int tcp_start_thread(char *name, char *version, 
						char *id1, char *url1, 
						char *id2, char *url2, 
						char *id3, char *url3, 
						char *id4, char *url4,
						char *token, char *userData2, char *userData3);
						
// Stop HTTP thread
extern int tcp_stop_thread(void);

// Fetch current thread state (String)
extern char *tcp_get_state(void);

// Feth current thread state (int)
extern int tcp_get_state_nr(void);

// Set current thread state (int)
int tcp_set_state(int state, char *userData3);

// Get version (String format)
extern char *tcp_get_version(void);

// Get releasenotes (HTML format)
extern char *tcp_get_releasenote(void);

// Get today highscore (XML format)
extern char *tcp_get_today_highscore(void);

// Get global highscore (XML format)
extern char *tcp_get_global_highscore(void);

#endif
