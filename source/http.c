/** 
 *  @file
 *  @brief Wii network module
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
#include <ctype.h>
#include <gccore.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h> 
#include <malloc.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "http.h"
#include "trace.h"

// -----------------------------------------------------------
// DEFINES
// -----------------------------------------------------------

#define TCP_CONNECT_TIMEOUT     5000
#define TCP_BLOCK_SIZE          (16 * 1024)
#define TCP_BLOCK_RECV_TIMEOUT  4000
#define TCP_BLOCK_SEND_TIMEOUT  4000
#define HTTP_TIMEOUT            300000
#define MAX_LEN        			  256
#define NUM_THREADS             1
#define MAX_BUFFER_SIZE		     10240

// -----------------------------------------------------------
// ENUMS
// -----------------------------------------------------------

typedef enum 
{
	HTTPR_OK,
	HTTPR_ERR_CONNECT,
	HTTPR_ERR_REQUEST,
	HTTPR_ERR_STATUS,
	HTTPR_ERR_TOOBIG,
	HTTPR_ERR_RECEIVE
} 
http_res;

// -----------------------------------------------------------
// VARIABLES
// -----------------------------------------------------------

lwp_t threads[NUM_THREADS];
mutex_t mutexcheck;
mutex_t mutexversion;
bool do_tcp_treat;

char *appl_host; 
char *appl_path;

char appl_new_version[MAX_LEN];
char appl_release_notes[MAX_BUFFER_SIZE];
char appl_token[MAX_LEN];
char appl_global_highscore[MAX_BUFFER_SIZE];
char appl_today_highscore[MAX_BUFFER_SIZE];

char appl_name[MAX_LEN];
char appl_version[MAX_LEN];

char appl_id1[MAX_LEN];
char appl_url1[MAX_LEN];

char appl_id2[MAX_LEN];
char appl_url2[MAX_LEN];

char appl_id3[MAX_LEN];
char appl_url3[MAX_LEN];

char appl_id4[MAX_LEN];
char appl_url4[MAX_LEN];

char appl_userData2[MAX_LEN];
char appl_userData3[MAX_LEN];

char var_cookie[MAX_LEN];

extern GXRModeObj *rmode;
int  tcp_state;
int  tcp_state_prev;

char *http_host;
u16 http_port;
char *http_path;
u32 http_max_size;

http_res result;
u32 http_status;
u32 content_length;
u8 *http_data;

int retval;
u32 http_status;
u32 outlen;
u8  *outbuf;

// -----------------------------------------------------------
// TCP METHODES
// -----------------------------------------------------------

/**
 * Open tcp socket.
 *
 * @author wplaat
 *
 * @return status (<0 failure, >=0 succesful)
 */
s32 tcp_socket(void) 
{
		char *s_fn="tcp_socket";
        traceEvent(s_fn, 1, "enter" ); 
        s32 s;

        s = net_socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s < 0) 
		{
			traceEvent(s_fn, 1, "net_socket failed: %d",s); 
        }
		else
		{
		   traceEvent(s_fn, 1, "net_socket succesfull"); 
		}

	    traceEvent(s_fn, 1, "leave [%d]",s); 
        return s;
}

/**
 * Make tcp connect.
 *
 * @author wplaat
 *
 * @param host		The host name
 * @param port		Port number
 *
 * @return Network handler
 */
s32 tcp_connect(char *host, const u16 port)
{
		char *s_fn="tcp_connect";
        traceEvent(s_fn, 1, "enter [host=%s|port=%d]",host,port ); 
				
        struct hostent *hp;
        struct sockaddr_in sa;
        s32 s, res;
        s64 t;

        hp = net_gethostbyname(host);
        if (!hp || !(hp->h_addrtype == PF_INET)) 
		{            
			traceEvent(s_fn, 1, "net_gethostbyname failed: errno=[%d]",errno); 
            return errno;
        }
		else
		{
		   traceEvent(s_fn, 1, "net_gethostbyname successfull resolved"); 
		}

        s = tcp_socket();
        if (s < 0)
		{
		   traceEvent(s_fn,1,"Error creating socket, exiting"); 
           return s;
	    }
		else
		{
		   traceEvent(s_fn, 1, "tcp_socket succesfull"); 
		}

        memset (&sa, 0, sizeof (struct sockaddr_in));
        //sa.sin_family= PF_INET;
		sa.sin_family= AF_INET;
        sa.sin_len = sizeof (struct sockaddr_in);
        sa.sin_port= htons(port);
        memcpy ((char *) &sa.sin_addr, hp->h_addr_list[0], hp->h_length);

        traceEvent(s_fn, 1,"DNS resolve=%d.%d.%d.%d", (unsigned char) hp->h_addr_list[0][0], (unsigned char) hp->h_addr_list[0][1], (unsigned char) hp->h_addr_list[0][2], (unsigned char)  hp->h_addr_list[0][3]); 

        t = gettime();
        while (true) 
		{
                if (ticks_to_millisecs (diff_ticks (t, gettime ())) > TCP_CONNECT_TIMEOUT) 
				{
					traceEvent(s_fn,1,"tcp_connect timeout"); 
                    net_close (s);
                    return -ETIMEDOUT;
                }

                res = net_connect (s, (struct sockaddr *) &sa, sizeof (struct sockaddr_in));

                if (res < 0) 
				{
                   if (res == -EISCONN)  break;

                   if (res == -EINPROGRESS || res == -EALREADY) 
				   {
                       usleep (20 * 1000);
                       continue;
                   }

   				   traceEvent(s_fn, 1,"net_connect failed [res=%d]",res);
                   net_close(s);
                   return res;
                }
                break;
        }
		
		traceEvent(s_fn, 1, "net_connect succesfull");
		traceEvent(s_fn, 1, "leave [%d]",s); 
        return s;
}

