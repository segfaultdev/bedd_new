#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <bedd.h>
#include <pty.h>

bedd_t *bedd_tabs = NULL;
int bedd_count = 0;

int bedd_active = 0;

int bedd_page_step = 0;

int bedd_mouse_x = 0;
int bedd_mouse_y = 0;

int bedd_init(bedd_t *tab, const char *path, int term) {
  if (!tab) return 0;
  
  tab->lines = NULL;
  tab->line_cnt = 0;
  
  tab->lines = malloc(sizeof(bedd_line_t));
  
  tab->lines[0].buffer = NULL;
  tab->lines[0].length = 0;
  
  tab->line_cnt++;
  
  tab->prog = -1;
  
  tab->term_row = 0;
  tab->term_col = 0;
  
  tab->term_length = 0;
  
  tab->tab_size = 2;
  
  if (term) {
    tab->tab_size = 8;
    tab->path = "sh";
    
    int term_fd;
    
    int pid = forkpty(&term_fd, NULL, NULL, NULL);
    if (pid < 0) return 0;
    
    if (!pid) {
      execlp(tab->path, tab->path, NULL);
      exit(0);
    } else {
      fcntl(term_fd, F_SETFL, FNDELAY);
      
      tab->prog = pid;
      tab->term_fd = term_fd;
    }
  } else if (path) {
    tab->path = malloc(strlen(path) + 1);
    memcpy(tab->path, path, strlen(path) + 1);
    
    FILE *file = fopen(tab->path, "r");
    
    if (!file) {
      return 0;
    }
    
    while (!feof(file)) {
      char c = fgetc(file);
      
      if (c >= 32 && c != 127) {
        tab->lines[tab->line_cnt - 1].buffer = realloc(tab->lines[tab->line_cnt - 1].buffer, tab->lines[tab->line_cnt - 1].length + 1);
        tab->lines[tab->line_cnt - 1].buffer[tab->lines[tab->line_cnt - 1].length++] = c;
      } else if (c == '\t') {
        int left = 2 - (tab->lines[tab->line_cnt - 1].length % 2);
        
        for (int i = 0; i < left; i++) {
          tab->lines[tab->line_cnt - 1].buffer = realloc(tab->lines[tab->line_cnt - 1].buffer, tab->lines[tab->line_cnt - 1].length + 1);
          tab->lines[tab->line_cnt - 1].buffer[tab->lines[tab->line_cnt - 1].length++] = ' ';
        }
      } else if (c == '\n') {
        tab->lines = realloc(tab->lines, (tab->line_cnt + 1) * sizeof(bedd_line_t));
        
        tab->lines[tab->line_cnt].buffer = NULL;
        tab->lines[tab->line_cnt].length = 0;
        
        tab->line_cnt++;
      }
    }
    
    fclose(file);
  } else {
    tab->path = NULL;
  }
  
  tab->dirty = 0;
  
  tab->row = 0;
  tab->col = 0;
  
  tab->sel_row = 0;
  tab->sel_col = 0;
  
  tab->off_row = 0;
  tab->off_col = 0;
  
  return 1;
}

void bedd_free(bedd_t *tab) {
  if (tab->prog >= 0) {
    kill(tab->prog, SIGKILL);
  }
  
  for (int i = 0; i < tab->line_cnt; i++) {
    if (tab->lines[i].buffer) {
      free(tab->lines[i].buffer);
    }
  }
  
  free(tab->lines);
}

int bedd_save(bedd_t *tab) {
  FILE *file = fopen(tab->path, "w");
  if (!file) return 0;
  
  for (int i = 0; i < tab->line_cnt; i++) {
    fwrite(tab->lines[i].buffer, 1, tab->lines[i].length, file);
    
    if (i < tab->line_cnt - 1 || tab->lines[i].length) {
      fputc('\n', file);
    }
  }
  
  fclose(file);
  
  tab->dirty = 0;
  return 1;
}

void bedd_write_raw(bedd_t *tab, int value) {
  if (tab->col > tab->lines[tab->row].length) {
    tab->col = tab->lines[tab->row].length;
  }

  tab->lines[tab->row].buffer = realloc(tab->lines[tab->row].buffer, tab->lines[tab->row].length + 1);

  memmove(tab->lines[tab->row].buffer + tab->col + 1, tab->lines[tab->row].buffer + tab->col, tab->lines[tab->row].length - tab->col);

  tab->lines[tab->row].buffer[tab->col++] = value;
  tab->lines[tab->row].length++;
}

