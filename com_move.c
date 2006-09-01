#include <stdio.h>
#include <string.h>		/* for strchr() */
#include <ctype.h>		/* for toupper() */

#include "rlgetc.h"
#include "db.h"
#include "token.h"
#include "xwin.h" 	
#include "lex.h"
#include "rubber.h"

#define POINT 0
#define REGION 1

static double x1, y1, x2, y2, x3, y3, x4, y4;
static double xmin, ymin, xmax, ymax;
void draw_bbox();
void move_draw_box();
STACK *stack;

/* 
    move a component in the current device.
    MOV <restrictor> { xysel xyref xynewref } ... <EOC>
*/

int com_move(LEXER *lp, char *arg)		
{

    enum {START,NUM1,COM1,NUM2,NUM3,COM2,NUM4,NUM5,COM3,NUM6,NUM7,COM4,NUM8,END} state = START;

    int done=0;
    TOKEN token;
    char word[BUFSIZE];
    int debug=0;
    int valid_comp=0;
    int i;
    DB_DEFLIST *p_best;
    int mode=POINT;

    int my_layer=0; 	/* internal working layer */

    int comp=ALL;

    /* check that we are editing a rep */
    if (currep == NULL ) {
	printf("must do \"EDIT <name>\" before MOVE\n");
	token_flush_EOL(lp);
	return(1);
    }

/* 
    To create a show set for restricting the pick, we look 
    here for and optional primitive indicator
    concatenated with an optional layer number:

	A[layer_num] ;(arc)
	C[layer_num] ;(circle)
	L[layer_num] ;(line)
	N[layer_num] ;(note)
	O[layer_num] ;(oval)
	P[layer_num] ;(poly)
	R[layer_num] ;(rectangle)
	T[layer_num] ;(text)

    or a instance name.  Instance names can be quoted, which
    allows the user to have instances which overlap the primitive
    namespace.  For example:  N7 is a note on layer seven, but
    "N7" is an instance call.

*/

    rl_saveprompt();
    rl_setprompt("MOVE> ");
    while(!done) {
	token = token_look(lp,word);
	if (debug) printf("got %s: %s\n", tok2str(token), word);
	if (token==CMD) {
	    state=END;
	} 
	switch(state) {	
	case START:		/* get option or first xy pair */
	    if (debug) printf("in START\n");
	    if (token == OPT ) {
		token_get(lp,word);
                if (word[0]==':') {
                    switch (toupper(word[1])) {
                        case 'R':
                            mode = REGION;
                            break;
                        case 'P':
                            mode = POINT;
                            break;
                        default:
                            printf("MOVE: bad option: %s\n", word);
                            state=END;
                            break;
                    }
                } else {
                    printf("MOVE: bad option: %s\n", word);
                    state = END;
                }
                state = START;
	    } else if (token == NUMBER) {
		state = NUM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just eat it up */
		state = START;
	    } else if (token == EOC || token == CMD) {
		state = END;
	    } else if (token == IDENT) {
		token_get(lp,word);
	    	state = NUM1;
		/* check to see if is a valid comp descriptor */
		valid_comp=0;
		if ((comp = is_comp(toupper(word[0])))) {
		    if (strlen(word) == 1) {
			/* my_layer = default_layer(); */
			/* printf("using default layer=%d\n",my_layer); */
			valid_comp++;	/* no layer given */
		    } else {
			valid_comp++;
			/* check for any non-digit characters */
			/* to allow instance names like "L1234b" */
			for (i=0; i<strlen(&word[1]); i++) {
			    if (!isdigit(word[1+i])) {
				valid_comp=0;
			    }
			}
			if (valid_comp) {
			    if(sscanf(&word[1], "%d", &my_layer) == 1) {
				if (debug) printf("given layer=%d\n",my_layer);
			    } else {
				valid_comp=0;
			    }
			} 
			if (valid_comp) {
			    if (my_layer > MAX_LAYER) {
				printf("layer must be less than %d\n",
				    MAX_LAYER);
				valid_comp=0;
				done++;
			    }
			    if (!show_check_modifiable(currep, comp, my_layer)) {
				printf("layer %d is not modifiable!\n",
				    my_layer);
				token_flush_EOL(lp);
				valid_comp=0;
				done++;
			    }
			}
		    }
		} else { 
		    /* here need to handle a valid cell name */
		    printf("looks like a descriptor to me: %s\n", word);
		}
	    } else {
		token_err("MOVE", lp, "expected DESC or NUMBER", token);
		state = END;	/* error */
	    }
	    break;
	case NUM1:		/* get pair of xy coordinates */
	    if (debug) printf("in NUM1\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x1);	/* scan it in */
		state = COM1;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM1:		
	    if (debug) printf("in COM1\n");
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM2;
	    } else {
		token_err("MOVE", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM2:
	    if (debug) printf("in NUM2\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y1);	/* scan it in */

		if (mode == POINT) {
		    /* printf("calling db_ident, 
		        %g %g 1 layer:%d comp:%d\n", x1, y1, my_layer, comp); */
		    if ((p_best=db_ident(currep, x1,y1,1,my_layer, comp, 0)) != NULL) {
			db_notate(p_best);	    /* print out id information */
			db_highlight(p_best);
			xmin=p_best->xmin;
			xmax=p_best->xmax;
			ymin=p_best->ymin;
			ymax=p_best->ymax;
			state = NUM5;
		    } else {
			printf("nothing here to move... try SHO command?\n");
			state = START;
		    }
                 } else {		           /* mode == REGION */
                    rubber_set_callback(move_draw_box);
                    state = NUM3; 
		 }
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("MOVE: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
        case NUM3:              /* get pair of xy coordinates */
            if (token == NUMBER) {
                token_get(lp,word);
                sscanf(word, "%lf", &x2);       /* scan it in */
                state = COM2;
            } else if (token == EOL) {
                token_get(lp,word);     /* just ignore it */
            } else if (token == EOC || token == CMD) {
                state = END;
            } else {
                token_err("IDENT", lp, "expected NUMBER", token);
                state = END;
            }
            break;
        case COM2:
            if (token == EOL) {
                token_get(lp,word); /* just ignore it */
            } else if (token == COMMA) {
                token_get(lp,word);
                state = NUM4;
            } else {
                token_err("IDENT", lp, "expected COMMA", token);
                state = END;
            }
            break;
        case NUM4:
            if (token == NUMBER) {
                token_get(lp,word);
                sscanf(word, "%lf", &y2);       /* scan it in */
		rubber_clear_callback();
		xmin=x1;
		xmax=x2;
		ymin=y1;
		ymax=y2;
                stack=db_ident_region(currep, x1,y1, x2, y2, 1, my_layer, comp, 0);
                state = NUM5;
            } else if (token == EOL) {
                token_get(lp,word);     /* just ignore it */
            } else if (token == EOC || token == CMD) {
                printf("IDENT: cancelling POINT\n");
                state = END;
            } else {
                token_err("IDENT", lp, "expected NUMBER", token);
                state = END;
            }
            break;

	case NUM5:
	    if (debug) printf("in NUM5\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x3);	/* scan it in */
		state = COM3;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM3:		
	    if (debug) printf("in COM3\n");
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM6;
	    } else {
		token_err("MOVE", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM6:
	    if (debug) printf("in NUM6\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y3);	/* scan it in */
		printf("got %g %g\n", x3, y3);
		rubber_set_callback(draw_bbox);
		state = NUM7;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("MOVE: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case NUM7:
	    if (debug) printf("in NUM7\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &x4);	/* scan it in */
		state = COM4;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = END;	
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case COM4:		
	    if (debug) printf("in COM4\n");
	    if (token == EOL) {
		token_get(lp,word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,word);
		state = NUM8;
	    } else {
		token_err("MOVE", lp, "expected COMMA", token);
	        state = END;
	    }
	    break;
	case NUM8:
	    if (debug) printf("in NUM8\n");
	    if (token == NUMBER) {
		token_get(lp,word);
		sscanf(word, "%lf", &y4);	/* scan it in */
		printf("got %g %g\n", x4, y4);
		rubber_clear_callback();
		if (mode == POINT) {
		    db_move_component(p_best, x4-x3, y4-y3);
		} else {
		    while ((p_best = (DB_DEFLIST *) stack_pop(&stack))!=NULL) {
			db_move_component(p_best, x4-x3, y4-y3);
		    }
		} 				/* mode == REGION */
		currep->modified++;
		need_redraw++;
		state = START;
	    } else if (token == EOL) {
		token_get(lp,word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		printf("MOVE: cancelling POINT\n");
	        state = END;
	    } else {
		token_err("MOVE", lp, "expected NUMBER", token);
		state = END; 
	    }
	    break;
	case END:
	default:
	    if (token == EOC || token == CMD) {
		;
	    } else {
		token_flush_EOL(lp);
	    }
	    done++;
	    rubber_clear_callback();
	    break;
	}
    }
    rl_restoreprompt();
    return(1);
}


/*

    if (!done) {
	if (valid_comp) {
	} else { 
	    if ((strlen(word) == 1) && toupper(word[0]) == 'I') {
		if((token=token_get(lp,word)) != IDENT) {
		    printf("DEL INST: bad inst name: %s\n", word);
		    done++;
		}
	    }
	    if (debug) printf("calling del_inst with %s\n", word);
	    if (!show_check_modifiable(currep, INST, my_layer)) {
		    printf("INST component is not modifiable!\n");
	    } else {
		;
	    }
	}
    }
} else if (token == QUOTE) {
    if (!show_check_modifiable(currep, INST, my_layer)) {
	    printf("INST component is not modifiable!\n");
    } else {
	;
*/

void draw_bbox(x, y, count)
double x, y;  /* offset */
int count; /* number of times called */
{
	static double x1old, x2old, y1old, y2old;
	static double xx1, yy1, xx2, yy2;
	BOUNDS bb;
	bb.init=0;

	xx1=xmin+x-x3;
	yy1=ymin+y-y3;
	xx2=xmax+x-x3;
	yy2=ymax+y-y3;

	if (count == 0) {		/* first call */
	    db_drawbounds(xx1, yy1, xx2, yy2, D_RUBBER);
	} else if (count > 0) {		/* intermediate calls */
	    db_drawbounds(x1old, y1old, x2old, y2old, D_RUBBER); 	/* erase old shape */
	    db_drawbounds(xx1, yy1, xx2, yy2, D_RUBBER);
	} else {			/* last call, cleanup */
	    db_drawbounds(x1old, y1old, x2old, y2old, D_RUBBER); 	/* erase old shape */
	}

	/* save old values */
	x1old=xx1;
	y1old=yy1;
	x2old=xx2;
	y2old=yy2;
	jump(&bb, D_RUBBER);
}


void move_draw_box(x2, y2, count)
double x2, y2;
int count; /* number of times called */
{
        static double x1old, x2old, y1old, y2old;
        BOUNDS bb;
        bb.init=0;

        if (count == 0) {               /* first call */
            db_drawbounds(x1,y1,x2,y2,D_RUBBER);                /* draw new shape */
        } else if (count > 0) {         /* intermediate calls */
            db_drawbounds(x1old,y1old,x2old,y2old,D_RUBBER);    /* erase old shape */
            db_drawbounds(x1,y1,x2,y2,D_RUBBER);                /* draw new shape */
        } else {                        /* last call, cleanup */
            db_drawbounds(x1old,y1old,x2old,y2old,D_RUBBER);    /* erase old shape */
        }

        /* save old values */
        x1old=x1;
        y1old=y1;
        x2old=x2;
        y2old=y2;
        jump(&bb, D_RUBBER);
}

