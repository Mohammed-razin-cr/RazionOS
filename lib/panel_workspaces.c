/**
 * @brief RazionOS virtual workspace indicator and switcher
 */
#include <stdio.h>
#include <toaru/yutani.h>
#include <toaru/graphics.h>
#include <toaru/text.h>
#include <toaru/panel.h>

#define CELL_WIDTH 26
#define CELL_GAP 3
#define MAX_WORKSPACES 8

static int widget_draw_workspaces(struct PanelWidget * this, gfx_context_t * ctx) {
	uint32_t count = panel_workspace_count;
	if (count < 1) count = 1;
	if (count > MAX_WORKSPACES) count = MAX_WORKSPACES;

	for (uint32_t i = 0; i < count; ++i) {
		int x = i * (CELL_WIDTH + CELL_GAP);
		int active = i == panel_active_workspace;
		uint32_t fill = active ? this->pctx->color_widget_bg_active : this->pctx->color_widget_bg_base;
		draw_rounded_rectangle(ctx, x, 5, CELL_WIDTH, ctx->height - 10, 5, fill);
		if (active) draw_rectangle(ctx, x + 6, ctx->height - 3, CELL_WIDTH - 12, 2, this->pctx->color_special);

		char label[4];
		snprintf(label, sizeof(label), "%u", i + 1);
		tt_set_size(this->pctx->font_bold, 13);
		int width = tt_string_width(this->pctx->font_bold, label);
		tt_draw_string(ctx, this->pctx->font_bold, x + (CELL_WIDTH - width) / 2, 20,
			label, active ? this->pctx->color_text_focused : this->pctx->color_text_normal);
	}
	return 0;
}

static int widget_click_workspaces(struct PanelWidget * this, struct yutani_msg_window_mouse_event * evt) {
	int local_x = evt->new_x - this->left;
	if (local_x < 0) return 0;
	uint32_t target = local_x / (CELL_WIDTH + CELL_GAP);
	uint32_t within = local_x % (CELL_WIDTH + CELL_GAP);
	if (within < CELL_WIDTH && target < panel_workspace_count) {
		yutani_workspace_switch(yctx, target);
		return 1;
	}
	return 0;
}

struct PanelWidget * widget_init_workspaces(void) {
	struct PanelWidget * widget = widget_new();
	widget->width = 4 * (CELL_WIDTH + CELL_GAP) - CELL_GAP;
	widget->draw = widget_draw_workspaces;
	widget->click = widget_click_workspaces;
	list_insert(widgets_enabled, widget);
	return widget;
}
