#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "mandelCore.h"


#define WinW 300
#define WinH 300
#define ZoomStepFactor 0.5
#define ZoomIterationFactor 2

static Display *dsp = NULL;
static unsigned long curC;
static Window win;
static GC gc;

/* basic win management rountines */

static void openDisplay() {
  if (dsp == NULL) {
    dsp = XOpenDisplay(NULL);
  }
}

static void closeDisplay() {
  if (dsp != NULL) {
    XCloseDisplay(dsp);
    dsp=NULL;
  }
}

void openWin(const char *title, int width, int height) {
  unsigned long blackC,whiteC;
  XSizeHints sh;
  XEvent evt;
  long evtmsk;

  whiteC = WhitePixel(dsp, DefaultScreen(dsp));
  blackC = BlackPixel(dsp, DefaultScreen(dsp));
  curC = blackC;

  win = XCreateSimpleWindow(dsp, DefaultRootWindow(dsp), 0, 0, WinW, WinH, 0, blackC, whiteC);

  sh.flags=PSize|PMinSize|PMaxSize;
  sh.width=sh.min_width=sh.max_width=WinW;
  sh.height=sh.min_height=sh.max_height=WinH;
  XSetStandardProperties(dsp, win, title, title, None, NULL, 0, &sh);

  XSelectInput(dsp, win, StructureNotifyMask|KeyPressMask);
  XMapWindow(dsp, win);
  do {
    XWindowEvent(dsp, win, StructureNotifyMask, &evt);
  } while (evt.type != MapNotify);

  gc = XCreateGC(dsp, win, 0, NULL);

}

void closeWin() {
  XFreeGC(dsp, gc);
  XUnmapWindow(dsp, win);
  XDestroyWindow(dsp, win);
}

void flushDrawOps() {
  XFlush(dsp);
}

void clearWin() {
  XSetForeground(dsp, gc, WhitePixel(dsp, DefaultScreen(dsp)));
  XFillRectangle(dsp, win, gc, 0, 0, WinW, WinH);
  flushDrawOps();
  XSetForeground(dsp, gc, curC);
}

void drawPoint(int x, int y) {
  XDrawPoint(dsp, win, gc, x, WinH-y);
  flushDrawOps();
}

void getMouseCoords(int *x, int *y) {
  XEvent evt;

  XSelectInput(dsp, win, ButtonPressMask);
  do {
    XNextEvent(dsp, &evt);
  } while (evt.type != ButtonPress);
  *x=evt.xbutton.x; *y=evt.xbutton.y;
}

/* color stuff */

void setColor(char *name) {
  XColor clr1,clr2;

  if (!XAllocNamedColor(dsp, DefaultColormap(dsp, DefaultScreen(dsp)), name, &clr1, &clr2)) {
    printf("failed\n"); return;
  }
  XSetForeground(dsp, gc, clr1.pixel);
  curC = clr1.pixel;
}

char *pickColor(int v, int maxIterations) {
  static char cname[128];

  if (v == maxIterations) {
    return("black");
  }
  else {
    sprintf(cname,"rgb:%x/%x/%x",v%64,v%128,v%256);
    return(cname);
  }
}



// NOTE: we could avoid having some variables (like maxiterations, res) global
// if we used as struct and pass them as parameters to every single worker

// needed by threads
volatile int *res;
mandel_Pars *slices;

pthread_mutex_t wait_for_work ; // used by workers to block after they are done with a task
pthread_mutex_t chain_block;	// used to control the way main unblocks workers
pthread_mutex_t cs_mtx;
pthread_mutex_t main_sleep;// cs mutex
volatile int *slice;			// 0:already_painted/not_calced started, 1:marked to be painted
volatile int to_paint = 0;      // how many slices are not yet painted
volatile int done_paint = 0;	// how many have slices have been painted
int maxIterations;

