#ifndef __BEDD_H__
#define __BEDD_H__

// MEGA TODO LIST:
// - add path prompt with autocompletion
//   - add saving and opening files
// - add find-in-file, find-in-work-dir and their respective replace counterparts
//   - add find and replace history
// - add dynamic parsing system with config files
//   - add c/c++, javascript, markdown, html, bash and nasm config files
//   - COLORS(and make them faster than in old bedd)
//   - automatically closing parentheses, indentation and such
//   - implement fully working hinting using the parser
// - rainbow parentheses so you can distinguish matching ones(thanks abbix!)

// word: individual symbol char., number with/without prefix or identifier

#define BEDD_INVERT "\x1B[7m"
#define BEDD_NORMAL "\x1B[27m"

#define BEDD_BLACK   "\x1B[30m"
#define BEDD_RED     "\x1B[31m"
#define BEDD_GREEN   "\x1B[32m"
#define BEDD_YELLOW  "\x1B[33m"
#define BEDD_BLUE    "\x1B[34m"
#define BEDD_MAGENTA "\x1B[35m"
#define BEDD_CYAN    "\x1B[36m"
#define BEDD_WHITE   "\x1B[97m"
#define BEDD_GRAY    "\x1B[37m"

#define BEDD_SELECT  "\x1B[100m"
#define BEDD_LINE    "\x1B[40m"
#define BEDD_DEFAULT "\x1B[49m"

#define BEDD_CTRL(k) ((k) & 0x1F)

#define BEDD_ISNUMBR(c) ((c) >= '0' && (c) <= '9')
#define BEDD_ISUPPER(c) ((c) >= 'A' && (c) <= 'Z')
#define BEDD_ISLOWER(c) ((c) >= 'a' && (c) <= 'z')
#define BEDD_ISALPHA(c) (BEDD_ISUPPER((c)) || BEDD_ISLOWER((c)))
#define BEDD_ISALNUM(c) (BEDD_ISALPHA((c)) || BEDD_ISNUMBR((c)))
#define BEDD_ISIDENT(c) (BEDD_ISALNUM((c)) || (c) == '_')

#define K_NONE      0
#define K_UP        256
#define K_DOWN      257
#define K_LEFT      258
#define K_RIGHT     259
#define K_S_UP      260
#define K_S_DOWN    261
#define K_S_LEFT    262
#define K_S_RIGHT   263
#define K_C_UP      264
#define K_C_DOWN    265
#define K_C_LEFT    266
#define K_C_RIGHT   267
#define K_HOME      268
#define K_END       269
#define K_TOP       270
#define K_BOTTOM    271
#define K_PAGE_UP   272
#define K_PAGE_DOWN 273
#define K_SCRL_UP   274
#define K_SCRL_DOWN 275
#define K_MOUS_UP   276
#define K_MOUS_DRAG 277
#define K_MOUS_DOWN 278
#define K_REFRESH   65536

typedef struct bedd_line_t bedd_line_t;
typedef struct bedd_t bedd_t;

typedef struct bedd_word_t bedd_word_t;

struct bedd_line_t {
  char *buffer;
  int length;
};

struct bedd_t {
  char *path;
  int dirty;
  
  bedd_line_t *lines;
  int line_cnt;
  
  // cursor position
  int row, col;
  
  // scroll offset
  int off_row, off_col;
  
  // selection position
  int sel_row, sel_col;
  
  // tab size
  int tab_size;
  
  // for terminal tabs only:
  
  // terminal program pid
  int prog;
  
  // terminal cursor position
  int term_row, term_col;
  
  // terminal file descriptor(master)
  int term_fd;
  
  // terminal ANSI buffer and length
  char term_buffer[32];
  int term_length;
};

extern bedd_t *bedd_tabs;
extern int bedd_count;

extern int bedd_active;

extern int bedd_page_step;

extern int bedd_mouse_x;
extern int bedd_mouse_y;

/*
void bedd_init(bedd_t *tab, const char *path);
void bedd_free(bedd_t *tab);
int  bedd_save(bedd_t *tab);

void bedd_tabs(bedd_t *tabs, int tab_pos, int tab_cnt, int width);
void bedd_stat(bedd_t *tab, const char *status);
int  bedd_color(bedd_t *tab, int state, int row, int col);
void bedd_indent(bedd_t *tab, int col, int on_block);

void bedd_write(bedd_t *tab, char c);
void bedd_delete(bedd_t *tab);

int  bedd_find(bedd_t *tab, const char *query, int whole_word);
int  bedd_replace(bedd_t *tab, const char *query, const char *replace, int whole_word);

void bedd_up(bedd_t *tab, int select);
void bedd_down(bedd_t *tab, int select);
void bedd_left(bedd_t *tab, int select);
void bedd_right(bedd_t *tab, int select);

void bedd_copy(bedd_t *tab);
void bedd_paste(bedd_t *tab);
*/

int  bedd_init(bedd_t *tab, const char *path, int term);
void bedd_free(bedd_t *tab);
int  bedd_save(bedd_t *tab);

void bedd_write_raw(bedd_t *tab, int value); // do not process newlines
void bedd_write_one(bedd_t *tab, int value); // process newlines and all that, do not autoindentate or things like that
void bedd_write(bedd_t *tab, int value);     // full thingie

void bedd_delete_raw(bedd_t *tab);
void bedd_delete(bedd_t *tab);

void bedd_up(bedd_t *tab, int select);
void bedd_down(bedd_t *tab, int select);
void bedd_left(bedd_t *tab, int select);
void bedd_right(bedd_t *tab, int select);

void rich_init(void);
void rich_exit(void);
void rich_tick(void);

int user_get_size(void);

int user_off_row(void);
int user_off_col(void);

void user_gotoxy(int x, int y);
void user_cursor(int type); // 1 = block(prompt, default), 3 = line(text)
void user_clear_end(void);

void user_force(void);

// returns 1 if success, or 0 if aborted(Ctrl+Q), stores result in *result

int user_prompt_text(const char *prompt, char *result);                                             // basic text prompting
int user_prompt_hist(const char *prompt, char *result, const char **history, int count);            // fish-like autocompletion and highlighting
int user_prompt_path(const char *prompt, char *result, const char *curr_dir);                       // again, fish-like autocompletion and highlighting but with paths
int user_prompt_optn(const char *prompt, int *result, const char **options, int count, int strict); // if strict is 1 will not return until a valid option is entered, if 0 will select the closest option

// function and variable hinting = chad

void user_hinter(int x, int y, const char *text);

void user_render_top(void); // show tabs
void user_render_btm(void); // show path and message and such

void user_init(void);
void user_exit(void);
void user_draw(void);
int  user_read_raw(void);
int  user_read(void);

void user_copy(void);
void user_paste(void);

#endif
