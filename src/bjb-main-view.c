/* bjb-main-view.c
 * Copyright (C) Pierre-Yves LUYTEN 2012 <py@luyten.fr>
 *
 * bijiben is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bijiben is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <libbiji/libbiji.h>

#include "bjb-application.h"
#include "bjb-controller.h"
#include "bjb-list-view.h"
#include "bjb-list-view-row.h"
#include "bjb-main-toolbar.h"
#include "bjb-main-view.h"
#include "bjb-note-view.h"
#include "bjb-organize-dialog.h"
#include "bjb-search-toolbar.h"
#include "bjb-selection-toolbar.h"
#include "bjb-window-base.h"
#include "bjb-utils.h"

enum
{
  PROP_0,
  PROP_WINDOW,
  PROP_BJB_CONTROLLER,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

enum {
  VIEW_SELECTION_CHANGED,
  BJB_MAIN_VIEW_SIGNALS
};

static guint bjb_main_view_signals [BJB_MAIN_VIEW_SIGNALS] = { 0 };

/************************** Gobject ***************************/

struct _BjbMainView
{
  GtkGrid              parent_instance;

  GtkWidget           *window;
  GtkWidget           *label;

  /* Toolbar */
  BjbMainToolbar      *main_toolbar;

  /* Selection Mode */
  BjbSelectionToolbar *select_bar;

  /* View Notes , model */
  BjbListView         *view;
  BjbController       *controller;
};

G_DEFINE_TYPE (BjbMainView, bjb_main_view, GTK_TYPE_GRID)

static void
bjb_main_view_init (BjbMainView *self)
{
}

static void
bjb_main_view_set_controller (BjbMainView   *self,
                              BjbController *controller)
{
  self->controller = controller;
}

