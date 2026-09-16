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
static int running=1, hover=-1, pressed=-1, focus=12;
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

static void card(int id,int x,int y,int width,int height,const char * title,const char * detail,int status_ok,int interactive) {
	uint32_t fill=interactive && id==pressed?RAZION_SELECTION:interactive && id==hover?RAZION_SURFACE_HOVER:RAZION_SURFACE;
	if (interactive && id==focus && window->focused)
		draw_rounded_rectangle(ctx,x-2,y-2,width+4,height+4,10,RAZION_FOCUS);
	draw_rounded_rectangle(ctx,x,y,width,height,8,fill);
	draw_rectangle_solid(ctx,x+10,y+height-1,width-20,1,status_ok?RAZION_ACCENT:RAZION_BORDER);
	label(x+16,y+25,14,title,interactive?RAZION_TEXT_PRIMARY:RAZION_TEXT_SECONDARY,1);
	label(x+16,y+47,11,detail,status_ok?RAZION_TEXT_SECONDARY:RAZION_WARNING,0);
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window,&b); draw_fill(ctx,RAZION_BACKGROUND);
	int x=b.left_width+28,top=b.top_height; label(x,top+42,24,"Quick Settings",RAZION_TEXT_PRIMARY,1);
	label(x,top+64,11,"Controls available on this RazionOS session",RAZION_TEXT_SECONDARY,0);
	card(10,x,top+88,212,70,"Network",network_available?"Interface available":"No interface available",network_available,0);
	char volume[64]; snprintf(volume,sizeof(volume),mixer>=0?"Volume %d%%  •  activate to raise":"Mixer unavailable",volume_percent);
	card(11,x+228,top+88,212,70,"Audio",volume,mixer>=0,mixer>=0);
	card(12,x,top+174,212,70,"Appearance",theme_light?"Light theme":"Dark theme",1,1);
	card(13,x+228,top+174,212,70,"Do Not Disturb",dnd?"Notifications paused":"Notifications enabled",1,1);
	card(14,x,top+260,212,70,"Settings","Open full settings",1,1);
	card(15,x+228,top+260,212,70,"Power","Privileged restart",1,1);
	label(x,window->height-b.bottom_height-24,10,"Tab/Arrows navigate  •  Enter activates  •  unavailable controls stay disabled",RAZION_TEXT_SECONDARY,0);
	render_decorations(window,ctx,"Razion Quick Settings"); flip(ctx); yutani_flip(yctx,window);
}

static int hit(int x,int y) {
	struct decor_bounds b; decor_get_bounds(window,&b); int left=b.left_width+28,top=b.top_height;
	for (int i=1;i<6;++i) { int id=10+i; if (id==11 && mixer<0) continue;
		int bx=left+(i%2)*228,by=top+88+(i/2)*86; if (x>=bx&&x<bx+212&&y>=by&&y<by+70) return id; }
	return -1;
}

static void move_focus(int direction) {
	int ids[5],count=0;
	if (mixer>=0) ids[count++]=11;
	for (int id=12;id<=15;++id) ids[count++]=id;
	int current=0;
	for (int i=0;i<count;++i) if (ids[i]==focus) { current=i; break; }
	focus=ids[(current+direction+count)%count];
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
			case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * k=(void*)msg->data;
				if(k->wid!=window->wid||k->event.action!=KEY_ACTION_DOWN) break;
				int changed=0;
				if(k->event.keycode==KEY_ESCAPE) running=0;
				else if(k->event.keycode=='\t') { move_focus(k->event.modifiers&(KEY_MOD_LEFT_SHIFT|KEY_MOD_RIGHT_SHIFT)?-1:1); changed=1; }
				else if(k->event.keycode==KEY_ARROW_LEFT||k->event.keycode==KEY_ARROW_UP) { move_focus(-1); changed=1; }
				else if(k->event.keycode==KEY_ARROW_RIGHT||k->event.keycode==KEY_ARROW_DOWN) { move_focus(1); changed=1; }
				else if(k->event.key=='\n'||k->event.key==' ') { activate(focus); changed=1; }
				if(changed) redraw();
				break; }
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: { struct yutani_msg_window_mouse_event * m=(void*)msg->data; if(m->wid!=window->wid) break;
				int decor=decor_handle_event(yctx,msg); if(decor==DECOR_CLOSE) running=0;
				int old_hover=hover,old_pressed=pressed,activated=0;
				int over=hit(m->new_x,m->new_y);
				if(m->command==YUTANI_MOUSE_EVENT_DOWN) { pressed=over; if(over>=0) focus=over; } else if(m->command==YUTANI_MOUSE_EVENT_LEAVE){hover=-1;pressed=-1;}
				else if(m->command==YUTANI_MOUSE_EVENT_RAISE||m->command==YUTANI_MOUSE_EVENT_CLICK){if(over>=0&&over==pressed){activate(over);activated=1;}pressed=-1;}
				hover=over;
				if(decor==DECOR_REDRAW||old_hover!=hover||old_pressed!=pressed||activated) redraw();
				break; }
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * f=(void*)msg->data; if(f->wid==window->wid){window->focused=f->focused;redraw();} break; }
			case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running=0;
		}
		free(msg);
	}
	if(mixer>=0)close(mixer);
	yutani_close(yctx,window);
	return 0;
}