/** 
 * Read line out received tcp information.
 *
 * @author wplaat
 *
 * @param s					Network handler.
 * @param max_length		Max length of return string.
 * @param start_time		Start time.
 * @param timeout			Max waiting before timeout.
 *
 * @return received line
 */
char * tcp_readln(const s32 s, const u16 max_length, const u64 start_time, const u16 timeout) 
{		
        char *s_fn="tcp_readln";
		traceEvent(s_fn,1,"enter" ); 
		
		char *buf;
        u16 c;
        s32 res;
        char *ret;

        buf = (char *) malloc (max_length);

        c = 0;
        ret = NULL;
        while (true) 
		{
                if (ticks_to_millisecs (diff_ticks (start_time, gettime ())) > timeout)
                        break;

                res = net_read (s, &buf[c], 1);

                if ((res == 0) || (res == -EAGAIN)) 
				{
                        usleep (20 * 1000);
                        continue;
                }

                if (res < 0) 
				{                       
						traceEvent(s_fn,1,"net_read failed [res=%d]", res); 
                        break;
                }

                if ((c > 0) && (buf[c - 1] == '\r') && (buf[c] == '\n')) 
				{
                        if (c == 1) 
						{
                                ret = strdup ("");
                                break;
                        }
                        ret = strndup (buf, c - 1);
                        break;
                }
                c++;
                if (c == max_length) break;
        }
        free (buf);
		traceEvent(s_fn,1,"leave [bufsize=%d]", c); 
		return ret;
}

/**
 * Receive data out tcp layer.
 *
 * @author wplaat
 *
 * @param s			Network handler.
 * @param buffer	Receive buffer.
 * @param length	Amount of bytes received.
 *
 * @return succesful
 */
bool tcp_read(const s32 s, u8 **buffer, const u32 length) 
{
	    char *s_fn="tcp_read";
		traceEvent(s_fn,1,"enter" ); 
		
        u8 *p;
        u32 step, left, block, received;
        s64 t;
        s32 res;

        step = 0;
        p = *buffer;
        left = length;
        received = 0;

        t = gettime ();
        while (left) 
		{
                if (ticks_to_millisecs (diff_ticks (t, gettime ())) > TCP_BLOCK_RECV_TIMEOUT)
			    {
				   traceEvent(s_fn,1,"net_read timeout"); 
                   break;
                }

                block = left;
                if (block > 2048) block = 2048;
				
                res = net_read (s, p, block);
                if ((res == 0) || (res == -EAGAIN)) 
				{
                    usleep (20 * 1000);
                    continue;
                }

                if (res < 0) 
				{
					traceEvent(s_fn,1,"net_read failed [res=%d]", res); 
                    break;
                }

                received += res;
                left -= res;
                p += res;

                if ((received / TCP_BLOCK_SIZE) > step) 
				{
                    t = gettime ();
                    step++;
                }
        }

		traceEvent(s_fn,1,"leave [left=%d]", left ); 
        return left == 0;
}

/** 
 * Write data to tcp layer.
 *
 * @author wplaat
 *
 * @param s			The network handler.
 * @param buffer	THe data buffer.
 * @param length	The data lenght.
 *
 * @return succusfull
 */
bool tcp_write(const s32 s, const u8 *buffer, const u32 length)
{
	    char *s_fn="tcp_write";
		traceEvent(s_fn,1,"enter [bufsize=%d]",length ); 
				
		traceEvent(s_fn,1,"buffer=%s",buffer ); 
		
        const u8 *p;
        u32 step, left, block, sent;
        s64 t;
        s32 res;

        step = 0;
        p = buffer;
        left = length;
        sent = 0;

        t = gettime ();
        while (left) 
		{
                if (ticks_to_millisecs (diff_ticks (t, gettime ())) > TCP_BLOCK_SEND_TIMEOUT) 
				{
 					traceEvent(s_fn,1,"net_write timeout"); 
                    break;
                }

                block = left;
                if (block > 2048) block = 2048;

                res = net_write (s, p, block);
                if ((res == 0) || (res == -56))
				{
                    usleep (20 * 1000);
                    continue;
                }

                if (res < 0) 
				{
					traceEvent(s_fn,1, "failed: [res=%d]",res); 
                    break;
                }
                sent += res;
                left -= res;
                p += res;

                if ((sent / TCP_BLOCK_SIZE) > step)
				{
                    t = gettime ();
                    step++;
                }
        }
		
		traceEvent(s_fn, 1, "leave [left=%d]",left); 
        return left == 0;
}


/** 
 * Init tcp layer.
 *
 * @author wplaat
 *
 * @return succesful (<0 failure)
 */
int tcp_init(void) 
{    
    char *s_fn="tcp_init";
	traceEvent(s_fn,1,"enter" ); 
	 	
    traceEvent(s_fn, 1,"Waiting for network to initialise..."); 
			
	s32 result;
    while ((result = net_init()) == -EAGAIN);
	
    if ( result>= 0) 
    {
        char myIP[16];
        if (if_config(myIP, NULL, NULL, true, 1) < 0) 
        {
            traceEvent(s_fn, 1, "Error reading IP address, exiting"); 
			traceEvent(s_fn, 1,"leave [-1]"); 
			return -1;
        }
        traceEvent(s_fn, 1,"Network initialised [%s].",myIP); 
    } 
    else 
    {
      traceEvent(s_fn, 1,"Unable to initialise network, exiting"); 
	  traceEvent(s_fn, 1,"leave [-2]"); 
	  return -2;
    }
	
	traceEvent(s_fn, 1,"leave [0]"); 
	return 0;
}


/**
 * Sleep some time.
 *
 * @author wplaat
 *
 * @param seconds 	The wait time in seconds.
 */
void tcp_sleep(unsigned int seconds)
{
   char *s_fn="tcp_sleep";
   traceEvent(s_fn,1,"enter [wait=%d sec]", seconds ); 

   sleep(seconds);
   
   traceEvent(s_fn, 1,"leave [void]"); 
}

