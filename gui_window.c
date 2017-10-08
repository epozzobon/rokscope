#include "rokscope.h"


void gl_area_realize(GtkWidget *widget, gpointer user_data) {
	GtkGLArea *area = GTK_GL_AREA(widget);
	gtk_gl_area_make_current(area);

	state_t *s = user_data;
	s->gloscope = zalloc(sizeof(*s->gloscope));
	if (gtk_gl_area_get_error(area) != NULL) {
		fprintf(stderr, "gl area error\n");
		return;
	}
	gloscope_init(s->gloscope, s->num_channels, 1024);
}


void gl_area_unrealize(GtkWidget *widget, gpointer user_data) {
	GtkGLArea *area = GTK_GL_AREA(widget);
	gtk_gl_area_make_current(area);

	state_t *s = user_data;
	UNUSED(s);
}


gboolean gl_area_render(GtkGLArea *area, GdkGLContext *ctx, gpointer user_data) {
	UNUSED(ctx);
	gtk_gl_area_make_current(area);

	state_t *s = user_data;
	gtk_gl_area_queue_render(area);

	gloscope_render(s->gloscope);
	return TRUE;
}


void gl_area_resize(GtkGLArea *area, gint width, gint height, gpointer user_data) {
	UNUSED(user_data);
	gtk_gl_area_make_current(area);
	glViewport(0, 0, width, height);
}


void scale_skip_value_changed(GtkRange *range, gpointer user_data) {
	state_t *s = user_data;
	gdouble value = gtk_range_get_value(range);
	cmd_set_skip(s, (int) value);
}


void combo_samplerate_changed(GtkComboBoxText *widget, gpointer user_data) {
	state_t *s = user_data;
	gchar *value_str = gtk_combo_box_text_get_active_text(widget);
	uint64_t value = strtoull(value_str, NULL, 0);
	cmd_set_samplerate(s, value);
}

GtkWidget *make_sample_rate_control(struct state *s) {
	GVariant *gvar;
	GtkWidget *combo_samplerate = gtk_combo_box_text_new();
	int res = sr_config_list(s->driver, s->device, NULL, SR_CONF_SAMPLERATE, &gvar);
	if (res == SR_OK) {
		GVariantDict *dict = g_variant_dict_new(gvar);
		GVariant *samplerates = g_variant_dict_lookup_value(dict, "samplerates", G_VARIANT_TYPE_ARRAY);
		g_variant_dict_unref(dict);
		gsize num_samplerates = g_variant_n_children(samplerates);
		for (gsize i = 0; i < num_samplerates; i++) {
			GVariant *rate_var = g_variant_get_child_value(samplerates, i);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_samplerate), g_variant_print(rate_var, FALSE));
		}
	}
	g_signal_connect(combo_samplerate, "changed", G_CALLBACK(combo_samplerate_changed), s);
	return combo_samplerate;
}


GtkWidget *make_skip_control(struct state *s) {
	GtkWidget *scale_skip = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 2048, 1);
	gtk_widget_set_size_request(scale_skip, 300, 1);
	g_signal_connect(scale_skip, "value-changed", G_CALLBACK(scale_skip_value_changed), s);
	return scale_skip;
}

GtkWindow *gui_create(struct state *s) {
	GtkWidget *window;


	window = gtk_application_window_new(s->application);
	gtk_window_set_title(GTK_WINDOW(window), "Window");
	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);


	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_container_add(GTK_CONTAINER(window), box);


	GtkWidget *gl_area = gtk_gl_area_new();
	g_signal_connect(gl_area, "realize", G_CALLBACK(gl_area_realize), s);
	g_signal_connect(gl_area, "unrealize", G_CALLBACK(gl_area_unrealize), s);
	g_signal_connect(gl_area, "render", G_CALLBACK(gl_area_render), s);
	g_signal_connect(gl_area, "resize", G_CALLBACK(gl_area_resize), s);
	gtk_widget_set_hexpand(gl_area, TRUE);
	gtk_widget_set_vexpand(gl_area, TRUE);
	gtk_container_add(GTK_CONTAINER(box), gl_area);


	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add(GTK_CONTAINER(box), vbox);
	
	
	GtkWidget *grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(vbox), grid);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Running"), 0, 0, 1, 1);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Skip"), 0, 1, 1, 1);
	GtkWidget *scale_skip = make_skip_control(s);
	gtk_grid_attach(GTK_GRID(grid), scale_skip, 1, 1, 1, 1);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Sample rate"), 0, 2, 1, 1);
	GtkWidget *combo_samplerate = make_sample_rate_control(s);
	gtk_grid_attach(GTK_GRID(grid), combo_samplerate, 1, 2, 1, 1);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Samples limit"), 0, 4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Trigger mode"), 0, 5, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Trigger level"), 0, 6, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Trigger channel"), 0, 7, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Horizontal scale"), 0, 8, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Vertical scale"), 0, 9, 1, 1);


	for (int g = 0; g < s->num_channel_groups; g++) {
		struct sr_channel_group *ch_grp = s->chgroups[g];
		GtkWidget *ch_grid = gtk_grid_new();
		gtk_container_add(GTK_CONTAINER(vbox), ch_grid);
		gtk_grid_attach(GTK_GRID(ch_grid), gtk_label_new(ch_grp->name), 0, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(ch_grid), gtk_label_new("Coupling"), 0, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(ch_grid), gtk_label_new("VDiv"), 0, 2, 1, 1);
	}


	gtk_widget_show_all(window);
	return (GtkWindow *) window;
}

