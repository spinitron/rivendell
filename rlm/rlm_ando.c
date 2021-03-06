/* rlm_ando.c
 *
 *   (C) Copyright 2009 Fred Gleason <fredg@paravelsystems.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This is a Rivendell Loadable Module.  It sends Now&Next PAD data to an
 * ANDO Media Streaming system.  Options are specified in the configuration 
 * file pointed to by the plugin argument.
 *
 * To compile this module, just do:
 * 
 *   gcc -shared -o rlm_ando.rlm rlm_ando.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <rlm/rlm.h>

int rlm_ando_devs;
char *rlm_ando_addresses;
uint16_t *rlm_ando_ports;
char *rlm_ando_titles;
char *rlm_ando_artists;
char *rlm_ando_albums;
char *rlm_ando_labels;
int *rlm_ando_masters;
int *rlm_ando_aux1s;
int *rlm_ando_aux2s;
int *rlm_ando_vlog101s;
int *rlm_ando_vlog102s;
int *rlm_ando_vlog103s;
int *rlm_ando_vlog104s;
int *rlm_ando_vlog105s;
int *rlm_ando_vlog106s;
int *rlm_ando_vlog107s;
int *rlm_ando_vlog108s;
int *rlm_ando_vlog109s;
int *rlm_ando_vlog110s;
int *rlm_ando_vlog111s;
int *rlm_ando_vlog112s;
int *rlm_ando_vlog113s;
int *rlm_ando_vlog114s;
int *rlm_ando_vlog115s;
int *rlm_ando_vlog116s;
int *rlm_ando_vlog117s;
int *rlm_ando_vlog118s;
int *rlm_ando_vlog119s;
int *rlm_ando_vlog120s;

int rlm_ando_GetLogStatus(void *ptr,const char *arg,const char *section,
			    const char *logname)
{
  const char *tag=RLMGetStringValue(ptr,arg,section,logname,"");
  if(strcasecmp(tag,"yes")==0) {
    return 1;
  }
  if(strcasecmp(tag,"on")==0) {
    return 1;
  }
  if(strcasecmp(tag,"true")==0) {
    return 1;
  }
  if(strcasecmp(tag,"no")==0) {
    return 0;
  }
  if(strcasecmp(tag,"off")==0) {
    return 0;
  }
  if(strcasecmp(tag,"false")==0) {
    return 0;
  }
  if(strcasecmp(tag,"onair")==0) {
    return 2;
  }  
  return 0;
}


void rlm_ando_RLMStart(void *ptr,const char *arg)
{
  char address[17];
  char section[256];
  char errtext[256];
  int i=1;

  rlm_ando_devs=0;
  rlm_ando_addresses=NULL;
  rlm_ando_ports=NULL;
  rlm_ando_masters=NULL;
  rlm_ando_aux1s=NULL;
  rlm_ando_aux2s=NULL;
  rlm_ando_vlog101s=NULL;
  rlm_ando_vlog102s=NULL;
  rlm_ando_vlog103s=NULL;
  rlm_ando_vlog104s=NULL;
  rlm_ando_vlog105s=NULL;
  rlm_ando_vlog106s=NULL;
  rlm_ando_vlog107s=NULL;
  rlm_ando_vlog108s=NULL;
  rlm_ando_vlog109s=NULL;
  rlm_ando_vlog110s=NULL;
  rlm_ando_vlog111s=NULL;
  rlm_ando_vlog112s=NULL;
  rlm_ando_vlog113s=NULL;
  rlm_ando_vlog114s=NULL;
  rlm_ando_vlog115s=NULL;
  rlm_ando_vlog116s=NULL;
  rlm_ando_vlog117s=NULL;
  rlm_ando_vlog118s=NULL;
  rlm_ando_vlog119s=NULL;
  rlm_ando_vlog120s=NULL;

  sprintf(section,"System%d",i++);
  strncpy(address,RLMGetStringValue(ptr,arg,section,"IpAddress",""),15);
  if(strlen(address)==0) {
    RLMLog(ptr,LOG_WARNING,"rlm_ando: no ando destinations specified");
    return;
  }
  while(strlen(address)>0) {
    rlm_ando_addresses=
      realloc(rlm_ando_addresses,(rlm_ando_devs+1)*(rlm_ando_devs+1)*16);
    strcpy(rlm_ando_addresses+16*rlm_ando_devs,address);
    rlm_ando_ports=realloc(rlm_ando_ports,(rlm_ando_devs+1)*sizeof(uint16_t));
    rlm_ando_ports[rlm_ando_devs]=
      RLMGetIntegerValue(ptr,arg,section,"UdpPort",0);
    rlm_ando_titles=realloc(rlm_ando_titles,(rlm_ando_devs+1)*256);
    strncpy(rlm_ando_titles+256*rlm_ando_devs,
	    RLMGetStringValue(ptr,arg,section,"Title",""),256);
    rlm_ando_artists=realloc(rlm_ando_artists,(rlm_ando_devs+1)*256);
    strncpy(rlm_ando_artists+256*rlm_ando_devs,
	    RLMGetStringValue(ptr,arg,section,"Artist",""),256);
    rlm_ando_albums=realloc(rlm_ando_albums,(rlm_ando_devs+1)*256);
    strncpy(rlm_ando_albums+256*rlm_ando_devs,
	    RLMGetStringValue(ptr,arg,section,"Album",""),256);
    rlm_ando_labels=realloc(rlm_ando_labels,(rlm_ando_devs+1)*256);
    strncpy(rlm_ando_labels+256*rlm_ando_devs,
	    RLMGetStringValue(ptr,arg,section,"Label",""),256);
    rlm_ando_masters=realloc(rlm_ando_masters,
			    (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_masters[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"MasterLog");
    rlm_ando_aux1s=realloc(rlm_ando_aux1s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_aux1s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"Aux1Log");
    rlm_ando_aux2s=realloc(rlm_ando_aux2s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_aux2s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"Aux2Log");

    rlm_ando_vlog101s=realloc(rlm_ando_vlog101s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog101s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog101");

    rlm_ando_vlog102s=realloc(rlm_ando_vlog102s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog102s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog102");

    rlm_ando_vlog103s=realloc(rlm_ando_vlog103s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog103s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog103");

    rlm_ando_vlog104s=realloc(rlm_ando_vlog104s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog104s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog104");

    rlm_ando_vlog105s=realloc(rlm_ando_vlog105s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog105s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog105");

    rlm_ando_vlog106s=realloc(rlm_ando_vlog106s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog106s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog106");

    rlm_ando_vlog107s=realloc(rlm_ando_vlog107s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog107s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog107");

    rlm_ando_vlog108s=realloc(rlm_ando_vlog108s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog108s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog108");

    rlm_ando_vlog109s=realloc(rlm_ando_vlog109s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog109s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog109");

    rlm_ando_vlog110s=realloc(rlm_ando_vlog110s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog110s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog110");

    rlm_ando_vlog111s=realloc(rlm_ando_vlog111s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog111s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog111");

    rlm_ando_vlog112s=realloc(rlm_ando_vlog112s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog112s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog112");

    rlm_ando_vlog113s=realloc(rlm_ando_vlog113s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog113s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog113");

    rlm_ando_vlog114s=realloc(rlm_ando_vlog114s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog114s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog114");

    rlm_ando_vlog115s=realloc(rlm_ando_vlog115s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog115s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog115");

    rlm_ando_vlog116s=realloc(rlm_ando_vlog116s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog116s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog116");

    rlm_ando_vlog117s=realloc(rlm_ando_vlog117s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog117s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog117");

    rlm_ando_vlog118s=realloc(rlm_ando_vlog118s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog118s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog118");

    rlm_ando_vlog119s=realloc(rlm_ando_vlog119s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog119s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog119");

    rlm_ando_vlog120s=realloc(rlm_ando_vlog120s,
			  (rlm_ando_devs+1)*sizeof(int));
    rlm_ando_vlog120s[rlm_ando_devs]=
      rlm_ando_GetLogStatus(ptr,arg,section,"VLog120");

    sprintf(errtext,"rlm_ando: configured destination \"%s:%d\"",address,
	    rlm_ando_ports[rlm_ando_devs]);
    rlm_ando_devs++;
    RLMLog(ptr,LOG_INFO,errtext);
    sprintf(section,"System%d",i++);
    strncpy(address,RLMGetStringValue(ptr,arg,section,"IpAddress",""),15);
  }
  RLMStartTimer(ptr,0,30000,RLM_TIMER_REPEATING);
}


void rlm_ando_RLMFree(void *ptr)
{
  RLMStopTimer(ptr,0);
  free(rlm_ando_addresses);
  free(rlm_ando_ports);
  free(rlm_ando_titles);
  free(rlm_ando_artists);
  free(rlm_ando_albums);
  free(rlm_ando_labels);
  free(rlm_ando_masters);
  free(rlm_ando_aux1s);
  free(rlm_ando_aux2s);
  free(rlm_ando_vlog101s);
  free(rlm_ando_vlog102s);
  free(rlm_ando_vlog103s);
  free(rlm_ando_vlog104s);
  free(rlm_ando_vlog105s);
  free(rlm_ando_vlog106s);
  free(rlm_ando_vlog107s);
  free(rlm_ando_vlog108s);
  free(rlm_ando_vlog109s);
  free(rlm_ando_vlog110s);
  free(rlm_ando_vlog111s);
  free(rlm_ando_vlog112s);
  free(rlm_ando_vlog113s);
  free(rlm_ando_vlog114s);
  free(rlm_ando_vlog115s);
  free(rlm_ando_vlog116s);
  free(rlm_ando_vlog117s);
  free(rlm_ando_vlog118s);
  free(rlm_ando_vlog119s);
  free(rlm_ando_vlog120s);
}


void rlm_ando_RLMPadDataSent(void *ptr,const struct rlm_svc *svc,
			     const struct rlm_log *log,
			     const struct rlm_pad *now,
			     const struct rlm_pad *next)
{
  int i;
  int flag=0;
  char fmt[1024];
  char msg[1500];
  int hours;
  int minutes;
  int seconds;

  for(i=0;i<rlm_ando_devs;i++) {
    switch(log->log_mach) {
    case 0:
      flag=rlm_ando_masters[i];
      break;

    case 1:
      flag=rlm_ando_aux1s[i];
      break;

    case 2:
      flag=rlm_ando_aux2s[i];
      break;

    case 100:
      flag=rlm_ando_vlog101s[i];
      break;

    case 101:
      flag=rlm_ando_vlog102s[i];
      break;

    case 102:
      flag=rlm_ando_vlog103s[i];
      break;

    case 103:
      flag=rlm_ando_vlog104s[i];
      break;

    case 104:
      flag=rlm_ando_vlog105s[i];
      break;

    case 105:
      flag=rlm_ando_vlog106s[i];
      break;

    case 106:
      flag=rlm_ando_vlog107s[i];
      break;

    case 107:
      flag=rlm_ando_vlog108s[i];
      break;

    case 108:
      flag=rlm_ando_vlog109s[i];
      break;

    case 109:
      flag=rlm_ando_vlog110s[i];
      break;

    case 110:
      flag=rlm_ando_vlog111s[i];
      break;

    case 111:
      flag=rlm_ando_vlog112s[i];
      break;

    case 112:
      flag=rlm_ando_vlog113s[i];
      break;

    case 113:
      flag=rlm_ando_vlog114s[i];
      break;

    case 114:
      flag=rlm_ando_vlog115s[i];
      break;

    case 115:
      flag=rlm_ando_vlog116s[i];
      break;

    case 116:
      flag=rlm_ando_vlog117s[i];
      break;

    case 117:
      flag=rlm_ando_vlog118s[i];
      break;

    case 118:
      flag=rlm_ando_vlog119s[i];
      break;

    case 119:
      flag=rlm_ando_vlog120s[i];
      break;
    }
    if((flag==1)||((flag==2)&&(log->log_onair!=0))) {
      if(strlen(rlm_ando_labels+256*i)==0) {  // Original format
	snprintf(fmt,1024,"^%s~%s~%02d:%02d~%%g~%s~%%n|",
		 rlm_ando_artists+256*i,
		 rlm_ando_titles+256*i,
		 now->rlm_len/60000,(now->rlm_len%60000)/1000,
		 rlm_ando_albums+256*i);
      }
      else {  // Enhanced format
	hours=now->rlm_len/3600000;
	minutes=(now->rlm_len-hours*3600000)/60000;
	seconds=(now->rlm_len-hours*3600000-minutes*60000)/1000;
	snprintf(fmt,1024,"^%s~%s~%02d:%02d:%02d~%%g~%%n~%s~%s|",
		 rlm_ando_artists+256*i,
		 rlm_ando_titles+256*i,
		 hours,minutes,seconds,
		 rlm_ando_albums+256*i,
		 rlm_ando_labels+256*i);
      }
      const char *str=RLMResolveNowNext(ptr,now,next,fmt);
      RLMSendUdp(ptr,rlm_ando_addresses+i*16,rlm_ando_ports[i],str,strlen(str));
      snprintf(msg,1500,"rlm_ando: sending pad update: \"%s\"",
	       (const char *)str);
      RLMLog(ptr,LOG_INFO,msg);
    }
  }
}


void rlm_ando_RLMTimerExpired(void *ptr,int timernum)
{
  int i;

  switch(timernum) {
    case 0:  // Heartbeat
      for(i=0;i<rlm_ando_devs;i++) {
	RLMSendUdp(ptr,rlm_ando_addresses+i*16,rlm_ando_ports[i],"HB",2);
      }
      break;
  }
}
