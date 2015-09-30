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


#include <Python.h>
#include <e.h>
#include "e_mod_main.h"
#include "e_mod_edgar.h"
#include "efl.eo_api.h"
#include <Elementary.h>


/* Local typedefs */
typedef struct {
   const char *name;      // ex: "ruler" (from folder name)
   const char *label;     // ex: "ruler" (from __gadget_name__)
   const char *path;      // ex: "/usr/local/lib/enlightenment/gadgets/ruler"
   const char *edjefile;  // ex: "/usr/local/lib/enlightenment/gadgets/ruler/ruler.edj"
   PyObject *mod;         // the python module for this gadget
   PyObject *instance;    // the python Gadget class instance
   Eina_List *pops_obj;   // list of edje objs that are set as popup content
   E_Gadcon_Client_Class *cclass;
   Eina_Bool opt_pop_on_desk;
}Edgar_Py_Gadget;


/* Local Prototypes */
static Edgar_Py_Gadget *edgar_gadget_load(const char *name, const char *path);
static void             edgar_gadget_unload(Edgar_Py_Gadget *gadget);
static void             edgar_gadgets_load_all(const char *path);
static void             edgar_gadgets_hash_free_func(void *data);
static Eina_Bool        edgar_theme_object_set(Edgar_Py_Gadget *gadget, Evas_Object *obj, const char *group);

/* Local Gadcon Prototypes */
static E_Gadcon_Client *_edgar_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void _edgar_gc_shutdown(E_Gadcon_Client *gcc);
static void _edgar_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char *_edgar_gc_label(const E_Gadcon_Client_Class *client_class);
static const char *_edgar_gc_id_new(const E_Gadcon_Client_Class *client_class);
static void _edgar_gc_id_del(const E_Gadcon_Client_Class *client_class, const char *id);
static Evas_Object *_edgar_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);

/* Python eapi module proto */
PyMODINIT_FUNC PyInit_eapi(void);


/* Local Variables */
static const char *edgar_gadgets_system_dir; //ex: "/usr/lib/enlightenment/gadgets/" 
static Eina_Hash *edgar_gadgets; //key: "gadget_name"  val: Edgar_Py_Gadget*
static PyObject *evasObjectType;
static PyObject *edjeEdjeType;


#define CURRENT_VAPI 1

#define PY_BOOTSTRAP_SCRIPT \
"import sys \n" \
"sys.path.append('%s') \n" \
"\n"


#define PY_ON_ERROR_RETURN(_extra_check_, _ret_val_, _msg_) \
   if (PyErr_Occurred()) {                                  \
      PyErr_Print();                                        \
      PyRun_SimpleString("sys.stdout.flush()");             \
      PyRun_SimpleString("sys.stderr.flush()");             \
      if (_msg_) DBG("EDGAR: ERROR "_msg_);                 \
      return _ret_val_;                                     \
   }                                                        \
   if (_extra_check_) {                                     \
      if (_msg_) DBG("EDGAR: ERROR "_msg_);                 \
      return _ret_val_;                                     \
   }                                                        \


#if (PY_VERSION_HEX < 0x03030000)
#define PyString_AsString(x) PyBytes_AsString(PyUnicode_AsUTF8String(x))
#else
#define PyString_AsString PyUnicode_AsUTF8
#endif


/*****************************************************************************/
/*****  Main stuff ***********************************************************/
/*****************************************************************************/