// -----------------------------------------------------------
// HTTP METHODES
// -----------------------------------------------------------

/**
 * Splits url in parts.
 *
 * @author wplaat
 *
 * @param host		The host part.
 * @param path		The path part.
 * @param url 		The input URL.
 *
 * @return succesfull
 */
bool http_split_url(char **host, char **path, const char *url)
{
	    char *s_fn="http_split_url";
		traceEvent(s_fn,1,"enter" ); 
		
        const char *p;
        char *c;

        if (strncasecmp (url, "http://", 7))
		{
		   traceEvent(s_fn, 1,"leave [false]"); 
           return false;
		}

        p = url + 7;
        c = strchr (p, '/');

        if (c[0] == 0)
		{
		    traceEvent(s_fn, 1,"leave [false]"); 
		    return false;
	    }

        *host = strndup (p, c - p);
        *path = strdup (c);

		traceEvent(s_fn, 1,"leave [true]"); 	
        return true;
}


char *http_replaceString(char *orgstr, char *oldstr, char *newstr)
{
  char *s_fn="http_replaceString";
  traceEvent(s_fn,1,"enter" ); 
		
  int oldlen, newlen;
  char *s, *p;
  s = orgstr;
  while (s != NULL)
  {
    p = strstr(s, oldstr);
    if (p == NULL ) return orgstr;
    oldlen = strlen(oldstr);
    newlen = strlen(newstr);
    orgstr = (char*)realloc(orgstr, strlen(orgstr)-oldlen+newlen+1);
    if (orgstr == NULL) return NULL;
    memmove(p + newlen, p + oldlen, strlen(p + oldlen) + 1);
    memcpy(p, newstr, newlen);
    s = p + newlen;
  }
  traceEvent(s_fn, 1,"leave [char *]"); 	  
  return orgstr;
}

// This function remove all html tag and return and ascii line buffer
bool http_convertHTMlToAscii(char *in, int inSize)
{  
   char *s_fn="http_convertHTMlToAscii";
   traceEvent(s_fn, 1,"enter [inSize=%d]", inSize ); 
   
   traceEvent(s_fn, 1,"in=%s", in ); 
   
   int i;
   int startpos=0;
   int size=0;
   bool tag=true;

   // replace some html tags 
   in = http_replaceString(in,"\r","");
   in = http_replaceString(in,"\n","");
   in = http_replaceString(in,"</title>","\n");
   in = http_replaceString(in,"<h1>","\n");
   in = http_replaceString(in,"<h2>","\n");
   in = http_replaceString(in,"<h3>","\n");
   in = http_replaceString(in,"</h1>","\n\n");
   in = http_replaceString(in,"</h2>","\n");
   in = http_replaceString(in,"</h3>","\n");
   in = http_replaceString(in,"</br>","\n");
   in = http_replaceString(in,"<br/>","\n");
   
   // remove all html tags
   for (i=0; i<inSize; i++)
   {
      if (in[i]=='<')
	  {	     
	  	 if (!tag)
		 {		    
		   int len=i-startpos;	
		   if (len>4) 
			{  
               if (size+len<MAX_BUFFER_SIZE)
			   {
			      strncat(appl_release_notes+size, (char *) in+startpos, len);	
			      size+=len;
			   }
			}				            
	     }
         tag=true;	
	  }
	  
	  if (in[i]=='>')
	  {	    
		 tag=false;	
		 startpos=i+1;	
	  }
   }   
   						
   traceEvent(s_fn, 1,"out=%s", in ); 
   traceEvent(s_fn,1, "leave [true]"); 
   return true;
}

char http_bin2hex(int val)
{
    int i;
	
    i = val & 0x0F;
    if (i >= 0 && i < 10)
        return '0' + i;
    else
        return 'A' + (i - 10);
}
 	
char *http_encode_url(char *buf, const char *str)
{
    char *s_fn="http_encode_url";
    traceEvent(s_fn,1,"enter" ); 

    int i, j;
    int len;
    unsigned char c;
	int buflen=MAX_LEN;
    len = strlen(str); 
   
    for (i = 0, j = 0; i < len && j < (buflen - 1); i++) 
	{
        c = (unsigned char) str[i];
        		
		if ((c!='.') && (!isalnum(c)))
		{
            buf[j++] = '%';
            if (j < (buflen - 1))
                buf[j++] = http_bin2hex((c >> 4) & 0x0F);
            if (j < (buflen - 1))
                buf[j++] = http_bin2hex(c & 0x0F);
        } 
		else 
		{
          buf[j] = str[i];
          j++;
        }
    }
    buf[j] = '\0';
	
	traceEvent(s_fn, 1,"buf=[%s]",buf); 		
	traceEvent(s_fn, 1,"leave [char *]"); 	
    return buf;
}
		
