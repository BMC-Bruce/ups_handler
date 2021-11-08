
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <system_error>

using namespace std;

extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
};

namespace fs = std::filesystem;

/*
 * Signals to be read by the UPS TO USB cable Part No. 00FV631
 * for monitoring the potentially attached UPS.
 *     CTS - Clear To Send       - UPS On
 *     DCD - Data Carrier Detect - UPS Batter Low
 *     RI  - Ring Indicator      - UPS Utility Fail
 *     DSR - Data Set Ready      - UPS Bypass
 */
int readSerialPortControlSignals(char *device, int *upsSignals) {
  if (0 != access(device, R_OK)) {
    cerr << "The device does not have read access, " << device << "\n";
    return -1; // let the caller check errno
  }

  struct stat deviceStat;
  if (-1 == lstat(device, &deviceStat)) {
    cerr << "Could not get the lstat (for file type and mode) of the device "
         << device << "\n";
    return -1; // let the caller check errno
  }

  if (S_IFCHR != (deviceStat.st_mode & S_IFMT)) {
    cerr << "This is not a character special device " << device << "\n";
    return -2; // let the caller know
  }

  int fd = ::open(device, O_RDONLY);
  if (-1 == fd) {
    cerr << "Cannnot open device " << device << " for reading.\n";
    return -1; // let the caller check errno
  }

  /*
   * man TTY_IOCTL for detials on serial lines ioctls,
   * we are checking Modem control lines:
   *     TIOCM_CTS       CTS (clear to send)
   *     TIOCM_CAR       DCD (data carrier detect)
   *     TIOCM_CD         see TIOCM_CAR
   *     TIOCM_RNG       RNG (ring)
   *     TIOCM_RI         see TIOCM_RNG
   *     TIOCM_DSR       DSR (data set ready)
   */
  int modemControlBits;
  int rc = ioctl(fd, TIOCMGET, &modemControlBits);
  if (-1 == rc) {
    ::close(fd);
    cerr << "Cannnot ioctl (with TIOCMGET) device " << device << "\n";
    return -1; // let the caller check errno
  }

  ::close(fd);

  // only keep the 4 (bits) Signals we are intrested in
  modemControlBits &= (TIOCM_CTS | TIOCM_CAR | TIOCM_RNG | TIOCM_DSR);
  *upsSignals = modemControlBits;
  return 0;
}

int main(int argc, char *argv[]) {
  if (2 > argc) {
    cerr << "Not enough argments.\n";
    cerr << "Example Usage: " << argv[0] << " /dev/ttyS0\n";
    cerr << "               " << argv[0] << " /dev/ttyS1\n";
    cerr << "               " << argv[0] << " /dev/ttyUSB0\n";
    cerr << "               " << argv[0] << " /dev/ttyUSB1\n";
    exit(-1);
  }

  int myUpsSignals;
  int result;
  result = readSerialPortControlSignals(argv[1], &myUpsSignals);

  switch (result) {
  case 0:
    //       fprintf(stderr, "myUpsSignals = %d (0x%08X)\n", myUpsSignals,
    //       myUpsSignals);
    cerr << "myUpsSignals = " << myUpsSignals << "\n";
    if (myUpsSignals & TIOCM_CTS) {
      cerr << "CTS (UPS On) is SET.\n";
    } else {
      cerr << "CTS (UPS On) is CLEAR.\n";
    }
    if (myUpsSignals & TIOCM_CAR) {
      cerr << "DCD (UPS Batter Low) is SET.\n";
    } else {
      cerr << "DCD (UPS Batter Low) is CLEAR.\n";
    }
    if (myUpsSignals & TIOCM_RNG) {
      cerr << "RI (UPS Utility Fail) is SET.\n";
    } else {
      cerr << "RI (UPS Utility Fail) is CLEAR.\n";
    }
    if (myUpsSignals & TIOCM_DSR) {
      cerr << "DSR (UPS Bypass) is SET.\n";
    } else {
      cerr << "DSR (UPS Bypass) is CLEAR.\n";
    }
    break;

  case -1:
    cerr << errno;
    perror(" errno ERROR:");

  // fall through from previous case
  case -2:
    cerr << "There error is with device " << argv[1] << "\n";
    break;

  default:
    cerr << "There error is with device " << argv[1] << "\n";
    cerr << "Unknow Error result " << result << ", this should NOT happen!\n";
    break;
  }

  exit(0);
}
