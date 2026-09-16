/** @brief Native RazionOS month calendar. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

static const char * months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
static const char * days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static yutani_t*yctx;static yutani_window_t*window;static gfx_context_t*ctx;static struct TT_Font*font,*bold;
static int year,month,today_year,today_month,today_day,running=1,hover,pressed,focus_id=1;
static int days_in_month(int y,int m){static const int count[]={31,28,31,30,31,30,31,31,30,31,30,31};if(m==1&&((y%4==0&&y%100!=0)||y%400==0))return 29;return count[m];}
static void change_month(int delta){month+=delta;if(month<0){month=11;year--;}if(month>11){month=0;year++;}}
static void redraw(void){struct decor_bounds b;decor_get_bounds(window,&b);draw_fill(ctx,RAZION_BACKGROUND);int left=b.left_width+30,top=b.top_height;
	char title[64];snprintf(title,sizeof(title),"%s %d",months[month],year);razion_draw_accent_bar(ctx,left,top+21,76);tt_set_size(bold,24);tt_draw_string(ctx,bold,left,top+44,title,RAZION_TEXT_PRIMARY);
	int previous=window->width-b.right_width-98,next=window->width-b.right_width-58;unsigned ps=hover==1?RAZION_CONTROL_HOVER:0,ns=hover==2?RAZION_CONTROL_HOVER:0;if(pressed==1)ps|=RAZION_CONTROL_PRESSED;if(pressed==2)ns|=RAZION_CONTROL_PRESSED;if(window->focused&&focus_id==1)ps|=RAZION_CONTROL_FOCUSED;if(window->focused&&focus_id==2)ns|=RAZION_CONTROL_FOCUSED;
	razion_draw_button(ctx,bold,previous,top+18,32,32,"‹",ps);razion_draw_button(ctx,bold,next,top+18,32,32,"›",ns);
	int usable=window->width-b.width-60,cell=usable/7,grid_top=top+88;tt_set_size(bold,11);
	for(int i=0;i<7;i++){int tw=tt_string_width(bold,days[i]);tt_draw_string(ctx,bold,left+i*cell+(cell-tw)/2,grid_top,days[i],RAZION_TEXT_SECONDARY);}
	struct tm first={0};first.tm_year=year-1900;first.tm_mon=month;first.tm_mday=1;mktime(&first);int offset=first.tm_wday,count=days_in_month(year,month);
	for(int d=1;d<=count;d++){int index=offset+d-1,row=index/7,col=index%7,cx=left+col*cell,cy=grid_top+26+row*52;
		int is_today=year==today_year&&month==today_month&&d==today_day;if(is_today)draw_rounded_rectangle(ctx,cx+cell/2-18,cy+5,36,36,18,RAZION_ACCENT);
		char number[16];snprintf(number,sizeof(number),"%d",d);tt_set_size(is_today?bold:font,14);int tw=tt_string_width(is_today?bold:font,number);
		tt_draw_string(ctx,is_today?bold:font,cx+(cell-tw)/2,cy+29,number,is_today?RAZION_BACKGROUND:RAZION_TEXT_PRIMARY);}
	tt_set_size(font,10);tt_draw_string(ctx,font,left,window->height-b.bottom_height-20,"Use Left/Right to change month  •  T returns to today",RAZION_TEXT_SECONDARY);
	render_decorations(window,ctx,"Razion Calendar");flip(ctx);yutani_flip(yctx,window);}
static int hit(int x,int y){struct decor_bounds b;decor_get_bounds(window,&b);int top=b.top_height,right=(int)window->width-b.right_width;if(y>=top+18&&y<top+50){if(x>=right-98&&x<right-66)return 1;if(x>=right-58&&x<right-26)return 2;}return 0;}
int main(void){time_t now=time(NULL);struct tm*local=localtime(&now);year=today_year=local?local->tm_year+1900:2026;month=today_month=local?local->tm_mon:0;today_day=local?local->tm_mday:1;
	yctx=yutani_init();if(!yctx)return 1;init_decorations();struct decor_bounds b;decor_get_bounds(NULL,&b);window=yutani_window_create(yctx,620+b.width,450+b.height);window->decorator_flags|=DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx,window,(yctx->display_width-window->width)/2,(yctx->display_height-window->height)/2);yutani_window_advertise_icon(yctx,window,"Razion Calendar","razion-calendar");ctx=init_graphics_yutani_double_buffer(window);font=tt_font_from_shm("sans-serif");bold=tt_font_from_shm("sans-serif.bold");redraw();
	while(running){yutani_msg_t*m=yutani_poll(yctx);if(!m)continue;switch(m->type){case YUTANI_MSG_KEY_EVENT:{struct yutani_msg_key_event*k=(void*)m->data;if(k->wid==window->wid&&k->event.action==KEY_ACTION_DOWN){if(k->event.keycode==KEY_ESCAPE)running=0;else if(k->event.keycode==KEY_ARROW_LEFT){focus_id=1;change_month(-1);redraw();}else if(k->event.keycode==KEY_ARROW_RIGHT){focus_id=2;change_month(1);redraw();}else if(k->event.key=='t'||k->event.key=='T'){year=today_year;month=today_month;redraw();}else if(k->event.keycode=='\t'){focus_id=focus_id==1?2:1;redraw();}else if(k->event.key=='\n'){change_month(focus_id==1?-1:1);redraw();}}break;}
	case YUTANI_MSG_WINDOW_MOUSE_EVENT:{struct yutani_msg_window_mouse_event*e=(void*)m->data;if(e->wid!=window->wid)break;if(decor_handle_event(yctx,m)==DECOR_CLOSE)running=0;hover=hit(e->new_x,e->new_y);if(e->command==YUTANI_MOUSE_EVENT_DOWN){pressed=hover;if(hover)focus_id=hover;}else if(e->command==YUTANI_MOUSE_EVENT_LEAVE){hover=pressed=0;}else if(e->command==YUTANI_MOUSE_EVENT_CLICK||e->command==YUTANI_MOUSE_EVENT_RAISE){if(hover&&hover==pressed)change_month(hover==1?-1:1);pressed=0;}redraw();break;}
	case YUTANI_MSG_WINDOW_FOCUS_CHANGE:{struct yutani_msg_window_focus_change*f=(void*)m->data;if(f->wid==window->wid){window->focused=f->focused;redraw();}break;}case YUTANI_MSG_WINDOW_CLOSE:case YUTANI_MSG_SESSION_END:running=0;}free(m);}yutani_close(yctx,window);return 0;}