// Google Analytic without JavaScript
void http_googleAnalysicUrl(char *buffer, char *domain, char *url, char *id)
{
    char *s_fn="http_googleAnalysicUrl";
    traceEvent(s_fn,1,"enter" ); 

	traceEvent(s_fn,1,"domain=[%s]", domain); 
	traceEvent(s_fn,1,"url=[%s]", url); 
	traceEvent(s_fn,1,"id=[%s]", id); 
		
    char utmhn[MAX_LEN];
    char utmac[MAX_LEN];
    char utmcs[MAX_LEN];
    char utmsr[MAX_LEN];
    char utmsc[MAX_LEN];
    char utmn[MAX_LEN];
    char utmp[MAX_LEN];
    char utmr[MAX_LEN];
    char utmdt[MAX_LEN];
    
    char var_random[MAX_LEN];
    char var_prev_day[MAX_LEN];
    char var_today[MAX_LEN];
    char var_next_day[MAX_LEN];
    char var_uservar[MAX_LEN];
	char tmp1[300];
    char tmp2[300];
	//the local arrays tmp1, tmp2 generate possible overflow warnings
	//fixing them as below crashes the Wii at runtime. It works so...
	//char tmp1[6*MAX_LEN];
    //char tmp2[12*MAX_LEN];

    memset( utmhn,0x00,sizeof(utmhn));
    memset( utmac,0x00,sizeof(utmac));
    memset( utmcs,0x00,sizeof(utmcs));
    memset( utmsr,0x00,sizeof(utmsr));
    memset( utmsc,0x00,sizeof(utmsc));
    memset( utmn,0x00,sizeof(utmn));
    memset( utmp,0x00,sizeof(utmp));
    memset( utmr,0x00,sizeof(utmr));
    memset( utmdt,0x00,sizeof(utmdt));
   
    memset( var_random,0x00,sizeof(var_random));
    memset( var_prev_day,0x00,sizeof(var_prev_day));
    memset( var_today,0x00,sizeof(var_today));
    memset( var_next_day,0x00,sizeof(var_next_day));
    memset( var_uservar,0x00,sizeof(var_uservar));
	memset( tmp1,0x00,sizeof(tmp1));
    memset( tmp2,0x00,sizeof(tmp2));
	
	 /* URL EXAMPLE 
 
    ?utmwv=1 				// Analytics version
    &utmn=634440486         // Random transaction number
	&utmcs=URF-8    		//document encoding
	&utmsr=1440x900 		//screen resolution
	&utmsc=32-bit 			//color depth
	&utmul=nl 				//user language
	&utmje=1 				//java enabled
	&utmfl=9.0%20%20r28 	//flash version
	&utmcr=1 				//carriage return
	&utmdt=Linklove » The optimum keyword density //document title
	&utmhn=www.vdgraaf.info //document hostname
	&utmr=http://www.google.nl/search?q=seo+optimal+keyword+density&sourceid=navclient-ff&ie=UTF-8&rlz=1B3GGGL_nlNL210NL211 //referer URL
	&utmp=/the-optimum-keyword-density.html //document page URL
	&utmac=UA-320536-6 		// Google Analytics account
	&utmcc= 				// cookie settings
	__utma=
					21661308. //cookie number
					1850772708. //number under 2147483647
					1169320752. //time (20-01-2007) cookie first set
					1172328503. //time (24-02-2007) cookie previous set
					1172935717. //time (03-03-2007) today
					3;+
	__utmb=
					21661308;+ //cookie number
	__utmc=
					21661308;+ //cookie number
	__utmz=
					21661308. //cookie number
					1172936273. //time (03-03-2007) today
					3.
					2.
		utmccn=(organic)| //utm_campaign
		utmcsr=google| //utm_source
		utmctr=seo+optimal+keyword+density| //utm_term
		utmcmd=organic;+ //utm_medium*/	
				
	// Your Domain
	http_encode_url(utmhn, domain);  
		
	// Referer url	
	sprintf(tmp1,"http://www.google.com/search?q=%s+Wii",appl_name); 
	http_encode_url(utmr, tmp1 );
	
	// Document page URL
    http_encode_url(utmp, url );
				
    // screen size
    sprintf(tmp1,"%dx%d", rmode->fbWidth, rmode->xfbHeight ); 	
    strcpy(utmsr, tmp1 );
	
	// color depth
    strcpy(utmsc, "32-bit" );
	
	// user language
	char *utmul = "en";											 
	
	// support for java
	char *utmje = "1";
				
    // flash version							 
	char *utmfl = "";			

    time_t  today =time(NULL);
	long base = (long) today;
				
	// random number under 2147483647
	long c = base + 100000000; 		
    sprintf(var_random, "%ld",c);
	
	// random request number (1000000000,9999999999);
	long d = base + 500000000; 		
    sprintf(utmn, "%ld",d);
	
    // page title				
    sprintf(tmp1,"WiiBrew %s %s",appl_name, appl_version); 				
    http_encode_url(utmdt, tmp1 );								
			
	// Google Analytics account	
	strcpy(utmac, id );

	traceEvent(s_fn, 1,"utmac=[%s]", utmac); 
		   
	sprintf(tmp1, "utmwv=1&utmn=%s&utmcs=UTF-8&utmsr=%s&utmsc=%s&utmul=%s&utmje=%s&utmfl=%s&utmcr=1&utmdt=%s&utmhn=%s&utmr=%s&utmp=%s&utmac=%s",
		utmn, 
		utmsr, 
		utmsc, 
		utmul, 
		utmje, 
		utmfl, 
		utmdt,
        utmhn, 
		utmr, 
		utmp,
		utmac );
			
	// Timestamps					
	sprintf(var_prev_day, "%ld", base - (60*60*24) );				
	sprintf(var_today, "%ld", base);		
	sprintf(var_next_day, "%ld", base + (60*60*24) );
		
	// User defined				
    http_encode_url(var_uservar, appl_userData2 );	
	
    sprintf(tmp2, "&utmcc=__utma=%s.%s.%s.%s.%s.3%%3B%%2B__utmb=%s%%3B%%2B__utmc=%s%%3B%%2B__utmz=%s.%s.3.2.utmccn=(direct)%%3Dutmcsr=(direct)%%3Dutmctr=(none)%%3Dutmcmd=(none)%%3B%%2B__utmv%%3D%s.%s%%3B",
	    var_cookie,
		var_random,
		var_prev_day,
		var_next_day,
		var_today,
		var_cookie,
		var_cookie,
		var_cookie,
		var_today,
		var_cookie,
		var_uservar);
				
    sprintf(buffer,"http://www.google-analytics.com/__utm.gif?%s%s", tmp1, tmp2 );
	
	traceEvent(s_fn,1,"buffer=%s",buffer); 
	traceEvent(s_fn,1,"leave [void]"); 
}

