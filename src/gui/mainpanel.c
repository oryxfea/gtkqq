#include <mainpanel.h>
#include <statusbutton.h>
#include <qq.h>

/*
 * The main event loop context of Gtk.
 */
static GMainContext *gtkctx;
extern QQInfo *info;
static QQMainPanelClass *this_class = NULL;

static void qq_mainpanel_init(QQMainPanel *panel);
static void qq_mainpanelclass_init(QQMainPanelClass *c);
static void qq_mainpanel_destory(QQMainPanel *panel);

static gboolean lnick_release_cb(GtkWidget *w, GdkEvent *event, gpointer data);
static gboolean lnick_focus_out_cb(GtkWidget *w, GdkEvent *event, gpointer data);
static gboolean lnick_enter_cb(GtkWidget *w, GdkEvent *event, gpointer data);
static gboolean lnick_leave_cb(GtkWidget *w, GdkEvent *event, gpointer data);

//button group callback.
static void btn_group_release_cb(GtkWidget *btn, GdkEvent *event, gpointer data);
static void btn_group_enter_cb(GtkWidget *btn, GdkEvent *event, gpointer data);
static void btn_group_leave_cb(GtkWidget *btn, GdkEvent *event, gpointer data);

//
// Update the long nick
//
static void update_long_nick(const gchar *new_ln);

//
// The map between the QQBuddy uin and the tree row.
//
typedef struct{
	GString *uin;
	GtkTreeRowReference *row_ref;
}QQTreeMap;

enum{
	TV_IMAGE = 0,	 	//The face image of buddy or group
	TV_NAME, 		//Group name or buddy nick
	TV_TYPE, 	//The client type
	COLUMNS 		//The number of columns
};
//
// Setup the tree view of contact tree, group list and recent list
//
static void tree_view_setup(GtkWidget *view);
static GtkTreeModel* create_contact_model(QQMainPanel *panel);
static GtkTreeModel* create_grp_or_recent_model(QQMainPanel *panel);

GType qq_mainpanel_get_type()
{
	static GType t = 0;
	if(!t){
		static const GTypeInfo info =
			{
				sizeof(QQMainPanelClass),
				NULL,
				NULL,
				(GClassInitFunc)qq_mainpanelclass_init,
				NULL,
				NULL,
				sizeof(QQMainPanel),
				0,
				(GInstanceInitFunc)qq_mainpanel_init,
				NULL
			};
		t = g_type_register_static(GTK_TYPE_VBOX, "QQMainPanel"
						, &info, 0);
	}
	return t;
}
GtkWidget* qq_mainpanel_new(GtkWidget *container)
{
	QQMainPanel *panel = g_object_new(qq_mainpanel_get_type(), NULL);
	panel -> container = container;

	return GTK_WIDGET(panel);
}

