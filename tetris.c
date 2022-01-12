#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/time.h>

/* Expension factor of shapes */
#define EXP_FACT 2

/* Frame dimension */
#define FRAMEW (int)(10*2.3)
#define FRAMEH (int)(9*2.3)
#define FRAMEW_NB 15
#define FRAMEH_NB 5

/* Shapes position */
#define N_POS ((current.pos < 3) ? current.pos + 1 : 0)
#define P_POS ((current.pos > 0) ? current.pos - 1 : 3)

/* Move the shape */
#define KEY_MOVE_LEFT  'D'
#define KEY_MOVE_RIGHT 'C'

/* Change the shape position */
#define KEY_CHANGE_POSITION_NEXT 'A'
#define KEY_CHANGE_POSITION_PREV 'B'

/* ' ' for space key */
#define KEY_DROP_SHAPE ' '

/* Other key */
#define KEY_PAUSE 'p'
#define KEY_QUIT  'q'
#define KEY_SPEED 's'

/* Timing in milisecond */
#define TIMING 300000

/* Draw the score.. */
#define DRAW_SCORE() set_color(Score);                             \
     printf("\033[%d;%dH %d", FRAMEH_NB + 3, FRAMEW + 10, score);   \
     printf("\033[%d;%dH %d", FRAMEH_NB + 4, FRAMEW + 10, lines);   \
     set_color(0);

/* Bool type */
typedef enum { False, True } Bool;

/* Shape structure */
typedef struct
{
     int num;
     int next;
     int pos;
     int x, y;
     Bool last_move;
} shape_t;

/* Color enum */
enum { Black, Blue, Red, Magenta, White, Green, Cyan, Yellow, Border, Score, ColLast };

/* Prototypes */

void clear_term(void);
void set_cursor(Bool);
void printxy(int, int, int, char*);
void set_color(int);
int nrand(int, int);
void sig_handler(int);
void frame_init(void);
void frame_nextbox_init(void);
void frame_refresh(void);
void frame_nextbox_refresh(void);
void shape_set(void);
void shape_unset(void);
void shape_new(void);
void shape_go_down(void);
void shape_set_position(int);
void shape_move(int);
void shape_drop(void);
void arrange_score(int l);
void check_plain_line(void);
int check_possible_pos(int, int);
void get_key_event(void);

/* Variables */

struct itimerval tv;
struct termios back_attr;
shape_t current;
int frame[FRAMEH + 1][FRAMEW + 1];
int frame_nextbox[FRAMEH_NB][FRAMEW_NB];
int score;
int lines;
Bool running;

/* Constants */

const int sattr[7][3] = {{0,2}, {-1,0}, {-1,1,1}, {-1,1}, {-1,1}, {0,1}, {0,1}};
const int shapes[7][4][4][2] =
{
     /* O */
     {
          {{0,0},{1,0},{0,1},{1,1}},
          {{0,0},{1,0},{0,1},{1,1}},
          {{0,0},{1,0},{0,1},{1,1}},
          {{0,0},{1,0},{0,1},{1,1}}
     },
     /* I */
     {
          {{1,0},{1,1},{1,2},{1,3}},
          {{0,1},{1,1},{2,1},{3,1}},
          {{1,0},{1,1},{1,2},{1,3}},
          {{0,1},{1,1},{2,1},{3,1}}
     },
     /* L */
     {
          {{0,1},{1,1},{2,1},{2,2}},
          {{1,0},{1,1},{1,2},{2,0}},
          {{0,0},{0,1},{1,1},{2,1}},
          {{1,0},{1,1},{1,2},{0,2}}
     },
     /* J */
     {
          {{1,0},{1,1},{1,2},{2,2}},
          {{0,2},{1,2},{2,2},{2,1}},
          {{0,0},{1,0},{1,1},{1,2}},
          {{0,1},{0,2},{1,1},{2,1}}
     },
     /* S */
     {
          {{1,1},{1,2},{2,0},{2,1}},
          {{0,1},{1,1},{1,2},{2,2}},
          {{1,1},{1,2},{2,0},{2,1}},
          {{0,1},{1,1},{1,2},{2,2}}
     },
     /* Z */
     {
          {{0,0},{0,1},{1,1},{1,2}},
          {{0,2},{1,1},{2,1},{1,2}},
          {{0,0},{0,1},{1,1},{1,2}},
          {{0,2},{1,1},{2,1},{1,2}}
     },
     /* T */
     {
          {{0,1},{1,0},{1,1},{1,2}},
          {{0,1},{1,1},{1,2},{2,1}},
          {{1,0},{1,1},{1,2},{2,1}},
          {{1,0},{0,1},{1,1},{2,1}}
     }
};

void
clear_term(void)
{
     puts("\e[2J");

     return;
}

void
set_cursor(Bool b)
{
     printf("\e[?25%c", ((b) ? 'h' : 'l'));

     return;
}

