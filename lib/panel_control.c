/** @brief Razion panel quick-settings control. */
#include <toaru/graphics.h>
#include <toaru/panel.h>
#include <toaru/yutani.h>

extern void launch_application(char * app);

static int draw_control(struct PanelWidget * widget, gfx_context_t * context) {
	panel_highlight_widget(widget, context, 0);
	uint32_t color = widget->highlighted ? widget->pctx->color_text_hilighted : widget->pctx->color_icon_normal;
	for (int i = 0; i < 3; ++i) {
		int y = 9 + i * 6;
		draw_rounded_rectangle(context, 10, y, 18, 2, 1, color);
		int knob = i == 0 ? 20 : i == 1 ? 14 : 23;
		draw_rounded_rectangle(context, knob, y - 2, 5, 6, 2, color);
	}
	return 0;
}

static int click_control(struct PanelWidget * widget, struct yutani_msg_window_mouse_event * event) {
	launch_application("/bin/quick-settings");
	return 1;
}

struct PanelWidget * widget_init_control(void) {
	struct PanelWidget * widget = widget_new();
	widget->width = 38;
	widget->draw = draw_control;
	widget->click = click_control;
	list_insert(widgets_enabled, widget);
	return widget;
}
