/*  Copyright (C) 2008-2014 Davide Andreoli (see AUTHORS)
 *
 *  This file is part of edgar.
 *  edgar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  edgar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with edgar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <e.h>
#include "config.h"
#include "e_mod_main.h"
#include "e_mod_edgar.h"
#include "e_mod_guardian.h"


/* Local Prototypes */
static unsigned char edgar_load_class(const char *path);
static unsigned char edgar_unload_class(E_Gadcon_Client_Class *cclass);
static void _edgar_file_monitor_cb(void *data, Ecore_File_Monitor *em, Ecore_File_Event event, const char *path);
static void _edgar_mouse_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void _edgar_edje_messagge_cb(void *data, Evas_Object *obj, Edje_Message_Type type, int id, void *message);
static void _edgar_message_send(Instance *inst, int id, const char *args, ...);
static Eina_Bool _edgar_exec_pipe_data(void *data, int ev_type, void *ev);
static Eina_Bool _edgar_exec_pipe_exit(void *data, int ev_type, void *ev);

/* Gadcon Prototypes */
static E_Gadcon_Client *_edgar_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void _edgar_gc_shutdown(E_Gadcon_Client *gcc);
static void _edgar_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char *_edgar_gc_label(const E_Gadcon_Client_Class *client_class);
static const char *_edgar_gc_id_new(const E_Gadcon_Client_Class *client_class);
static void _edgar_gc_id_del(const E_Gadcon_Client_Class *client_class, const char *id);
static Evas_Object *_edgar_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);

