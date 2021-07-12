#ifndef _SERIAL_FILE_BROWSER_H_
#define _SERIAL_FILE_BROWSER_H_

#include "xymodem.h"

// https://isocpp.org/wiki/faq/pointers-to-members
#define CALL_MEMBER_FN(object,ptrToMember)  ((object)->*(ptrToMember))

class SerialFileBrowser {
  public:
    SerialFileBrowser(Stream &port, FS &fs) {
      this->port = &port;
      fsptr = &fs;
    }

    void setup_cli(void);
    void loop_cli(void);

  private:
    typedef void (SerialFileBrowser::*action_func_t)(char *aLine);

    typedef struct {
      char command[7+1];    // max 7 characters plus '\0' terminator
      action_func_t action;
    } command_action_t;

    command_action_t commands[15] = {
      // Name of command user types, function that implements the command.
      {"dir", &SerialFileBrowser::print_dir},
      {"ls", &SerialFileBrowser::print_dir},
      {"pwd", &SerialFileBrowser::print_working_dir},
      {"cd", &SerialFileBrowser::change_dir},
      {"mkdir", &SerialFileBrowser::make_dir},
      {"rmdir", &SerialFileBrowser::remove_dir},
      {"del", &SerialFileBrowser::remove_file},
      {"rm", &SerialFileBrowser::remove_file},
      {"type", &SerialFileBrowser::print_file},
      {"cat", &SerialFileBrowser::print_file},
      {"capture", &SerialFileBrowser::capture_file},
      {"rx", &SerialFileBrowser::recv_xmodem},
      {"rb", &SerialFileBrowser::recv_ymodem},
      {"help", &SerialFileBrowser::print_commands},
      {"?", &SerialFileBrowser::print_commands},
    };

    int make_full_pathname(char *name, char *pathname, size_t pathname_len);
    void remove_file(char *aLine);
    void change_dir(char *aLine);
    void make_dir(char *aLine);
    void remove_dir(char *aLine);
    void print_dir(char *aLine);
    void print_file(char *aLine);
    void capture_file(char *aLine);
    void print_working_dir(char *aLine);
    void recv_xmodem(char *aLine);
    void recv_ymodem(char *aLine);
    void toLower(char *s);
    void print_commands(char *aLine);
    void execute(char *aLine);

    uint8_t bytesIn;
    char aLine[80+1];
    char cwd[128+1];     // Current Working Directory
    bool CaptureMode = false;
    bool XYmodemMode = false;
    File CaptureFile;
    FS *fsptr;

    Stream *port;
    XYmodem rxymodem;
};

#endif
