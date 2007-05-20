#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <config_frontend.h>

extern void print_status(char msg[]);

void frontend_set_signal(GtkWidget *widget, char* signal, void* func, int data)
{
	gtk_signal_connect(GTK_OBJECT(widget),	signal,	GTK_SIGNAL_FUNC(func),	GINT_TO_POINTER(data));
}

GtkWidget *frontend_create_window(int type, char *name)
{
	GtkWidget *window;

	print_status("creating window");
	print_status(name);
  	window = gtk_window_new(type);
	gtk_signal_connect(GTK_OBJECT(window),
				"delete-event",
				GTK_SIGNAL_FUNC(signal_window_close),
				NULL);

	print_status("setting title");
	gtk_window_set_title(GTK_WINDOW(window), name);
	print_status("done");
	gtk_widget_show(window);
	return window;
}

GtkWidget *frontend_create_box(int box_type, GtkWidget *container, char *label,
				int attach)
{
	GtkWidget *box;

	print_status("creating box");
	print_status(label);



	switch (box_type) {
	case VBOX:
		box = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(box), 5);
		break;

	case HBOX:
		box = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(box), 5);
		break;
	case HBBOX:
		box = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(box),
					  GTK_BUTTONBOX_END);
		gtk_button_box_set_spacing(GTK_BUTTON_BOX(box), 5);
		break;
	case HBBOX2:
		box = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(box),
					  GTK_BUTTONBOX_EDGE);
		gtk_button_box_set_spacing(GTK_BUTTON_BOX(box), 4);
		break;
	case FRAME:
		box = gtk_frame_new(label);
		gtk_container_set_border_width(GTK_CONTAINER(box), 5);
		break;
	default:
		print_status("error");
		print_status("trying to create vbox");
		box = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	}

	print_status("attaching");
	switch (attach) {
	case ATTACH_TO_NOTEBOOK:
		gtk_notebook_append_page(GTK_NOTEBOOK(container),
					 box, gtk_label_new(label));
		break;

	case ATTACH_TO_CONTAINER:
		gtk_container_add(GTK_CONTAINER(container), box);
		break;
	case ATTACH_TO_BOX:
		gtk_box_pack_start(GTK_BOX(container), box, TRUE, TRUE, 0);
		break;
	default:
		print_status("error");
		print_status("trying to attach to container");
		gtk_container_add(GTK_CONTAINER(container), box);
	}
	gtk_widget_show(box);
	print_status("done");
	return box;
}

GtkWidget *frontend_create_notebook(GtkWidget *box)
{
	GtkWidget *notebook;

	print_status("creating notebook");
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, 0);
	gtk_widget_show(notebook);
	return notebook;
}

GtkWidget *frontend_create_button(GtkWidget *container, char *label)
{
	GtkWidget *button;

	print_status("adding button");
	print_status(label);

	button = gtk_button_new_with_label(label);
	gtk_widget_show(button);

	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(container), button, TRUE, TRUE, 0);

	return button;
}

GtkWidget *frontend_create_check(GtkWidget *container, char *label)
{
	GtkWidget *check;

	print_status("creating check");
	print_status(label);

	check = gtk_check_button_new_with_label(label);
	gtk_widget_show(check);

	gtk_container_add(GTK_CONTAINER(container), check);

	print_status("done");
	return check;
}

GtkWidget *frontend_create_label(GtkWidget *container, char *text)
{
	GtkWidget *label;

	print_status("creating label");
	print_status(text);

	label = gtk_label_new(text);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);

	gtk_container_add(GTK_CONTAINER(container), label);

	print_status("done");
	return label;
}

/*
GtkWidget *rootvis_create_frame_and_attach(char *name, GtkWidget *box)
{
	GtkWidget *frame;

	frame = gtk_frame_new(name);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);
	return frame;
}
*/

