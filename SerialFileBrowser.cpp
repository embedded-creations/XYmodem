#include "SerialFileBrowser.h"

void SerialFileBrowser::setup_cli(void) {
  port->setTimeout(0);
  strcpy(cwd, "/");
  port->print("$ ");
}

void SerialFileBrowser::loop_cli(void) {
  if (XYmodemMode){
    if (rxymodem.loop() == 0) {
      XYmodemMode = false;
      port->println();
      port->print("$ ");
    }
  }
  else {
    if (port->available() > 0) {
      int b = port->read();
      if (CaptureMode) {
        if (b == 0x04) {   // ^D end of input
          CaptureMode = false;
          // Close the file when finished reading.
          CaptureFile.close();
          port->print("$ ");
        }
        if (b != -1) {
          CaptureFile.print((char)b);
        }
      }
      else {
        if (b != -1) {
          switch (b) {
            case '\n':
              break;
            case '\r':
              port->println();
              aLine[bytesIn] = '\0';
              execute(aLine);
              bytesIn = 0;
              if (!CaptureMode) port->print("$ ");
              break;
            case '\b':  // backspace
              if (bytesIn > 0) {
                bytesIn--;
                port->print((char)b); port->print(' '); port->print((char)b);
              }
              break;
            case 0x03:  // ^C
              port->println("^C");
              bytesIn = 0;
              port->print("$ ");
              break;
            default:
              port->print((char)b);
              aLine[bytesIn++] = (char)b;
              if (bytesIn >= sizeof(aLine)-1) {
                aLine[bytesIn] = '\0';
                execute(aLine);
                bytesIn = 0;
                if (!CaptureMode && !XYmodemMode) port->print("$ ");
              }
              break;
          }
        }
      }
    }
  }
}

int SerialFileBrowser::make_full_pathname(char *name, char *pathname, size_t pathname_len)
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
    strcpy(pathname, cwd);
    if (cwd[strlen(cwd)-1] == '/') {
      if (strlen(cwd) + strlen(name) >= pathname_len) {
        port->println("pathname too long");
        return -2;
      }
    }
    else {
      if (strlen(cwd) + 1 + strlen(name) >= pathname_len) {
        port->println("pathname too long");
        return -2;
      }
      strcat(pathname, "/");
    }
    strcat(pathname, name);
  }
  return 0;
}

void SerialFileBrowser::remove_file(char *aLine) {
  char *filename = strtok(NULL, " \t");
  char pathname[128+1];
  if (make_full_pathname(filename, pathname, sizeof(pathname)) != 0) return;
  // Delete a file with the remove command.  For example create a test2.txt file
  // inside /test/foo and then delete it.
  if (!fsptr->remove(pathname)) {
    port->println("Error, couldn't delete file!");
    return;
  }
}

void SerialFileBrowser::change_dir(char *aLine) {
  char *dirname = strtok(NULL, " \t");
  char pathname[128+1];

  if (make_full_pathname(dirname, pathname, sizeof(pathname)) != 0) return;
  if ((strcmp(pathname, "/") != 0) && !fsptr->exists(pathname)) {
    port->println("Directory does not exist.");
    return;
  }
  File d = fsptr->open(pathname);
  if (d.isDirectory()) {
    strcpy(cwd, pathname);
  }
  else {
    port->println("Not a directory");
  }
  d.close();
}

void SerialFileBrowser::make_dir(char *aLine) {
  char *dirname = strtok(NULL, " \t");
  char pathname[128+1];

  if (make_full_pathname(dirname, pathname, sizeof(pathname)) != 0) return;
  // Check if directory exists and create it if not there.
  // Note you should _not_ add a trailing slash (like '/test/') to directory names!
  // You can use the same exists function to check for the existance of a file too.
  if (!fsptr->exists(pathname)) {
    // Use mkdir to create directory (note you should _not_ have a trailing slash).
    if (!fsptr->mkdir(pathname)) {
      port->println("Error, failed to create directory!");
      return;
    }
  }
}