/* Gadgets API Prototypes */
static void _edgar_api_dbg      (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_conf_set (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_conf_get (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_id_get   (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_min_set  (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_exec     (Instance *inst, int id, Edje_Message_String_Set *msg);
static void _edgar_api_action   (Instance *inst, int id, Edje_Message_String_Set *msg);

/* Local Variables */
static const char *gadgets_user_dir;
static const char *gadgets_system_dir;

static Eina_List *classes;
static Eina_List *instances;
static Ecore_File_Monitor *mon1, *mon2;

extern Config *edgar_conf;
extern E_Config_DD *conf_edd;
extern E_Config_DD *conf_item_edd;

Ecore_Event_Handler *eeh1, *eeh2, *eeh3;

/* Implementation */
void
edgar_init(void)
{
   char buf[PATH_MAX];
   Eina_List *files;
   char *filename;

   classes = NULL;

   INF("EDGAR: ================================");
   INF("EDGAR: Init");

   snprintf(buf, sizeof(buf), "%s/.e/e/gadgets", e_user_homedir_get());
   gadgets_user_dir = eina_stringshare_add(buf);

   snprintf(buf, sizeof(buf), "%s/data/gadgets", e_prefix_data_get());
   gadgets_system_dir = eina_stringshare_add(buf);

   //Create user dir if not exists
   if (!ecore_file_exists(gadgets_user_dir))
      ecore_file_mkpath(gadgets_user_dir);
   
   /* search in user dir */
   DBG("EDGAR: Search in %s", gadgets_user_dir);
   files = ecore_file_ls(gadgets_user_dir);
   EINA_LIST_FREE(files, filename)
   {
      snprintf(buf, sizeof(buf), "%s/%s", gadgets_user_dir, filename);
      edgar_load_class(buf);
      free(filename);
   }
   eina_list_free(files);
   
   /* search in system dir */
   DBG("EDGAR: Search in %s", gadgets_system_dir);
   files = ecore_file_ls(gadgets_system_dir);
   EINA_LIST_FREE(files, filename)
   {
      snprintf(buf, PATH_MAX, "%s/%s", gadgets_system_dir, filename);
      edgar_load_class(buf);
      free(filename);
   }
   eina_list_free(files);
   
   /* Monitor the directories */
   mon1 = ecore_file_monitor_add(gadgets_user_dir, _edgar_file_monitor_cb, NULL);
   mon2 = ecore_file_monitor_add(gadgets_system_dir, _edgar_file_monitor_cb, NULL);
   
   /* Pipe Handler */
   eeh1 = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                        _edgar_exec_pipe_exit, NULL);
   eeh2 = ecore_event_handler_add(ECORE_EXE_EVENT_DATA,
                                        _edgar_exec_pipe_data, NULL);
   eeh3 = ecore_event_handler_add(ECORE_EXE_EVENT_ERROR,
                                        _edgar_exec_pipe_data, NULL);
}

void
edgar_shutdown(void)
{
   INF("EDGAR Shutdown (%d)", eina_list_count(classes));

   if (eeh1) ecore_event_handler_del(eeh1);
   if (eeh2) ecore_event_handler_del(eeh2);
   if (eeh3) ecore_event_handler_del(eeh3);
   eeh1 = eeh2 = eeh3 = NULL;
   
   if (mon1) ecore_file_monitor_del(mon1);
   if (mon2) ecore_file_monitor_del(mon2);
   mon1 = mon2 = NULL;

   while (classes)
   {
      edgar_unload_class(classes->data);
      classes = eina_list_remove_list(classes, classes);
   }
   eina_stringshare_del(gadgets_user_dir);
   eina_stringshare_del(gadgets_system_dir);
}

/* Internals */
static Eina_Bool
_edgar_monitor_dalayed(void *data)
{
   edgar_shutdown();
   edgar_init();
   return 0;
}

static void
_edgar_file_monitor_cb(void *data, Ecore_File_Monitor *em, Ecore_File_Event event, const char *path)
{
   static Ecore_Timer *monitor_timer = NULL;

   switch (event)
   {
      case ECORE_FILE_EVENT_CREATED_FILE:
      case ECORE_FILE_EVENT_DELETED_FILE:
      case ECORE_FILE_EVENT_MODIFIED:
         DBG("EDGAR: FILE EVENT [%s]", path);
         //TODO check if is an edje file
         if (monitor_timer) ecore_timer_del(monitor_timer);
         monitor_timer = ecore_timer_add(1.5, _edgar_monitor_dalayed, NULL);
         break;
      case ECORE_FILE_EVENT_DELETED_SELF:
         //TODO handle this catasastrofic case...
         break;
      case ECORE_FILE_EVENT_CREATED_DIRECTORY:
      case ECORE_FILE_EVENT_DELETED_DIRECTORY:
      default:
         break;
   }
   
}

static unsigned char
edgar_load_class(const char *path)
{
   E_Gadcon_Client_Class *cclass;
   Eina_List *l;
   char *style;

   if (!path || !ecore_file_exists(path))
      return 0;

   //TODO check better if is an edje file
   if (!eina_str_has_suffix(path, ".edj"))
      return 0;

   DBG("EDGAR: Load Gadget %s", path);

   //Do not load twice
   EINA_LIST_FOREACH(classes, l, cclass)
     {
        //DBG("EDGAR: SEARCH: %s", cclass->name);
        if (!strcmp(ecore_file_file_get(path),
                    ecore_file_file_get(cclass->name)))
           return 0;
     }

   //Alloc & populate the gc_class
   cclass = E_NEW(E_Gadcon_Client_Class, 1);
   if (!cclass) return 0;
   cclass->version = GADCON_CLIENT_CLASS_VERSION;
   cclass->name = eina_stringshare_add(path);
   cclass->func.init = _edgar_gc_init;
   cclass->func.shutdown = _edgar_gc_shutdown;
   cclass->func.orient = _edgar_gc_orient;
   cclass->func.label = _edgar_gc_label;
   cclass->func.icon = _edgar_gc_icon;
   cclass->func.id_new = _edgar_gc_id_new;
   cclass->func.id_del = _edgar_gc_id_del;
   
   style = edje_file_data_get(path, "e/gadget/apparence");
   if (!style) style = E_GADCON_CLIENT_STYLE_PLAIN;
   cclass->default_style = (char*)eina_stringshare_add(style);

   //Register the new class
   e_gadcon_provider_register(cclass);

   classes = eina_list_append(classes, cclass);
   return 1;
}

static unsigned char
edgar_unload_class(E_Gadcon_Client_Class *cclass)
{
   DBG("EDGAR: UnLoad Gadgets %s", cclass->name);

   /* Free the cclass */
   e_gadcon_provider_unregister(cclass);
   eina_stringshare_del(cclass->name);
   eina_stringshare_del(cclass->default_style);
   free(cclass);
   
   return 1;
}

/********************
 *   Gadcon IFace   *
 ********************/

// static void
// _edgar_focus_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
// {
   // Instance *inst = data;
  // ERR("FOCUSIN on %s!!!", inst->gcc->cf->id);
   // evas_object_focus_set(inst->obj, 1);
// }

static E_Gadcon_Client *
_edgar_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   //E_Gadcon_Client *gcc;
   Instance *inst;
   Config_Item *ci = NULL;

   DBG("EDGAR: Gadcon Init name: %s id: %s", name, id);

   /* Search for a config for this id */
   ci = eina_hash_find(edgar_conf->conf_items_hash, id);
   if (!ci)
   {
      ci = E_NEW(Config_Item, 1);
      ci->id = eina_stringshare_add(id); //TODO freeme
      ci->data_hash = NULL;
      eina_hash_direct_add(edgar_conf->conf_items_hash, ci->id, ci);
   }

   /* Create the gadget instance */
   inst = E_NEW(Instance, 1);
   if (!inst) return 0;
   inst->ci = ci;

   /* Create the edje object */
   inst->obj = edje_object_add(gc->evas);
   edje_object_file_set(inst->obj, name, "e/gadget/main");//TODO Here search different group names...or load the first one
   evas_object_data_set(inst->obj, "EDGAR:instance", inst);
   //evas_object_repeat_events_set(inst->obj, 0);
   //evas_object_pass_events_set(inst->obj, 0);
   

   /* setup callbacks on the edje object */
   edje_object_message_handler_set(inst->obj, _edgar_edje_messagge_cb, NULL);
   //TODO MOUSE_DOWN?? I prefer MOUSE_IN!!
   //evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_MOUSE_IN,
   //                               _edgar_mouse_down_cb, inst);
   
   // evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_FOCUS_IN,
                                  // _edgar_focus_cb, inst);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_MOUSE_DOWN,
                                  _edgar_mouse_down_cb, inst);

   /* Create the Gadcon_Client*/
   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->obj);
   inst->gcc->data = inst;

   instances = eina_list_append(instances, inst);
   return inst->gcc;
}

static void
_edgar_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = gcc->data;

   DBG("EDGAR: Shutdown");
   instances = eina_list_remove(instances, inst);
   evas_object_del(inst->obj);
   E_FREE(inst);
}