void bedd_write_one(bedd_t *tab, int value) {
  tab->dirty = 1;
  
  if (tab->col > tab->lines[tab->row].length) {
    tab->col = tab->lines[tab->row].length;
  }
  
  if (tab->sel_row != tab->row || tab->sel_col != tab->col) {
    if (tab->sel_row > tab->row || (tab->sel_row == tab->row && tab->sel_col > tab->col)) {
      int temp_row = tab->sel_row;
      int temp_col = tab->sel_col;
      
      tab->sel_row = tab->row;
      tab->sel_col = tab->col;
      
      tab->row = temp_row;
      tab->col = temp_col;
    }
    
    while (tab->sel_row != tab->row || tab->sel_col != tab->col) {
      bedd_delete(tab);
    }
  }
  
  if (tab->term_length) {
    tab->term_buffer[tab->term_length++] = value;
    
    if (isalpha(value)) {
      // TODO: ansi
      tab->term_length = 0;
    }
    
    return;
  }
  
  if (value == '\n') {
    tab->lines = realloc(tab->lines, (tab->line_cnt + 1) * sizeof(bedd_line_t));
    tab->row++;
    
    memmove(tab->lines + tab->row + 1, tab->lines + tab->row, (tab->line_cnt - tab->row) * sizeof(bedd_line_t));
    
    tab->lines[tab->row].buffer = NULL;
    tab->lines[tab->row].length = 0;
    
    int col = tab->col;
    tab->col = 0;
    
    int new_col = tab->col;
    
    for (int i = col; i < tab->lines[tab->row - 1].length; i++) {
      bedd_write_raw(tab, tab->lines[tab->row - 1].buffer[i]);
    }
    
    if (col < tab->lines[tab->row - 1].length) {
      tab->lines[tab->row - 1].length = col;
      tab->lines[tab->row - 1].buffer = realloc(tab->lines[tab->row - 1].buffer, tab->lines[tab->row - 1].length);
    }
    
    tab->col = new_col;
    tab->line_cnt++;
    
    return;
  } else if (value == '\r') {
    tab->term_col = 0;
    return;
  } else if (value == '\x1B') {
    tab->term_length = 0;
    tab->term_buffer[tab->term_length++] = value;
    
    return;
  } else if (value == '\t') {
    int count = tab->tab_size - (tab->term_col % tab->tab_size);
    for (int i = 0; i < count; i++) bedd_write_raw(tab, ' '); // TODO: support actual tabs
    
    return;
  } else if (value < ' ') {
    /*
    bedd_write_raw(tab, '(');
    bedd_write_raw(tab, "0123456789ABCDEF"[(value / 16) % 16]);
    bedd_write_raw(tab, "0123456789ABCDEF"[(value /  1) % 16]);
    bedd_write_raw(tab, ')');
    */
    
    return;
  }
  
  bedd_write_raw(tab, value);
}

void bedd_write(bedd_t *tab, int value) {
  bedd_write_one(tab, value);
}

void bedd_delete_raw(bedd_t *tab) {
  if (tab->col > tab->lines[tab->row].length) {
    tab->col = tab->lines[tab->row].length;
  }
  
  if (tab->col == 0) {
    if (tab->row == 0) {
      return;
    }
    
    tab->row--;
    tab->col = tab->lines[tab->row].length;
    
    int col = tab->col;
    
    for (int i = 0; i < tab->lines[tab->row + 1].length; i++) {
      bedd_write_raw(tab, tab->lines[tab->row + 1].buffer[i]);
    }
    
    memmove(tab->lines + tab->row + 1, tab->lines + tab->row + 2, (tab->line_cnt - (tab->row + 1)) * sizeof(bedd_line_t));
    
    tab->col = col;
    tab->line_cnt--;
    tab->lines = realloc(tab->lines, tab->line_cnt * sizeof(bedd_line_t));
  } else {
    tab->col--;
    
    memmove(tab->lines[tab->row].buffer + tab->col, tab->lines[tab->row].buffer + tab->col + 1, tab->lines[tab->row].length - tab->col);
    
    tab->lines[tab->row].length--;
    tab->lines[tab->row].buffer = realloc(tab->lines[tab->row].buffer, tab->lines[tab->row].length);
  }
  
  tab->dirty = 1;
}

