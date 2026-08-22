/** @brief Compact RazionOS control center with hardware-backed controls only. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel/mod/sound.h>
#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WIDTH 500
#define HEIGHT 430
#define VOLUME_DEVICE 0
#define VOLUME_KNOB 0

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running=1, hover=-1, pressed=-1;
static int network_available, mixer=-1, volume_percent, theme_light, dnd;
static char accent[16]="teal";

static int ensure_config(char * directory, size_t size) {
	const char * home=getenv("HOME");
	if (!home || snprintf(directory,size,"%s/.razion",home)>=(int)size) return -1;
	return mkdir(directory,0700) && errno!=EEXIST ? -1 : 0;
}

static void read_state(void) {
	DIR * dir=opendir("/dev/net"); network_available=0;
	if (dir) { struct dirent * entry; while ((entry=readdir(dir))) if (entry->d_name[0]!='.') { network_available=1; break; } closedir(dir); }
	mixer=open("/dev/mixer",O_RDONLY|O_CLOEXEC);
	if (mixer>=0) { snd_knob_value_t value={0}; value.device=VOLUME_DEVICE; value.id=VOLUME_KNOB;
		if (!ioctl(mixer,SND_MIXER_READ_KNOB,&value)) volume_percent=(int)((unsigned long long)value.val*100ULL/0xFC000000ULL); }
	char directory[512],path[1024],line[64]; if (ensure_config(directory,sizeof(directory))) return;
	snprintf(path,sizeof(path),"%s/theme.conf",directory); FILE * file=fopen(path,"r");
	if (file) { while (fgets(line,sizeof(line),file)) { if (!strncmp(line,"theme=light",11)) theme_light=1;
		else if (!strncmp(line,"accent=",7)) sscanf(line+7,"%15s",accent); } fclose(file); }
	snprintf(path,sizeof(path),"%s/notifications.conf",directory); file=fopen(path,"r");
	if (file) { while (fgets(line,sizeof(line),file)) if (!strncmp(line,"dnd=1",5)) dnd=1; fclose(file); }
}

static void write_theme(void) {
	char directory[512],path[1024]; if (ensure_config(directory,sizeof(directory))) return;
	snprintf(path,sizeof(path),"%s/theme.conf",directory); FILE * file=fopen(path,"w"); if (!file) return;
	fprintf(file,"theme=%s\naccent=%s\n",theme_light?"light":"dark",accent); fclose(file);
}

static void write_dnd(void) {
	char directory[512],path[1024]; if (ensure_config(directory,sizeof(directory))) return;
	snprintf(path,sizeof(path),"%s/notifications.conf",directory); FILE * file=fopen(path,"w"); if (!file) return;
	fprintf(file,"dnd=%d\n",dnd); fclose(file);
}

static void set_volume(int amount) {
	if (mixer<0) return;
	volume_percent+=amount;
	if (volume_percent<0) volume_percent=0;
	if (volume_percent>100) volume_percent=100;
	snd_knob_value_t value={0}; value.device=VOLUME_DEVICE; value.id=VOLUME_KNOB;
	value.val=(unsigned long long)volume_percent*0xFC000000ULL/100ULL; ioctl(mixer,SND_MIXER_WRITE_KNOB,&value);
}

static void launch(const char * executable,const char * first) {
	if (fork()) return;
	char * args[]={(char*)executable,(char*)first,NULL};
	execv(executable,args);
	_Exit(127);
}

static void label(int x,int y,int size,const char * text,uint32_t color,int strong) {
	tt_set_size(strong?bold:font,size); tt_draw_string(ctx,strong?bold:font,x,y,text,color);
}

static void card(int id,int x,int y,int width,int height,const char * title,const char * detail,int enabled) {
	uint32_t fill=id==pressed?RAZION_SELECTION:id==hover?RAZION_SURFACE_HOVER:RAZION_SURFACE;
	draw_rounded_rectangle(ctx,x,y,width,height,8,fill); draw_rectangle_solid(ctx,x+10,y+height-1,width-20,1,enabled?RAZION_ACCENT:RAZION_BORDER);
	label(x+16,y+25,14,title,RAZION_TEXT_PRIMARY,1); label(x+16,y+47,11,detail,enabled?RAZION_TEXT_SECONDARY:RAZION_WARNING,0);
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window,&b); draw_fill(ctx,RAZION_BACKGROUND);
	int x=b.left_width+28,top=b.top_height; label(x,top+42,24,"Quick Settings",RAZION_TEXT_PRIMARY,1);
	label(x,top+64,11,"Controls available on this RazionOS session",RAZION_TEXT_SECONDARY,0);
	card(10,x,top+88,212,70,"Network",network_available?"Interface available":"No interface available",network_available);
	char volume[64]; snprintf(volume,sizeof(volume),mixer>=0?"Volume %d%%  •  click to raise":"Mixer unavailable",volume_percent);
	card(11,x+228,top+88,212,70,"Audio",volume,mixer>=0);
	card(12,x,top+174,212,70,"Appearance",theme_light?"Light theme":"Dark theme",1);
	card(13,x+228,top+174,212,70,"Do Not Disturb",dnd?"Notifications paused":"Notifications enabled",dnd);
	card(14,x,top+260,212,70,"Settings","Open full settings",1);
	card(15,x+228,top+260,212,70,"Power","Privileged restart",1);
	label(x,window->height-b.bottom_height-24,10,"Bluetooth, brightness, and airplane mode are hidden when no working backend exists.",RAZION_TEXT_SECONDARY,0);
	render_decorations(window,ctx,"Razion Quick Settings"); flip(ctx); yutani_flip(yctx,window);
}

static int hit(int x,int y) {
	struct decor_bounds b; decor_get_bounds(window,&b); int left=b.left_width+28,top=b.top_height;
	for (int i=0;i<6;++i) { int bx=left+(i%2)*228,by=top+88+(i/2)*86; if (x>=bx&&x<bx+212&&y>=by&&y<by+70) return 10+i; }
	return -1;
}

static void activate(int id) {
	if (id==11 && mixer>=0) set_volume(10);
	else if (id==12) { theme_light=!theme_light; write_theme(); }
	else if (id==13) { dnd=!dnd; write_dnd(); }
	else if (id==14) launch("/bin/settings",NULL);
	else if (id==15) launch("/bin/gsudo","reboot");
}

int main(void) {
	yctx=yutani_init(); if (!yctx) return 1; init_decorations(); struct decor_bounds b; decor_get_bounds(NULL,&b);
	window=yutani_window_create(yctx,WIDTH+b.width,HEIGHT+b.height); if (!window) return 1;
	window->decorator_flags|=DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx,window,yctx->display_width-window->width-28,42);
	yutani_window_advertise_icon(yctx,window,"Razion Quick Settings","razion-settings"); ctx=init_graphics_yutani_double_buffer(window);
	font=tt_font_from_shm("sans-serif"); bold=tt_font_from_shm("sans-serif.bold"); read_state(); redraw();
	while (running) { yutani_msg_t * msg=yutani_poll(yctx); if (!msg) continue;
		switch(msg->type) {
			case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * k=(void*)msg->data; if(k->wid==window->wid&&k->event.action==KEY_ACTION_DOWN&&k->event.keycode==KEY_ESCAPE) running=0; break; }
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: { struct yutani_msg_window_mouse_event * m=(void*)msg->data; if(m->wid!=window->wid) break;
				if(decor_handle_event(yctx,msg)==DECOR_CLOSE) running=0;
				int over=hit(m->new_x,m->new_y);
				if(m->command==YUTANI_MOUSE_EVENT_DOWN) pressed=over; else if(m->command==YUTANI_MOUSE_EVENT_LEAVE){hover=-1;pressed=-1;}
				else if(m->command==YUTANI_MOUSE_EVENT_RAISE||m->command==YUTANI_MOUSE_EVENT_CLICK){if(over==pressed)activate(over);pressed=-1;}
				hover=over; redraw(); break; }
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * f=(void*)msg->data; if(f->wid==window->wid){window->focused=f->focused;redraw();} break; }
			case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running=0;
		}
		free(msg);
	}
	if(mixer>=0)close(mixer);
	yutani_close(yctx,window);
	return 0;
}