GtkWidget *frontend_create_entry(int type, GtkWidget *container,
					char *entry_changed,
					char *label, ...)
{
	va_list ap;
	char *list_element;
	char *signal;

	GtkWidget *entry;
	GList *list = NULL;

	print_status("creating entry");
	print_status(label);

	va_start(ap, label);
	switch (type) {
	case COMBO:
		entry = gtk_combo_new();
		while ((list_element = va_arg(ap, char *))) {
			print_status("adding element to list");
			print_status(list_element);
			list = g_list_append(list, list_element);
		}
		print_status("attaching string list to combo");
		gtk_combo_set_popdown_strings(GTK_COMBO(entry), list);
		break;
	case ENTRY:
		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), 6);
		while ((signal = va_arg(ap, char *))) {
			print_status("adding signal to entry");
			print_status(signal);
			gtk_signal_connect(GTK_OBJECT(entry),
					   /* signal */
					   signal,
					   /* function */
					   GTK_SIGNAL_FUNC(va_arg(ap,
								  void *)),
					   /* data */
					   va_arg(ap, char *));
		}
		break;
	default:
		return NULL;
	}
	va_end(ap);

	print_status("attaching entry to container");
	gtk_container_add(GTK_CONTAINER(container), entry);
	gtk_widget_show(entry);

	print_status("done");

	return entry;
}

void frontend_create_colorpicker(struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	GtkWidget *vbox;
	GtkWidget *options_frame, *options_vbox;

	static GtkWidget *bbox, *ok, *cancel;

	print_status("pressing button ... ");
	gtk_button_set_relief(GTK_BUTTON(colorsel->button), GTK_RELIEF_HALF);


	print_status("casting window ...");

	colorsel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(colorsel->window), colorsel->complete_name);

	gtk_container_set_border_width(GTK_CONTAINER(colorsel->window), 10);
	gtk_window_set_policy(GTK_WINDOW(colorsel->window), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(colorsel->window),
				GTK_WIN_POS_MOUSE);
	gtk_signal_connect(GTK_OBJECT(colorsel->window), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &(colorsel->window));

	vbox = gtk_vbox_new(FALSE, 5);

	printf("setting name ...");
	options_frame = gtk_frame_new(colorsel->complete_name);
	printf("done. \n");
	gtk_container_set_border_width(GTK_CONTAINER(options_frame), 5);

	options_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(options_vbox), 5);

	colorsel->color_picker = gtk_color_selection_new();
	gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(colorsel->color_picker), TRUE);
	gtk_color_selection_set_color(GTK_COLOR_SELECTION(colorsel->color_picker), colorsel->color);
	gtk_signal_connect(GTK_OBJECT(colorsel->color_picker), "color_changed", GTK_SIGNAL_FUNC(signal_colorselector_update), cvar);

	gtk_box_pack_start(GTK_BOX(options_vbox), colorsel->color_picker,
			   FALSE, FALSE, 0);
        gtk_widget_show(colorsel->color_picker);
	printf("raising the curtain \n");

	gtk_container_add(GTK_CONTAINER(options_frame), options_vbox);
	gtk_widget_show(options_vbox);

	gtk_box_pack_start(GTK_BOX(vbox), options_frame, TRUE, TRUE, 0);
	gtk_widget_show(options_frame);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	ok = gtk_button_new_with_label("Ok");
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(signal_colorselector_ok), cvar);
        GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
	gtk_widget_show(ok);


	cancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(signal_colorselector_cancel), cvar);
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_widget_show(cancel);
	gtk_widget_show(bbox);

	gtk_container_add(GTK_CONTAINER(colorsel->window), vbox);
	gtk_widget_show(vbox);
	gtk_widget_grab_default(ok);
}

