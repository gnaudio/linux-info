/* MIT License
 *
 * Copyright (c) 2017 GN Audio A/S (Jabra)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file   jabra_hiddev_demo.c
 *
 * @brief  Demonstration program for basic call control functionality:
 *         mute/offhook/ringer using the Linux hiddev device interface.
 *
 *         This program will work with most Jabra devices.
 *
 *         The program must have priviledges to read and write the
 *         /dev/usb/hiddev[0-19] device.
 *
 *         To compile:
 *         gcc jabra_hiddev_demo.c -o jabra_hiddev_demo -lpthread
 *
 * @author Flemming Mortensen
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hiddev.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/
#define HIDDEBUG 0

/* Jabra Vendor Id */
#define JABRA_VID            ((__u16) 0x0B0E)

/* HID Usage Page definitions */
#define TelephonyUsagePage   ((__u16) 0x000B)
#define ConsumerUsagePage    ((__u16) 0x000C)
#define LEDUsagePage         ((__u16) 0x0008)
#define ButtonUsagePage      ((__u16) 0x0009)

/* HID Usage Id definitions: LED usage page (0x08) */
#define Led_Mute             ((__u16) 0x0009)
#define Led_Off_Hook         ((__u16) 0x0017)
#define Led_Ring             ((__u16) 0x0018)
#define Led_Hold             ((__u16) 0x0020)
#define Led_Microphone       ((__u16) 0x0021)
#define Led_On_Line          ((__u16) 0x002A)
#define Led_Off_Line         ((__u16) 0x002B)

/* HID Usage Id definitions:  Telephony usage page (0x0B) */
#define Tel_Hook_Switch      ((__u16) 0x0020)
#define Tel_Flash            ((__u16) 0x0021)
#define Tel_Feature          ((__u16) 0x0022)
#define Tel_Hold             ((__u16) 0x0023)
#define Tel_Redial           ((__u16) 0x0024)
#define Tel_Transfer         ((__u16) 0x0025)
#define Tel_Drop             ((__u16) 0x0026)
#define Tel_Park             ((__u16) 0x0027)
#define Tel_Forward          ((__u16) 0x0028)
#define Tel_Alternate        ((__u16) 0x0029)
#define Tel_Line             ((__u16) 0x002A)
#define Tel_Speaker          ((__u16) 0x002B)
#define Tel_Conference       ((__u16) 0x002C)
#define Tel_Ring_Enable      ((__u16) 0x002D)
#define Tel_Ring_Select      ((__u16) 0x002E)
#define Tel_Phone_Mute       ((__u16) 0x002F)
#define Tel_Caller           ((__u16) 0x0030)
#define Tel_Send             ((__u16) 0x0031)
#define Tel_VoiceMail        ((__u16) 0x0070)
#define Tel_Ringer           ((__u16) 0x009E)
#define Tel_Phone_Key_0      ((__u16) 0x00B0)
#define Tel_Phone_Key_1      ((__u16) 0x00B1)
#define Tel_Phone_Key_2      ((__u16) 0x00B2)
#define Tel_Phone_Key_3      ((__u16) 0x00B3)
#define Tel_Phone_Key_4      ((__u16) 0x00B4)
#define Tel_Phone_Key_5      ((__u16) 0x00B5)
#define Tel_Phone_Key_6      ((__u16) 0x00B6)
#define Tel_Phone_Key_7      ((__u16) 0x00B7)
#define Tel_Phone_Key_8      ((__u16) 0x00B8)
#define Tel_Phone_Key_9      ((__u16) 0x00B9)
#define Tel_Phone_Key_Star   ((__u16) 0x00BA)
#define Tel_Phone_Key_Pound  ((__u16) 0x00BB)
#define Tel_Phone_Key_A      ((__u16) 0x00BC)
#define Tel_Phone_Key_B      ((__u16) 0x00BD)
#define Tel_Phone_Key_C      ((__u16) 0x00BE)
#define Tel_Phone_Key_D      ((__u16) 0x00BF)
#define Tel_Control          ((__u16) 0xFFFF)