Eina_Bool
edgar_init()
{
   char buf[PATH_MAX];
   int ret;

   // prepare the local scope
   snprintf(buf, sizeof(buf), "%s/enlightenment/gadgets", e_prefix_lib_get());
   edgar_gadgets_system_dir = eina_stringshare_add(buf);
   edgar_gadgets = eina_hash_string_superfast_new(edgar_gadgets_hash_free_func);

   // Add the eapi module as a built-in module
   PyImport_AppendInittab("eapi", PyInit_eapi);

   // init python
   /// TOTRY Py_SetPath()
   Py_Initialize();

   // This import the C functions from the efl eo api.
   // The module do NOT need to be linked with the .so files, python
   // import machinery is used here.
   // Used functions: object_from_instance() and instance_from_object()
   if (import_efl__eo() != 0)
   {
      DBG("EDGAR: Cannot import python-efl");
      e_util_dialog_internal("Edgar Error",
                     "<font align=left><b>Python-efl not found.</b><br>"
                     "Your python gadgets will not work.<br><br>"
                     "Please install or update <i>python-efl</i>.</font>");
      return EINA_FALSE;
   }

   // keep some efl types around, will be used for type-checking
   PyObject *module, *moduleDict;

   module = PyImport_ImportModule("efl.evas");
   moduleDict = PyModule_GetDict(module);
   evasObjectType = PyDict_GetItemString(moduleDict, "Object");
   Py_DECREF(module);

   module = PyImport_ImportModule("efl.edje");
   /// TO TRY: PyObject * pClass = PyObject_GetAttrString(pModule, "Grasp_Behavior");
   moduleDict = PyModule_GetDict(module);
   edjeEdjeType = PyDict_GetItemString(moduleDict, "Edje");
   Py_DECREF(module);

   // Run the bootstrap python code
   snprintf(buf, sizeof(buf), PY_BOOTSTRAP_SCRIPT, edgar_gadgets_system_dir);
   DBG("EDGAR: Bootstrap (sys.path: '%s')", edgar_gadgets_system_dir);
   ret = PyRun_SimpleString(buf);
   PY_ON_ERROR_RETURN(ret != 0, EINA_FALSE, "Python bootstrap failed!");
   DBG("EDGAR: Bootstrap successful");

   // Load all the available gadgets
   edgar_gadgets_load_all(edgar_gadgets_system_dir);

   return EINA_TRUE;
}

void
edgar_shutdown()
{
   DBG("EDGAR: Shutdown started ...");

   // unload and free all the gadgets
   eina_hash_free(edgar_gadgets);

   // shutdown python
   // ...still getting some strange segfault here :/
   // if (Py_IsInitialized())
   //    Py_Finalize();

   // clean local scope
   eina_stringshare_del(edgar_gadgets_system_dir);

   DBG("EDGAR: Shutdown completed!");
}

static void
edgar_gadgets_load_all(const char *path)
{
   Eina_List *files;
   char buf[PATH_MAX];
   char *filename;

   DBG("EDGAR: Seaching python gadgets in %s", path);
   files = ecore_file_ls(path);
   EINA_LIST_FREE(files, filename)
   {
      snprintf(buf, sizeof(buf), "%s/%s", path, filename);
      if (ecore_file_is_dir(buf))
         edgar_gadget_load(filename, buf);

      free(filename);
   }
   eina_list_free(files);
}