static void qq_mainpanel_init(QQMainPanel *panel)
{
	panel -> tree_maps = g_hash_table_new(g_str_hash, g_str_equal);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget *box = NULL;
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	panel -> faceimgframe = gtk_frame_new(NULL);
	gtk_box_pack_start(GTK_BOX(hbox), panel -> faceimgframe
				, FALSE, FALSE, 15);

	panel -> status_btn = qq_statusbutton_new();
	panel -> nick = gtk_label_new("");
	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), panel -> status_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), panel -> nick, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 2);

	panel -> longnick = gtk_label_new("");
	panel -> longnick_entry = gtk_entry_new();	
	gtk_widget_set_size_request(GTK_WIDGET(panel -> longnick_entry), 0, 0);
	panel -> longnick_box = gtk_hbox_new(FALSE, 0);
	panel -> longnick_eventbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(panel -> longnick_eventbox)
						, panel -> longnick);
	gtk_box_pack_start(GTK_BOX(panel -> longnick_box)
							, panel -> longnick_eventbox
							, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(panel -> longnick_box)
							, panel -> longnick_entry
							, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), panel -> longnick_box, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(panel), hbox, FALSE, FALSE, 10);

	g_signal_connect(GTK_WIDGET(panel -> longnick_eventbox)
						, "button-release-event"
						, G_CALLBACK(lnick_release_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> longnick_eventbox)
						, "enter-notify-event"
						, G_CALLBACK(lnick_enter_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> longnick_eventbox)
						, "leave-notify-event"
						, G_CALLBACK(lnick_leave_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> longnick_entry)
						, "focus-out-event"
						, G_CALLBACK(lnick_focus_out_cb), panel);

	panel -> contact_btn = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(panel -> contact_btn)
						, this_class -> contact_img[0]);
	panel -> grp_btn = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(panel -> grp_btn)
						, this_class -> grp_img[1]);
	panel -> recent_btn = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(panel -> recent_btn)
						, this_class -> recent_img[1]);

	g_signal_connect(GTK_WIDGET(panel -> contact_btn), "button-release-event"
								, G_CALLBACK(btn_group_release_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> grp_btn), "button-release-event"
								, G_CALLBACK(btn_group_release_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> recent_btn), "button-release-event"
								, G_CALLBACK(btn_group_release_cb), panel);

	g_signal_connect(GTK_WIDGET(panel -> contact_btn), "enter-notify-event"
								, G_CALLBACK(btn_group_enter_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> grp_btn), "enter-notify-event"
								, G_CALLBACK(btn_group_enter_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> recent_btn), "enter-notify-event"
								, G_CALLBACK(btn_group_enter_cb), panel);

	g_signal_connect(GTK_WIDGET(panel -> contact_btn), "leave-notify-event"
								, G_CALLBACK(btn_group_leave_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> grp_btn), "leave-notify-event"
								, G_CALLBACK(btn_group_leave_cb), panel);
	g_signal_connect(GTK_WIDGET(panel -> recent_btn), "leave-notify-event"
								, G_CALLBACK(btn_group_leave_cb), panel);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), panel -> contact_btn, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), panel -> grp_btn, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), panel -> recent_btn, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(panel), hbox, FALSE, FALSE, 0);


	panel -> notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(panel -> notebook), FALSE);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(panel -> notebook), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(panel -> notebook), TRUE);
	gtk_box_pack_start(GTK_BOX(panel), panel -> notebook, TRUE, TRUE, 3);

	panel -> contact_tree = gtk_tree_view_new();
	panel -> grp_list = gtk_tree_view_new();
	panel -> recent_list = gtk_tree_view_new();
	tree_view_setup(panel -> contact_tree);
	tree_view_setup(panel -> grp_list);
	tree_view_setup(panel -> recent_list);

	GtkWidget *scrolled_win; 
	scrolled_win= gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_win), panel -> contact_tree);
	gtk_notebook_append_page(GTK_NOTEBOOK(panel -> notebook)
							, scrolled_win, NULL);

	scrolled_win= gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_win), panel -> grp_list);
	gtk_notebook_append_page(GTK_NOTEBOOK(panel -> notebook)
							, scrolled_win, NULL);

	scrolled_win= gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_win),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_win), panel -> recent_list);
	gtk_notebook_append_page(GTK_NOTEBOOK(panel -> notebook)
							, scrolled_win, NULL);

}
static void qq_mainpanelclass_init(QQMainPanelClass *c)
{
	this_class = c;
	GdkPixbuf * pb;
	pb= gdk_pixbuf_new_from_file(IMGDIR"ContactMainTabButton1.png", NULL);
	c -> contact_img[0] = gtk_image_new_from_pixbuf(pb);
	pb = gdk_pixbuf_new_from_file(IMGDIR"ContactMainTabButton2.png", NULL);
	c -> contact_img[1] = gtk_image_new_from_pixbuf(pb);
	pb = gdk_pixbuf_new_from_file(IMGDIR"GroupMainTabButton1.png", NULL);
	c -> grp_img[0] = gtk_image_new_from_pixbuf(pb);
	pb = gdk_pixbuf_new_from_file(IMGDIR"GroupMainTabButton2.png", NULL);
	c -> grp_img[1] = gtk_image_new_from_pixbuf(pb);
	pb = gdk_pixbuf_new_from_file(IMGDIR"RecentMainTabButton1.png", NULL);
	c -> recent_img[0] = gtk_image_new_from_pixbuf(pb);
	pb = gdk_pixbuf_new_from_file(IMGDIR"RecentMainTabButton2.png", NULL);
	c -> recent_img[1] = gtk_image_new_from_pixbuf(pb);

	//increase the reference count
	g_object_ref(G_OBJECT(c -> contact_img[0]));
	g_object_ref(G_OBJECT(c -> contact_img[1]));
	g_object_ref(G_OBJECT(c -> grp_img[0]));
	g_object_ref(G_OBJECT(c -> grp_img[1]));
	g_object_ref(G_OBJECT(c -> recent_img[0]));
	g_object_ref(G_OBJECT(c -> recent_img[1]));

	//show the image
	gtk_widget_show(c -> contact_img[0]);
	gtk_widget_show(c -> contact_img[1]);
	gtk_widget_show(c -> grp_img[0]);
	gtk_widget_show(c -> grp_img[1]);
	gtk_widget_show(c -> recent_img[0]);
	gtk_widget_show(c -> recent_img[1]);

	c -> hand = gdk_cursor_new(GDK_HAND1);
	gtkctx = g_main_context_default();
}
static void qq_mainpanel_destory(QQMainPanel *panel)
{

}