/* HID Usage Id definitions: Consumer usage page (0x0C) */
#define Con_Volume_Incr      ((__u16) 0x00E9)
#define Con_Volume_Decr      ((__u16) 0x00EA)

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/
static int fd;
static int mutestate;
static int hookstate;
static int ringerstate;
static int run = 1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/* empty */

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/
static const char *usagePageName(__u32 usage_code) {
  __u16 hi = (usage_code >> 16) & 0xFFFF;

  switch (hi) {
    case TelephonyUsagePage:   return "TelephonyUsagePage";
    case ConsumerUsagePage:    return "ConsumerUsagePage";
    case LEDUsagePage:         return "LEDUsagePage";
    case ButtonUsagePage:      return "ButtonUsagePage";
    default:                   return "not translated";
  }
}

#if (HIDDEBUG == 1)
static void showReports(int fd, __u16 report_type) {
  struct hiddev_report_info rinfo;
  struct hiddev_field_info finfo;
  struct hiddev_usage_ref uref;
  int ret;

  rinfo.report_type = report_type;
  rinfo.report_id = HID_REPORT_ID_FIRST;
  ret = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);

  while (ret >= 0) {
    printf("HIDIOCGREPORTINFO: report_id=0x%X (%u fields)\n", rinfo.report_id, rinfo.num_fields);
    for (int i = 0; i < rinfo.num_fields; i++) {
      finfo.report_type = rinfo.report_type;
      finfo.report_id   = rinfo.report_id;
      finfo.field_index = i;
      ioctl(fd, HIDIOCGFIELDINFO, &finfo);

      fprintf(stdout, "HIDIOCGFIELDINFO: field_index=%u maxusage=%u flags=0x%X\n"
          "\tphysical=0x%X logical=0x%X application=0x%X reportid=0x%X\n"
          "\tlogical_minimum=%d,maximum=%d physical_minimum=%d,maximum=%d\n",
        finfo.field_index,
        finfo.maxusage,
        finfo.flags,
        finfo.physical,
        finfo.logical,
        finfo.application,
        finfo.report_id,
        finfo.logical_minimum,
        finfo.logical_maximum,
        finfo.physical_minimum,
        finfo.physical_maximum);

      for (int j = 0; j < finfo.maxusage; j++) {
        uref.report_type = finfo.report_type;
        uref.report_id   = finfo.report_id;
        uref.field_index = i;
        uref.usage_index = j;
        ioctl(fd, HIDIOCGUCODE, &uref);
        ioctl(fd, HIDIOCGUSAGE, &uref);

        fprintf(stdout, " >> usage_index=%u usage_code=0x%X (%s) value=%d\n",
          uref.usage_index,
          uref.usage_code,
          usagePageName(uref.usage_code),
          uref.value);

      }
    }
    fprintf(stdout, "\n");

    rinfo.report_id |= HID_REPORT_ID_NEXT;
    ret = ioctl(fd, HIDIOCGREPORTINFO, &rinfo);
  }
}
#endif

static int doListDev(char *path) {
  int fd;
  struct hiddev_devinfo devinfo;
  char name[128];
  int version;

  if ((fd = open(path, O_RDONLY)) != -1) {
    if (ioctl(fd, HIDIOCGDEVINFO, &devinfo) == -1) {
      perror("ioctl HIDIOCGDEVINFO");
      return -1;
    }
    if (ioctl(fd, HIDIOCGNAME(sizeof(name)), name) == -1) {
      perror("ioctl HIDIOCGNAME");
      return -1;
    }
    if (ioctl(fd, HIDIOCGVERSION, &version) == -1) {
      perror("ioctl HIDIOCGVERSION");
      return -1;
    }
    return (devinfo.vendor == JABRA_VID);
  }

  if (errno == ENOENT) {
    return 0;
  }

  perror("ioctl HIDIOCGVERSION");
  return -1;
}

