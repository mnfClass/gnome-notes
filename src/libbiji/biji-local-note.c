/* bjb-local-note.c
 * Copyright (C) Pierre-Yves LUYTEN 2013 <py@luyten.fr>
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

#include "biji-local-note.h"
#include "serializer/biji-lazy-serializer.h"


struct BijiLocalNotePrivate_
{
  BijiProvider *provider;

  GFile *location;
  gchar *basename;
  gchar *html;

  gboolean trashed;
};


G_DEFINE_TYPE (BijiLocalNote, biji_local_note, BIJI_TYPE_NOTE_OBJ)

/* Iface */


const gchar *
local_note_get_place (BijiItem *local)
{
  BijiLocalNote *self;
  const BijiProviderInfo *info;

  g_return_if_fail (BIJI_IS_LOCAL_NOTE (local));

  self = BIJI_LOCAL_NOTE (local);
  info = biji_provider_get_info (self->priv->provider);

  return info->name;
}


gchar *
local_note_get_html (BijiNoteObj *note)
{
  if (BIJI_IS_LOCAL_NOTE (note))
    return BIJI_LOCAL_NOTE (note)->priv->html;

  else
    return NULL;
}


void
local_note_set_html (BijiNoteObj *note,
                     gchar *html)
{
  if (BIJI_LOCAL_NOTE (note)->priv->html)
    g_free (BIJI_LOCAL_NOTE (note)->priv->html);

  if (html)
    BIJI_LOCAL_NOTE (note)->priv->html = g_strdup (html);
}


void
local_note_save (BijiNoteObj *note)
{
  const BijiProviderInfo *prov_info;
  BijiInfoSet *info;
  BijiItem *item;
  BijiLocalNote *self;

  g_return_if_fail (BIJI_IS_LOCAL_NOTE (note));

  self = BIJI_LOCAL_NOTE (note);
  item = BIJI_ITEM (note);

  /* File save */
  biji_lazy_serialize (note);

  /* Tracker */
  prov_info = biji_provider_get_info (self->priv->provider);
  info = biji_info_set_new ();

  info->url = (gchar*) biji_item_get_uuid (item);
  info->title = (gchar*) biji_item_get_title (item);
  info->content = (gchar*) biji_note_obj_get_raw_text (note);
  info->mtime = biji_item_get_mtime (item);
  info->created = biji_note_obj_get_create_date (note);
  info->datasource_urn = g_strdup (prov_info->datasource);

  biji_tracker_ensure_ressource_from_info  (biji_item_get_manager (item),
                                            info);
}


/* GObj */

static void
biji_local_note_finalize (GObject *object)
{
  BijiLocalNote *self;

  g_return_if_fail (BIJI_IS_LOCAL_NOTE (object));

  self = BIJI_LOCAL_NOTE (object);

  if (self->priv->html)
    g_free (self->priv->html);

  G_OBJECT_CLASS (biji_local_note_parent_class)->finalize (object);
}


static void
biji_local_note_init (BijiLocalNote *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, BIJI_TYPE_LOCAL_NOTE, BijiLocalNotePrivate);
  self->priv->html = NULL;
  self->priv->trashed = FALSE;
}


static gboolean
item_yes         (BijiItem * item)
{
  return TRUE;
}


static gboolean
note_yes         (BijiNoteObj *item)
{
  return TRUE;
}