void bedd_delete(bedd_t *tab) {
  if (tab->col > tab->lines[tab->row].length) {
    tab->col = tab->lines[tab->row].length;
  }
  
  if (tab->sel_row != tab->row || tab->sel_col != tab->col) {
    if (tab->sel_row > tab->row || (tab->sel_row == tab->row && tab->sel_col > tab->col)) {
      int temp_row = tab->sel_row;
      int temp_col = tab->sel_col;
      
      tab->sel_row = tab->row;
      tab->sel_col = tab->col;
      
      tab->row = temp_row;
      tab->col = temp_col;
    }
    
    while (tab->sel_row != tab->row || tab->sel_col != tab->col) {
      bedd_delete_raw(tab);
    }
    
    return;
  }
  
  bedd_delete_raw(tab);
}

void bedd_up(bedd_t *tab, int select) {
  if (tab->row > 0) {
    tab->row--;
  } else {
    tab->col = 0;
  }
  
  if (!select) {
    tab->sel_row = tab->row;
    tab->sel_col = tab->col;
  }
}

void bedd_down(bedd_t *tab, int select) {
  if (tab->row < tab->line_cnt - 1) {
    tab->row++;
  } else {
    tab->col = tab->lines[tab->row].length;
  }
  
  if (!select) {
    tab->sel_row = tab->row;
    tab->sel_col = tab->col;
  }
}

void bedd_left(bedd_t *tab, int select) {
  if (tab->col > tab->lines[tab->row].length) {
    tab->col = tab->lines[tab->row].length;
  }
  
  if (tab->col > 0) {
    tab->col--;
  } else if (tab->row > 0) {
    tab->row--;
    tab->col = tab->lines[tab->row].length;
  }
  
  if (!select) {
    tab->sel_row = tab->row;
    tab->sel_col = tab->col;
  }
}

void bedd_right(bedd_t *tab, int select) {
  if (tab->col < tab->lines[tab->row].length) {
    tab->col++;
  } else if (tab->row < tab->line_cnt - 1) {
    tab->row++;
    tab->col = 0;
  }
  
  if (!select) {
    tab->sel_row = tab->row;
    tab->sel_col = tab->col;
  }
}