static void
_edgar_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   Instance *inst = gcc->data;

   DBG("EDGAR: Gadcon Orient: %d", orient);
   switch (orient)
   {
      case E_GADCON_ORIENT_FLOAT:
         _edgar_message_send(inst, 0, "EDGAR_ORIENT", "FLOAT", NULL);
         break;
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
         _edgar_message_send(inst, 0, "EDGAR_ORIENT", "HORIZ", NULL);
         break;
      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
         _edgar_message_send(inst, 0, "EDGAR_ORIENT", "VERT", NULL);
         break;
      default:
         break;
   }

   if (!inst->min_w && !inst->min_h)
      edje_object_size_min_get(inst->obj, &inst->min_w, &inst->min_h);
   
   //edje_object_size_min_calc(inst->obj, &inst->min_w, &inst->min_h);
   // DBG("EDGAR: w:%d h:%d", inst->min_w, inst->min_h);

   e_gadcon_client_min_size_set(gcc, inst->min_w, inst->min_h);
   // DBG("EDGAR: DOPO");
}

static const char*
_edgar_gc_label(const E_Gadcon_Client_Class *client_class)
{
   char *label;
   DBG("EDGAR: Gadcon Label for class: %s", client_class->name);

   //Search the label in the edje file or use the filename as label
   label = edje_file_data_get(client_class->name, "e/gadget/name");
   if (!label)
     label = (char*)ecore_file_file_get(client_class->name);

   return label;
}