extern bool http_request(char *url, const u32 max_size)
{
    char *s_fn="http_request";
    traceEvent(s_fn,1,"enter"); 
 		
        int linecount;
		bool chunked=false;
		int emptycount=0;
		
        if (!http_split_url(&http_host, &http_path, url)) return false;

        http_port = 80;
        http_max_size = max_size;
       
        http_status = 404;
        content_length = 0;
        http_data = NULL;

        int s = tcp_connect(http_host, http_port);

		traceEvent(s_fn, 1,"tcp_connect(%s, %hu) = %d",http_host, http_port, s); 
        if (s < 0) 
		{
			traceEvent(s_fn, 1,"HTTPR_ERR_CONNECT"); 
			traceEvent(s_fn, 1,"leave [false]"); 
			
            result = HTTPR_ERR_CONNECT;
            return false;
        }

        char *request = (char *) malloc(2048);
        char *r = request;
        r += sprintf (r,"GET %s HTTP/1.1\r\n", http_path); 
        r += sprintf (r,"Host: %s\r\n", http_host);
		r += sprintf (r,"User-Agent: %s/%s (compatible; v3.X; Nintendo Wii)\r\n", appl_name, appl_version);  
        r += sprintf (r,"Accept: text/xml,text/html,text/plain,image/gif\r\n");
        r += sprintf (r,"Cache-Control: no-cache\r\n\r\n");

        traceEvent(s_fn, 1,"request=%s", request); 

        bool b = tcp_write (s, (u8 *) request, strlen (request));

        free (request);
        linecount = 0;

        for (linecount=0; linecount < 32; linecount++) 
		{
            char *line = tcp_readln ((s32) s, (u16) 0x1ff, (u64) gettime(), (u16) HTTP_TIMEOUT);
		    traceEvent(s_fn, 1,"%s", line ? line : "(null)"); 
          
            if (!line) 
			{
                http_status = 404;
                result = HTTPR_ERR_REQUEST;
                break;
            }

            if (chunked && (emptycount==1))
			{			   
			   sscanf (line, "%x", &content_length);	
			   
			   // Protect against crash frame
			   if (content_length>5000) content_length=0;
			   
			   free (line);
               line = NULL;			 
               break;		
			}
			
            if (strlen(line)<1) 
			{
			    if (chunked) 
				{
				    if (++emptycount>1) 
					{
					  free (line);
					  line = NULL;
                      break;
					}					
				}	
                else
                {				
                  free (line);
                  line = NULL;
                  break;
			    }
            }

            sscanf (line, "HTTP/1.%*u %u", &http_status);
            sscanf (line, "Content-Length: %u", &content_length);
			if ( strstr(line, "Transfer-Encoding: chunked")!=0 )
			{
				traceEvent(s_fn, 1,"Chunked frame found"); 
				free (line);
				line = NULL;
				chunked=true;
            }
						
            free (line);
            line = NULL;
        }
        
		traceEvent(s_fn, 1,"content_length=%d, status=%d, linecount=%d", content_length, http_status, linecount); 

        if (http_status != 200) 
		{
            result = HTTPR_ERR_STATUS;
            net_close (s);
            return false;
        }
      	
	    if (linecount == 32 || !content_length) http_status = 404;
				
        if (content_length > http_max_size) 
		{
           result = HTTPR_ERR_TOOBIG;
           net_close (s);
           return false;
        }
        http_data = (u8 *) malloc (content_length);
        b = tcp_read (s, &http_data, content_length);
        if (!b) 
		{
           free (http_data);
           http_data = NULL;
           result = HTTPR_ERR_RECEIVE;
           net_close (s);
           return false;
        }
        		
        result = HTTPR_OK;
        net_close(s);

		traceEvent(s_fn, 1,"leave [true]"); 
        return true;
}

extern bool http_get_result(u32 *_http_status, u8 **content, u32 *length) 
{
		char *s_fn="http_get_result";
		traceEvent(s_fn, 1,"enter"); 
		
        if (http_status) *_http_status = http_status;

        if (result == HTTPR_OK) 
		{
           *content = http_data;
           *length = content_length;
        } 
		else 
		{
           *content = NULL;
           *length = 0;
        }

        free (http_host);
        free (http_path);

		traceEvent(s_fn, 1,"leave [true]"); 
        return true;
}

char * http_findToken(u8 *buffer, int bufsize, char *token)
{
	char *s_fn="http_findToken";
	traceEvent(s_fn, 1,"enter [%s]", token); 
   
   int i, j=0;
   int startpos=-1;
   int endpos;
   int len=strlen(token);
   bool found=false;
   
   // find token begin
   startpos=-1;
   j=0;
   
   traceEvent(s_fn, 1,"len=%d",len); 
	
   for (i=0; i<bufsize; i++)
   {
      if (buffer[i]==token[j])
	  {	     
	     if (startpos==-1) startpos=i;	
		 if (++j>=len) 
		 {  
		    traceEvent(s_fn, 1,"startpos=%d",startpos); 
		    found=true;
			break;
	     }
	  }
	  else
	  {
	     // No token match, continue...
	     if (startpos!=-1)
		 {
		    j=0;
			startpos=-1;
	     }
	  }
   }
   
   if (found) 
   {
       startpos+=len;
	  
       // find end of token value   
       for (i=startpos; i<bufsize; i++)
       {	      
	   if ((buffer[i]=='\n') || (buffer[i]=='\r') || (buffer[i]==' ') || (buffer[i]=='}'))
	     {  	     
		    endpos=i;
			traceEvent(s_fn, 1,"endpos=%d",endpos); 
			
			// (small hack) End string
			buffer[endpos]=0x00;
			
			traceEvent(s_fn, 1,"leave [%s]", buffer+startpos); 
			return (char*) buffer+startpos;
	     }
	  }
   }   
   traceEvent(s_fn, 1,"leave [NULL]"); 
   return NULL;
}