int main(int argc, const char **argv) {
  for (int i = 1; i < argc; i++) {
    bedd_tabs = realloc(bedd_tabs, (bedd_count + 1) * sizeof(bedd_t));
    
    if (bedd_init(bedd_tabs + bedd_count, argv[i], 0)) {
      bedd_active = bedd_count++;
    } else {
      bedd_tabs = realloc(bedd_tabs, bedd_count * sizeof(bedd_t));
    }
  }
  
  if (!bedd_count) {
    bedd_tabs = realloc(bedd_tabs, (++bedd_count) * sizeof(bedd_t));
    bedd_init(bedd_tabs, NULL, 0);
  }
  
  rich_init();
  rich_tick();
  
  user_init();
  user_get_size();
  
  int update = 1;
  
  for (;;) {
    int key = K_NONE;
    
    if (bedd_tabs[bedd_active].prog) {
      char buffer[257];
      int length;
      
      while ((length = read(bedd_tabs[bedd_active].term_fd, buffer, 256)) > 0) {
        bedd_tabs[bedd_active].row = bedd_tabs[bedd_active].term_row;
        bedd_tabs[bedd_active].col = bedd_tabs[bedd_active].term_col;
        
        bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].term_row;
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].term_col;
        
        for (int i = 0; i < length; i++) {
          if (buffer[i] == BEDD_CTRL('h')) {
            bedd_delete(bedd_tabs + bedd_active);
          } else {
            bedd_write_one(bedd_tabs + bedd_active, buffer[i]);
          }
          
          bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
          bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
          
          bedd_tabs[bedd_active].term_col = bedd_tabs[bedd_active].col;
          bedd_tabs[bedd_active].term_row = bedd_tabs[bedd_active].row;
        }
        
        user_force();
        update = 1;
      }
    }
    
    if (key = user_read()) {
      if (key == BEDD_CTRL('q')) {
        int index = 2;
        
        if (bedd_tabs[bedd_active].dirty) {
          const char *options[] = {"yes", "no", "cancel"};
          user_prompt_optn("boi you didn't save, wanna save before sayin' bye?", &index, options, 3, 0);
        } else {
          index = 1;
        }
        
        if (index != 2) {
          if (!index) {
            // TODO: get path if unnamed
            bedd_save(bedd_tabs + bedd_active);
          }
          
          if (bedd_count == 1) break;
          bedd_free(bedd_tabs + bedd_active);
          
          for (int i = bedd_active; i < bedd_count - 1; i++) {
            bedd_tabs[i] = bedd_tabs[i + 1];
          }
          
          bedd_tabs = realloc(bedd_tabs, (--bedd_count) * sizeof(bedd_t));
          if (bedd_active >= bedd_count) bedd_active = bedd_count - 1;
        }
        
        rich_tick();
      } else if (key == BEDD_CTRL('m') || key == '\n') {
        if (bedd_tabs[bedd_active].prog) {
          write(bedd_tabs[bedd_active].term_fd, "\n", 1);
        } else {
          bedd_write(bedd_tabs + bedd_active, '\n');
          
          bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
          bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
          
          user_force();
        }
      } else if (key == BEDD_CTRL('h') || key == '\x7F') {
        if (bedd_tabs[bedd_active].prog) {
          write(bedd_tabs[bedd_active].term_fd, "\b", 1);
        } else {
          bedd_delete(bedd_tabs + bedd_active);
          
          bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
          bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
          
          user_force();
        }
      } else if (key == BEDD_CTRL('n')) {
        bedd_tabs = realloc(bedd_tabs, (bedd_count + 1) * sizeof(bedd_t));
        
        if (bedd_init(bedd_tabs + bedd_count, NULL, 0)) {
          bedd_active = bedd_count++;
        } else {
          bedd_tabs = realloc(bedd_tabs, bedd_count * sizeof(bedd_t));
        }
        
        rich_tick();
      } else if (key == BEDD_CTRL('t')) {
        bedd_tabs = realloc(bedd_tabs, (bedd_count + 1) * sizeof(bedd_t));
        
        if (bedd_init(bedd_tabs + bedd_count, NULL, 1)) {
          bedd_active = bedd_count++;
        } else {
          bedd_tabs = realloc(bedd_tabs, bedd_count * sizeof(bedd_t));
        }
        
        rich_tick();
      } else if (key == BEDD_CTRL('o')) {
        bedd_tabs = realloc(bedd_tabs, (bedd_count + 1) * sizeof(bedd_t));
        
        // TODO
        
        if (bedd_init(bedd_tabs + bedd_count, NULL, 0)) {
          bedd_active = bedd_count++;
        } else {
          bedd_tabs = realloc(bedd_tabs, bedd_count * sizeof(bedd_t));
        }
        
        rich_tick();
      } else if (key == BEDD_CTRL('s')) {
        // TODO: get path if unnamed
        
        bedd_save(bedd_tabs + bedd_active);
        rich_tick();
      } else if (key == BEDD_CTRL('a')) {
        bedd_tabs[bedd_active].row = 0;
        bedd_tabs[bedd_active].col = 0;
        
        bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].line_cnt - 1;
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].sel_row].length;
      } else if (key == BEDD_CTRL('c')) {
        user_copy();
      } else if (key == BEDD_CTRL('x')) {
        user_copy();
        bedd_delete(bedd_tabs + bedd_active);
        
        user_force();
      } else if (key == BEDD_CTRL('v')) {
        user_paste();
        user_force();
      } else if (key == K_PAGE_UP) {
        for (int i = 0; i < bedd_page_step; i++) {
          bedd_up(bedd_tabs + bedd_active, 0);
        }
        
        user_force();
      } else if (key == K_PAGE_DOWN) {
        for (int i = 0; i < bedd_page_step; i++) {
          bedd_down(bedd_tabs + bedd_active, 0);
        }
        
        user_force();
      } else if (key == K_SCRL_UP) {
        for (int i = 0; i < 2; i++) {
          if (bedd_tabs[bedd_active].off_row == 0) {
            break;
          }
                        
          bedd_tabs[bedd_active].off_row--;
        }
      } else if (key == K_SCRL_DOWN) {
        for (int i = 0; i < 2; i++) {
          if (bedd_tabs[bedd_active].off_row >= bedd_tabs[bedd_active].line_cnt - 1) {
            break;
          }
          
          bedd_tabs[bedd_active].off_row++;
        }
      } else if (key == K_UP) {
        bedd_up(bedd_tabs + bedd_active, 0);
        user_force();
      } else if (key == K_DOWN) {
        bedd_down(bedd_tabs + bedd_active, 0);
        user_force();
      } else if (key == K_LEFT) {
        bedd_left(bedd_tabs + bedd_active, 0);
        user_force();
      } else if (key == K_RIGHT) {
        bedd_right(bedd_tabs + bedd_active, 0);
        user_force();
      } else if (key == K_S_UP) {
        bedd_up(bedd_tabs + bedd_active, 1);
        user_force();
      } else if (key == K_S_DOWN) {
        bedd_down(bedd_tabs + bedd_active, 1);
        user_force();
      } else if (key == K_S_LEFT) {
        bedd_left(bedd_tabs + bedd_active, 1);
        user_force();
      } else if (key == K_S_RIGHT) {
        bedd_right(bedd_tabs + bedd_active, 1);
        user_force();
      } else if (key == K_C_UP) {
        bedd_tabs[bedd_active].row = 0;
        bedd_tabs[bedd_active].col = 0;
        
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
        bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
        
        user_force();
      } else if (key == K_C_DOWN) {
        bedd_tabs[bedd_active].row = bedd_tabs[bedd_active].line_cnt - 1;
        bedd_tabs[bedd_active].col = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
        
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
        bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
        
        user_force();
      } else if (key == K_C_LEFT) {
        if (bedd_active) {
          bedd_active--;
        }
        
        rich_tick();
      } else if (key == K_C_RIGHT) {
        if (bedd_active < bedd_count - 1) {
          bedd_active++;
        }
        
        rich_tick();
      } else if (key == K_HOME) {
        bedd_tabs[bedd_active].col = 0;
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
        
        user_force();
      } else if (key == K_END) {
        bedd_tabs[bedd_active].col = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
        
        user_force();
      } else if (key == K_MOUS_DOWN) {
        bedd_tabs[bedd_active].col = bedd_mouse_x + user_off_col();
        bedd_tabs[bedd_active].row = bedd_mouse_y + user_off_row();
        
        if (bedd_tabs[bedd_active].row < 0) {
          bedd_tabs[bedd_active].row = 0;
        } else if (bedd_tabs[bedd_active].row >= bedd_tabs[bedd_active].line_cnt) {
          bedd_tabs[bedd_active].row = bedd_tabs[bedd_active].line_cnt - 1;
        }
        
        int length = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
        
        if (bedd_tabs[bedd_active].col < 0) {
          bedd_tabs[bedd_active].col = 0;
        } else if (bedd_tabs[bedd_active].col > length) {
          bedd_tabs[bedd_active].col = length;
        }
        
        bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
        bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
        
        user_force();
      } else if (key == K_MOUS_DRAG) {
        bedd_tabs[bedd_active].col = bedd_mouse_x + user_off_col();
        bedd_tabs[bedd_active].row = bedd_mouse_y + user_off_row();
        
        if (bedd_tabs[bedd_active].row < 0) {
          bedd_tabs[bedd_active].row = 0;
        } else if (bedd_tabs[bedd_active].row >= bedd_tabs[bedd_active].line_cnt) {
          bedd_tabs[bedd_active].row = bedd_tabs[bedd_active].line_cnt - 1;
        }
        
        int length = bedd_tabs[bedd_active].lines[bedd_tabs[bedd_active].row].length;
        
        if (bedd_tabs[bedd_active].col < 0) {
          bedd_tabs[bedd_active].col = 0;
        } else if (bedd_tabs[bedd_active].col > length) {
          bedd_tabs[bedd_active].col = length;
        }
        
        user_force();
      } else if (key >= 32 && key < 256) {
        if (bedd_tabs[bedd_active].prog) {
          write(bedd_tabs[bedd_active].term_fd, &key, 1);
        } else {
          bedd_write(bedd_tabs + bedd_active, key);
          
          bedd_tabs[bedd_active].sel_col = bedd_tabs[bedd_active].col;
          bedd_tabs[bedd_active].sel_row = bedd_tabs[bedd_active].row;
          
          user_force();
        }
      }
      
      update = 1;
    }
    
    if (update) user_draw();
    update = 0;
  }
  
  user_cursor(1);
  user_exit();
  
  rich_exit();
  
  return 0;
}
