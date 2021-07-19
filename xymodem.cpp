/*
MIT License

Copyright (c) 2018 gdsports625@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Arduino.h>
#include <xymodem.h>

#define dbprint(...) if(debugPort) debugPort->print(__VA_ARGS__)
#define dbprintln(...) if(debugPort) debugPort->println(__VA_ARGS__)

/*
 * Start XMODEM receive. rx = receive XMODEM
 */
int XYmodem::start_rx(Stream &port, FS &filesys, const char *rx_filename, bool rx_buf_1k, bool useCRC)
{
  YMODEM = false;
  return start(&port, &filesys, rx_filename, rx_buf_1k, useCRC);
}

/*
 * Start YMODEM receive. YMODEM is also known as batch mode. rb = receive batch.
 */
int XYmodem::start_rb(Stream &port, FS &filesys, bool rx_buf_1k, bool useCRC)
{
  YMODEM = true;
  return start(&port, &filesys, NULL, rx_buf_1k, useCRC);
}

int XYmodem::start_rb(Stream &port, FS &filesys, const char *rx_directory, bool rx_buf_1k, bool useCRC)
{
  YMODEM = true;
  return start(&port, &filesys, rx_directory, rx_buf_1k, useCRC);
}

int XYmodem::start(Stream *port, FS *filesys, const char *rx_filename, bool rx_buf_1k, bool useCRC)
{
  rx_buf_size = 128;
  if (rx_buf_1k) {
    rx_buf_size = 1024;
  }
  dbprint("rx_buf_size=");
  dbprintln(rx_buf_size);
  if (rx_buf == NULL) {
    rx_buf = (uint8_t*)malloc(rx_buf_size);
    if (rx_buf == NULL) {
      rxmodem.close();
      dbprintln("XYmodem malloc failed");
      return 1;
    }
  }
  CRC_on = useCRC;
  rxmodem_state = BLOCKSTART;
  next_block = 1;
  reply = (CRC_on)? 'C' : NAK;
  this->port = port;
  this->fsptr = (FS *)filesys;
  port->write(reply);
  port->flush();
  next_millis = millis()+ TIMEOUT_LONG;
  if(YMODEM) {
    if (rx_filename != NULL && *rx_filename != '\0') {
      strncpy(this->rx_dirname, (char *)rx_filename, sizeof(this->rx_filename)-1);
    } else {
      strcpy(this->rx_dirname, "");
    }
  } else if (rx_filename != NULL && *rx_filename != '\0') {
    strcpy(this->rx_dirname, "");
    dbprint("rx file name (rx_buf)="); dbprintln((char *)rx_filename);
    make_full_pathname((char*)rx_filename, this->rx_filename, sizeof(this->rx_filename)-1);
    dbprint("XYmodem starting <"); dbprint(this->rx_filename); dbprintln('>');
    this->fsptr->remove((char *)rx_filename);
    rxmodem = this->fsptr->open(this->rx_filename, FILE_WRITE);
    if (rxmodem) {
      return 0;
    }
    else {
      dbprintln("XYmodem open file failed");
      return 1;
    }
  }
  return 0;
}

