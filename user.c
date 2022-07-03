#include <sys/ioctl.h>
#include <sys/stat.h>
#include <strings.h>
#include <termios.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <bedd.h>

struct termios old_termios;

static int user_width = 0;
static int user_height = 0;

static int user_bar_off = -1;

int user_get_size(void) {
  struct winsize ws;
  
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return 0;
  } else {
    user_width = ws.ws_col;
    user_height = ws.ws_row;
    
    return 1;
  }
}

int user_off_row(void) {
  return bedd_tabs[bedd_active].off_row - 1;
}

int user_off_col(void) {
  int line_length = 0;
  int line_tmp = bedd_tabs[bedd_active].line_cnt;
  
  while (line_tmp) {
    line_length++;
    line_tmp /= 10;
  }
  
  if (line_length < 0) {
    line_length++;
  }
  
  return bedd_tabs[bedd_active].off_col - (5 + line_length);
}

void user_gotoxy(int x, int y) {
  printf("\x1B[%d;%dH", y + 1, x + 1);
}

void user_cursor(int type) {
  if (type >= 0) printf("\x1B[?25h\x1B[%d q", type);
  else printf("\x1B[?25l");
}

void user_clear_end(void) {
  printf("\x1B[K");
}

void user_force(void) {
  int line_length = 0;
  int line_tmp = bedd_tabs[bedd_active].line_cnt;
  
  while (line_tmp) {
    line_length++;
    line_tmp /= 10;
  }
  
  if (line_length < 0) {
    line_length++;
  }
  
  if (bedd_tabs[bedd_active].off_row > bedd_tabs[bedd_active].row) {
    bedd_tabs[bedd_active].off_row = bedd_tabs[bedd_active].row;
  }
  
  if (bedd_tabs[bedd_active].off_row < bedd_tabs[bedd_active].row - (user_height - 3)) {
    bedd_tabs[bedd_active].off_row = bedd_tabs[bedd_active].row - (user_height - 3);
  }
  
  if (bedd_tabs[bedd_active].off_col > bedd_tabs[bedd_active].col) {
    bedd_tabs[bedd_active].off_col = bedd_tabs[bedd_active].col;
  }
  
  if (bedd_tabs[bedd_active].off_col < bedd_tabs[bedd_active].col - (user_width - (7 + line_length))) {
    bedd_tabs[bedd_active].off_col = bedd_tabs[bedd_active].col - (user_width - (7 + line_length));
  }
}

int user_prompt_text(const char *prompt, char *result) {
  int pos = 0;
  
  result[pos] = '\0';
  int first = 1;

  printf("\x1B[?25h");
  user_cursor(1);

  for (;;) {
    user_get_size();
    char c = '\0';

    if (read(STDIN_FILENO, &c, 1) > 0) {
      if (c == BEDD_CTRL('q')) {
        return 0;
      } else if (c == BEDD_CTRL('m')) {
        return 1;
      } else if (c == '\x7F' || c == BEDD_CTRL('h')) {
        if (pos) {
          result[--pos] = '\0';
        }
      } else if (c == '\x1B') {
        while (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z')) {
          if (read(STDIN_FILENO, &c, 1) <= 0) break;
        }
      } else if (c >= 32 && c < 127) {
        result[pos++] = c;
        result[pos] = '\0';
      }
    } else if (first) {
      first = 0;
    } else {
      continue;
    }
    
    user_gotoxy(0, user_height - 1);
    printf(BEDD_INVERT " %s " BEDD_NORMAL " %s", prompt, result);
    
    user_clear_end();
    user_gotoxy(pos + strlen(prompt) + 3, user_height - 1);
    
    fflush(stdout);
  }

  return 1;
}