//
// Update the information of all the widgets.
//
void qq_mainpanel_update(QQMainPanel *panel)
{
	GtkBin *faceimgframe = GTK_BIN(panel -> faceimgframe);

	//free the old image.
	GtkWidget *faceimg = gtk_bin_get_child(faceimgframe);
	if(faceimg != NULL){
		gtk_container_remove(GTK_CONTAINER(faceimgframe)
						, faceimg);
	}

	GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
					info -> me -> faceimgfile -> str
					, 48, 48, TRUE, NULL);
	if(pb != NULL){
		GtkWidget *img = gtk_image_new_from_pixbuf(pb);
		//we MUST show it!
		gtk_widget_show(img);
		gtk_container_add(GTK_CONTAINER(faceimgframe), img);
	}else{
		g_warning("Create face image for %s error!(%s, %d)"
				, info -> me -> faceimgfile -> str
				, __FILE__, __LINE__ );
	}

	GString *nick, *lnick;
	nick = g_string_new(info -> me -> nick -> str);
	lnick = g_string_new(info -> me -> lnick -> str);
	gtk_label_set_text(GTK_LABEL(panel -> nick), nick -> str);
	gtk_label_set_text(GTK_LABEL(panel -> longnick), lnick -> str);

	//update the contact tree
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel -> contact_tree)
							, create_contact_model(panel));
}

//
// click the long nick label, replace it with the gtkentry.
//
static gboolean lnick_release_cb(GtkWidget *w, GdkEvent *event, gpointer data)
{
	QQMainPanel *panel = QQ_MAINPANEL(data);

	gtk_widget_hide(GTK_WIDGET(panel -> longnick_eventbox));
	gtk_widget_show(GTK_WIDGET(panel -> longnick_entry));

	gtk_entry_set_text(GTK_ENTRY(panel -> longnick_entry), 
						gtk_label_get_text(GTK_LABEL(panel -> longnick)));
	gtk_widget_show(GTK_WIDGET(panel -> longnick_entry));
	gtk_widget_set_size_request(GTK_WIDGET(panel -> longnick_entry)
								, -1, 22);
	gtk_widget_grab_focus(GTK_WIDGET(panel -> longnick_entry));
	return FALSE;
}

//
//  longnick entry lose focus, replace it with label
//
static gboolean lnick_focus_out_cb(GtkWidget *w, GdkEvent *event
										, gpointer data)
{
	QQMainPanel *panel = QQ_MAINPANEL(data);
	gtk_widget_show(panel -> longnick_eventbox);
	gtk_widget_hide(panel -> longnick_entry);
	gtk_label_set_text(GTK_LABEL(panel -> longnick)
				, gtk_entry_get_text(GTK_ENTRY(panel -> longnick_entry)));
	return FALSE;
}

//
//Button group callbutton
//
//The buttons which connect to this callback function will
//only one button actived in any time.
//
static void btn_group_release_cb(GtkWidget *btn, GdkEvent *event, gpointer data)
{
	//grab the focus
	gtk_widget_grab_focus(btn);

	//clear
	QQMainPanel *panel = QQ_MAINPANEL(data);
	gtk_container_remove(GTK_CONTAINER(panel -> contact_btn)
						, gtk_bin_get_child(GTK_BIN(panel -> contact_btn)));
	gtk_container_remove(GTK_CONTAINER(panel -> grp_btn)
						, gtk_bin_get_child(GTK_BIN(panel -> grp_btn)));
	gtk_container_remove(GTK_CONTAINER(panel -> recent_btn)
						, gtk_bin_get_child(GTK_BIN(panel -> recent_btn)));

	if(btn == panel -> contact_btn){
		gtk_container_add(GTK_CONTAINER(panel -> contact_btn)
							, this_class -> contact_img[0]);
		gtk_container_add(GTK_CONTAINER(panel -> grp_btn)
							, this_class -> grp_img[1]);
		gtk_container_add(GTK_CONTAINER(panel -> recent_btn)
							, this_class -> recent_img[1]);
	}else if(btn == panel -> grp_btn){
		gtk_container_add(GTK_CONTAINER(panel -> contact_btn)
							, this_class -> contact_img[1]);
		gtk_container_add(GTK_CONTAINER(panel -> grp_btn)
							, this_class -> grp_img[0]);
		gtk_container_add(GTK_CONTAINER(panel -> recent_btn)
							, this_class -> recent_img[1]);
	}else if(btn == panel -> recent_btn){
		gtk_container_add(GTK_CONTAINER(panel -> contact_btn)
							, this_class -> contact_img[1]);
		gtk_container_add(GTK_CONTAINER(panel -> grp_btn)
							, this_class -> grp_img[1]);
		gtk_container_add(GTK_CONTAINER(panel -> recent_btn)
							, this_class -> recent_img[0]);
	}

}
static void btn_group_enter_cb(GtkWidget *btn, GdkEvent *event, gpointer data)
{
	GdkWindow *win = gtk_widget_get_window(btn);	
	gdk_window_set_cursor(win, this_class -> hand);
	GdkColor color;
	gdk_color_parse("#C4E6FF", &color);
	gtk_widget_modify_bg(btn, GTK_STATE_NORMAL, &color);
}
static void btn_group_leave_cb(GtkWidget *btn, GdkEvent *event, gpointer data)
{
	GdkWindow *win = gtk_widget_get_window(btn);	
	gdk_window_set_cursor(win, NULL);
	gtk_widget_modify_bg(btn, GTK_STATE_NORMAL, NULL);
}

