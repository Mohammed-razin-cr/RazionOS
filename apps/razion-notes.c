/** @brief Lightweight native Razion Notes editor. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define BUFFER_SIZE 16384
static yutani_t * yctx; static yutani_window_t * window; static gfx_context_t * ctx;
static struct TT_Font * font,*bold; static char text[BUFFER_SIZE],path[1024];
static size_t length; static int running=1,modified;

static void load_note(void) {
	const char * home=getenv("HOME"); char documents[512];
	if (!home) { strcpy(path,"/tmp/Notes.txt"); return; }
	snprintf(documents,sizeof(documents),"%s/Documents",home); if (mkdir(documents,0755)&&errno!=EEXIST) return;
	snprintf(path,sizeof(path),"%s/Notes.txt",documents); FILE * file=fopen(path,"r"); if (!file) return;
	length=fread(text,1,sizeof(text)-1,file); text[length]='\0'; fclose(file);
}
static void save_note(void) { FILE * file=fopen(path,"w"); if(!file)return; fwrite(text,1,length,file); fclose(file); modified=0; }
static void line(int x,int y,const char * value,size_t count,uint32_t color) {
	char shown[256]; if(count>=sizeof(shown))count=sizeof(shown)-1; memcpy(shown,value,count); shown[count]='\0';
	char * cropped=tt_ellipsify(shown,14,font,window->width-100,NULL);
	tt_draw_string(ctx,font,x,y,cropped,color);
	free(cropped);
}
static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window,&b); draw_fill(ctx,RAZION_BACKGROUND);
	int x=b.left_width+28,top=b.top_height; tt_set_size(bold,22); tt_draw_string(ctx,bold,x,top+38,"Notes",RAZION_TEXT_PRIMARY);
	tt_set_size(font,11); tt_draw_string(ctx,font,x,top+59,"Ctrl+S saves  •  Plain text stays on this device",RAZION_TEXT_SECONDARY);
	draw_rounded_rectangle(ctx,x,top+78,window->width-b.width-56,window->height-b.height-126,8,RAZION_SURFACE);
	int y=top+106,max_y=window->height-b.bottom_height-58; const char * start=text; const char * cursor_start=text;
	tt_set_size(font,14);
	for(const char * p=text;;++p) if(*p=='\n'||!*p){ if(y<=max_y)line(x+18,y,start,p-start,RAZION_TEXT_PRIMARY); y+=22; if(!*p){cursor_start=start;break;} start=p+1; }
	if(window->focused&&y<=max_y+22) draw_rectangle_solid(ctx,x+18+tt_string_width(font,cursor_start),y-17,2,18,RAZION_ACCENT);
	char state[96]; snprintf(state,sizeof(state),"Notes.txt%s",modified?"  •  Unsaved":"  •  Saved");
	tt_set_size(font,10); tt_draw_string(ctx,font,x,window->height-b.bottom_height-20,state,modified?RAZION_WARNING:RAZION_TEXT_SECONDARY);
	render_decorations(window,ctx,"Razion Notes"); flip(ctx); yutani_flip(yctx,window);
}
static void resize_finish(unsigned w,unsigned h){if(w<520)w=520;if(h<360)h=360;yutani_window_resize_accept(yctx,window,w,h);reinit_graphics_yutani(ctx,window);yutani_window_resize_done(yctx,window);redraw();}
int main(void){yctx=yutani_init();if(!yctx)return 1;init_decorations();struct decor_bounds b;decor_get_bounds(NULL,&b);
	window=yutani_window_create(yctx,720+b.width,500+b.height);yutani_window_move(yctx,window,(yctx->display_width-window->width)/2,(yctx->display_height-window->height)/2);
	yutani_window_advertise_icon(yctx,window,"Razion Notes","razion-notes");ctx=init_graphics_yutani_double_buffer(window);font=tt_font_from_shm("sans-serif");bold=tt_font_from_shm("sans-serif.bold");load_note();redraw();
	while(running){yutani_msg_t*m=yutani_poll(yctx);if(!m)continue;switch(m->type){case YUTANI_MSG_KEY_EVENT:{struct yutani_msg_key_event*k=(void*)m->data;if(k->wid!=window->wid||k->event.action!=KEY_ACTION_DOWN)break;
		if((k->event.modifiers&(KEY_MOD_LEFT_CTRL|KEY_MOD_RIGHT_CTRL))&&k->event.keycode=='s')save_note();
		else if(k->event.keycode==KEY_ESCAPE)running=0;else if((k->event.key=='\b'||k->event.keycode==KEY_BACKSPACE)&&length){text[--length]='\0';modified=1;}
		else if((k->event.key=='\n'||(k->event.key>=0x20&&k->event.key<0x7f))&&length+1<sizeof(text)){text[length++]=k->event.key;text[length]='\0';modified=1;}
		redraw();break;}
	case YUTANI_MSG_WINDOW_MOUSE_EVENT:if(((struct yutani_msg_window_mouse_event*)m->data)->wid==window->wid&&decor_handle_event(yctx,m)==DECOR_CLOSE)running=0;break;
	case YUTANI_MSG_RESIZE_OFFER:{struct yutani_msg_window_resize*r=(void*)m->data;if(r->wid==window->wid)resize_finish(r->width,r->height);break;}
	case YUTANI_MSG_WINDOW_FOCUS_CHANGE:{struct yutani_msg_window_focus_change*f=(void*)m->data;if(f->wid==window->wid){window->focused=f->focused;redraw();}break;}
	case YUTANI_MSG_WINDOW_CLOSE:case YUTANI_MSG_SESSION_END:running=0;}free(m);}if(modified)save_note();yutani_close(yctx,window);return 0;}