int user_prompt_hist(const char *prompt, char *result, const char **history, int count) {
  int pos = 0;
  
  result[pos] = '\0';
  int first = 1;

  printf("\x1B[?25h");
  user_cursor(1);
  
  const char *match = NULL;
  
  for (;;) {
    user_get_size();
    char c = '\0';
    
    if (read(STDIN_FILENO, &c, 1) > 0) {
      if (c == BEDD_CTRL('q')) {
        return 0;
      } else if (c == BEDD_CTRL('m')) {
        return 1;
      } else if (c == '\x7F' || c == BEDD_CTRL('h')) {
        if (pos) {
          result[--pos] = '\0';
        }
      } else if (c == '\x1B') {
        while (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z')) {
          if (read(STDIN_FILENO, &c, 1) <= 0) break;
        }
      } else if (c >= 32 && c < 127) {
        result[pos++] = c;
        result[pos] = '\0';
      } else if (c == '\t') {
        if (match) {
          strcpy(result, match);
          pos = strlen(match);
        }
      }
    } else if (first) {
      first = 0;
    } else {
      continue;
    }
    
    match = NULL;
    
    for (int i = 0; i < count; i++) {
      if (!strncmp(result, history[i], pos)) {
        match = history[i];
        break;
      }
    }
    
    if (match) {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_GREEN " %s" BEDD_GRAY "%s", prompt, result, match + pos);
    } else {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_YELLOW " %s", prompt, result);
    }
    
    user_clear_end();
    user_gotoxy(pos + strlen(prompt) + 3, user_height - 1);
    
    fflush(stdout);
  }

  return 1;
}

int user_prompt_path(const char *prompt, char *result, const char *curr_dir) {
  /*
  char path[256];
  realpath(curr_dir, path);
  
  strcpy(result, path);
  int pos = strlen(result);
  
  printf("\x1B[?25h");
  user_cursor(1);
  
  for (;;) {
    user_get_size();
    char c = '\0';
    
    if (read(STDIN_FILENO, &c, 1) > 0) {
      if (c == BEDD_CTRL('q')) {
        return 0;
      } else if (c == BEDD_CTRL('m')) {
        return 1;
      } else if (c == '\x7F' || c == BEDD_CTRL('h')) {
        if (pos) {
          result[--pos] = '\0';
        }
      } else if (c == '\x1B') {
        while (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z')) {
          if (read(STDIN_FILENO, &c, 1) <= 0) break;
        }
      } else if (c >= 32 && c < 127) {
        result[pos++] = c;
        result[pos] = '\0';
      }
    } else {
      continue;
    }
    
    strcpy(path, result);
    
    if (path[pos - 1] == '/') {
      path[pos - 1] = '\0';
    }
    
    char *ptr = strrchr(path, '/');
    
    if (ptr && ptr != path) {
      *ptr = '\0';
    }
    
    DIR *dir = opendir(path);
    
    if (dir) {
      
      closedir(dir);
    }
    
    if (match) {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_GREEN " %s" BEDD_GRAY "%s", prompt, result, match + pos);
    } else {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_YELLOW " %s", prompt, result);
    }
    
    user_clear_end();
    user_gotoxy(pos + strlen(prompt) + 3, user_height - 1);
    
    fflush(stdout);
  }

  */
  return 1;
}

int user_prompt_optn(const char *prompt, int *result, const char **options, int count, int strict) {
  char buffer[100];
  int pos = 0;
  
  buffer[pos] = '\0';
  int first = 1;

  printf("\x1B[?25h");
  user_cursor(1);
  
  const char *match = NULL;
  
  for (;;) {
    user_get_size();
    char c = '\0';
    
    if (read(STDIN_FILENO, &c, 1) > 0) {
      if (c == BEDD_CTRL('q')) {
        return 0;
      } else if (c == BEDD_CTRL('m')) {
        if (pos && match && (!strict || !strcmp(buffer, match))) return 1;
      } else if (c == '\x7F' || c == BEDD_CTRL('h')) {
        if (pos) buffer[--pos] = '\0';
      } else if (c == '\x1B') {
        while (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z')) {
          if (read(STDIN_FILENO, &c, 1) <= 0) break;
        }
      } else if (c >= 32 && c < 127) {
        buffer[pos++] = c;
        buffer[pos] = '\0';
      } else if (c == '\t') {
        if (match) {
          strcpy(buffer, match);
          pos = strlen(match);
        }
      }
    } else if (first) {
      first = 0;
    } else {
      continue;
    }
    
    match = NULL;
    
    for (int i = 0; i < count; i++) {
      if (!strncasecmp(buffer, options[i], strlen(buffer))) {
        match = options[i];
        *result = i;
        
        break;
      }
    }
    
    if (match) {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_GREEN " %s" BEDD_GRAY "%s", prompt, buffer, match + pos);
    } else {
      user_gotoxy(0, user_height - 1);
      printf(BEDD_WHITE BEDD_INVERT " %s " BEDD_NORMAL BEDD_RED " %s", prompt, buffer);
    }
    
    user_clear_end();
    user_gotoxy(pos + strlen(prompt) + 3, user_height - 1);
    
    fflush(stdout);
  }
  
  return 1;
}