static const char*
_edgar_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   Config_Item *ci = NULL;
   char buf[128];

   snprintf(buf, sizeof(buf), "ED.%s.%.0f",
            ecore_file_file_get(client_class->name), ecore_time_get() * 1000000);

   DBG("EDGAR: Gadcon ID New [%s]", buf);

   ci = E_NEW(Config_Item, 1);
   ci->id = eina_stringshare_add(buf);
   ci->data_hash = eina_hash_string_superfast_new(NULL);
   eina_hash_direct_add(edgar_conf->conf_items_hash, ci->id, ci);

   return ci->id;
}

static void 
_edgar_gc_id_del(const E_Gadcon_Client_Class *client_class, const char *id)
{
   //DBG("EDGAR: Gadcon ID Del [%s]", id);
   //~ Config_Item *ci = NULL;

   //~ if (!(ci = _skel_conf_item_get(id))) return;

   //~ /* cleanup !! */
   //~ if (ci->id) eina_stringshare_del(ci->id);

   //~ skel_conf->conf_items = eina_list_remove(skel_conf->conf_items, ci);
   //~ E_FREE(ci);
}

static Evas_Object*
_edgar_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas)
{
   Evas_Object *icon;

   DBG("EDGAR: Gadcon Icon for class: %s", client_class->name);

   icon = edje_object_add(evas);
   if (edje_object_file_set(icon, client_class->name, "e/gadget/icon"))
      return icon;

   evas_object_del(icon);
   return NULL;
}

/************************
 *   Gadgets Callbacks  *
 ************************/
static void
_edgar_menu_post_cb(void *data, E_Menu *m)
{
   E_Gadcon_Client *gcc;

   if (!(gcc = data)) return;
   if (gcc->gadcon) e_gadcon_locked_set(gcc->gadcon, 0);
   if (!gcc->menu) return;
   if (gcc->gadcon && gcc->gadcon->shelf && (gcc->menu == gcc->gadcon->shelf->menu))
     gcc->gadcon->shelf->menu = NULL;
   e_object_del(E_OBJECT(gcc->menu));
   gcc->menu = NULL;
}

static void
_edgar_menu_configure_cb(void *data, E_Menu *m, E_Menu_Item *mi)
{
#ifdef HAVE_E19
   e_int_config_edgar_module(m->zone->comp, NULL);
#else
   e_int_config_edgar_module(m->zone->container, NULL);
#endif
}

static void
_edgar_mouse_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event_info;
   
   // DBG("EDGAR: DONW on %s!!!", inst->gcc->cf->id);
   // evas_object_focus_set(inst->obj, 1);

   if (inst->gcc->menu) return;
   if (ev->button == 3)
     {
        E_Menu_Item *mi;
        E_Zone *zone;
        int cx, cy, cw, ch;

        e_gadcon_locked_set(inst->gcc->gadcon, 1);
        inst->gcc->menu = e_menu_new();

        mi = e_menu_item_new(inst->gcc->menu);
        e_menu_item_label_set(mi, "Gadgets settings"); // TODO i18n
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _edgar_menu_configure_cb, NULL);

        inst->gcc->menu = e_gadcon_client_util_menu_items_append(inst->gcc, inst->gcc->menu, 0);
        e_menu_post_deactivate_callback_set(inst->gcc->menu, _edgar_menu_post_cb, inst->gcc);
        
        if (inst->gcc->gadcon->shelf) inst->gcc->gadcon->shelf->menu = inst->gcc->menu;

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &cx, &cy, &cw, &ch);
        zone = inst->gcc->gadcon->zone;
        if (!zone) zone = e_util_zone_current_get(e_manager_current_get());
        e_menu_activate_mouse(inst->gcc->menu, zone,
                              cx + ev->output.x,
                              cy + ev->output.y, 1, 1,
                              E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
     }
}