//
// The long nick label enter-notify-event and leave-notify-event call back.
// Change the background and cursor.
//
static gboolean lnick_enter_cb(GtkWidget *w, GdkEvent *event, gpointer data)
{
	QQMainPanel *panel = QQ_MAINPANEL(data);
	GdkWindow *win = gtk_widget_get_window(w);	
	gdk_window_set_cursor(win, this_class -> hand);
	GdkColor color;
	gdk_color_parse("#C4E6FF", &color);
	gtk_widget_modify_bg(GTK_WIDGET(panel -> longnick_eventbox)
						, GTK_STATE_NORMAL, &color);
}
static gboolean lnick_leave_cb(GtkWidget *w, GdkEvent *event, gpointer data)
{
	QQMainPanel *panel = QQ_MAINPANEL(data);
	GdkWindow *win = gtk_widget_get_window(w);	
	gdk_window_set_cursor(win, NULL);
	gtk_widget_modify_bg(GTK_WIDGET(panel -> longnick_eventbox)
						, GTK_STATE_NORMAL, NULL);
}

//
// Update the image widget of map -> row_ref to file
//
static void update_image_widget(GtkTreeModel *model, QQTreeMap *map
								, const gchar *file)
{
	GError *err = NULL;
	GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(file, 20, 20, &err);
	if(pb != NULL){
		GtkTreePath *path;
		GtkTreeIter iter;
		path = gtk_tree_row_reference_get_path(map -> row_ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, TV_IMAGE, pb, -1);
	}else{
		g_warning("Update face image widget error!!%s (%s, %d)"
						, err -> message, __FILE__, __LINE__);
	}
}

typedef struct{
	QQBuddy *bdy;
	QQMainPanel *panel;
}GetFaceIdlePar;

//
// Update the image widget
// Run in the gtk main event loop
//
static gboolean get_face_img_gtk_idle(gpointer data)
{
	GetFaceIdlePar *par = (GetFaceIdlePar*)data;
	QQBuddy *bdy = par -> bdy;
	QQMainPanel *panel = par -> panel;
	g_slice_free(GetFaceIdlePar, par);
	
	QQTreeMap *map = (QQTreeMap*)g_hash_table_lookup(panel -> tree_maps
													, bdy -> uin -> str);
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(
							panel -> contact_tree));
	gchar path[100];
	g_sprintf(path, CONFIGDIR"/%s/icons/%s.%s", info -> me -> uin -> str
								, bdy -> faceimg -> uin -> str
								, bdy -> faceimg -> type -> str);
	update_image_widget(model, map, path);
	return FALSE;
}

//
// The callback of qq_get_face_img.
// Just save the image to the server and call the get_face_img_gtk_idle
// in the gtk main event loop;
//
static void get_face_image_cb(CallBackResult re, gpointer redata
									, gpointer usrdata)
{
	if(re != CB_SUCCESS){
		g_warning("Get face image error.(%s, %d)", __FILE__, __LINE__);
		return;
	}

	QQFaceImg *img = (QQFaceImg *)redata;
	QQBuddy *bdy = qq_info_lookup_buddy(info, img -> uin -> str);

	if(bdy == NULL || img == NULL){
		return;
	}
	bdy -> faceimg = img;

	gchar path[100];
	g_sprintf(path, CONFIGDIR"%s/icons/", info -> me -> uin -> str);
	if(qq_save_face_img(bdy, path) == -1){
		g_warning("Save face image error.(%s, %d)", __FILE__, __LINE__);
		return;
	}

	//update the image widget
	GetFaceIdlePar *par = g_slice_new(GetFaceIdlePar);
	par -> bdy = bdy;
	par -> panel = usrdata;
	GSource *src = g_idle_source_new();
	g_source_set_callback(src, (GSourceFunc)get_face_img_gtk_idle
								, par, NULL);
	g_source_attach(src, gtkctx);
	g_source_unref(src);
}