static void writeUsage(int fd, unsigned report_type, unsigned page, unsigned code, __s32 value) {
  struct hiddev_report_info rinfo;
  struct hiddev_field_info finfo;
  struct hiddev_usage_ref uref;

  /* find the requested usage code */
  uref.report_type = report_type;
  uref.report_id   = HID_REPORT_ID_UNKNOWN;
  uref.usage_code  = (page << 16) | code;
  if (ioctl(fd, HIDIOCGUSAGE, &uref) < 0) {
    perror("HIDIOCGUSAGE");
    return;
  }
#if (HIDDEBUG == 1)
  fprintf(stdout, " >> usage_index=%u usage_code=0x%X (%s) value=%d\n",
    uref.usage_index,
    uref.usage_code,
    usagePageName(uref.usage_code),
    uref.value);
#endif
  /* retrieve field info */
  finfo.report_type = uref.report_type;
  finfo.report_id   = uref.report_id;
  finfo.field_index = uref.field_index;
  if (ioctl(fd, HIDIOCGFIELDINFO, &finfo) < 0) {
    perror("HIDIOCGFIELDINFO");
    return;
  }
#if (HIDDEBUG == 1)
  fprintf(stdout, "HIDIOCGFIELDINFO: field_index=%u maxusage=%u flags=0x%X\n"
   "\tphysical=0x%X logical=0x%X application=0x%X reportid=0x%X\n"
   "\tlogical_minimum=%d,maximum=%d physical_minimum=%d,maximum=%d\n",
    finfo.field_index,
    finfo.maxusage,
    finfo.flags,
    finfo.physical,
    finfo.logical,
    finfo.application,
    finfo.report_id,
    finfo.logical_minimum,
    finfo.logical_maximum,
    finfo.physical_minimum,
    finfo.physical_maximum);
#endif
  if ((value < finfo.logical_minimum) || (value > finfo.logical_maximum)) {
    fprintf(stdout, "%s: value %d outside of allowed range (%d-%d)\n",
      usagePageName(uref.usage_code),
      value,
      finfo.logical_minimum,
      finfo.logical_maximum);
    return;
  }

  /* set value */
  uref.value = value;
  if (ioctl(fd, HIDIOCSUSAGE, &uref) < 0) {
    perror("HIDIOCSUSAGE");
    return;
  }

  rinfo.report_type = uref.report_type;
  rinfo.report_id   = uref.report_id;
  if (ioctl(fd, HIDIOCSREPORT, &rinfo) < 0) {
    perror("HIDIOCSREPORT");
  }
}

static void readUsage(int fd, unsigned report_type, unsigned page, unsigned code, __s32* value) {
  struct hiddev_report_info rinfo;
  struct hiddev_field_info finfo;
  struct hiddev_usage_ref uref;

  /* find the requested usage code */
  uref.report_type = report_type;
  uref.report_id   = HID_REPORT_ID_UNKNOWN;
  uref.usage_code  = (page << 16) | code;
  if (ioctl(fd, HIDIOCGUSAGE, &uref) < 0) {
    perror("HIDIOCGUSAGE");
    return;
  }
#if (HIDDEBUG == 1)
  fprintf(stdout, " >> usage_index=%u usage_code=0x%X (%s) value=%d\n",
    uref.usage_index,
    uref.usage_code,
    usagePageName(uref.usage_code),
    uref.value);
#endif
  /* retrieve field info */
  finfo.report_type = uref.report_type;
  finfo.report_id   = uref.report_id;
  finfo.field_index = uref.field_index;
  if (ioctl(fd, HIDIOCGFIELDINFO, &finfo) < 0) {
    perror("HIDIOCGFIELDINFO");
    return;
  }
#if (HIDDEBUG == 1)
  fprintf(stdout, "HIDIOCGFIELDINFO: field_index=%u maxusage=%u flags=0x%X\n"
   "\tphysical=0x%X logical=0x%X application=0x%X reportid=0x%X\n"
   "\tlogical_minimum=%d,maximum=%d physical_minimum=%d,maximum=%d\n",
    finfo.field_index,
    finfo.maxusage,
    finfo.flags,
    finfo.physical,
    finfo.logical,
    finfo.application,
    finfo.report_id,
    finfo.logical_minimum,
    finfo.logical_maximum,
    finfo.physical_minimum,
    finfo.physical_maximum);
#endif
#if 0
  if ((value < finfo.logical_minimum) || (value > finfo.logical_maximum)) {
    fprintf(stdout, "%s: value %d outside of allowed range (%d-%d)\n",
      usagePageName(uref.usage_code),
      value,
      finfo.logical_minimum,
      finfo.logical_maximum);
    return;
  }
#endif
  /* get value */
  //  uref.value = value;
  if (ioctl(fd, HIDIOCGUSAGE, &uref) < 0) {
    perror("HIDIOCGUSAGE");
    return;
  }
  *value = uref.value;

  rinfo.report_type = uref.report_type;
  rinfo.report_id   = uref.report_id;
  if (ioctl(fd, HIDIOCSREPORT, &rinfo) < 0) {
    perror("HIDIOCSREPORT");
  }
}