// mutex initializations
void mtx_init() {

	// mutex wait_for_work = 0
	// we want every worker to be blocked every time he finishes his task
	if (pthread_mutex_init(&wait_for_work, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&wait_for_work)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// we want main to be blocked after assgning a job to a worker
	// main will be awaken right after the worker gets unblocked
	// This 'chain unblocking' logic prevents main from calling up(wait_for_work)
	// before workers become blocked
	if (pthread_mutex_init(&chain_block, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&chain_block)) {
		perror("pthread_mutex_lock");
		exit(1);
	}


	// main_sleep = 0
	// main should wait if there is nothing to draw
	if (pthread_mutex_init(&main_sleep, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&main_sleep)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// cs mutex between workers and main
	if (pthread_mutex_init(&cs_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
}


// worker function
void *pthread_work(void *arg) {
	int i;


	i = *((int *)arg);	//read arg

	while(1) {


		// down(cs_mtx)
		if (pthread_mutex_lock(&cs_mtx)) {
			perror("pthread_mutex_lock");
		}

		mandel_Calc(&slices[i],maxIterations,&res[i*slices[i].imSteps*slices[i].reSteps]);
		slice[i] = 1; // slice is now available to be painted
		to_paint++;   // remaining_ready_slices to be painted +1

		// main thread is sleeping. need to wake it up..
		if (to_paint == 0) {
			if (pthread_mutex_unlock(&main_sleep)) {
				perror("pthread_mutex_unlock");
			}
		}
		printf("\nthread %d: done\n", i);

		// up(cs_mtx)
		if (pthread_mutex_unlock(&cs_mtx)) {
			perror("pthread_mutex_unlock");
		}


		// now every single worker will block until unblocked by main
		// main unblocks only 1 worker at a time and waits until gets permission from
		// him to unblock another one

		// down(wait_for_work)
		if (pthread_mutex_lock(&wait_for_work)) {
			perror("pthread_mutex_lock");
		}
		// up(chain_block)
		if (pthread_mutex_unlock(&chain_block)) {
			perror("pthread_mutex_unlock");
		}
	}
	return(NULL);
}



int main(int argc, char *argv[]) {
  mandel_Pars pars;
  int i,j,x,y,nofslices,level;
  int xoff,yoff;
  long double reEnd,imEnd,reCenter,imCenter;

  int *tmp;
  pthread_t *worker;

  printf("\n");
  printf("This program starts by drawing the default Mandelbrot region\n");
  printf("When done, you can click with the mouse on an area of interest\n");
  printf("and the program will automatically zoom around this point\n");
  printf("\n");
  printf("Press enter to continue\n");
  getchar();

  pars.reSteps = WinW; /* never changes */
  pars.imSteps = WinH; /* never changes */

  /* default mandelbrot region */

  pars.reBeg = (long double) -2.0;
  reEnd = (long double) 1.0;
  pars.imBeg = (long double) -1.5;
  imEnd = (long double) 1.5;
  pars.reInc = (reEnd - pars.reBeg) / pars.reSteps;
  pars.imInc = (imEnd - pars.imBeg) / pars.imSteps;

  printf("enter max iterations (50): ");
  scanf("%d",&maxIterations);
  printf("enter no of slices: ");
  scanf("%d",&nofslices);

  /* adjust slices to divide win height */

  while (WinH % nofslices != 0) { nofslices++;}

  /* allocate slice parameter and result arrays */

  slices = (mandel_Pars *)malloc(sizeof(mandel_Pars)*nofslices);
  res = (volatile int *)malloc(sizeof(int)*pars.reSteps*pars.imSteps);

  mtx_init();

  slice = (volatile int *)malloc(sizeof(int)*nofslices);
  if (slice == NULL) {
	  printf("memory allocating problems\n");
	  exit(1);
  }

  tmp = (int *)malloc(sizeof(int)*nofslices);
  if (tmp == NULL) {
	  printf("memory allocating problems\n");
	  exit(1);
  }

  worker = (pthread_t *)malloc(sizeof(pthread_t)*nofslices);
  if (worker == NULL) {
	  printf("memory allocating problems\n");
	  exit(1);
  }

   /* open window for drawing results */

  openDisplay();
  openWin(argv[0], WinW, WinH);

  level = 1;

  clearWin();
  mandel_Slice(&pars,nofslices,slices);

  // create workers and give them initial task
  for(i=0; i<nofslices; i++) {
	  tmp[i] = i;
	  slice[i] = 0;
	  if(pthread_create(&worker[i], NULL, pthread_work, &tmp[i])) {
		  perror("pthread_create");
		  exit(1);
	  }
  }


  while (1) {

	// down(cs_mtx)
	if (pthread_mutex_lock(&cs_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	to_paint--;

	// if there is nothing to paint... main blocks until something become available
	if (to_paint == -1) {
		// up(cs_mtx);
		if (pthread_mutex_unlock(&cs_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
		// down(main_sleep);
		if (pthread_mutex_lock(&main_sleep)) {
			perror("pthread_mutex_lock");
			exit(1);
		}
		// down(cs_mtx);
		if (pthread_mutex_lock(&cs_mtx)) {
			perror("pthread_mutex_lock");
			exit(1);
		}
	}


	  // drawing code
	  printf("main checking for undrawn slices\n");
	  // draw every slice available... (slice[i] == 1)
	  for (i=0; i < nofslices; i++) {
		if (slice[i] == 1) {
			//painting code
			y = i*slices[i].imSteps;
			for (j=0; j<slices[i].imSteps; j++) {
				for (x=0; x<slices[i].reSteps; x++) {
				  setColor(pickColor(res[y*slices[i].reSteps+x],maxIterations));
				  drawPoint(x,y);
				}
				y++;
			}
			printf("slice paint: %d total: %d\n",i, done_paint);
			done_paint++;
			slice[i] = 0;	// mark as painted. no need to paint again
		}
	  }

	// up(cs_mtx);
	if (pthread_mutex_unlock(&cs_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}


	if (done_paint == nofslices) {
		// get next task
		getMouseCoords(&x,&y);
		xoff = x;
		yoff = WinH-y;
		reCenter = pars.reBeg + xoff*pars.reInc;
		imCenter = pars.imBeg + yoff*pars.imInc;
		pars.reInc = pars.reInc*ZoomStepFactor;
		pars.imInc = pars.imInc*ZoomStepFactor;
		pars.reBeg = reCenter - (WinW/2)*pars.reInc;
		pars.imBeg = imCenter - (WinH/2)*pars.imInc;

		maxIterations = maxIterations*ZoomIterationFactor;
		level++;

		// re intialize window and done_paint
		clearWin();
		mandel_Slice(&pars,nofslices,slices);
		done_paint = 0;

		// ublock 1 by 1 all nofslices workers
		// main will block after unblocking a worker just to make sure they
		// are getting unblocked 1 by 1
		for (i=0; i<nofslices; i++) {
			// up(wait_for_work);
			if(pthread_mutex_unlock(&wait_for_work)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
			// down(chain_block);
			if (pthread_mutex_lock(&chain_block)) {
				perror("pthread_mutex_lock");
				exit(1);
			}
		}
	}

  }

  /* never reach this point; for cosmetic reasons */

  free(slices);
  free((void *)res);
  free((void *)slice);
  closeWin();
  closeDisplay();

}