void
set_color(int color)
{
     int bg = 0, fg = 0;

     switch(color)
     {
     default:
     case Black:   bg = 0;  break;
     case Blue:    bg = 44; break;
     case Red:     bg = 41; break;
     case Magenta: bg = 45; break;
     case White:   bg = 47; break;
     case Green:   bg = 42; break;
     case Cyan:    bg = 46; break;
     case Yellow:  bg = 43; break;
     case Border:  bg = 47; break;
     case Score:   fg = 37; bg = 49; break;
     }

     printf("\e[%d;%dm", fg, bg);

     return;
}

void
printxy(int color, int x, int y, char *str)
{
     set_color(color);
     printf("\e[%d;%dH%s", ++x, ++y, str);
     set_color(0);

     return;
}

int
nrand(int min, int max)
{
     return (rand() % (max - min + 1)) + min;
}

void
sig_handler(int sig)
{
     switch(sig)
     {
     case SIGTERM:
     case SIGINT:
     case SIGSEGV:
          running = False;
          break;
     case SIGALRM:
          tv.it_value.tv_usec -= tv.it_value.tv_usec / 3000;
          setitimer(0, &tv, NULL);
          break;
     }

     return;
}

void
frame_init(void)
{
     int i;

     /* Frame border */
     for(i = 0; i < FRAMEW + 1; ++i)
     {
          frame[0][i] = Border;
          frame[FRAMEH][i] = Border;
     }
     for(i = 0; i < FRAMEH; ++i)
     {
          frame[i][0] = Border;
          frame[i][1] = Border;
          frame[i][FRAMEW] = Border;
          frame[i][FRAMEW - 1] = Border;
     }

     frame_refresh();

     return;
}

void
frame_nextbox_init(void)
{
     int i;

     for(i = 0; i < FRAMEH_NB; ++i)
     {
          frame_nextbox[i][0] = Border;
          frame_nextbox[i][1] = Border;
          frame_nextbox[i][FRAMEW_NB - 1] = Border;
          frame_nextbox[i][FRAMEW_NB] = Border;

     }
     for(i = 0; i < FRAMEW_NB + 1; ++i)
          frame_nextbox[0][i] = frame_nextbox[FRAMEH_NB][i] = Border;

     frame_nextbox_refresh();

     return;
}

void
frame_refresh(void)
{
     int i, j;

     for(i = 0; i < FRAMEH + 1; ++i)
          for(j = 0; j < FRAMEW + 1; ++j)
                    printxy(frame[i][j], i, j, " ");
     return;
}

void
frame_nextbox_refresh(void)
{
     int i, j;

     /* Clean frame_nextbox[][] */
     for(i = 1; i < FRAMEH_NB; ++i)
          for(j = 2; j < FRAMEW_NB - 1; ++j)
               frame_nextbox[i][j] = 0;

     /* Set the shape in the frame */
     for(i = 0; i < 4; ++i)
          for(j = 0; j < EXP_FACT; ++j)
               frame_nextbox
                    [2 + shapes[current.next][sattr[current.next][2]][i][0] + sattr[current.next][0]]
                    [4 + shapes[current.next][sattr[current.next][2]][i][1] * EXP_FACT + j + sattr[current.next][1]]
                    = current.next + 1;

     /* Draw the frame */
     for(i = 0; i < FRAMEH_NB + 1; ++i)
          for(j = 0; j < FRAMEW_NB + 1; ++j)
               printxy(frame_nextbox[i][j], i, j + FRAMEW + 3, " ");

     return;
}

void
shape_set(void)
{
     int i, j;

     for(i = 0; i < 4; ++i)
          for(j = 0; j < EXP_FACT; ++j)
               frame[current.x + shapes[current.num][current.pos][i][0]]
                    [current.y + shapes[current.num][current.pos][i][1] * EXP_FACT + j]
                    = current.num + 1;

     if(current.x < 1)
          for(i = 0; i < FRAMEW + 1; ++i)
               frame[0][i] = Border;

     return;
}

void
shape_unset(void)
{
     int i, j;

     for(i = 0; i < 4; ++i)
          for(j = 0; j < EXP_FACT; ++j)
               frame[current.x + shapes[current.num][current.pos][i][0]]
                    [current.y + shapes[current.num][current.pos][i][1] * EXP_FACT + j] = 0;

     if(current.x < 1)
          for(i = 0; i < FRAMEW + 1; ++i)
               frame[0][i] = Border;
     return;
}

void
shape_new(void)
{
     int i;

     /* Draw the previous shape for it stay there */
     shape_set();
     check_plain_line();

     /* Set the new shape property */
     current.num = current.next;
     current.x = 1;
     current.y = (FRAMEW / 2) - 1;;
     current.next = nrand(0, 6);

     frame_nextbox_refresh();

     if(current.x > 1)
          for(i = 2; i < FRAMEW - 1; ++i)
               frame[1][i] = 0;

     return;
}