// -----------------------------------------------------------
// THREAD STUFF
// -----------------------------------------------------------
	
 void *tcp_thread(void *threadid)
{
	char *s_fn="tcp_thread";
	traceEvent(s_fn,1,"enter"); 

   int retval;
   u32 http_status;
   u8  *outbuf;
   u32 outlen;
  
   //int tid = (int)threadid;

   LWP_MutexLock (mutexcheck);
   while(do_tcp_treat)
   {
      LWP_MutexUnlock(mutexcheck);
	  
	  switch (tcp_state)
	  {
	    // Init ethernet DHCP network
	    case TCP_INIT:   
		{
		  traceEvent(s_fn,1,"stateMachine=TCP_INIT"); 
		  retval=tcp_init();
		  if (retval==0) 
		  { 
		     tcp_state=TCP_REQUEST1a; 
		  }
		  else 
		  {
		     tcp_state=TCP_ERROR;
		  }
		}
		break;
		
		// Error ocure during network init, retry after 60 seconds.				  
		case TCP_ERROR:
		{
		    traceEvent(s_fn, 1,"stateMachine=TCP_ERROR"); 
	        tcp_sleep(60);
		    tcp_state=TCP_INIT;
	    }
		break;
		
        // Check for new application version					 
		case TCP_REQUEST1a: 
		{		
			if(strlen(appl_url1) <= 0)
			{
				tcp_state=TCP_REQUEST1b;
				break;
			}
			traceEvent(s_fn,1, "stateMachine=TCP_REQUEST1a"); 
         retval = http_request(appl_url1, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request1a"); 
         }
         else
         {     
	         char *tmp;	
	         http_get_result(&http_status, &outbuf, &outlen);   	 			
				tmp=http_findToken(outbuf, outlen, appl_token);		 
			 
				LWP_MutexLock(mutexversion);
				if (tmp!=NULL) 
				{
					strncpy(appl_new_version, tmp, sizeof(appl_new_version)-1);
					traceEvent(s_fn, 1,"version=%s",appl_new_version); 
				}
				else
				{
					traceEvent(s_fn,1, "version=<none>");  
				}
				LWP_MutexUnlock(mutexversion);
			 
	         free(outbuf);			 
				tcp_state=TCP_REQUEST1b;
         } 	 
		} 	
      break;	 
		
		// Update Google Statistics
		case TCP_REQUEST1b: 
		{	
			if(strlen(appl_id1) <= 0)
			{
				tcp_state=TCP_REQUEST2a;
				break;
			}
			traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST1b"); 
				  
			char buffer[512];
			memset(buffer,0x00,sizeof(buffer));
		
         char url1[3*MAX_LEN];
         memset(url1,0x00,sizeof(url1));
         sprintf(url1,"/start/%s/%s",appl_name,appl_version);		    
			
			if (!http_split_url(&appl_host, &appl_path, appl_url1)) return false;
			
			http_googleAnalysicUrl(buffer,appl_host,url1,appl_id1);
			free (appl_host);
         free (appl_path);	 
			 
         retval = http_request(buffer, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn,1, "Error making http request1b"); 			 
         }
         else
         {     		    
	         http_get_result(&http_status, &outbuf, &outlen);   	
				int i; 
				for (i=0; i<outlen; i++) traceEventRaw(outbuf[i]);  
				free(outbuf);
				tcp_state=TCP_REQUEST2a;
         } 	 
	    }
		break;
				
		// Get Release note information
		case TCP_REQUEST2a: 
		{		  
			if(strlen(appl_url2) <= 0)
			{
				tcp_state=TCP_REQUEST2b;
				break;
			}
		   traceEvent(s_fn,1,"stateMachine=TCP_REQUEST2a"); 
         retval = http_request(appl_url2, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request2a"); 
         }
         else
         {     		    
				http_get_result(&http_status, &outbuf, &outlen);   			 
				http_convertHTMlToAscii((char *) outbuf, outlen);  	          
			 
				// Terminated received data
				char* result=strstr(appl_release_notes,"Other:");
				if (result!=NULL) result[0]=0x00;
			 			
				int i;  
				for (i=0; i<strlen(appl_release_notes); i++) 
				{ 
					traceEventRaw( appl_release_notes[i] ); 
				}
	
				free(outbuf);			 
				tcp_state=TCP_REQUEST2b;			
         } 	
		} 	
        break;	 
		
		// Update Google Statistics
		case TCP_REQUEST2b: 
		{		
			char buffer[512];
			memset(buffer,0x00,sizeof(buffer));
		  
			if(strlen(appl_id2) <= 0)
			{
				tcp_state=TCP_REQUEST3a;
				break;
			}
			traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST2b"); 
		 	  
			if (!http_split_url(&appl_host, &appl_path, appl_url2)) return false;
			  		  		  		    
			http_googleAnalysicUrl(buffer, appl_host, appl_path, appl_id2);
			free (appl_host);
         free (appl_path);	
         
			retval = http_request(buffer, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request2b"); 
         }
         else
         {     		    
	         http_get_result(&http_status, &outbuf, &outlen);   	
				int i; 
				for (i=0; i<outlen; i++) traceEventRaw(outbuf[i] );  
				free(outbuf);
				tcp_state=TCP_REQUEST3a;
			}  
		} 	
        break;	 
		
		// Get today high score from all players in the world.
		case TCP_REQUEST3a: 
		{
			if(strlen(appl_url3) <= 0)
			{
				tcp_state=TCP_REQUEST3b;
				break;
			}
			traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST3a"); 
		  

         char url[2*MAX_LEN];
		  		  
			memset(url,0x00,sizeof(url));		    
			sprintf(url, "%s?%s",appl_url3, appl_userData3);		
         retval = http_request(url, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request3a"); 		 
         }
         else
         {  
				http_get_result(&http_status, &outbuf, &outlen);   	
 
				int i;   		   					 
				memset(appl_today_highscore,0x00,sizeof(appl_today_highscore));
				for (i=0; i<outlen; i++) appl_today_highscore[i]=outbuf[i];	

			 
				char* result=strstr(appl_today_highscore,"</highscore>");
				if (result!=NULL) 
				{
					// Terminated received xml data 
					result[13]=0x00; 
					for (i=0; i<strlen(appl_today_highscore); i++) 
					{
						traceEventRaw(appl_today_highscore[i] ); 
					}
				   
					tcp_state=TCP_REQUEST3b;
				}
				else
				{
					// Something is wrong with the response
					traceEvent(s_fn, 1,"Received today highscore xml data was corrupt" ); 
				
					// Clear received trash
					memset(appl_today_highscore,0x00,sizeof(appl_today_highscore));
				
					// Try again.
					tcp_state_prev=tcp_state;
					tcp_state=TCP_RETRY;  
				}
				free(outbuf);			 			
         } 	 
		} 	
      break;	 
				
		// Update Google Statistics
		case TCP_REQUEST3b: 
		{		
			char buffer[512];
			memset(buffer,0x00,sizeof(buffer));
		  
			if(strlen(appl_id3) <= 0)
			{
				tcp_state=TCP_REQUEST4a;
				break;
			}
			traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST3b"); 
		 		  
			if (!http_split_url(&appl_host, &appl_path, appl_url3)) return false;
		  		  
			http_googleAnalysicUrl(buffer, appl_host, appl_path, appl_id3);
			free (appl_host);
         free (appl_path);
			
         retval = http_request(buffer, 1 << 31);
         if (!retval) 
         {
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request3b"); 
         }
         else
         {     		    
	         http_get_result(&http_status, &outbuf, &outlen);   	
				int i; for (i=0; i<outlen; i++) traceEventRaw( outbuf[i] ); 
				free(outbuf);
				tcp_state=TCP_REQUEST4a;
         } 	
		} 	
      break;	
		
		// Get global high score from all players in the world.
		case TCP_REQUEST4a: 
		{
			if(strlen(appl_url4) <= 0)
			{
				tcp_state=TCP_REQUEST4b;
				break;
			}
		  traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST4a"); 
		  
          char url[2*MAX_LEN];
		  memset(url,0x00,sizeof(url));		  
  
		  sprintf(url, "%s?%s",appl_url4, appl_userData3);		
          retval = http_request(url, 1 << 31);
          if (!retval) 
          {
		  	 tcp_state_prev=tcp_state;
		     tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request4a"); 		 
          }
          else
          {  
		     http_get_result(&http_status, &outbuf, &outlen);   	
 
			 int i;   		   					 
			 memset(appl_global_highscore,0x00,sizeof(appl_global_highscore));
  		     for (i=0; i<outlen; i++) appl_global_highscore[i]=outbuf[i];	

			 
		     char* result=strstr(appl_global_highscore,"</highscore>");
			 if (result!=NULL) 
			 {
			    // Terminated received xml data 
			    result[13]=0x00; 
    			for (i=0; i<strlen(appl_global_highscore); i++) traceEventRaw(appl_global_highscore[i] ); 
				   
				tcp_state=TCP_REQUEST4b;
			 }
			 else
			 {
			    // Something is wrong with the response
				traceEvent(s_fn, 1,"Received global highscore xml data was corrupt" ); 
				
				// Clear received trash
				memset(appl_global_highscore,0x00,sizeof(appl_global_highscore));
				
				// Try again.
				tcp_state_prev=tcp_state;
		        tcp_state=TCP_RETRY;  
			 }
			 free(outbuf);			 			
          } 	 
		} 	
        break;	 
				
		// Update Google Statistics
		case TCP_REQUEST4b: 
		{		
			char buffer[512];
			memset(buffer,0x00,sizeof(buffer));
		  
			if(strlen(appl_id4) <= 0)
			{
				tcp_state=TCP_IDLE;
				break;
			}
			traceEvent(s_fn, 1,"stateMachine=TCP_REQUEST4b"); 
		 		  
			if (!http_split_url(&appl_host, &appl_path, appl_url4)) return false;
		  		  
			http_googleAnalysicUrl(buffer, appl_host, appl_path, appl_id4);
			free (appl_host);
         free (appl_path);
			
         retval = http_request(buffer, 1 << 31);
         if (!retval) 
			{
				tcp_state_prev=tcp_state;
				tcp_state=TCP_RETRY;
	         traceEvent(s_fn, 1,"Error making http request4b"); 
         }
         else
         {     		    
	         http_get_result(&http_status, &outbuf, &outlen);   	
				int i; for (i=0; i<outlen; i++) traceEventRaw( outbuf[i] ); 
				free(outbuf);
				tcp_state=TCP_IDLE;
         } 	
		} 	
      break;	 
		
		// Error ocure during http request, retry after 120 seconds.
		case TCP_RETRY:
		{
		    traceEvent(s_fn,1,"stateMachine=TCP_RETRY" ); 
	        tcp_sleep(10);
		    tcp_state=tcp_state_prev;
	    }
		break;
				
        // Do nothing, wait for new order.
		case TCP_IDLE:
		{
		    traceEvent(s_fn,1, "stateMachine=TCP_IDLE" ); 
	        tcp_sleep(2);	
	    }
		break;
	
		// Thread is ready, shut it down.
		case TCP_END:
		{
		   traceEvent(s_fn,1, "stateMachine=TCP_END"); 
           do_tcp_treat=false;
		}		
        break;			
      }	  
      LWP_MutexLock(mutexcheck);
   }
   LWP_MutexUnlock(mutexcheck);
   //LWP_SuspendThread(tid);
   
   traceEvent(s_fn, 1,"leave [0]"); 
   return 0;
}