int XYmodem::loop(void)
{
  static uint16_t blocksize;
  static uint16_t blocksizenext;
  static uint8_t block;
  int inchar = 0;
  static uint8_t *p;
  static uint8_t datachecksum = 0;
  static uint16_t CRC = 0;
  static uint16_t CRCRx;

  if (rxmodem_state == IDLE) return 0;

  if (millis() > next_millis) {
    port->write(reply);
    port->flush();
    if (reply == NAK || reply == 'C') {
      next_millis = millis() + TIMEOUT_LONG;
      rxmodem_state = BLOCKSTART;
      dbprintln("timeout, send NAK or C");
    }
    else if (reply == CAN) {
      rxmodem_state = IDLE;
      reply = NAK;
      dbprintln("timeout, send CAN");
    }
    return rxmodem_state;
  }
  while (port->available() > 0) {
    inchar = port->read();
    next_millis = millis() + TIMEOUT_SHORT;
    dbprint("inchar=0x"); dbprintln(inchar, HEX);
    switch (rxmodem_state) {
      case IDLE:
        break;
      case BLOCKSTART:
        dbprint("BLOCKSTART 0x"); dbprintln(inchar, HEX);
        switch (inchar) {
          case SOH:
            blocksize = blocksizenext = 128;
            rxmodem_state = BLOCKNUM;
            break;
          case STX:
            blocksize = blocksizenext = 1024;
            if (blocksize > rx_buf_size) {
              reply = NAK;
              rxmodem_state = DATAPURGE;
            }
            else {
              rxmodem_state = BLOCKNUM;
            }
            break;
          case EOT:
            port->write(ACK);
            port->flush();
            next_block = 1;
            if (rxmodem) {
              dbprint("YMODEM="); dbprintln(YMODEM, DEC);
              dbprint("filename="); dbprintln(rx_filename);
              if (YMODEM && ((strcmp(rx_filename, "") == 0) || (strcmp(rx_filename, "/") == 0)))
                rxmodem_state = IDLE;
              else
                rxmodem_state = BLOCKSTART;
              rxmodem.close();
            }
            else {
              rxmodem_state = IDLE;
            }
            break;
        }
        break;
      case BLOCKNUM:
        dbprint("BLOCKNUM block=");
        block = inchar;
        dbprintln(block);
        rxmodem_state = BLOCKCHECK;
        break;
      case BLOCKCHECK:
        dbprint("BLOCKCHECK blockchk=0x");
        dbprintln((uint8_t)(inchar ^ block), HEX);
        if ((uint8_t)(inchar ^ block) == 0xFF) {
          if (((block == next_block) || (block == (next_block-1)))) {
            p = rx_buf;
            datachecksum = 0;
            CRC = 0;
            rxmodem_state = DATABLOCK;
          }
          else {
            reply = CAN;
            rxmodem_state = DATAPURGE;
          }
        }
        else {
          reply = NAK;
          rxmodem_state = DATAPURGE;
        }
        break;
      case DATABLOCK:
        dbprintln("DATABLOCK");
        *p++ = inchar;
        if (CRC_on) {
          CRC = updcrc(inchar, CRC);
        }
        else {
          datachecksum += inchar;
        }
        blocksize--;
        if (blocksize == 0) {
          rxmodem_state = DATACHECK;
        }
        else {
          int bytesAvail, bytesIn;
          bytesAvail = port->available();
          dbprint("bytesAvail=");
          dbprintln(bytesAvail);
          if (bytesAvail > 0) {
            bytesIn = port->readBytes((char *)p, min((uint16_t)bytesAvail, blocksize));
            dbprint("blocksize=");
            dbprint(blocksize);
            dbprint(" bytesIn=");
            dbprintln(bytesIn);
            while (bytesIn--) {
              if (CRC_on) {
                CRC = updcrc(*p++, CRC);
              }
              else {
                datachecksum += *p++;
              }
              blocksize--;
            }
            if (blocksize == 0) rxmodem_state = DATACHECK;
          }
        }
        break;
      case DATACHECK:
        if (CRC_on) {
          dbprint("DATACHECK CRC=0x");
          dbprintln(CRC, HEX);
          CRCRx = inchar << 8;
          rxmodem_state = DATACHECKCRC;
        }
        else {
          dbprint("DATACHECK datachecksum=0x");
          dbprintln(datachecksum, HEX);
          if (datachecksum == inchar) {
            dbprintln("Checksum OK");
            port->write(ACK);
            port->flush();
            if (block == next_block) {
              dbprintln("Good block");
              next_block++;
              uint32_t bytesOut = min((uint32_t)blocksizenext, rx_file_remaining);
              if(!YMODEM) bytesOut = blocksizenext; // with XMODEM transfer, expepcted length is unknown
              rxmodem.write(rx_buf, bytesOut);
              rx_file_remaining -= bytesOut;
              dbprint("rx_file_remaining="); dbprint(rx_file_remaining);
              dbprint(" bytesOut="); dbprintln(bytesOut);
              next_millis = millis() + TIMEOUT_LONG;
              rxmodem_state = BLOCKSTART;
            }
            else if (block == 0) {
              // ymodem block 0 file name, file size, etc.
              dbprint("rx file name (rx_buf)="); dbprintln((char *)rx_buf);
              make_full_pathname((char*)rx_buf, rx_filename, sizeof(rx_filename)-1);
              dbprint("rx dir name="); dbprintln((char *)rx_dirname);
              dbprint("rx file name="); dbprintln((char *)rx_filename);
              if (rx_buf[0] != '\0') {
                fsptr->remove((char *)rx_filename);
                rx_filename[sizeof(rx_filename)-1] = '\0';
                rxmodem = fsptr->open(rx_filename, FILE_WRITE);
                if (rxmodem) {
                  rxmodem_state = BLOCKSTART;
                  next_block = 1;
                  reply = (CRC_on)? 'C' : NAK;
                  port->write(reply);
                  port->flush();
                  next_millis = millis()+ TIMEOUT_LONG;
                  dbprintln("rxmodem starting");
                  dbprintln((char *)rx_buf + strlen((const char *)rx_buf)+1);
                  rx_file_remaining = strtoul(
                      (char *)&rx_buf[strlen((const char *)rx_buf)+1], NULL, 10);
                  dbprint("rx_file_remaining=");
                  dbprintln(rx_file_remaining);
                }
                else {
                  dbprintln("rx file open failed");
                }
              }
              else {
                rxmodem_state = IDLE;
              }
            }
          }
          else {
            dbprintln("Checksum bad");
            port->write(NAK);
            port->flush();
            rxmodem_state = BLOCKSTART;
          }
        }
        break;
      case DATACHECKCRC:
        CRCRx |= inchar;
        dbprint("DATACHECK CRCRx=0x");
        dbprintln(CRCRx, HEX);
        if (CRCRx == CRC) {
          dbprintln("CRC OK");
          port->write(ACK);
          port->flush();
          if (block == next_block) {
            dbprintln("Good block");
            next_block++;
            uint32_t bytesOut = min((uint32_t)blocksizenext, rx_file_remaining);
            if(!YMODEM) bytesOut = blocksizenext; // with XMODEM transfer, expepcted length is unknown
            rxmodem.write(rx_buf, bytesOut);
            rx_file_remaining -= bytesOut;
            dbprint("rx_file_remaining="); dbprint(rx_file_remaining);
            dbprint(" bytesOut="); dbprintln(bytesOut);
            next_millis = millis() + TIMEOUT_LONG;
            rxmodem_state = BLOCKSTART;
          }
          else if (block == 0) {
            // ymodem block 0 file name, file size, etc.
            dbprint("rx file name (rx_buf)="); dbprintln((char *)rx_buf);
            make_full_pathname((char*)rx_buf, rx_filename, sizeof(rx_filename)-1);
            dbprint("rx dir name="); dbprintln((char *)rx_dirname);
            dbprint("rx file name="); dbprintln((char *)rx_filename);
            if (rx_buf[0] != '\0') {
              fsptr->remove((char *)rx_filename);
              rx_filename[sizeof(rx_filename)-1] = '\0';
              rxmodem = fsptr->open(rx_filename, FILE_WRITE);
              if (rxmodem) {
                rxmodem_state = BLOCKSTART;
                next_block = 1;
                reply = (CRC_on)? 'C' : NAK;
                port->write(reply);
                port->flush();
                next_millis = millis()+ TIMEOUT_LONG;
                dbprintln("rxmodem starting");
                dbprintln((char *)rx_buf + strlen((const char *)rx_buf)+1);
                rx_file_remaining = strtoul(
                    (char *)&rx_buf[strlen((const char *)rx_buf)+1], NULL, 10);
                dbprint("rx_file_remaining=");
                dbprintln(rx_file_remaining);
              }
              else {
                dbprintln("rx file open failed");
              }
            }
            else {
              rxmodem_state = IDLE;
            }
          }
        }
        else {
          dbprintln("Checksum bad");
          port->write(NAK);
          port->flush();
          rxmodem_state = BLOCKSTART;
        }
        break;
      case DATAPURGE:
        dbprintln("DATAPURGE");
        int bytesAvail = port->available();
        dbprint("bytesAvail=");
        dbprintln(bytesAvail);
        if (bytesAvail > 0) {
          port->readBytes((char *)p, bytesAvail);
        }
        break;
    }
  } // while available()
  return rxmodem_state;
}