static Edgar_Py_Gadget*
edgar_gadget_load(const char *name, const char *path)
{
   Edgar_Py_Gadget *gadget;
   E_Gadcon_Client_Class *cclass;
   char fname[PATH_MAX];
   const char *label;
   PyObject *mod, *attr, *opts;
   long vapi = 0;

   // check the given path
   snprintf(fname, sizeof(fname), "%s/__init__.pyc", path);
   if (!ecore_file_exists(fname))
   {
      snprintf(fname, sizeof(fname), "%s/__init__.py", path);
      if (!ecore_file_exists(fname))
         return NULL;
   }

   // import the gadget in python
   DBG("EDGAR:   * importing gadget: '%s' from: '%s'", name, fname);
   mod = PyImport_ImportModule(name);
   PY_ON_ERROR_RETURN(mod == NULL, NULL, "Gadget import failed!");

   // check the api version
   attr = PyObject_GetAttrString(mod, "__gadget_vapi__");
   if ((attr == NULL) || (!PyLong_Check(attr)) ||
       ((vapi = PyLong_AsLong(attr)) != CURRENT_VAPI))
   {
      e_util_dialog_show("Edgar Error",
            "<font align=left><b>The %s gadget is outdated !</b><br><br>"
            "Please update the gadget or contact the gadget maintainer.<br><br>"
            "current edgar api: %d<br>"
            "gadget api version: %ld<br>"
            "</font>", name, CURRENT_VAPI, vapi);
      return NULL;
   }
   Py_DECREF(attr);

   // cache the label
   attr = PyObject_GetAttrString(mod, "__gadget_name__");
   PY_ON_ERROR_RETURN(attr == NULL, NULL, "Gadget name not found");
   label = eina_stringshare_add(PyString_AsString(attr));
   Py_DECREF(attr);

   //Alloc & populate the gc_class
   cclass = E_NEW(E_Gadcon_Client_Class, 1);
   if (!cclass) return NULL;
   cclass->version = GADCON_CLIENT_CLASS_VERSION;
   cclass->name = eina_stringshare_add(name);
   cclass->default_style = eina_stringshare_add("plain");
   cclass->func.init = _edgar_gc_init;
   cclass->func.shutdown = _edgar_gc_shutdown;
   cclass->func.orient = _edgar_gc_orient;
   cclass->func.label = _edgar_gc_label;
   cclass->func.icon = _edgar_gc_icon;
   cclass->func.id_new = _edgar_gc_id_new;
   cclass->func.id_del = _edgar_gc_id_del;

   //Alloc & populate the gadget struct
   gadget = E_NEW(Edgar_Py_Gadget, 1);
   if (!gadget) return NULL;
   gadget->label = label;
   gadget->name = eina_stringshare_add(name);
   gadget->path = eina_stringshare_add(path);
   gadget->cclass = cclass;
   gadget->mod = mod;
   gadget->opt_pop_on_desk = EINA_FALSE;
   // do gadget have a local edj ?
   snprintf(fname, sizeof(fname), "%s/%s.edj", path, name);
   if (ecore_file_exists(fname))
      gadget->edjefile = eina_stringshare_add(fname);
   else
      gadget->edjefile = NULL;

   // read the gadget options dict
   if (PyObject_HasAttrString(mod, "__gadget_opts__") && 
       (opts = PyObject_GetAttrString(mod, "__gadget_opts__")))
   {
      attr = PyDict_GetItemString(opts, "popup_on_desktop");
      if (attr && PyObject_IsTrue(attr))
         gadget->opt_pop_on_desk = EINA_TRUE;

      Py_DECREF(opts);
   }

   //Register the new class
   e_gadcon_provider_register(cclass);
   eina_hash_add(edgar_gadgets, name, gadget);

   return gadget;
}

static void
edgar_gadget_unload(Edgar_Py_Gadget *gadget)
{
   Evas_Object *popup_content;
   Eina_List *l, *l2;

   DBG("EDGAR: Unload gadget: %s", gadget->name);

   // kill all the active popups edje obj
   EINA_LIST_FOREACH_SAFE(gadget->pops_obj, l, l2, popup_content)
      // E_FREE_FUNC(popup_content, evas_object_del);
      E_FN_DEL( evas_object_del, popup_content);
 

   // Free the gadcon client class
   e_gadcon_provider_unregister(gadget->cclass);
   eina_stringshare_del(gadget->cclass->name);
   eina_stringshare_del(gadget->cclass->default_style);
   free(gadget->cclass);

   // Free the gadget itself
   eina_stringshare_del(gadget->name);
   eina_stringshare_del(gadget->label);
   eina_stringshare_del(gadget->path);
   eina_stringshare_del(gadget->edjefile);
   Py_XDECREF(gadget->instance);
   Py_XDECREF(gadget->mod);
   free(gadget);
}

static void
edgar_gadgets_hash_free_func(void *data)
{
   Edgar_Py_Gadget *gadget = data;
   edgar_gadget_unload(gadget);
}


/*****************************************************************************/
/*****  Gadgets Utils   ******************************************************/
/*****************************************************************************/
 