static void
bjb_main_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BjbMainView *self = BJB_MAIN_VIEW (object);

  switch (prop_id)
  {
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;
    case PROP_BJB_CONTROLLER:
      g_value_set_object (value, self->controller);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
bjb_main_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BjbMainView *self = BJB_MAIN_VIEW (object);

  switch (prop_id)
  {
    case PROP_WINDOW:
      self->window = g_value_get_object(value);
      break;
    case PROP_BJB_CONTROLLER:
      bjb_main_view_set_controller(self,g_value_get_object(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Callbacks */

void
switch_to_note_view (BjbMainView *self,
                     BijiNoteObj *note)
{
  bjb_window_base_switch_to_item (BJB_WINDOW_BASE (self->window),
                                  BIJI_ITEM (note));
}

static void
show_window_if_note (gpointer data,
                     gpointer user_data)
{
  BjbWindowBase *window = data;
  BijiNoteObj   *to_open = user_data;
  BijiNoteObj   *cur = bjb_window_base_get_note (window);

  if (cur && biji_note_obj_are_same (to_open, cur))
    gtk_window_present (data);
}

static void
switch_to_item (BjbMainView *self,
                BijiItem    *to_open)
{
  if (BIJI_IS_NOTE_OBJ (to_open))
    {
      /* If the note is already opened in another window, just show it. */
      if (biji_note_obj_is_opened (BIJI_NOTE_OBJ (to_open)))
        {
          GList *notes ;

          notes = gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ()));
          g_list_foreach (notes, show_window_if_note, to_open);
          return ;
        }

      /* Otherwise, leave main view */
      switch_to_note_view (self, BIJI_NOTE_OBJ (to_open));
    }

  /* Notebook
   * TODO : check if already opened (same as above) */
  else if (BIJI_IS_NOTEBOOK (to_open))
    {
      bjb_controller_set_notebook (self->controller,
                                   BIJI_NOTEBOOK (to_open));
    }
}

static gchar *
get_note_url_from_tree_path(GtkTreePath *path,
                            BjbMainView *self)
{
  GtkTreeIter   iter;
  gchar        *note_path;
  GtkTreeModel *model;

  model = bjb_controller_get_model (self->controller);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, BJB_MODEL_COLUMN_UUID, &note_path, -1);

  return note_path;
}


GList *
bjb_main_view_get_selected_items (BjbMainView *self)
{
  GList    *l, *result = NULL;
  gchar    *url;
  BijiItem *item;
  GList    *paths = bjb_controller_get_selection (self->controller);

  for (l = paths; l != NULL; l = l->next)
    {
      url = get_note_url_from_tree_path (l->data, self);
      item = biji_manager_get_item_at_path (bjb_window_base_get_manager (self->window),
                                            url);
      if (BIJI_IS_ITEM (item))
        result = g_list_prepend (result, item);

      g_free (url);
    }

  g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
  return result;
}

static void
on_selected_rows_changed_cb (BjbMainView *self)
{
  /* Workaround if items are selected
   * but selection mode not really active (?) */
  GList *select = bjb_controller_get_selection (self->controller);

  if (select)
    {
      g_list_free (select);
      bjb_controller_set_selection_mode (self->controller, TRUE);
    }

  /* Any case, tell */
  g_signal_emit (G_OBJECT (self),
                 bjb_main_view_signals[VIEW_SELECTION_CHANGED], 0);
}

/* Select all, escape */
static gboolean
on_key_press_event_cb (BjbMainView *self,
                       GdkEvent    *event)
{
  switch (event->key.keyval)
    {
      case GDK_KEY_a:
      case GDK_KEY_A:
        if (bjb_controller_get_selection_mode (self->controller) && event->key.state & GDK_CONTROL_MASK)
          {
            if (bjb_controller_is_all_selected (self->controller))
              bjb_controller_unselect_all (self->controller);
            else
              bjb_controller_select_all (self->controller);
            return TRUE;
          }
        break;

      case GDK_KEY_Escape:
        if (bjb_controller_get_selection_mode (self->controller))
          {
            bjb_controller_set_selection_mode (self->controller, FALSE);
            return TRUE;
          }

      default:
        break;
    }

  return FALSE;
}

static void
on_row_activated (BjbMainView    *self,
                  BjbListViewRow *row)
{
  BijiManager *manager;
  BijiItem    *to_open;

  if (bjb_main_view_get_selection_mode (self))
    return;

  manager = bjb_window_base_get_manager (self->window);
  to_open = biji_manager_get_item_at_path (manager, bjb_list_view_row_get_uuid (row));

  if (to_open)
    switch_to_item (self, to_open);
}

static GtkTargetEntry target_list[] = {
  { (gchar *) "text/plain", 0, 2 }
};

static void
on_drag_data_received (BjbMainView      *self,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *data,
                       guint             info,
                       guint             time)
{
  gint length = gtk_selection_data_get_length (data);

  if (length >= 0)
    {
      guchar *text = gtk_selection_data_get_text(data);

      if (text)
        {
          BijiManager *manager;
          BijiNoteObj *ret;
          BjbSettings *settings;

          /* FIXME Text is guchar utf 8, conversion to perform */
          manager = bjb_window_base_get_manager (self->window);
          settings = bjb_app_get_settings (g_application_get_default ());
          ret = biji_manager_note_new (manager,
                                       (gchar*) text,
                                       bjb_settings_get_default_location (settings));
          switch_to_note_view (self, ret); // maybe AFTER drag finish?

          g_free (text);
        }
    }

  /* Return false to ensure text is not removed from source
   * We just want to create a note. */
  gtk_drag_finish (context, FALSE, FALSE, time);
}

static void
bjb_main_view_connect_signals (BjbMainView *self)
{
  GtkListBox *list_box = bjb_list_view_get_list_box (self->view);

  g_signal_connect_object (list_box,
                           "selected-rows-changed",
                           G_CALLBACK (on_selected_rows_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (list_box,
                           "row-activated",
                           G_CALLBACK (on_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->view,
                           "drag-data-received",
                           G_CALLBACK (on_drag_data_received),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->window,
                           "key-press-event",
                           G_CALLBACK (on_key_press_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
on_show_more_cb (BjbMainView     *self,
                 GtkPositionType  pos)
{
  if (pos == GTK_POS_BOTTOM)
    bjb_controller_show_more (self->controller);
}

static void
bjb_main_view_constructed (GObject *o)
{
  BjbMainView *self;

  G_OBJECT_CLASS (bjb_main_view_parent_class)->constructed (G_OBJECT (o));

  self = BJB_MAIN_VIEW (o);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  self->view = bjb_list_view_new ();
  g_object_add_weak_pointer (G_OBJECT (self->view), (gpointer*) &(self->view));

  /* Main view */
  bjb_controller_set_selection_mode (self->controller, FALSE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self->view),
                                       GTK_SHADOW_NONE);
  bjb_list_view_setup (self->view, self->controller);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->view));
  gtk_widget_show (GTK_WIDGET (self->view));

  g_signal_connect_object (GTK_SCROLLED_WINDOW (self->view),
                           "edge-overshot",
                           G_CALLBACK (on_show_more_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->select_bar = bjb_selection_toolbar_new (self->view, self);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->select_bar));

  gtk_drag_dest_set (GTK_WIDGET (self->view), GTK_DEST_DEFAULT_ALL,
                     target_list, 1, GDK_ACTION_COPY);

  bjb_main_view_connect_signals (self);
  gtk_widget_show (GTK_WIDGET (self));
}

static void
bjb_main_view_class_init (BjbMainViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = bjb_main_view_get_property;
  object_class->set_property = bjb_main_view_set_property;
  object_class->constructed = bjb_main_view_constructed;

  properties[PROP_WINDOW] = g_param_spec_object ("window",
                                                 "Window",
                                                 "Parent Window",
                                                 GTK_TYPE_WIDGET,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_BJB_CONTROLLER] = g_param_spec_object ("controller",
                                                         "Controller",
                                                         "BjbController",
                                                         BJB_TYPE_CONTROLLER,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  bjb_main_view_signals[VIEW_SELECTION_CHANGED] = g_signal_new ( "view-selection-changed" ,
                                                  G_OBJECT_CLASS_TYPE (klass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);
}

BjbMainView *
bjb_main_view_new (GtkWidget     *win,
                   BjbController *controller)
{
  return g_object_new(BJB_TYPE_MAIN_VIEW,
                      "window", win,
                      "controller", controller,
                      NULL);
}

GtkWidget *
bjb_main_view_get_window (BjbMainView *self)
{
  return self->window;
}

void
bjb_main_view_update_model (BjbMainView *self)
{
  bjb_controller_update_view (self->controller);
  bjb_list_view_update (self->view);
}

gboolean
bjb_main_view_get_selection_mode (BjbMainView *self)
{
  return bjb_controller_get_selection_mode (self->controller);
}

void
bjb_main_view_set_selection_mode (BjbMainView *self,
                                  gboolean     mode)
{
  bjb_controller_set_selection_mode (self->controller, mode);
  bjb_list_view_set_selection_mode (self->view, mode);

  g_signal_emit (G_OBJECT (self),
                 bjb_main_view_signals[VIEW_SELECTION_CHANGED], 0);
}