static gboolean
local_note_archive (BijiNoteObj *note)
{
  BijiLocalNote *self;
  GFile *parent, *trash, *archive;
  gchar *parent_path, *trash_path, *backup_path;
  GError *error = NULL;
  gboolean result = FALSE;

  self = BIJI_LOCAL_NOTE (note);

  /* Create the trash directory
   * No matter if already exists */
  parent = g_file_get_parent (self->priv->location);
  parent_path = g_file_get_path (parent);
  trash_path = g_build_filename (parent_path, ".Trash", NULL);
  trash = g_file_new_for_path (trash_path);
  g_file_make_directory (trash, NULL, NULL);

  /* Move the note to trash */

  backup_path = g_build_filename (trash_path, self->priv->basename, NULL);
  archive = g_file_new_for_path (backup_path);

  result = g_file_move (self->priv->location,
                        archive,
                        G_FILE_COPY_OVERWRITE,
                        NULL, // cancellable
                        NULL, // progress callback
                        NULL, // progress_callback_data,
                        &error);

  if (error)
  {
    g_message ("%s", error->message);
    g_error_free (error);
    error = NULL;
  }


  g_free (parent_path);
  g_object_unref (parent);
  g_free (trash_path);
  g_object_unref (trash);
  g_free (backup_path);  
  g_object_unref (archive);

  return result;
}


static gboolean
local_note_restore (BijiItem *item)
{
  BijiLocalNote *self;

  g_return_val_if_fail (BIJI_IS_LOCAL_NOTE (item), FALSE);
  self = BIJI_LOCAL_NOTE (item);

  g_warning ("local note restore");

  return FALSE;
}


static void
delete_file (GObject *note,
             GAsyncResult *res,
             gpointer user_data)
{
  BijiLocalNote *self;
  GError *error;

  self = BIJI_LOCAL_NOTE (user_data);
  error = NULL;
  g_file_delete_finish (self->priv->location, res, &error);

  if (error != NULL)
  {
    g_warning ("local note delete failed, %s", error->message);
    g_error_free (error);
  }
}


static gboolean
local_note_delete (BijiItem *item)
{
  BijiLocalNote *self;

  g_return_val_if_fail (BIJI_IS_LOCAL_NOTE (item), FALSE);
  self = BIJI_LOCAL_NOTE (item);

  g_debug ("local note delete : %s", g_file_get_path (self->priv->location));

  if (self->priv->trashed == TRUE)
  {
    g_file_delete_async (self->priv->location,
                         G_PRIORITY_LOW,
                         NULL,                  /* Cancellable */
                         delete_file,
                         self);
    return TRUE;
  }

  return FALSE;
}


static gchar*
local_note_get_basename (BijiNoteObj *note)
{
  return BIJI_LOCAL_NOTE (note)->priv->basename;
}


static void
biji_local_note_class_init (BijiLocalNoteClass *klass)
{
  GObjectClass *g_object_class;
  BijiItemClass    *item_class;
  BijiNoteObjClass *note_class;

  g_object_class = G_OBJECT_CLASS (klass);
  item_class = BIJI_ITEM_CLASS (klass);
  note_class = BIJI_NOTE_OBJ_CLASS (klass);

  g_object_class->finalize = biji_local_note_finalize;

  item_class->is_collectable = item_yes;
  item_class->has_color = item_yes;
  item_class->get_place = local_note_get_place;
  item_class->restore = local_note_restore;
  item_class->delete = local_note_delete;

  note_class->get_basename = local_note_get_basename;
  note_class->get_html = local_note_get_html;
  note_class->set_html = local_note_set_html;
  note_class->save_note = local_note_save;
  note_class->can_format = note_yes;
  note_class->archive = local_note_archive;

  g_type_class_add_private ((gpointer)klass, sizeof (BijiLocalNotePrivate));
}


BijiNoteObj *
biji_local_note_new_from_info   (BijiProvider *provider,
                                 BijiManager *manager,
                                 BijiInfoSet *set)
{
  BijiNoteID *id;
  BijiNoteObj *obj;
  BijiLocalNote *local;

  id = biji_note_id_new_from_info (set);

  obj = g_object_new (BIJI_TYPE_LOCAL_NOTE,
                       "manager", manager,
                       "id",        id,
                       NULL);

  local = BIJI_LOCAL_NOTE (obj);
  local->priv->location = g_file_new_for_commandline_arg (set->url);
  local->priv->basename = g_file_get_basename (local->priv->location);
  local->priv->provider = provider;

  if (strstr (set->url, "Trash") != NULL)
    local->priv->trashed = TRUE;

  return obj;
}