void user_hinter(int x, int y, const char *text) {
  if (y < user_height - 2) y++;
  else y--;
  
  // TODO: stop top collisions and all that shit
  
  if (x > user_width - (strlen(text) + 1)) {
    x = user_width - (strlen(text) + 1);
  }
  
  user_gotoxy(x, y);
  printf(BEDD_INVERT "%s" BEDD_NORMAL, text);
}

void user_render_top(void) {
  user_gotoxy(0, 0);
  
  for (int i = 0; i < bedd_count; i++) {
    // weird trick to overcome integer limits
    int tab_width = (((i + 1) * user_width) / bedd_count) - ((i * user_width) / bedd_count);
    
    printf(BEDD_WHITE);
    
    if (i == bedd_active) {
      printf(BEDD_INVERT BEDD_DEFAULT);
    } else {
      printf(BEDD_NORMAL BEDD_LINE);
    }
    
    const char *path_buffer = "no name";
    int path_length = strlen(path_buffer);

    if (bedd_tabs[i].path) {
      path_buffer = bedd_tabs[i].path;
      path_length = strlen(bedd_tabs[i].path);
    }

    for (int j = 0; j < tab_width; j++) {
      if (j > 0 && j < tab_width - 1 && j - 1 < path_length) {
        printf("%c", path_buffer[j - 1]);
      } else {
        printf(" ");
      }
    }
  }
  
  printf(BEDD_NORMAL BEDD_LINE);
  fflush(stdout);
}

void user_render_btm(void) {
  user_gotoxy(0, user_height - 1);
  
  printf(BEDD_WHITE BEDD_INVERT BEDD_DEFAULT " bedd r02 " BEDD_NORMAL BEDD_LINE " %s (", bedd_tabs[bedd_active].path ? bedd_tabs[bedd_active].path : "no name");
  
  if (bedd_tabs[bedd_active].row != bedd_tabs[bedd_active].sel_row || bedd_tabs[bedd_active].col != bedd_tabs[bedd_active].sel_col) {
    printf("%d,%d to %d,%d)", bedd_tabs[bedd_active].row, bedd_tabs[bedd_active].col, bedd_tabs[bedd_active].sel_row, bedd_tabs[bedd_active].sel_col);
  } else {
    printf("%d,%d)", bedd_tabs[bedd_active].row, bedd_tabs[bedd_active].col);
  }
  
  user_clear_end();
  
  fflush(stdout);
}

void user_init(void) {
  tcgetattr(STDIN_FILENO, &old_termios);
  struct termios new_termios = old_termios;
  
  new_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  new_termios.c_oflag &= ~(OPOST);
  new_termios.c_cflag |= (CS8);
  new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  new_termios.c_cc[VMIN] = 0;
  new_termios.c_cc[VTIME] = 1;
  
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
  
  printf("\x1B[2J\x1B[H\x1B[?47h");
  printf("\x1B[?1000;1002;1006;1015h" BEDD_DEFAULT BEDD_NORMAL BEDD_WHITE);
}

void user_exit(void) {
  printf(BEDD_DEFAULT BEDD_NORMAL "\x1B[?25h");
  printf("\x1B[?1000;1002;1006;1015l");
  printf("\x1B[2J\x1B[H");
  printf("\x1B[?47l");
  printf("\x1B[1 q");
  
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
}