//
// Update the face images
//
static void update_face_image(QQMainPanel *panel)
{
	gchar path[100];
	GtkTreeModel *model;
	QQTreeMap *map;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(panel -> contact_tree));

	gint i;
	gboolean need_get_img = FALSE;
	QQBuddy *bdy;

	for( i = 0; i < info -> buddies -> len; ++i ){
		bdy = (QQBuddy*)info -> buddies -> pdata[i];
		map = (QQTreeMap*)g_hash_table_lookup(panel -> tree_maps
									, bdy -> uin -> str);
		if(map == NULL){
			g_warning("Lookup buddy return NULL.%s (%s, %d)", bdy -> uin -> str
									, __FILE__, __LINE__);
			continue;
		}
		// test if we have the image file;
		if(bdy -> faceimg != NULL && bdy -> faceimg -> uin != NULL){
			g_snprintf(path, 100, CONFIGDIR"/%s/%s.%s", info -> me -> uin -> str
									, bdy -> faceimg -> uin -> str
									, bdy -> faceimg -> type -> str);
			if(!g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)){
				need_get_img = FALSE;
			}
		}else{
			need_get_img = TRUE;
		}

		if(need_get_img){
			need_get_img = FALSE;
			// we need get image from the server.
			qq_get_face_img(info, bdy -> uin -> str, get_face_image_cb, panel);
		}else{
			//update the image widget
			update_image_widget(model, map, path);
		}
	}
}

//
// Create the model of the contact tree
//
static GtkTreeModel* create_contact_model(QQMainPanel *panel)
{
	GtkTreeIter iter, child;
	GtkTreeStore *store = gtk_tree_store_new(COLUMNS
											, GDK_TYPE_PIXBUF
											, G_TYPE_STRING
											, GDK_TYPE_PIXBUF);
	QQCategory *cate;
	QQTreeMap *map;
	QQBuddy *bdy;
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	GdkPixbuf *pb;
	gchar buf[500];
	gint i, j, k;
	gint num;
	for(i = 0; i < info -> categories -> len; ++i){
		//add the categories

		//find the ith catogory.
		k = 0;
		do{
			cate = (QQCategory*)info -> categories -> pdata[k];
			if(cate -> index == i){
				break;
			}
			++k;
		}while(k < info -> categories -> len);

		num = cate -> members -> len;
		g_sprintf(buf, "%s  [%d/%d]", cate -> name -> str, 0, num);
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, TV_NAME, buf, -1);

		//add the buddies in this category.
		for(j = 0; j < cate -> members -> len; ++j){
			gtk_tree_store_append(store, &child, &iter);
			bdy = (QQBuddy*) cate -> members -> pdata[j];
			if(bdy -> markname == NULL || bdy -> markname -> len <=0){
				g_sprintf(buf, "%s", bdy -> nick -> str);
			}else{
				g_sprintf(buf, "%s  (%s)", bdy -> markname -> str
										, bdy -> nick -> str);
			}
			pb = gdk_pixbuf_new_from_file_at_size(IMGDIR"/avatar.gif"
										, 20, 20, NULL);
			gtk_tree_store_set(store, &child, TV_IMAGE, pb, TV_NAME, buf, -1);

			//get the tree row reference
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &child);
			ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
			gtk_tree_path_free(path);

			//create and add a tree map
			map = g_slice_new0(QQTreeMap);
			map -> uin = bdy -> uin;
			map -> row_ref = ref;
			g_hash_table_insert(panel -> tree_maps, bdy -> uin -> str
									, map);

		}
	}

	//update the face image.
	update_face_image(panel);
	
	return GTK_TREE_MODEL(store);
}
static GtkTreeModel* create_grp_or_recent_model(QQMainPanel *panel)
{
}

//
// Setup the tree view of contact tree, group list and recent list
//
static void tree_view_setup(GtkWidget *view)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	// The face image column
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Face");
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "pixbuf", TV_IMAGE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	// The name column
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Name");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", TV_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	// The client type image column
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Client");
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "pixbuf", TV_TYPE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
}