static Eina_Bool
edgar_theme_object_set(Edgar_Py_Gadget *gadget, Evas_Object *obj, const char *group)
{
   char buf[PATH_MAX];

   if (!gadget || !obj || !group)
      return EINA_FALSE;

   snprintf(buf, sizeof(buf), "e/gadgets/%s/%s", gadget->name, group);

   // TODO search in E theme

   // search in gadget local edje file
   if (!gadget->edjefile)
      return EINA_FALSE;

   if (eo_isa(obj, EDJE_OBJECT_CLASS))
      return edje_object_file_set(obj, gadget->edjefile, buf);

   if (eo_isa(obj, ELM_LAYOUT_CLASS))
      return elm_layout_file_set(obj, gadget->edjefile, buf);

   return EINA_FALSE;
}

static Eina_Bool
edgar_popup_del_cb(void *data, Eo *obj, const Eo_Event_Description *desc, void *event_info)
{
   Edgar_Py_Gadget *gadget = data;

   // call the popup_destoyed() method of the gadget.
   PyObject *pyobj = object_from_instance(obj);
   PyObject *ret = PyObject_CallMethod(gadget->instance, "popup_destroyed",
                                       "(S)", pyobj);
   PY_ON_ERROR_RETURN(!ret, EO_CALLBACK_CONTINUE, "Cannot call popup_destroyed()");
   Py_DECREF(pyobj);
   Py_DECREF(ret);

   gadget->pops_obj = eina_list_remove(gadget->pops_obj, obj);

   return EO_CALLBACK_CONTINUE;
}

static E_Gadcon_Popup *
edgar_popup_new(Edgar_Py_Gadget *gadget, E_Gadcon_Client *gcc)
{
   E_Gadcon_Popup  *popup;
   Evas_Object *content;

   // create the popup content from the e/gadgets/name/popup group
   content = edje_object_add(gcc->gadcon->evas);
   if (!edgar_theme_object_set(gadget, content, "popup"))
   {
      evas_object_del(content);
      return NULL;
   }
   // NOTE: del cb with priority to be called before the python-efl one.
   // Otherwise python-efl delete the python obj too soon
   eo_do(content, eo_event_callback_priority_add(
         EO_EV_DEL, EO_CALLBACK_PRIORITY_BEFORE, edgar_popup_del_cb, gadget));

   // call the popup_created() method of the gadget.
   PyObject *pyobj = object_from_instance(content);
   PyObject *ret = PyObject_CallMethod(gadget->instance, "popup_created",
                                       "(S)", pyobj);
   PY_ON_ERROR_RETURN(!ret, NULL, "Cannot call popup_created()");
   Py_DECREF(pyobj);
   Py_DECREF(ret);

   // put the popup content in a gadcon popup and show it
#if E_VERSION_MAJOR >= 19
   popup = e_gadcon_popup_new(gcc, 0);
#else
   popup = e_gadcon_popup_new(gcc);
#endif
   e_gadcon_popup_content_set(popup, content);
   e_gadcon_popup_show(popup);

   gadget->pops_obj = eina_list_append(gadget->pops_obj, content);

   return popup;
}