void user_draw(void) {
  user_render_top();
  
  int line_length = 0;
  int line_tmp = bedd_tabs[bedd_active].line_cnt;
  
  while (line_tmp) {
    line_length++;
    line_tmp /= 10;
  }
  
  if (line_length < 0) {
    line_length++;
  }
  
  int start_row = bedd_tabs[bedd_active].sel_row;
  int start_col = bedd_tabs[bedd_active].sel_col;
  
  int end_row = bedd_tabs[bedd_active].row;
  int end_col = bedd_tabs[bedd_active].col;
  
  if (end_row < start_row || (end_row == start_row && end_col < start_col)) {
    int temp_row = start_row;
    int temp_col = start_col;
    
    start_row = end_row;
    start_col = end_col;
    
    end_row = temp_row;
    end_col = temp_col;
  }
  
  int view_height = bedd_tabs[bedd_active].line_cnt + (user_height - 3);
  
  float bar_start = (float)(bedd_tabs[bedd_active].off_row) / (float)(view_height);
  float bar_size = (float)(user_height - 2) / (float)(view_height);
  
  bar_start *= (user_height - 2);
  bar_size *= (user_height - 2);
  
  for (int i = 0; i < user_height - 2; i++) {
    int row = i + bedd_tabs[bedd_active].off_row;
    user_gotoxy(0, i + 1);
    
    if (row >= 0 && row < bedd_tabs[bedd_active].line_cnt) {
      char off_chr = bedd_tabs[bedd_active].off_col ? '<' : ' ';
      
      if (bedd_tabs[bedd_active].row == row) {
        printf(BEDD_WHITE BEDD_INVERT BEDD_DEFAULT "  %*d  " BEDD_NORMAL  BEDD_WHITE BEDD_LINE    "%c", line_length, row + 1, off_chr);
      } else {
        printf(BEDD_WHITE BEDD_NORMAL BEDD_LINE    "  %*d |" BEDD_DEFAULT BEDD_WHITE BEDD_DEFAULT "%c", line_length, row + 1, off_chr);
      }
      
      int real_length = bedd_tabs[bedd_active].lines[row].length;
      if (row < bedd_tabs[bedd_active].line_cnt - 1) real_length++;
      
      for (int j = 0; j < user_width - (6 + line_length); j++) {
        int col = j + bedd_tabs[bedd_active].off_col;
        if (col >= real_length) break;
        
        if (row > start_row || (row == start_row && col >= start_col)) {
          if (row < end_row || (row == end_row && col < end_col)) {
            printf(BEDD_SELECT);
          }
        }
        
        if (col >= bedd_tabs[bedd_active].lines[row].length) {
          putchar(' ');
        } else {
          putchar(bedd_tabs[bedd_active].lines[row].buffer[col]);
        }
        
        if (row > start_row || (row == start_row && col >= start_col)) {
          if (row < end_row || (row == end_row && col < end_col)) {
            printf(bedd_tabs[bedd_active].row == row ? BEDD_LINE : BEDD_DEFAULT);
          }
        }
      }
    } else {
      printf(BEDD_WHITE BEDD_NORMAL BEDD_DEFAULT "  %*s : ", line_length, "");
    }
    
    user_clear_end();
    user_gotoxy(user_width - 1, i + 1);
    
    if (i >= bar_start && i < bar_start + bar_size) {
      printf(BEDD_WHITE BEDD_INVERT BEDD_DEFAULT " ");
    } else {
      printf(BEDD_WHITE BEDD_NORMAL "|");
    }
  }
  
  printf(BEDD_WHITE BEDD_NORMAL BEDD_DEFAULT);
  fflush(stdout);
  
  user_render_btm();
  
  int cursor_col = bedd_tabs[bedd_active].col;
  
  if (cursor_col > bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length) {
    cursor_col = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
  }
  
  int rel_row = 1 + (bedd_tabs[bedd_active].row - bedd_tabs[bedd_active].off_row);
  int rel_col = 5 + line_length + (cursor_col - bedd_tabs[bedd_active].off_col);
  
  if (start_row == end_row && start_col == end_col) {
    // user_hinter(rel_col, rel_row, "void user_hinter(int x, int y, const char *text)"); // TODO
  }
  
  if (rel_row >= 1 && rel_row < user_height - 1) {
    user_gotoxy(rel_col, rel_row);
    user_cursor(3);
  } else {
    user_cursor(-1);
  }
  
  fflush(stdout);
  
  bedd_page_step = (user_height - 2) / 2;
}