void tcp_clear_memory(void)
{
    char *s_fn="tcp_clear_memory";
	traceEvent(s_fn,1,"enter"); 
	
	memset( appl_release_notes,0x00,sizeof(appl_release_notes));
	memset( appl_today_highscore,0x00,sizeof(appl_today_highscore));
	memset( appl_global_highscore,0x00,sizeof(appl_global_highscore));

	traceEvent(s_fn,1, "leave"); 
}
	
// -----------------------------------------------------------
// INTERFACE API
// -----------------------------------------------------------

extern int tcp_get_state_nr(void)
{
   return tcp_state;
}

extern char *tcp_get_state(void)
{
   switch (tcp_state)
   {
     case TCP_INIT: 
	        return "INIT";
            break;
					
	 case TCP_REQUEST1a:
	        return "REQUEST_1A";
            break;
	 
	 case TCP_REQUEST1b:
	        return "REQUEST_1B";
            break;
	 
	 case TCP_REQUEST2a:
	        return "REQUEST_2A";
            break;
	 
	 case TCP_REQUEST2b:
	        return "REQUEST_2B";
            break;	 
			
	 case TCP_REQUEST3a:
	 	    return "REQUEST_3A";
            break;
			
	 case TCP_REQUEST3b:
	        return "REQUEST_3B";
            break;
	 	 
	 case TCP_REQUEST4a:
	 	    return "REQUEST_4A";
            break;
			
	 case TCP_REQUEST4b:
	        return "REQUEST_4B";
            break;
			
	 case TCP_ERROR:
	        return "ERROR";
            break;
	 
	 case TCP_RETRY:
	        return "RETRY";
            break;
	 
	 case TCP_IDLE:
	        return "IDLE";
            break;
	 
	 case TCP_END:
	        return "END";
            break;
	 
	 default: return "UNKNOWN";
	          break;
   }
}