void frontend_create_color_button(struct config_value* cvar, GtkWidget *container, char *name,
				  char *channel_name)
{
	struct rootvis_colorsel *color_struct;

	print_status("Allocating memory for color struct");
	color_struct = malloc(sizeof(struct rootvis_colorsel));
	cvar->valc.frontend = color_struct;

	frontend_set_color(cvar);
	color_struct->window = NULL;
	color_struct->name = name; //complete_name;

	print_status("reallocating name");
	color_struct->complete_name =
	  malloc((sizeof(name) + sizeof(channel_name) + 10)*sizeof(char));
	print_status("done");
	sprintf(color_struct->complete_name, "%s - %s", channel_name, name);
	print_status("done");


	char* labeltext = (char*)malloc(strlen(name) + 2);
	sprintf(labeltext, "%s:", name);
	color_struct->label = gtk_label_new(labeltext);
	gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(color_struct->label));

	color_struct->button = gtk_button_new();
	print_status("adding container ... ");
	gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(color_struct->button));
	print_status("done.\nraising ... ");

	print_status("done.\nmaking preview ... ");

	color_struct->preview = gtk_preview_new(GTK_PREVIEW_COLOR);
	print_status("done.\nsetting size ... ");
	gtk_preview_size(GTK_PREVIEW(color_struct->preview), 30, 28);
	print_status("done.\nraising ... ");
	print_status("done.\n");
	gtk_container_add(GTK_CONTAINER(color_struct->button), color_struct->preview);

	gtk_widget_set_usize(color_struct->button, 32, 26);

	gtk_signal_connect(GTK_OBJECT(color_struct->button), "clicked",
				GTK_SIGNAL_FUNC(signal_toggle_colorselector), cvar);

	frontend_update_color(cvar, 0);
	gtk_widget_show(GTK_WIDGET(color_struct->label));
	gtk_widget_show(GTK_WIDGET(color_struct->button));
	gtk_widget_show(color_struct->preview);
}

GtkWidget *frontend_create_channel(int channel)
{
	GtkWidget *window;
	char name[12];

	print_status("creating gtk window ... ");

	sprintf(name, "Channel %d", channel+1);
	print_status(name);

	print_status("debug 2");

	window = frontend_create_window(GTK_WINDOW_TOPLEVEL, name);

	print_status("done.");

	{
		GtkWidget *vbox_0, *notebook_1, *button_box_1,
			*vbox_2[4], *frame_3[4], *vbox_3[1], *hbox_4[5],
			*check_debug, *check_stereo,
			*close_button, *revert_button;

		vbox_0 = frontend_create_box(VBOX, window, "rootvis_config_vbox", ATTACH_TO_CONTAINER);
		{

			notebook_1 = frontend_create_notebook(vbox_0);
			{
			/*	vbox_2[0] = frontend_create_box(VBOX, notebook_1, "General", ATTACH_TO_NOTEBOOK);
				{
				}
				vbox_2[1] = frontend_create_box(VBOX, notebook_1, "Geometry", ATTACH_TO_NOTEBOOK);
				{
				}
				vbox_2[2] = frontend_create_box(VBOX, notebook_1, "Look & Feel", ATTACH_TO_NOTEBOOK);
				{
				}*/
				vbox_2[3] = frontend_create_box(VBOX, notebook_1, "Colors", ATTACH_TO_NOTEBOOK);
				{
					frame_3[0] = frontend_create_box(FRAME, vbox_2[3], "Gradient", ATTACH_TO_BOX);
					hbox_4[0] = frontend_create_box(HBOX, frame_3[0], "Bar", ATTACH_TO_CONTAINER);
					{
						frontend_create_color_button(&Cchannel[channel].def[11], hbox_4[0], "Begin", name);
						frontend_create_color_button(&Cchannel[channel].def[12], hbox_4[0], "2/5", name);
						frontend_create_color_button(&Cchannel[channel].def[13], hbox_4[0], "4/5", name);
						frontend_create_color_button(&Cchannel[channel].def[14], hbox_4[0], "End", name);
					}
					frame_3[1] = frontend_create_box(FRAME, vbox_2[3], "Bevel, Peaks & Shadow", ATTACH_TO_BOX);
					hbox_4[1] = frontend_create_box(HBOX, frame_3[1], "etc", ATTACH_TO_CONTAINER);
					{
						frontend_create_color_button(&Cchannel[channel].def[15], hbox_4[1], "Bevel", name);
						frontend_create_color_button(&Cchannel[channel].def[20], hbox_4[1], "Peaks", name);
						frontend_create_color_button(&Cchannel[channel].def[16], hbox_4[1], "Shadow", name);
					}
				}
			}


			button_box_1 = frontend_create_box(HBBOX2, vbox_0, "Button Box", ATTACH_TO_BOX);
			{
				revert_button = frontend_create_button(button_box_1, "Revert");
				frontend_set_signal(revert_button, "clicked", signal_revert, channel);
				close_button = frontend_create_button(button_box_1, "Close");
				frontend_set_signal(close_button, "clicked", signal_hide, channel);
			}
		}
	}
	config_set_widgets(channel);
	return window;
}

