/** @brief Native lightweight PCM media player front end. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

static yutani_t*yctx;static yutani_window_t*window;static gfx_context_t*ctx;static struct TT_Font*font,*bold;
static char path[512];static size_t path_length;static int running=1,editing=1,hover=-1,pressed=-1;static pid_t player;
static char status[128]="Enter a 48 kHz, 16-bit stereo PCM file path.";
static void stop_player(void){if(player>0){kill(player,SIGTERM);waitpid(player,NULL,WNOHANG);player=0;snprintf(status,sizeof(status),"Playback stopped.");}}
static void play(void){if(!path[0])return;stop_player();if(access(path,R_OK)){snprintf(status,sizeof(status),"File is not readable.");return;}player=fork();if(!player){char*args[]={"/bin/play",path,NULL};execv(args[0],args);_Exit(127);}snprintf(status,sizeof(status),"Playback started.");}
static void redraw(void){struct decor_bounds b;decor_get_bounds(window,&b);draw_fill(ctx,RAZION_BACKGROUND);int x=b.left_width+32,top=b.top_height;
	tt_set_size(bold,24);tt_draw_string(ctx,bold,x,top+44,"Media Player",RAZION_TEXT_PRIMARY);tt_set_size(font,11);tt_draw_string(ctx,font,x,top+66,"Native low-overhead PCM playback",RAZION_TEXT_SECONDARY);
	draw_rounded_rectangle(ctx,x,top+96,window->width-b.width-64,52,7,editing?RAZION_SURFACE_HOVER:RAZION_SURFACE);tt_set_size(font,13);
	char*shown=path[0]?path:"/home/local/Music/example.wav";char*cropped=tt_ellipsify(shown,13,font,window->width-b.width-96,NULL);tt_draw_string(ctx,font,x+16,top+128,cropped,path[0]?RAZION_TEXT_PRIMARY:RAZION_TEXT_SECONDARY);free(cropped);
	if(editing&&window->focused)draw_rectangle_solid(ctx,x+16+tt_string_width(font,path),top+108,2,24,RAZION_ACCENT);
	const char*names[]={"Play","Stop","Open Music folder"};for(int i=0;i<3;i++){int bx=x+i*148;uint32_t fill=i==pressed?RAZION_SELECTION:i==hover?RAZION_SURFACE_HOVER:RAZION_SURFACE_SECONDARY;
		draw_rounded_rectangle(ctx,bx,top+174,136,42,7,fill);tt_set_size(font,12);int tw=tt_string_width(font,names[i]);tt_draw_string(ctx,font,bx+(136-tw)/2,top+200,names[i],RAZION_TEXT_PRIMARY);}
	tt_set_size(font,11);tt_draw_string(ctx,font,x,top+254,status,RAZION_TEXT_SECONDARY);tt_draw_string(ctx,font,x,top+282,"Supported by the current audio backend: raw 48 kHz signed 16-bit stereo PCM.",RAZION_TEXT_SECONDARY);
	render_decorations(window,ctx,"Razion Media Player");flip(ctx);yutani_flip(yctx,window);}
static int hit(int x,int y){struct decor_bounds b;decor_get_bounds(window,&b);int left=b.left_width+32,top=b.top_height;if(y>=top+96&&y<top+148)return 10;for(int i=0;i<3;i++)if(x>=left+i*148&&x<left+i*148+136&&y>=top+174&&y<top+216)return i;return-1;}
static void activate(int id){if(id==0)play();else if(id==1)stop_player();else if(id==2){if(!fork()){const char*home=getenv("HOME");char music[512];snprintf(music,sizeof(music),"%s/Music",home?home:"");char*args[]={"/bin/file-browser",music,NULL};execv(args[0],args);_Exit(127);}}else if(id==10)editing=1;}
int main(void){yctx=yutani_init();if(!yctx)return 1;init_decorations();struct decor_bounds b;decor_get_bounds(NULL,&b);window=yutani_window_create(yctx,560+b.width,350+b.height);window->decorator_flags|=DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx,window,(yctx->display_width-window->width)/2,(yctx->display_height-window->height)/2);yutani_window_advertise_icon(yctx,window,"Razion Media Player","razion-media");ctx=init_graphics_yutani_double_buffer(window);font=tt_font_from_shm("sans-serif");bold=tt_font_from_shm("sans-serif.bold");redraw();
	while(running){if(player>0&&waitpid(player,NULL,WNOHANG)==player){player=0;snprintf(status,sizeof(status),"Playback finished.");redraw();}yutani_msg_t*m=yutani_poll(yctx);if(!m)continue;switch(m->type){case YUTANI_MSG_KEY_EVENT:{struct yutani_msg_key_event*k=(void*)m->data;if(k->wid!=window->wid||k->event.action!=KEY_ACTION_DOWN)break;if(k->event.keycode==KEY_ESCAPE)running=0;else if(editing&&(k->event.key=='\b'||k->event.keycode==KEY_BACKSPACE)&&path_length){path[--path_length]='\0';redraw();}else if(editing&&k->event.key=='\n'){editing=0;play();redraw();}else if(editing&&k->event.key>=0x20&&k->event.key<0x7f&&path_length+1<sizeof(path)){path[path_length++]=k->event.key;path[path_length]='\0';redraw();}break;}
	case YUTANI_MSG_WINDOW_MOUSE_EVENT:{struct yutani_msg_window_mouse_event*e=(void*)m->data;if(e->wid!=window->wid)break;if(decor_handle_event(yctx,m)==DECOR_CLOSE)running=0;int over=hit(e->new_x,e->new_y);if(e->command==YUTANI_MOUSE_EVENT_DOWN)pressed=over;else if(e->command==YUTANI_MOUSE_EVENT_LEAVE){hover=-1;pressed=-1;}else if(e->command==YUTANI_MOUSE_EVENT_CLICK||e->command==YUTANI_MOUSE_EVENT_RAISE){if(over==pressed)activate(over);pressed=-1;}hover=over;redraw();break;}
	case YUTANI_MSG_WINDOW_FOCUS_CHANGE:{struct yutani_msg_window_focus_change*f=(void*)m->data;if(f->wid==window->wid){window->focused=f->focused;redraw();}break;}case YUTANI_MSG_WINDOW_CLOSE:case YUTANI_MSG_SESSION_END:running=0;}free(m);}stop_player();yutani_close(yctx,window);return 0;}