static void
_edgar_edje_messagge_cb(void *data, Evas_Object *obj, Edje_Message_Type type, int id, void *message)
{
   Edje_Message_String_Set *msg;
   Instance *inst;
   char *cmd;

   if (type != EDJE_MESSAGE_STRING_SET)
      return;

   msg = message;
   if (msg->count < 1)
      return;
   cmd = msg->str[0];
   
   inst = evas_object_data_get(obj, "EDGAR:instance");
   if (!inst) return;

   // int i;
   // printf("CMD [%s](%d) ", cmd, msg->count);
   // for(i = 1; i < msg->count; i++) printf("[%s]", msg->str[i]);
   // printf("");
   
   //command must start with EDGAR_ and must have at least 1 char after it
   if (strlen(cmd) < 7 || strncmp(cmd, "EDGAR_", 6))
      return;
   cmd += 6;

   if      (!strcmp(cmd, "DBG"))      _edgar_api_dbg      (inst, id, msg);
   else if (!strcmp(cmd, "MIN_SET"))  _edgar_api_min_set  (inst, id, msg);
   else if (!strcmp(cmd, "EXEC"))     _edgar_api_exec     (inst, id, msg);
   else if (!strcmp(cmd, "ACTION"))   _edgar_api_action   (inst, id, msg);
   else if (!strcmp(cmd, "CONF_SET")) _edgar_api_conf_set (inst, id, msg);
   else if (!strcmp(cmd, "CONF_GET")) _edgar_api_conf_get (inst, id, msg);
   else if (!strcmp(cmd, "ID_GET"))   _edgar_api_id_get   (inst, id, msg);
   else WRN("Warning: unknow command '%s'", cmd);
}

static void
_edgar_message_send(Instance *inst, int id, const char *args, ...)
{
   Edje_Message_String_Set *rmsg;
   va_list ap;
   const char *param;
   int n = 0;
   int i;

   //DBG("MSG");
   
   //Count the args
   va_start(ap, args);
   param = args;
   while (param != 0)
   {
      param = va_arg(ap, const char *);
      n++;
   }
   if (n < 1) return;
   
   //Alloc messagge
   rmsg = alloca(sizeof(Edje_Message_String_Set) + ((n - 1) * sizeof(char *))); //TODO FREE???
   rmsg->count = n;

   //Populate messagge
   va_start(ap, args);
   param = args;
   for (i = 0; i < n; i++)
   {
      rmsg->str[i] = (char*)eina_stringshare_add(param);  //NEED TO FREE???
      param = va_arg(ap, const char *);
   }
   va_end(ap);
   
   //Send the messagge
   edje_object_message_send(inst->obj, EDJE_MESSAGE_STRING_SET, id, rmsg);
}

/*******************
 *   Gadgets API   *
 *******************/

/**  EDGAR_DBG "string to print" "another one" ... "as many as you wish"
 *
 */
static void
_edgar_api_dbg(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   int i;

   printf("\033[31;1m[GADGET %s] ",
          ecore_file_file_get(inst->gcc->client_class->name));
   for (i = 1; i < msg->count; i++)
      printf("%s ", msg->str[i]);
   printf("\033[0m");
   printf("\n");
}

/**  EDGAR_ID_GET no_params
 * 
 * replay with EDGAR_ID_GET_REPLAY "id"
 */
static void
_edgar_api_id_get(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   DBG("EDGAR: ID_GET");
   _edgar_message_send(inst, id, "EDGAR_ID_GET_REPLY", (char*)inst->ci->id);
}

/**  EDGAR_MIN_SET "min_w min_h"
 * 
 */