GtkWidget *frontend_create_main(void)
{
	GtkWidget *window, *channel_button[2],
		  *button_box[2], *channels_frame, *main_frame, *vbox,
		  *main_vbox, *channels_hbox, *channel_vbox[2],
		  *save_button, *revert_button, *close_button;

	window = frontend_create_window(GTK_WINDOW_TOPLEVEL, "Main");
	{
	 vbox = frontend_create_box(VBOX, window, "vbox", ATTACH_TO_CONTAINER);
	 {
	  main_frame = frontend_create_box(FRAME, vbox, "Global Settings", ATTACH_TO_BOX);
	  {
	   main_vbox = frontend_create_box(VBOX, main_frame, "main_hbox",
					ATTACH_TO_CONTAINER);

		widgets.stereo_check = frontend_create_check(main_vbox, "stereo support");
		frontend_set_signal(widgets.stereo_check, "toggled", signal_stereo_toggled, 0);
		widgets.debug_check = frontend_create_check(main_vbox, "debug messages (stdout)");
		frontend_set_signal(widgets.debug_check, "toggled", signal_check_toggled, 0);

	  }
	  channels_frame = frontend_create_box(FRAME, vbox, "Channel-specific",
					     ATTACH_TO_BOX);
	  {
	   button_box[0] = frontend_create_box(HBOX, channels_frame, "Main Button Box",
					 ATTACH_TO_CONTAINER);
	   {
	    channel_vbox[0] = frontend_create_box(VBOX, button_box[0], "channel_vbox_0",
					ATTACH_TO_CONTAINER);
	    {
		channel_button[0] = frontend_create_button(channel_vbox[0], "First Channel");
		frontend_set_signal(channel_button[0], "clicked", signal_show, 0);
		widgets.stereo_status[0] = frontend_create_label(channel_vbox[0], "renders both channels");
	    }
	    channel_vbox[1] = frontend_create_box(VBOX, button_box[0], "channel_vbox_0",
					ATTACH_TO_CONTAINER);
	    {

		channel_button[1] = frontend_create_button(channel_vbox[1], "Second Channel");
		frontend_set_signal(channel_button[1], "clicked", signal_show, 1);
		widgets.stereo_status[1] = frontend_create_label(channel_vbox[1], "unused / inactive");
	    }
	   }
	  }
  	  button_box[1] = frontend_create_box(HBBOX, vbox, "Button Box", ATTACH_TO_BOX);
	  {

		revert_button = frontend_create_button(button_box[1], "Revert All");
		frontend_set_signal(revert_button, "clicked", signal_revert, 2);
		save_button = frontend_create_button(button_box[1], "Save Settings");
		frontend_set_signal(save_button, "clicked", signal_save, 2);
		close_button = frontend_create_button(button_box[1], "Close Windows");
		frontend_set_signal(close_button, "clicked", signal_hide, 2);

	  }
	 }
	}
	config_set_widgets(2);
	return window;
}