static void
edgar_menu_info_cb(void *data, E_Menu *m, E_Menu_Item *mi)
{
   Edgar_Py_Gadget *gadget = data;
   Evas_Object *hbox, *tb, *icon, *img;
   Eina_Strbuf *strbuf;
   Evas_Coord w, h;
   PyObject *attr;
   E_Dialog *dia;

   // build the text
   strbuf = eina_strbuf_new();

   if ((attr = PyObject_GetAttrString(gadget->mod, "__gadget_name__")))
   {
      eina_strbuf_append_printf(strbuf, "<title>%s</title>",
                                PyString_AsString(attr));
      Py_DECREF(attr);
   }
   if ((attr = PyObject_GetAttrString(gadget->mod, "__gadget_vers__")))
   {
      eina_strbuf_append_printf(strbuf, " v%s", PyString_AsString(attr));
      Py_DECREF(attr);
   }
   if ((attr = PyObject_GetAttrString(gadget->mod, "__gadget_auth__")))
   {
      eina_strbuf_append_printf(strbuf, "<br>Author: %s", PyString_AsString(attr));
      Py_DECREF(attr);
   }
   if ((attr = PyObject_GetAttrString(gadget->mod, "__gadget_mail__")))
   {
      eina_strbuf_append_printf(strbuf, "<br>Contact: %s", PyString_AsString(attr));
      Py_DECREF(attr);
   }
   if ((attr = PyObject_GetAttrString(gadget->mod, "__gadget_desc__")))
   {
      eina_strbuf_append_printf(strbuf, "<br><br>%s", PyString_AsString(attr));
      Py_DECREF(attr);
   }

   // dialog
#if E_VERSION_MAJOR >= 19
   dia = e_dialog_new(NULL, "gadget_info", "class");
#else
   E_Container *con = e_container_current_get(e_manager_current_get());
   dia = e_dialog_new(con, "gadget_info", "class");
#endif

   e_dialog_resizable_set(dia, 0);
   e_dialog_title_set(dia, "Gadget info");
   e_dialog_button_add(dia, "Close", NULL, NULL, NULL);

#if E_VERSION_MAJOR >= 20
   Evas *evas = evas_object_evas_get(dia->win);
#else
   Evas *evas = dia->win->evas;
#endif

   // hbox
   hbox = e_widget_list_add(evas, 0, 1);

   // icon
   icon = edje_object_add(evas);
   edgar_theme_object_set(gadget, icon, "icon");
   img = e_widget_image_add_from_object(evas, icon,
                                        70 * e_scale, 70 * e_scale);
   e_widget_list_object_append(hbox, img, 1, 0, 0.0);

   // text
   tb = e_widget_textblock_add(evas);
   e_widget_textblock_markup_set(tb, eina_strbuf_string_get(strbuf));
   e_widget_size_min_set(tb, 250 * e_scale, 100 * e_scale);
   e_widget_list_object_append(hbox, tb, 1, 1, 0.0);

   // resize & show the dialog
   e_widget_size_min_get(hbox, &w, &h);
   e_dialog_content_set(dia, hbox, w, h);
   e_dialog_show(dia);

   eina_strbuf_free(strbuf);
}

static void
edgar_mouse_down3_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   E_Gadcon_Client *gcc = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Edgar_Py_Gadget *gadget = gcc->data;

   if (ev->button == 3)
   {
      E_Zone *zone;
      E_Menu *m;
      int x, y;

#if E_VERSION_MAJOR >= 20
      zone = e_zone_current_get();
#else
      zone = e_util_zone_current_get(e_manager_current_get());
#endif
      m = e_menu_new();

      E_Menu_Item *mi;
      mi = e_menu_item_new(m);
      e_menu_item_label_set(mi, "Gadget info");
      e_util_menu_item_theme_icon_set(mi, "help-about");
      e_menu_item_callback_set(mi, edgar_menu_info_cb, gadget);

      m = e_gadcon_client_util_menu_items_append(gcc, m, 0);

      e_gadcon_canvas_zone_geometry_get(gcc->gadcon, &x, &y, NULL, NULL);
      e_menu_activate_mouse(m, zone, x + ev->output.x, y + ev->output.y,
                            1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
      evas_event_feed_mouse_up(gcc->gadcon->evas, ev->button,
                               EVAS_BUTTON_NONE, ev->timestamp, NULL);
   }
}

static void
edgar_mouse_down1_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   E_Gadcon_Client *gcc = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Edgar_Py_Gadget *gadget = gcc->data;
   E_Gadcon_Popup *popup;

   if (ev->button == 1)
   {
      if ((popup = evas_object_data_get(obj, "popup")))
      {
         //E_FREE_FUNC(popup, e_object_del);
         E_FN_DEL( e_object_del, popup);
         evas_object_data_del(obj, "popup");
      }
      else if ((popup = edgar_popup_new(gadget, gcc)))
      {
         evas_object_data_set(obj, "popup", popup);
      }
   }
}


/*****************************************************************************/
/*****  Gadcon IFace   *******************************************************/
/*****************************************************************************/