int user_read_raw(void) {
  char c = '\0';
  
  if (read(STDIN_FILENO, &c, 1) > 0) {
    if (c == '\x1B') {
      char seq[6] = {0};
      
      if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
        if (seq[0] == '[') {
          if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) >= 1) {
              if (seq[2] == '~') {
                if (seq[1] == '1' || seq[1] == '7') {
                  // bedd_tabs[bedd_active].col = 0;
                  // bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
                } else if (seq[1] == '4' || seq[1] == '8') {
                  // bedd_tabs[bedd_active].col = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
                  // bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
                } else if (seq[1] == '3') {
                  // if (bedd_tabs[bedd_active].sel_row == bedd_tabs[bedd_active].row && bedd_tabs[bedd_active].sel_col == bedd_tabs[bedd_active].col) {
                  //   bedd_right(bedd_tabs + bedd_active, 0);
                  // }
                  
                  // bedd_delete(bedd_tabs + bedd_active);
                  
                  /*
                  bedd_tabs[bedd_active].step++;
                  
                  if (bedd_tabs[bedd_active].step >= BEDD_STEP) {
                    bedd_push_undo(bedd_tabs + bedd_active);
                    bedd_tabs[bedd_active].step = 0;
                  }
                  */
                } else if (seq[1] == '5') {
                  return K_PAGE_UP;
                } else if (seq[1] == '6') {
                  return K_PAGE_DOWN;
                }
              } else if (seq[2] == ';') {
                if (read(STDIN_FILENO, &seq[3], 1) >= 1 && read(STDIN_FILENO, &seq[4], 1) >= 1) {
                  if (seq[3] == '2') {
                    if (seq[4] == 'A') {
                      return K_S_UP;
                    } else if (seq[4] == 'B') {
                      return K_S_DOWN;
                    } else if (seq[4] == 'C') {
                      return K_S_RIGHT;
                    } else if (seq[4] == 'D') {
                      return K_S_LEFT;
                    }
                  } else if (seq[3] == '5') {
                    if (seq[4] == 'A') {
                      return K_C_UP;
                    } else if (seq[4] == 'B') {
                      return K_C_DOWN;
                    } else if (seq[4] == 'C') {
                      return K_C_RIGHT;
                    } else if (seq[4] == 'D') {
                      return K_C_LEFT;
                    }
                  }
                }
              }
            }
          } else {
            if (seq[1] == 'A') {
              return K_UP;
            } else if (seq[1] == 'B') {
              return K_DOWN;
            } else if (seq[1] == 'C') {
              return K_RIGHT;
            } else if (seq[1] == 'D') {
              return K_LEFT;
            } else if (seq[1] == 'H') {
              return K_HOME;
            } else if (seq[1] == 'F') {
              return K_END;
            } else if (seq[1] == '<') {
              int key = K_NONE;
              
              if (read(STDIN_FILENO, &seq[2], 1) >= 1) {
                if (seq[2] == '0') {
                  if (read(STDIN_FILENO, &seq[3], 1) >= 1) {
                    if (seq[3] == ';') {
                      int row = 0;
                      int col = 0;

                      for (;;) {
                        if (!read(STDIN_FILENO, &c, 1)) {
                          break;
                        }

                        if (!(c >= '0' && c <= '9')) {
                          break;
                        }

                        col *= 10;
                        col += (c - '0');
                      }

                      for (;;) {
                        if (!read(STDIN_FILENO, &c, 1)) {
                          break;
                        }

                        if (!(c >= '0' && c <= '9')) {
                          break;
                        }

                        row *= 10;
                        row += (c - '0');
                      }
                      
                      bedd_mouse_x = col - 1;
                      bedd_mouse_y = row - 1;
                      
                      key = K_MOUS_DOWN;
                    }
                  }
                } else if (seq[2] == '3') {
                  if (read(STDIN_FILENO, &seq[3], 1) >= 1 && read(STDIN_FILENO, &seq[4], 1) >= 1) {
                    if (seq[4] == ';') {
                      int row = 0;
                      int col = 0;
                      
                      for (;;) {
                        if (!read(STDIN_FILENO, &c, 1)) {
                          break;
                        }
                        
                        if (!(c >= '0' && c <= '9')) {
                          break;
                        }
                        
                        col *= 10;
                        col += (c - '0');
                      }
                      
                      for (;;) {
                        if (!read(STDIN_FILENO, &c, 1)) {
                          break;
                        }
                        
                        if (!(c >= '0' && c <= '9')) {
                          break;
                        }
                        
                        row *= 10;
                        row += (c - '0');
                      }
                      
                      bedd_mouse_x = col - 1;
                      bedd_mouse_y = row - 1;
                      
                      key = K_MOUS_DRAG;
                    }
                  }
                } else if (seq[2] == '6') {
                  if (read(STDIN_FILENO, &seq[3], 1) >= 1) {
                    if (seq[3] == '4') {
                      key = K_SCRL_UP;
                    } else if (seq[3] == '5') {
                      key = K_SCRL_DOWN;
                    }
                  }
                }
                
                while (c != 'M' && c != 'm') {
                  if (read(STDIN_FILENO, &c, 1) < 1) {
                    break;
                  }
                }
                
                if (c == 'm' && key == K_MOUS_DOWN) key = K_MOUS_UP;
                return key;
              }
            }
          }
        } else if (seq[0] == 'O') {
          if (seq[1] == 'H') {
            return K_HOME;
          } else if (seq[1] == 'F') {
            return K_END;
          }
        }
      }
    } else {
      return c;
    }
  }
  
  return K_NONE;
}