void
shape_go_down(void)
{

     shape_unset();

     /* Fall the shape else; collision with the ground or another shape
      * then stop it and create another */
     if(!check_possible_pos(current.x + 1, current.y))
          ++current.x;
     else
          if(current.x > 2)
               shape_new();
          else
          {
               shape_new();
               frame_refresh();
               sleep(2);
               running = False;
          }




     return;
}

void
shape_set_position(int p)
{
     int old = current.pos;

     shape_unset();

     current.pos = p ;

     if(check_possible_pos(current.x, current.y))
          current.pos = old;

     return;
}


void
shape_move(int n)
{

     shape_unset();

     if(!check_possible_pos(current.x, current.y + n))
          current.y += n;

     return;
}

void
shape_drop(void)
{
     while(!check_possible_pos(current.x + 1, current.y))
     {
          shape_unset();
          ++current.x;
     }
     score += FRAMEH - current.x;

     return;
}

void
init(void)
{
     struct sigaction siga;
     struct termios term;

     /* Clean term */
     clear_term();
     set_cursor(False);

     /* Make rand() really random :) */
     srand(getpid());

     /* Init variables */
     score = lines = 0;
     running = True;
     current.y = (FRAMEW / 2) - 1;
     current.num = nrand(0, 6);
     current.next = nrand(0, 6);

     /* Score */
     printxy(0, FRAMEH_NB + 2, FRAMEW + 3, "Score:");
     printxy(0, FRAMEH_NB + 3, FRAMEW + 3, "Lines:");
     DRAW_SCORE();

     /* Init signal */
     sigemptyset(&siga.sa_mask);
     siga.sa_flags = 0;
     siga.sa_handler = sig_handler;
     sigaction(SIGALRM, &siga, NULL);
     sigaction(SIGTERM, &siga, NULL);
     sigaction(SIGINT,  &siga, NULL);
     sigaction(SIGSEGV, &siga, NULL);

     /* Init timer */
     tv.it_value.tv_usec = TIMING;
     sig_handler(SIGALRM);

     /* Init terminal (for non blocking & noecho getchar(); */
     tcgetattr(STDIN_FILENO, &term);
     tcgetattr(STDIN_FILENO, &back_attr);
     term.c_lflag &= ~(ICANON|ECHO);
     tcsetattr(0, TCSANOW, &term);

     return;
}

void
get_key_event(void)
{
     int c = getchar();

     if(c > 0)
          --current.x;

     switch(c)
     {
     case KEY_MOVE_LEFT:            shape_move(-EXP_FACT);              break;
     case KEY_MOVE_RIGHT:           shape_move(EXP_FACT);               break;
     case KEY_CHANGE_POSITION_NEXT: shape_set_position(N_POS);          break;
     case KEY_CHANGE_POSITION_PREV: shape_set_position(P_POS);          break;
     case KEY_DROP_SHAPE:           shape_drop();                       break;
     case KEY_SPEED:                ++current.x; ++score; DRAW_SCORE(); break;
     case KEY_PAUSE:                while(getchar() != KEY_PAUSE);      break;
     case KEY_QUIT:                 running = False;                    break;
     }

     return;
}

void
arrange_score(int l)
{
     /* Standard score count */
     switch(l)
     {
     case 1: score += 40;   break; /* One line */
     case 2: score += 100;  break; /* Two lines */
     case 3: score += 300;  break; /* Three lines */
     case 4: score += 1200; break; /* Four lines */
     }

     lines += l;

     DRAW_SCORE();

     return;
}

void
check_plain_line(void)
{
     int i, j, k, f, c = 0, nl = 0;

     for(i = 1; i < FRAMEH; ++i)
     {
          for(j = 1; j < FRAMEW; ++j)
               if(frame[i][j] == 0)
                    ++c;
          if(!c)
          {
               ++nl;
               for(k = i - 1; k > 1; --k)
                    for(f = 1; f < FRAMEW; ++f)
                         frame[k + 1][f] = frame[k][f];
          }
          c = 0;
     }
     arrange_score(nl);
     frame_refresh();

     return;
}

int
check_possible_pos(int x, int y)
{
     int i, j, c = 0;

     for(i = 0; i < 4; ++i)
          for(j = 0; j < EXP_FACT; ++j)
               if(frame[x + shapes[current.num][current.pos][i][0]]
                  [y + shapes[current.num][current.pos][i][1] * EXP_FACT + j] != 0)
                    ++c;

     return c;
}

void
quit(void)
{
     frame_refresh(); /* Redraw a last time the frame */
     set_cursor(True);
     tcsetattr(0, TCSANOW, &back_attr);
     printf("\nBye! Your score: %d\n", score);

     return;
}

int
main(int argc, char **argv)
{
     init();
     frame_init();
     frame_nextbox_init();;

     current.last_move = False;

     while(running)
     {
          get_key_event();
          shape_set();
          frame_refresh();
          shape_go_down();
     }

     quit();

     return 0;
}