static E_Gadcon_Client *
_edgar_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Edgar_Py_Gadget *gadget;
   Evas_Object *obj;
   const char *group;
   Eina_Bool pop_on_desk = EINA_FALSE;

   gadget = eina_hash_find(edgar_gadgets, name);
   if (!gadget) return NULL;
   
   DBG("EDGAR: Gadcon Init name: %s id: %s (gc orient: %d) (edjefile: %s",
       name, id, gc->orient, gadget->edjefile);

   // create the python Gadget class instance (if not done yet)
   if (!gadget->instance)
   {
      DBG("EDGAR:   Instantiate the python class");
      gadget->instance = PyObject_CallMethod(gadget->mod, "Gadget", "");
      PY_ON_ERROR_RETURN(!gadget->instance, NULL, "Cannot create the Gadget instance");
   }

   // do we want the popup expanded on desktop ?
   if (gc->location->site == E_GADCON_SITE_DESKTOP && gadget->opt_pop_on_desk)
      pop_on_desk = EINA_TRUE;

   // create the edje object (popup or main)
   obj = edje_object_add(gc->evas);
   group = pop_on_desk ? "popup" : "main";
   if (!edgar_theme_object_set(gadget, obj, group))
   {
      DBG("EDGAR:   ERROR, cannot find a theme for the gadget: '%s'", name);
      evas_object_del(obj);
      return NULL;
   }

   // create the Gadcon_Client
   E_Gadcon_Client *gcc = e_gadcon_client_new(gc, name, id, style, obj);
   gcc->data = gadget;

   // setup default callbacks on object (right-click for menu, left for popup)
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN,
                                  edgar_mouse_down3_cb, gcc);
   if (!pop_on_desk)
      evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN,
                                     edgar_mouse_down1_cb, gcc);


   // notify the gadget about the new obj
   PyObject *pyobj = object_from_instance(obj);
   PyObject *ret = NULL;
   if (pop_on_desk)
   {
      ret = PyObject_CallMethod(gadget->instance, "popup_created",
                                "(S)", pyobj);
      PY_ON_ERROR_RETURN(!ret, NULL, "Cannot call popup_created()");
   }
   else
   {
      ret = PyObject_CallMethod(gadget->instance, "instance_created",
                                "(Si)", pyobj, gc->location->site);
      PY_ON_ERROR_RETURN(!ret, NULL, "Cannot call instance_created()");
   }
   Py_XDECREF(ret);
   Py_XDECREF(pyobj);

   return gcc;
}

static void
_edgar_gc_shutdown(E_Gadcon_Client *gcc)
{
   Edgar_Py_Gadget *gadget = gcc->data;
   PyObject *pyobj, *ret;

   if (!gcc || !gcc->o_base)
      return;

   DBG("EDGAR: Gadcon Shutdown. NAME: %s, ID: %d", gcc->name, gcc->id);

   pyobj = object_from_instance(gcc->o_base);
   if (gcc->gadcon->location->site == E_GADCON_SITE_DESKTOP &&
       gadget->opt_pop_on_desk)
   {
      // call the popup_destroyed() method of the gadget.
      ret = PyObject_CallMethod(gadget->instance, "popup_destroyed",
                                "(S)", pyobj);
      PY_ON_ERROR_RETURN(!ret, , "Cannot call popup_destroyed()");
   }
   else
   {
      // call the instance_destroyed() method of the gadget.
      ret = PyObject_CallMethod(gadget->instance, "instance_destroyed",
                                "(S)", pyobj);
      PY_ON_ERROR_RETURN(!ret, , "Cannot call instance_destroyed()");
   }
   Py_XDECREF(ret);
   Py_XDECREF(pyobj);

   // destroy the object
   evas_object_del(gcc->o_base);
   gcc->o_base = NULL;
}