inline uint16_t XYmodem::updcrc(uint8_t c, uint16_t crc)
{
  int count;

  crc = crc ^ (uint16_t) c << 8;
  count = 8;
  do {
    if (crc & 0x8000) {
      crc = crc << 1 ^ 0x1021;
    }
    else {
      crc = crc << 1;
    }
  } while (--count);
  return crc;
}

int XYmodem::make_full_pathname(char *name, char *pathname, size_t pathname_len)
{
  if (name == NULL || name == '\0') return -1;
  if (pathname == NULL || pathname_len == 0) return -1;

  // if dir name starts with '/', it is a full pathname so make it.
  // else form full pathname with current working directory.
  if (*name == '/') {
    strncpy(pathname, name, pathname_len);
    pathname[pathname_len-1] = '\0';
  }
  else {
    strcpy(pathname, rx_dirname);
    if (rx_dirname[strlen(rx_dirname)-1] == '/') {
      if (strlen(rx_dirname) + strlen(name) >= pathname_len) {
        port->println("pathname too long");
        return -2;
      }
    }
    else {
      if (strlen(rx_dirname) + 1 + strlen(name) >= pathname_len) {
        port->println("pathname too long");
        return -2;
      }
      strcat(pathname, "/");
    }
    strcat(pathname, name);
  }
  return 0;
}