int user_read(void) {
  int key = user_read_raw();
  
  if (key == K_MOUS_DOWN || key == K_MOUS_DRAG) {
    if (user_bar_off < 0 && bedd_mouse_y == 0) {
      if (key == K_MOUS_DOWN) {
        int new_tab = (bedd_mouse_x * bedd_count) / user_width;
        
        if (new_tab >= 0 && new_tab < bedd_count) {
          bedd_active = new_tab;
        }
        
        return K_REFRESH;
      }
    } else if (user_bar_off >= 0 || (bedd_mouse_x == user_width - 1 && bedd_mouse_y < user_height - 1)) {
      int view_height = bedd_tabs[bedd_active].line_cnt + (user_height - 3);
      
      float bar_start = (float)(bedd_tabs[bedd_active].off_row) / (float)(view_height);
      float bar_size = (float)(user_height - 2) / (float)(view_height);
      
      bar_start *= (user_height - 2);
      bar_size *= (user_height - 2);
      
      if (key == K_MOUS_DOWN) {
        if (bedd_mouse_y - 1 >= bar_start && bedd_mouse_y - 1 < bar_start + bar_size) {
          user_bar_off = (bedd_mouse_y - 1) - bar_start;
        } else {
          user_bar_off = bar_size / 2.0f;
          bedd_tabs[bedd_active].off_row = view_height * ((float)((bedd_mouse_y - 1) - user_bar_off) / (float)(user_height - 2));
          
          if (bedd_tabs[bedd_active].off_row < 0) {
            bedd_tabs[bedd_active].off_row = 0;
          } else if (bedd_tabs[bedd_active].off_row >= bedd_tabs[bedd_active].line_cnt) {
            bedd_tabs[bedd_active].off_row = bedd_tabs[bedd_active].line_cnt - 1;
          }
        }
      } else {
        user_bar_off = bar_size / 2.0f;
        bedd_tabs[bedd_active].off_row = view_height * ((float)((bedd_mouse_y - 1) - user_bar_off) / (float)(user_height - 2));
        
        if (bedd_tabs[bedd_active].off_row < 0) {
          bedd_tabs[bedd_active].off_row = 0;
        } else if (bedd_tabs[bedd_active].off_row >= bedd_tabs[bedd_active].line_cnt) {
          bedd_tabs[bedd_active].off_row = bedd_tabs[bedd_active].line_cnt - 1;
        }
      }
      
      return K_REFRESH;
    }
  } else if (key == K_MOUS_UP) {
    if (user_bar_off) {
      user_bar_off = -1;
      return K_REFRESH;
    }
  }
  
  return key;
}

void user_copy(void) {
  FILE *xclip = popen("xclip -selection clipboard -i", "w");
  if (!xclip) return;
  
  int min_row = bedd_tabs[bedd_active].row;
  int min_col = bedd_tabs[bedd_active].col;
  
  if (min_col > bedd_tabs[bedd_active].lines[min_row].length) {
    min_col = bedd_tabs[bedd_active].lines[min_row].length;
  }
  
  int max_row = bedd_tabs[bedd_active].sel_row;
  int max_col = bedd_tabs[bedd_active].sel_col;
  
  if (max_col > bedd_tabs[bedd_active].lines[max_row].length) {
    max_col = bedd_tabs[bedd_active].lines[max_row].length;
  }
  
  if (min_row > max_row || (min_row == max_row && min_col > max_col)) {
    min_row = max_row;
    min_col = max_col;
    
    max_row = bedd_tabs[bedd_active].row;
    max_col = bedd_tabs[bedd_active].col;
  }
  
  while (min_row != max_row || min_col != max_col) {
    if (min_col >= bedd_tabs[bedd_active].lines[min_row].length) {
      min_col = 0;
      min_row++;

      fputc('\n', xclip);
    } else {
      fputc(bedd_tabs[bedd_active].lines[min_row].buffer[min_col++], xclip);
    }
  }
  
  pclose(xclip);
}

void user_paste(void) {
  FILE *xclip = popen("xclip -selection clipboard -o", "r");
  if (!xclip) return;
  
  while (!feof(xclip)) {
    int c = fgetc(xclip);
    if (!c || c == EOF) continue;
    
    bedd_write_one(bedd_tabs + bedd_active, c);
    
    bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
    bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
  }
  
  pclose(xclip);
}