static void* event_loop(void *ptr) {
  int i;
  int debug = 0;
  struct timeval tv;
  fd_set fdset;
  FD_ZERO(&fdset);

  while (run == 1) {
    struct hiddev_event ev[64];
    FD_SET(fd, &fdset);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int rd = select(fd + 1, &fdset, NULL, NULL, &tv);

    if (rd > 0) {
      rd = read(fd, ev, sizeof(ev));
      if (rd < (int) sizeof(ev[0])) {
        if (rd < 0)
          perror("error reading");
        else
          fprintf(stderr, "got too short read from device\n");
        run = 0;
        return (void*) -1;
      }

      for (i = 0; i < rd / sizeof(ev[0]); i++) {
        if (debug)
          fprintf(stdout, "Event: %x = %d\n", ev[i].hid, ev[i].value);

        (void)pthread_mutex_lock(&lock);

        switch (ev[i].hid >> 16) {
          case TelephonyUsagePage:
            //fprintf(stdout, "Event: %x = %d\n", ev[i].hid, ev[i].value);
            switch (ev[i].hid & 0xFFFF) {
              case Tel_Hook_Switch:
                if (hookstate != ev[i].value) {
                  if (hookstate == 0) {
                    writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Ring, 0);
                    writeUsage(fd, HID_REPORT_TYPE_OUTPUT, TelephonyUsagePage, Tel_Ringer, 0);
                  }
                  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Off_Hook, ev[i].value);
                  hookstate = ev[i].value;
                  hookstate == 0 ? fprintf(stdout, "--> Hook in place\n") : fprintf(stdout, "--> Hook lifted\n");
                }
                break;
              case Tel_Phone_Mute:
                //fprintf(stdout, "Event: %x = %d\n", ev[i].hid, ev[i].value);
                if (ev[i].value == 1) {
                  mutestate = !mutestate;
                  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Mute, mutestate);
                  mutestate == 0 ? fprintf(stdout, "--> Unmuted\n") : fprintf(stdout, "--> Muted\n");
                }
                break;
              default:
                break;
            }
            break;
          case ConsumerUsagePage:
            //fprintf(stdout, "Event: %x = %d\n", ev[i].hid, ev[i].value);
            switch (ev[i].hid & 0xFFFF) {
              case Con_Volume_Decr:
                if (ev[i].value) fprintf(stdout, "Volume decrement = 0x%x\n", ev[i].value);
                break;
              case Con_Volume_Incr:
                if (ev[i].value) fprintf(stdout, "Volume increment = 0x%x\n", ev[i].value);
                break;
              default:
                break;
            }
            break;
          default:
            break;
        }
        (void)pthread_mutex_unlock(&lock);
      }
    }
    fflush(stdout);
  }
  return (void*)0;
}