static void
_edgar_api_min_set(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   char *value;
   int min_w, min_h;
   //~ char *value;
   //~ Data_Item *data;

   if (msg->count < 2)
      return;
   
   value = msg->str[1];
   //~ value = msg->str[2];
   
   DBG("EDGAR: MIN_SET '%s'", value);
   
   sscanf(value, "%d %d", &min_w, &min_h);
   DBG("EDGAR: MIN_SET '%d' '%d'", min_w, min_h);
   inst->gcc->min.w = min_w;
   inst->gcc->min.h = min_h;
   
   //e_gadcon_client_min_size_set(inst->gcc, min_w, min_h);
   
   //~ e_config_save_queue();
}

static Eina_Bool
_edgar_exec_pipe_data(void *data, int ev_type, void *ev)
{
   Ecore_Exe_Event_Data *e = (Ecore_Exe_Event_Data *)ev;
   Edgar_Exe_Data *exe_data;

   if (!e->exe) return 1;
   if (!ecore_exe_tag_get(e->exe) )return 1;
   if (strcmp(ecore_exe_tag_get(e->exe), "EdG")) return 1;
   
   exe_data = ecore_exe_data_get(e->exe);
   if (!exe_data) return 1;

   //DBG("EDGAR: data received.");
   if (e->lines)
   {
      int i;
      for (i = 0; e->lines[i].line != NULL; i++)
      {
          _edgar_message_send(exe_data->inst, exe_data->id_msg,
                              "EDGAR_EXEC_DATA", e->lines[i].line, NULL);

         if (exe_data->last_line) eina_stringshare_del(exe_data->last_line);
         exe_data->last_line = eina_stringshare_add(e->lines[i].line);
      }
   }
   return 1;
}

static Eina_Bool
_edgar_exec_pipe_exit(void *data, int ev_type, void *ev)
{
   Ecore_Exe_Event_Del *e = (Ecore_Exe_Event_Del *)ev;
   Edgar_Exe_Data *exe_data;

   if (!e->exe) return 1;
   if (!ecore_exe_tag_get(e->exe) )return 1;
   if (strcmp(ecore_exe_tag_get(e->exe), "EdG")) return 1;

   exe_data = ecore_exe_data_get(e->exe);
   if (!exe_data) return 1;

   if (e->exit_code)
   {
      //~ DBG("EDGAR: Error in EXEC, exit code: %d", e->exit_code);
       _edgar_message_send(exe_data->inst, exe_data->id_msg,
                           "EDGAR_EXEC_REPLY", "ERR", exe_data->last_line, NULL);
   }
   else
   {
      //~ DBG("EDGAR: Operation completed");
      _edgar_message_send(exe_data->inst, exe_data->id_msg,
                          "EDGAR_EXEC_REPLY", "OK", exe_data->last_line, NULL);
   }

   if (exe_data->last_line) eina_stringshare_del(exe_data->last_line);
   E_FREE(exe_data);
   
   return 0;
}

/**  EDGAR_EXEC "command"
 *   
 * replay with EDGAR_EXEC_REPLY "status" "last_line" when the command end.
 * status can be 'OK' 'ERR'.
 * You can also listen for EDGAR_EXEC_DATA "line" to catch every line of the 
 * output.
 */
static void
_edgar_api_exec(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   char *exe;
   Edgar_Exe_Data *exe_data;
   Ecore_Exe *ee;
   
   if (msg->count < 2)
      return;

   if (!edgar_guardian_is_allowed(inst, EDGAR_GUARDIAN_TYPE_COMMAND, msg->str[1]))
      return;
   
   exe = msg->str[1];
   // DBG("EDGAR: EXEC '%s'", exe);
   
   // if (inst->eeh1)
   // {
      // WRN("There is another EXEC running...skipping.");
      // return;
   // }
   
   exe_data = E_NEW(Edgar_Exe_Data, 1);
   exe_data->id_msg = id;
   exe_data->inst = inst;
   exe_data->last_line = NULL;
   
   ee = ecore_exe_pipe_run(exe,
      ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_READ_LINE_BUFFERED |
      ECORE_EXE_PIPE_ERROR | ECORE_EXE_PIPE_ERROR_LINE_BUFFERED,
      exe_data);
   ecore_exe_tag_set(ee, "EdG");
}