extern int tcp_set_state(int state, char *userData3)
{
   char *s_fn="tcp_set_state";
   traceEvent(s_fn,1,"enter [state=%d]",state); 
	
   LWP_MutexLock(mutexversion);
   if (tcp_state==TCP_IDLE)
   {
      traceEvent(s_fn,1,"Update state!"); 
	  
      strcpy(appl_userData3, userData3);
      tcp_state=state;
   }
   LWP_MutexUnlock(mutexversion);
   traceEvent(s_fn,1,"leave [state=%d]",state); 
   return state;
}

extern char *tcp_get_version(void)
{
   LWP_MutexLock(mutexversion);
   return appl_new_version;
   LWP_MutexUnlock(mutexversion);
}

extern char *tcp_get_releasenote(void)
{
   LWP_MutexLock(mutexversion);  
   return appl_release_notes;
   LWP_MutexUnlock(mutexversion);
}

extern char *tcp_get_global_highscore(void)
{
   LWP_MutexLock(mutexversion);  
   return appl_global_highscore;
   LWP_MutexUnlock(mutexversion);
}

extern char *tcp_get_today_highscore(void)
{
   LWP_MutexLock(mutexversion);  
   return appl_today_highscore;
   LWP_MutexUnlock(mutexversion);
}

extern int tcp_start_thread(char *name, char *version, 
							char *id1, char *url1, 
							char *id2, char *url2, 
							char *id3, char *url3, 
							char *id4, char *url4, 
							char *token, char *userData2, char *userData3)
{
	char *s_fn="tcp_start_thread";
	traceEvent(s_fn,1,"enter"); 

    tcp_state=TCP_INIT;
    do_tcp_treat = true;

    tcp_clear_memory();

    memset(appl_new_version,0x00,sizeof(appl_new_version));
	memset(appl_name,0x00,sizeof(appl_name));
	memset(appl_version,0x00,sizeof(appl_version));
	
	memset(appl_id1,0x00,sizeof(appl_id1));
	memset(appl_url1,0x00,sizeof(appl_url1));
	
	memset(appl_id2,0x00,sizeof(appl_id2));
	memset(appl_url2,0x00,sizeof(appl_url2));
	
	memset(appl_id3,0x00,sizeof(appl_id3));	
	memset(appl_url3,0x00,sizeof(appl_url3));

	memset(appl_id4,0x00,sizeof(appl_id4));	
	memset(appl_url4,0x00,sizeof(appl_url4));

	memset(appl_userData2,0x00,sizeof(appl_userData2));
	memset(appl_userData3,0x00,sizeof(appl_userData3));
	
	memset(appl_token,0x00,sizeof(appl_token));	
	memset(var_cookie,0x00,sizeof(var_cookie));

	strcpy(appl_name, name);
	strcpy(appl_version, version);
	
	strcpy(appl_id1, id1);
	strcpy(appl_url1, url1);
	
	strcpy(appl_id2, id2);
	strcpy(appl_url2, url2);
	
	strcpy(appl_id3, id3);
	strcpy(appl_url3, url3);
		
	strcpy(appl_id4, id4);
	strcpy(appl_url4, url4);

	strcpy(appl_userData2, userData2);
	strcpy(appl_userData3, userData3);
	
	strcpy(appl_token, token);
	
	// Create cookie 
	time_t today =time(NULL);
	long base = (long) today;		
	long b = base - 1200009099; 			
    sprintf(var_cookie, "%ld",b);
	
	int rc = LWP_CreateThread(&threads[0], tcp_thread, NULL, NULL, 0, 1);
    if(rc!=0) 
	{
         traceEvent(s_fn,1,"ERROR; return code from LWP_CreateThread is %d", rc); 
    }
	
	traceEvent(s_fn,1,"leave [%d]",rc); 
	   
	return rc;
}
	
extern int tcp_stop_thread(void)
{
   char *s_fn="tcp_stop_thread";
   traceEvent(s_fn,1,"enter"); 

   LWP_MutexLock (mutexcheck);
   do_tcp_treat = false;
   LWP_MutexUnlock (mutexcheck);
   
   traceEvent(s_fn,1,"leave [0]"); 
   return 0;
}

// -----------------------------------------------------------
// THE END
// -----------------------------------------------------------