// TODO: move these out into the CPE-specific examples
#if 0
#if defined(ADAFRUIT_SPIFLASH)
void XYmodem::format_flash() {
  // Partition the flash with 1 partition that takes the entire space.
  Serial.println("Partitioning flash with 1 primary partition...");
  DWORD plist[] = {100, 0, 0, 0};  // 1 primary partition with 100% of space.
  uint8_t buf[512] = {0};          // Working buffer for f_fdisk function.
  FRESULT r = f_fdisk(0, plist, buf);
  if (r != FR_OK) {
    Serial.print("Error, f_fdisk failed with error code: "); Serial.println(r, DEC);
    while(1);
  }
  Serial.println("Partitioned flash!");

  // Make filesystem.
  Serial.println("Creating and formatting FAT filesystem (this takes ~60 seconds)...");
  r = f_mkfs("", FM_ANY, 0, buf, sizeof(buf));
  if (r != FR_OK) {
    Serial.print("Error, f_mkfs failed with error code: "); Serial.println(r, DEC);
    while(1);
  }
  Serial.println("Formatted flash!");

  // Finally test that the filesystem can be mounted.
  if (!FATFILESYS.begin()) {
    Serial.println("Error, failed to mount newly formatted filesystem!");
    while(1);
  }
}
#endif

int XYmodem::begin()
{
#if defined(ADAFRUIT_ITSYBITSY_M0) || defined(ADAFRUIT_CIRCUITPLAYGROUND_M0)
  Serial.println("M0 SPI Flash");
  // Initialize flash library and check its chip ID.
  if (!flash.begin(FLASH_TYPE)) {
    Serial.println("Error, failed to initialize flash chip!");
    return -1;
  }
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.GetJEDECID(), HEX);

  // Call fatfs activate to make it the active chip that receives low level fatfs
  // callbacks. This is necessary before making any manual fatfs function calls
  // (like the f_fdisk and f_mkfs functions further below).  Be sure to call
  // activate before you call any fatfs functions yourself!
  FATFILESYS.activate();

#elif defined(ADAFRUIT_METRO_M4_EXPRESS)
  Serial.println("M4 QSPI Flash");
  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    Serial.println("Error, failed to initialize flash chip!");
    return -1;
  }
  flash.setFlashType(FLASH_TYPE);
  // Call fatfs activate to make it the active chip that receives low level fatfs
  // callbacks. This is necessary before making any manual fatfs function calls
  // (like the f_fdisk and f_mkfs functions further below).  Be sure to call
  // activate before you call any fatfs functions yourself!
  FATFILESYS.activate();

#endif

  // Mount the filesystem. Format it if it fails.
  if (!FATFILESYS.begin(chipSelect)) {
    Serial.println("Error, failed to mount filesystem!");
#if defined(ADAFRUIT_SPIFLASH)
    // Wait for user to send OK to continue.
    Serial.setTimeout(30000);  // Increase timeout to print message less frequently.
    do {
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("This sketch will ERASE ALL DATA on the flash chip and format it with a new filesystem!");
      Serial.println("Type OK (all caps) and press enter to continue.");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    while (!Serial.find((char *)"OK"));
    format_flash();

    Serial.println("Flash chip successfully formatted with new empty filesystem!");
#else
    return -2;
#endif
  }
  return 0;
}
#endif