void SerialFileBrowser::remove_dir(char *aLine) {
  // Delete a directory with the rmdir command.  Be careful as
  // this will delete EVERYTHING in the directory at all levels!
  // I.e. this is like running a recursive delete, rm -rf, in
  // unix filesystems!
  char *dirname = strtok(NULL, " \t");
  char pathname[128+1];

  if (make_full_pathname(dirname, pathname, sizeof(pathname)) != 0) return;
  if (!fsptr->rmdir(pathname)) {
    port->println("Error, couldn't delete test directory!");
    return;
  }
  // Check that test is really deleted.
  if (fsptr->exists(pathname)) {
    port->println("Error, test directory was not deleted!");
    return;
  }
}

void SerialFileBrowser::print_dir(char *aLine) {
  File dir = fsptr->open(cwd);
  if (!dir) {
    port->println("Directory open failed");
    return;
  }
  if (!dir.isDirectory()) {
    port->println("Not directory");
    dir.close();
    return;
  }
  File child = dir.openNextFile();
  while (child) {
    // Print the file name and mention if it's a directory.
    port->print(child.name());
    port->print(" "); port->print(child.size(), DEC);
    if (child.isDirectory()) {
      port->print(" <DIR>");
    }
    port->println();
    // Keep calling openNextFile to get a new file.
    // When you're done enumerating files an unopened one will
    // be returned (i.e. testing it for true/false like at the
    // top of this while loop will fail).
    child = dir.openNextFile();
  }
}

void SerialFileBrowser::print_file(char *aLine) {
  char *filename = strtok(NULL, " \t");
  char pathname[128+1];

  if (make_full_pathname(filename, pathname, sizeof(pathname)) != 0) return;
  File readFile = fsptr->open(pathname, FILE_READ);
  if (!readFile) {
    port->println("Error, failed to open file for reading!");
    return;
  }
  readFile.setTimeout(0);
  while (readFile.available()) {
    char buf[512];
    size_t bytesIn = readFile.readBytes(buf, sizeof(buf));
    if (bytesIn > 0) {
      port->write(buf, bytesIn);
    }
    else {
      break;
    }
  }

  // Close the file when finished reading.
  readFile.close();
}

void SerialFileBrowser::capture_file(char *aLine) {
  char *filename = strtok(NULL, " \t");
  char pathname[128+1];

  if (make_full_pathname(filename, pathname, sizeof(pathname)) != 0) return;
  CaptureFile = fsptr->open(pathname, FILE_WRITE);
  if (!CaptureFile) {
    port->println("Error, failed to open file!");
    return;
  }
  CaptureMode = true;
}

void SerialFileBrowser::print_working_dir(char *aLine) {
  port->println(cwd);
}

void SerialFileBrowser::recv_xmodem(char *aLine) {
  char *filename = strtok(NULL, " \t");

  rxymodem.start_rx(*port, filename, true, true);
  XYmodemMode = true;
}

void SerialFileBrowser::recv_ymodem(char *aLine) {
  rxymodem.start_rb(*port, *fsptr, true, true);
  XYmodemMode = true;
}

// force lower case
void SerialFileBrowser::toLower(char *s) {
  while (*s) {
    if (isupper(*s)) {
      *s += 0x20;
    }
    s++;
  }
}

void SerialFileBrowser::print_commands(char *aLine) {
  port->print(commands[0].command);
  for (size_t i = 1; i < sizeof(commands)/sizeof(commands[0]); i++) {
    port->print(','); port->print(commands[i].command);
  }
  port->println();
}

void SerialFileBrowser::execute(char *aLine) {
  if (aLine == NULL || *aLine == '\0') return;
  char *cmd = strtok(aLine, " \t");
  if (cmd == NULL || *cmd == '\0') return;
  toLower(cmd);
  for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
    if (strcmp(cmd, commands[i].command) == 0) {
      CALL_MEMBER_FN(this, commands[i].action)(aLine);
      return;
    }
  }
  port->println("command not found");
}

