/*
 * Command Line Interface (CLI) out Serial port for filesystem basic
 * operations. The Adafruit M0/M4 Express/CPX have a SPI Flash chip which is
 * very useful for parameters, data logging, etc. This program provides an easy
 * way to access the files stored in the SPI Flash chip using a terminal
 * program. minicom on Linux/Unix works well. The Arduino Serial monitor also
 * works. TeraTerm or Putty on Windows should work.
 *
 * Test on Adafruit Metro M4 Express.
 *
 * This is much easier to implement and understand than USB Mass Storage or USB
 * Media Transfer Protocol.
 *
 * ## Print file and directory names. dir and ls are synonyms. [dirname] is
 * optional. Defaults to working directory.
 *
 *      dir [dirname], ls [dirname]
 *
 * ## Print working directory. pwd and cd (no parameter) are synonyms.
 *
 *      pwd, cd
 *
 * ## Change working directory. If no dirname is specified, prints the working
 * directory. If full path is not specified the working directory is used.
 *
 *      cd <dirname>
 *
 * ## Make directory. Multiple levels may be specified. E.g. mkdir
 * dir1/dir2/dir3. If full path is not specified the working directory is used.
 *
 *      mkdir <dirname>
 *
 * ## Remove directory and ALL files and subdirectories. Works like "rm -rf" so
 * be careful. If full path is not specified the working directory is used.
 *
 *      rmdir <dirname>
 *
 * ## Remove file. del and rm are synonyms. If full path is not specified the
 * working directory is used.
 *
 *      del <filename>, rm <filename>
 *
 * ## Print file contents. type and cat are synonyms. If full path is not
 * specified the working directory is used. Use the terminal program logging or
 * ASCII file capture to save the contents to a computer file.
 *
 *      type <filename>, cat <filename>
 *
 * ## Read from Serial port and write to a file. Terminate with ^D. If full
 * path is not specified the working directory is used.
 *
 *      capture <filename>
 *
 * E.g. Run "capture params.json". In the terminal program, send the contents
 * of an ASCII file. Use copy and paste or ASCII upload feature of the terminal
 * program. Close the file by pressing ^D. Examine the file using "cat
 * params.json".
 *
 * ## Receive YMODEM batch mode. The sender may send 0 or more files including
 * file names. rb receives and creates the files.
 *
 *    rb
 *
 * ## Receive one file using XMODEM. The XMODEM protocol does not allow the
 * sender to send the filename. Do not use this unless YMODEM is not available.
 * The XMODEM protocol also pads files to multiples of 128 bytes.
 *
 *    rx <filename>
 *
 * ## TODO maybe, not too useful
 *
 *    ren <fromfilename> <tofilename>, mv <fromfilename> <tofilename>
 *
 *    copy <fromfilename> <tofilename>, cp <fromfilename> <tofilename>
 *
 */

#include <SerialFileBrowser.h>

// select and include the header for the filesystem you want to use here
#include <SD.h>
#define FATFILESYS SD
#define chipSelect BUILTIN_SDCARD
#define XMODEM_PORT Serial

SerialFileBrowser filebrowser(XMODEM_PORT, FATFILESYS);

void setup() {
  // Initialize serial port and wait for it to open before continuing.
  // But do not wait forever!
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(100);
  }
  Serial.println("FatFs Command Line Interpreter");
  SerialUSB1.begin(115200);
  SerialUSB1.println("(Debug) FatFs Command Line Interpreter");

  // If your filesystem needs more initialization than a single begin() call, do that here:
  if(!FATFILESYS.begin(chipSelect)) {
    Serial.println("Error, failed to mount filesystem!");
    while(1);
  }

  filebrowser.setup_cli();
}

void loop() {
  filebrowser.loop_cli();
}