/**  EDGAR_ACTION "action"
 *
 * replay with EDGAR_ACTION_REPLY "status".
 * status can be 'OK' 'ERR'.
 */
static void
_edgar_api_action(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   E_Action *ac;
   char *action;
   char *params;

   if (msg->count != 2)
   {
      ERR("Wrong args count: %d", msg->count);
      return;
   }

   if (!edgar_guardian_is_allowed(inst, EDGAR_GUARDIAN_TYPE_EACTION, msg->str[1]))
     return;
   
   action = strdup(msg->str[1]);
   params = strchr(action, ' ');
   if (params)
   {
      *params = '\0';
      params++;
   }
   else
   {
      params = "";
   }

   ac = e_action_find(action);
   if (!ac)
   {
      ERR("Cannot find action: %s", action);
      _edgar_message_send(inst, id, "EDGAR_ACTION_REPLY", "ERR", NULL);
      free(action);
      return;
   }

   if (!ac->func.go)
   {
      ERR("Cannot find go() func in action: %s", action);
      _edgar_message_send(inst, id, "EDGAR_ACTION_REPLY", "ERR", NULL);
      free(action);
      return;
   }

   DBG("EDGAR: GO! action:'%s' params:'%s'", action, params);
   ac->func.go(NULL, params);
   _edgar_message_send(inst, id, "EDGAR_ACTION_REPLY", "OK", NULL);

   free(action);

   /*** print the list of available actions
   {
   Eina_List *l, *l2;

   E_Action_Group *actg;
   E_Action_Description *actd;
   int g, a;
   for (l = e_action_groups_get(), g = 0; l; l = l->next, g++)
     {
        actg = l->data;

        if (!actg->acts) continue;
         printf("*** %s\n", actg->act_grp);

        for (l2 = actg->acts, a = 0; l2; l2 = l2->next, a++)
          {
            actd = l2->data;
            printf(" - %s [%s]\n", actd->act_name, actd->act_cmd);
            printf("    params:'%s' '%s'\n", actd->act_params, actd->param_example);
          }
     }
   }
   */
   
}

/**  EDGAR_CONF_SET "key" "value"
 * 
 */
static void
_edgar_api_conf_set(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   char *key;
   char *value;
   Data_Item *data;

   if (msg->count < 3)
      return;
   
   key = msg->str[1];
   value = msg->str[2];
   
   DBG("EDGAR: CONF_SET key '%s' val: '%s'", key, value);
   
   data = eina_hash_find(inst->ci->data_hash, key);
   if (!data)
   {
      DBG("EDGAR: DATA NOT EXIST. Create new");
      data = E_NEW(Data_Item, 1);
      data->val_str = NULL;
      data->val_int = 0;
      eina_hash_add(inst->ci->data_hash, key, data);
   }

   if (data->val_str) eina_stringshare_del(data->val_str);
   data->val_str = eina_stringshare_add(value);

   e_config_save_queue();
}

/**  EDGAR_CONF_GET "key"
 * 
 * replay with EDGAR_CONF_GET_REPLAY "key" "value"
 * note: if no conf with that key exists, than no REPLAY msg is sent.
 */
static void
_edgar_api_conf_get(Instance *inst, int id, Edje_Message_String_Set *msg)
{
   Data_Item *data;
   char *key;
   
   DBG("EDGAR: CONF_GET");
   
   if (msg->count < 2)
      return;
   
   key = msg->str[1];
   data = eina_hash_find(inst->ci->data_hash, key);
   if (!data) return;
   
    _edgar_message_send(inst, id, "EDGAR_CONF_GET_REPLY",  key, data->val_str);
}