static void
_edgar_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   Edgar_Py_Gadget *gadget = gcc->data;
   E_Gadcon_Orient generic;
   Evas_Coord w, h, mw, mh;
   Evas_Aspect_Control aspect;

   DBG("EDGAR: Gadcon Orient: %d", orient);
   switch (orient)
   {
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
         generic = E_GADCON_ORIENT_HORIZ;
         break;
      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
         generic = E_GADCON_ORIENT_VERT;
         break;
      case E_GADCON_ORIENT_FLOAT:
      default:
         generic = E_GADCON_ORIENT_FLOAT;
         break;
   }

   // call the instance_orient() method of the gadget.
   PyObject *py_evas_obj = object_from_instance(gcc->o_base);
   PyObject *ret = PyObject_CallMethod(gadget->instance, "instance_orient",
                                       "(Sii)", py_evas_obj, generic, orient);
   PY_ON_ERROR_RETURN(!ret, , "Cannot call instance_orient()");
   Py_DECREF(py_evas_obj);
   Py_DECREF(ret);

   // apply obj size hints
   edje_object_message_signal_process(gcc->o_base);
   evas_object_size_hint_min_get(gcc->o_base, &mw, &mh);
   edje_object_size_min_restricted_calc(gcc->o_base, &w, &h, mw, mh);
   e_gadcon_client_min_size_set(gcc, w, h);

   // also aspect hints if requested
   evas_object_size_hint_aspect_get(gcc->o_base, &aspect, &w, &h);
   if (aspect != EVAS_ASPECT_CONTROL_NONE)
      e_gadcon_client_aspect_set(gcc, w, h);
}

static const char*
_edgar_gc_label(const E_Gadcon_Client_Class *client_class)
{
   Edgar_Py_Gadget *gadget;

   DBG("EDGAR: Gadcon Label for class: %s", client_class->name);

   gadget = eina_hash_find(edgar_gadgets, client_class->name);
   if (!gadget) return NULL;

   return gadget->label;
}

static const char*
_edgar_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   Edgar_Py_Gadget *gadget;
   char buf[256];

   gadget = eina_hash_find(edgar_gadgets, client_class->name);
   if (!gadget) return NULL;

   snprintf(buf, sizeof(buf), "ED.%s.%.0f",
            gadget->name, ecore_time_get() * 1000000);

   DBG("EDGAR: Gadcon ID New: %s", buf);
   //TODO manage config here
   return eina_stringshare_add(buf);
}

static void 
_edgar_gc_id_del(const E_Gadcon_Client_Class *client_class, const char *id)
{

}

static Evas_Object*
_edgar_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas)
{
   Edgar_Py_Gadget *gadget;
   Evas_Object *icon;

   DBG("EDGAR: Gadcon Icon for class: %s", client_class->name);

   gadget = eina_hash_find(edgar_gadgets, client_class->name);
   if (!gadget) return NULL;

   icon = edje_object_add(evas);
   if (edgar_theme_object_set(gadget, icon, "icon"))
      return icon;

   evas_object_del(icon);
   return NULL;
}


/*****************************************************************************/
/*****  Gadget Python API (the python eapi module)  **************************/
/*****************************************************************************/

#define EXCEPTION(_type_, _msg_)    \
{                                   \
   PyErr_SetString(_type_, _msg_);  \
   return NULL;                     \
}                                   \


static PyObject*
_eapi_theme_object_set(PyObject *self, PyObject *args)
{
   Edgar_Py_Gadget *gadget;
   const char *name, *group;
   Evas_Object *obj;
   PyObject *pyobj;

   if (!PyArg_ParseTuple(args, "Oss", &pyobj, &name, &group))
      return NULL;

   if (!(gadget = eina_hash_find(edgar_gadgets, name)))
      EXCEPTION(PyExc_ValueError, "name is not a valid gadget name");

   obj = instance_from_object(pyobj);
   if (!edgar_theme_object_set(gadget, obj, group))
      EXCEPTION(PyExc_RuntimeError, "cannot find group in theme");

   Py_RETURN_NONE;
}


static PyMethodDef EApiMethods[] = {
   {"theme_object_set", _eapi_theme_object_set, METH_VARARGS,
    "Load the given group from the theme file."},
   {NULL, NULL, 0, NULL}
};

static struct PyModuleDef EApiModule = {
   PyModuleDef_HEAD_INIT,
   "eapi",   /* name of module */
   NULL,     /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   EApiMethods
};


PyMODINIT_FUNC
PyInit_eapi(void)
{
   return PyModule_Create(&EApiModule);
}