static void hit_key(char key) {

  switch (key) {
    case 'o':
      (void)pthread_mutex_lock(&lock);
      hookstate = !hookstate;
      if (hookstate == 1) {
        writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Ring, 0);
        writeUsage(fd, HID_REPORT_TYPE_OUTPUT, TelephonyUsagePage, Tel_Ringer, 0);
      }
      writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Off_Hook, hookstate);
      hookstate == 0 ? fprintf(stdout, "<-- Put back Hook\n") : fprintf(stdout, "<-- Lift Hook\n");
      (void)pthread_mutex_unlock(&lock);
      break;
    case 'm':
      (void)pthread_mutex_lock(&lock);
      mutestate = !mutestate;
      writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Mute, mutestate);
      mutestate == 0 ? fprintf(stdout, "<-- Unmute\n") : fprintf(stdout, "<-- Mute\n");
      (void)pthread_mutex_unlock(&lock);
      break;
    case 'r':
      (void)pthread_mutex_lock(&lock);
      ringerstate = !ringerstate;
      writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Ring, ringerstate);
      writeUsage(fd, HID_REPORT_TYPE_OUTPUT, TelephonyUsagePage, Tel_Ringer, ringerstate);
      (void)pthread_mutex_unlock(&lock);
      break;
    case 'q':
      run = 0;
      break;
    case '?':
      fprintf(stdout, "Usage:\n");
      fprintf(stdout, " o = offhook tooggle\n");
      fprintf(stdout, " m = mute tooggle\n");
      fprintf(stdout, " r = ringer tooggle\n");
      fprintf(stdout, " q = quit\n");
      fprintf(stdout, " ? = this help\n");
      break;
    default:
      break;
  }
}

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/
int main(int argc, char**argv) {
  int i;
  char name[128];
  int retval = 0;
  pthread_t event_thread;

  for (i = 0; i < 19; i++) {
    sprintf(name, "/dev/usb/hiddev%d", i);
    if (doListDev(name) == 1) {
      break;
    }
  }

  if (i == 19) {
    fprintf(stderr, "No Jabra device found\n");
    return -1;
  }

  sprintf(name, "/dev/usb/hiddev%d", i);
  fprintf(stdout, "Using device %s\n", name);

  if ((fd = open(name, O_RDONLY)) < 0) {
    if (errno == EACCES) {
      fprintf(stderr, "No permission, try this as root.\n");
      return -1;
    }
  }

  ioctl(fd, HIDIOCINITREPORT, 0);
  ioctl(fd, HIDIOCGNAME(sizeof(name)), name);
  printf("HID device name: \"%s\"\n", name);
#if (HIDDEBUG == 1)
  fprintf(stdout, "\n*** INPUT:\n"); showReports(fd, HID_REPORT_TYPE_INPUT);
  fprintf(stdout, "\n*** OUTPUT:\n"); showReports(fd, HID_REPORT_TYPE_OUTPUT);
  fprintf(stdout, "\n*** FEATURE:\n"); showReports(fd, HID_REPORT_TYPE_FEATURE);
#endif

  /* set initial values */
  mutestate = 0;
  hookstate = 0;
  ringerstate = 0;
  printf("Reading\n");
  readUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Mute, &mutestate);
  printf("mutestate=%i\n", mutestate);
  readUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Off_Hook, &hookstate);
  printf("hookstate=%i\n", hookstate);
  readUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Ring, &ringerstate);
  printf("ringerstate=%i\n", ringerstate);
#if 0
  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Mute, 0);
  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Ring, 0);
  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, TelephonyUsagePage, Tel_Ringer, 0);
  writeUsage(fd, HID_REPORT_TYPE_OUTPUT, LEDUsagePage, Led_Off_Hook, 0);
#endif
  if (pthread_create(&event_thread, NULL, event_loop, &retval)) {
    fprintf(stderr, "Error creating thread\n");
    close(fd);
    return -1;
  }

  hit_key('?');

  fcntl(0, F_SETFL, O_NONBLOCK);

  while(run == 1) {
    char c;
    if (read(0, &c, 1) == 1) {
      hit_key(c);
    }
    usleep(1000*100);
  }

  if (pthread_join(event_thread, NULL)) {
    fprintf(stderr, "Error joining thread\n");
    retval = -1;
  }

  close(fd);
  return retval;
}
