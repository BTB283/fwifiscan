#include <gps.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <iwlib.h>
#include <alsa/asoundlib.h>
#define PCM_DEVICE "default"


// blacklist stuff
struct strptr
{
  char* str;
  struct strptr *next;
};
struct strptr *blptr;
struct strptr *blhead;


// gps function (updates time and coordinates continually if gps is found)
void gps_run();
static struct gps_data_t gpsdata;


// sound function (runs on separate thread so program doesn't pause and wifi card scans faster
void beep();
static unsigned int numnetworks;
snd_pcm_t * pcm_handle;
// baked in beep sounds, all 44100 frames per second, and 1 second long
extern char _binary___beepone_start[];
extern char _binary___beeptwo_start[];
extern char _binary___beepthree_start[];


int main(int argc, char ** argv)
{
  //---------------------------------------------------
  // CHECK ARGUMENT COUNT AND PRINT USAGE DISCLAIMER
  //---------------------------------------------------
  if (argc != 2 && argc != 3)
  {
    printf("fwifiscan <wireless interface> <blacklist file>\n");
    printf("Beeps indicate networks found - 3 beeps indicate 3 or more networks\n");
    return -1;
  }


  //---------------------------------------------------
  // SET UP ESSID BLACKLIST ARRAY
  //---------------------------------------------------
  if (argc == 3)
  {
    char * buff;
    FILE * fp = fopen(argv[2], "r");
    if (fp == 0)
    {
      printf("specified blacklist file does not exist\n");
      return -1;
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    // its empty - or, what if there was a single character and nothing else? think about it
    if (length < 2)
    {
      printf("blacklist file is empty\n");
      return -1;
    }
    fseek(fp, 0, SEEK_SET);
    buff = malloc(length);
    fgets(buff, length + 1, fp);
    // it would be really good if this below part, down to the fclose was a singly linked list. ill fix it later
    blhead = malloc(sizeof(struct strptr));
    blhead->str = strtok(buff, ",");
    blptr = blhead;
    while (1)
    {
        char *newstr = strtok(NULL, ",");
        if (newstr == NULL) break;
        struct strptr *new  = malloc(sizeof(struct strptr));
        blptr->next = new;
        blptr = new;
        blptr->str = newstr;
    }
    // reset pointer to head, so we can traverse later
    blptr = blhead;
    fclose(fp);
  }


  //---------------------------------------------------
  // SET UP GPS
  //---------------------------------------------------
  bool has_gps = false;
  int ret;
  pthread_t gps_thread;
  if (gps_open("localhost", "2947", & gpsdata) == 0)
  {
    has_gps = true;
    (void) gps_stream( & gpsdata, WATCH_ENABLE | WATCH_JSON, NULL);
    // update gpsdata struct in the background, continually
    pthread_create( & gps_thread, NULL, gps_run, NULL);
  }


  //---------------------------------------------------
  // ANOTHER DISCLAIMER (NEEDED GPS STATUS FOR THIS ONE)
  //---------------------------------------------------
  if (has_gps)
  {
    printf("essid,gpsunixtime,longitude,latitude\n");
  }
  else
  {
    printf("essid,unixtime\n");
  }


  //---------------------------------------------------
  // SET UP SCANNER
  //---------------------------------------------------
  wireless_scan_head head;
  wireless_scan * result;
  iwrange range;
  time_t ctime;
  struct tm * ltime;
  int sock = iw_sockets_open();
  // get scanner specs (throughput, sensitivity, etc)
  if (iw_get_range_info(sock, argv[1], & range) < 0)
  {
    printf("iw_get_range_info failed\n");
    return(-1);
  }


  //---------------------------------------------------
  // SOUND SET UP
  //---------------------------------------------------
  int pcm;
  int rate = 44100;
  snd_pcm_hw_params_t * params;
  // open pcm in playback mode
  if (snd_pcm_open( & pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0)
  {
    printf("cant open %s\n", PCM_DEVICE);
  }
  // set pcm to play 16 bit wav
  snd_pcm_hw_params_alloca( & params);
  snd_pcm_hw_params_any(pcm_handle, params);
  snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
  snd_pcm_hw_params_set_rate_near(pcm_handle, params, & rate, 0);
  snd_pcm_hw_params(pcm_handle, params);
  pthread_t beep_thread;


  //---------------------------------------------------
  // SCAN
  //---------------------------------------------------
  bool blacklisted = false;
  while (1)
  {
    if (iw_scan(sock, argv[1], range.we_version_compiled, & head) < 0)
    {
      printf("iw_scan failed\n");
      return(-1);
    }
    // print scan results
    result = head.result;
    time( & ctime);
    ltime = localtime( & ctime);
    numnetworks = 0;

    while (NULL != result)
    {
      blacklisted = false;
        if (argc == 3)
        {
           while (1)
           {
            if (strcmp(blptr->str, result->b.essid) == 0)
            {
              blacklisted = true;
              break;
            }
            if ( blptr->next == NULL) break; else blptr = blptr->next;
           }
           // reset pointer to head, again
          blptr = blhead;
        }
      // pretty sure 34816 means unsecured and 2048 means secured
      if (result -> b.key_flags == 34816 && !blacklisted)
      {
        // print network essid
        if (strlen(result -> b.essid) == 0)
        {
          printf("NO ESSID,");
        }
        else
        {
          printf("%s,", result -> b.essid);
        }
        // if gps is available, use its time, and print coordinates
        if (has_gps)
        {
          printf("%10.0f,%f,%f\n", gpsdata.fix.time, gpsdata.fix.longitude, gpsdata.fix.latitude);
        }
        // if not, then use computer time, and don't print coordinates
        else
        {
          printf("%lu\n", (unsigned long)time(NULL));
        }
        numnetworks++;
      }
      result = result -> next;
    }
    // run the sound on a separate thread so it doesn't interrupt the program and slow down scanning
    pthread_create( & beep_thread, NULL, beep, NULL);
  }
  snd_pcm_drain(pcm_handle);
  snd_pcm_close(pcm_handle);
  return(-1);
}

void gps_run()
{
  while (1)
  {
    // wait 500ms if no new input
    if (gps_waiting( & gpsdata, 500))
    {
      gps_read( & gpsdata);
    }
  }
  pthread_exit(NULL);
}

void beep()
{
  snd_pcm_prepare(pcm_handle);
  switch (numnetworks)
  {
  case 0:
    // no networks, therefore no sound
    break;
  case 1:
    snd_pcm_writei(pcm_handle, _binary___beepone_start, 44100);
    break;
  case 2:
    snd_pcm_writei(pcm_handle, _binary___beeptwo_start, 44100);
    break;
  default:
    snd_pcm_writei(pcm_handle, _binary___beepthree_start, 44100);
    break;
  }
  pthread_exit(NULL);
}
