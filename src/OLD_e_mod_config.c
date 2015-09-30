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
 
#include <e.h>
#include "e_mod_main.h"

struct _E_Config_Dialog_Data 
{
   Eina_List *key_list;
   Evas_Object *ilist;
   Evas_Object *allow_btn, *deny_btn, *ask_btn;
};

/* Local Function Prototypes */
static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
extern Config *edgar_conf;


/* External Functions */
#ifdef HAVE_E19
EAPI E_Config_Dialog *
e_int_config_edgar_module(E_Comp *con, const char *params)
#else
EAPI E_Config_Dialog *
e_int_config_edgar_module(E_Container *con, const char *params)
#endif
{
   E_Config_Dialog_View *v = NULL;
   char buf[PATH_MAX];

   /* is this config dialog already visible ? */
   if (edgar_conf->cfd)
     {
        e_win_raise(edgar_conf->cfd->dia->win);
#ifndef HAVE_E19
        e_border_focus_set(edgar_conf->cfd->dia->win->border, 1, 1);
#endif
        return NULL;
     }

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return NULL;

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.create_widgets = _basic_create;
   v->basic.apply_cfdata = _basic_apply;

   /* Icon in the theme */
   //snprintf(buf, sizeof(buf), "%s/e-module-edgar.edj", edgar_conf->module->dir);

   /* create new config dialog */
   snprintf(buf, sizeof(buf), "%s/e-module-edgar.edj", edgar_conf->module->dir);
   edgar_conf->cfd = e_config_dialog_new(con, "Edgar Gadgets Loader",  // TODO i18n
                                         "E", "extensions/edgar",
                                         buf, 0, v, NULL);

   e_dialog_resizable_set(edgar_conf->cfd->dia, 1);

   return edgar_conf->cfd;
}

/* Local Functions */
static int
_cmp_func(const void *a, const void *b)
{
   const char *_a = a;
   const char *_b = b;
   if (_a[0] == ':' && _a[1] == ':') _a = _a + 2;
   if (_b[0] == ':' && _b[1] == ':') _b = _b + 2;
   return strcmp(_a, _b);
}

static void *
_create_data(E_Config_Dialog *cfd) 
{
   E_Config_Dialog_Data *cfdata;
   Eina_Iterator *it;
   const char *key;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   if (!cfdata) return NULL;

   // build a sorted list (with headers on top) of all the hash keys
   it = eina_hash_iterator_key_new(edgar_conf->conf_guardian_hash);
   while (eina_iterator_next(it, (void *)&key))
     cfdata->key_list = eina_list_sorted_insert(cfdata->key_list, _cmp_func,
                                                eina_stringshare_add(key));
   eina_iterator_free(it);

   return cfdata;
}

static void 
_free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata) 
{
   const char *key;

   edgar_conf->cfd = NULL;
   EINA_LIST_FREE(cfdata->key_list, key)
     eina_stringshare_del(key);
   E_FREE(cfdata);
}

static void
_list_selected_cb(void *data)
{
   E_Config_Dialog_Data *cfdata = data;
   Guardian_Item *allow;
   const char *key;

   key = e_widget_ilist_selected_value_get(cfdata->ilist);
   allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);

   if (!allow || allow->allow == EDGAR_GUARDIAN_ASK)
     {
        e_widget_disabled_set(cfdata->allow_btn, 0);
        e_widget_disabled_set(cfdata->deny_btn, 0);
        e_widget_disabled_set(cfdata->ask_btn, 1);
        return;
     }

   e_widget_disabled_set(cfdata->ask_btn, 0);
   e_widget_disabled_set(cfdata->allow_btn,
         (allow->allow == EDGAR_GUARDIAN_ALLOW) ||
         (allow->allow == EDGAR_GUARDIAN_ALLOW_ALL) ? 1 : 0);
   e_widget_disabled_set(cfdata->deny_btn,
         (allow->allow == EDGAR_GUARDIAN_ALLOW) ||
         (allow->allow == EDGAR_GUARDIAN_ALLOW_ALL) ? 0 : 1);
}

static void
_allow_btn_cb(void *data, void *data2)
{
   E_Config_Dialog_Data *cfdata = data;
   Guardian_Item *allow;
   Evas_Object *end;
   const char *key;

   key = e_widget_ilist_selected_value_get(cfdata->ilist);
   allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);
   allow->allow = ((key[0] == ':') && (key[1] == ':')) ?
                     EDGAR_GUARDIAN_ALLOW_ALL : EDGAR_GUARDIAN_ALLOW;

   e_widget_disabled_set(cfdata->allow_btn, 1);
   e_widget_disabled_set(cfdata->deny_btn, 0);
   e_widget_disabled_set(cfdata->ask_btn, 0);

   end = e_widget_ilist_selected_end_get(cfdata->ilist);
   edje_object_signal_emit(end, "e,state,checked", "e");
}

static void
_deny_btn_cb(void *data, void *data2)
{
   E_Config_Dialog_Data *cfdata = data;
   Guardian_Item *allow;
   Evas_Object *end;
   const char *key;

   key = e_widget_ilist_selected_value_get(cfdata->ilist);
   allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);
   allow->allow = ((key[0] == ':') && (key[1] == ':')) ?
                     EDGAR_GUARDIAN_DENY_ALL : EDGAR_GUARDIAN_DENY;

   e_widget_disabled_set(cfdata->allow_btn, 0);
   e_widget_disabled_set(cfdata->deny_btn, 1);
   e_widget_disabled_set(cfdata->ask_btn, 0);

   end = e_widget_ilist_selected_end_get(cfdata->ilist);
   edje_object_signal_emit(end, "e,state,alert", "e");
}

static void
_ask_btn_cb(void *data, void *data2)
{
   E_Config_Dialog_Data *cfdata = data;
   Guardian_Item *allow;
   Evas_Object *end;
   const char *key;

   key = e_widget_ilist_selected_value_get(cfdata->ilist);
   allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);
   allow->allow = EDGAR_GUARDIAN_ASK;

   e_widget_disabled_set(cfdata->allow_btn, 0);
   e_widget_disabled_set(cfdata->deny_btn, 0);
   e_widget_disabled_set(cfdata->ask_btn, 1);

   end = e_widget_ilist_selected_end_get(cfdata->ilist);
   edje_object_signal_emit(end, "e,state,unchecked", "e");
}

static void
_basic_populate_list(E_Config_Dialog_Data *cfdata)
{
   Evas_Object *icon, *end;
   Evas *evas;
   Eina_List *l;
   Guardian_Item *allow;
   const char *key;
   char gadget[1024];
   char action[4096];

   evas = evas_object_evas_get(cfdata->ilist);

   // populate the ilist with items and headers
   EINA_LIST_FOREACH(cfdata->key_list, l, key)
     {
        DBG("KEY: '%s'", key);
        if ((key[0] == ':') && (key[1] == ':'))
          {
             // extract the gadget name   '::gadget_name::'
             if (sscanf(key, "::%[^:]::", gadget) != 1) return;
             DBG("'%s'", gadget);

             // append the header
             icon = e_icon_add(evas);
             if (!e_icon_file_edje_set(icon, gadget, "e/gadget/icon"))
               {
                  evas_object_del(icon);
                  icon = NULL;
               }  
             e_widget_ilist_header_append(cfdata->ilist, icon, gadget);

             // append the 'trust all' item
             end = edje_object_add(evas);
             if (!e_theme_edje_object_set(end, "base/theme/widgets",
                                            "e/widgets/ilist/toggle_end"))
               {
                  evas_object_del(end);
                  end = NULL;
               }
             else
               {
                  edje_object_signal_emit(end, "e,state,unchecked", "e");
                  allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);
                  if (allow && allow->allow == EDGAR_GUARDIAN_ALLOW_ALL)
                    edje_object_signal_emit(end, "e,state,checked", "e");
                  else if (allow && allow->allow == EDGAR_GUARDIAN_DENY_ALL)
                    edje_object_signal_emit(end, "e,state,alert", "e");
               }
             e_widget_ilist_append_full(cfdata->ilist, NULL, end,
                                        "Trust or Deny ALL from this gadget",
                                        _list_selected_cb, cfdata, key);
          }
        else
          {
             // extract the gadget name and the action  'gadget_name::action'
             // this is not really correct, will truncate action with the char ':'
             if (sscanf(key, "%[^:]::%[^:]", gadget, action) != 2) return;
             DBG("'%s'  '%s'", gadget, action);

             // append the action
             allow = eina_hash_find(edgar_conf->conf_guardian_hash, key);
             end = edje_object_add(evas);
             if (!e_theme_edje_object_set(end, "base/theme/widgets",
                                     "e/widgets/ilist/toggle_end"))
               {
                  evas_object_del(end);
                  end = NULL;
               }
             else
               {
                  edje_object_signal_emit(end, "e,state,unchecked", "e");
                  if (allow && allow->allow == EDGAR_GUARDIAN_ALLOW)
                    edje_object_signal_emit(end, "e,state,checked", "e");
                  else if (allow && allow->allow == EDGAR_GUARDIAN_DENY)
                    edje_object_signal_emit(end, "e,state,alert", "e");
               }
             e_widget_ilist_append_full(cfdata->ilist, NULL, end, action,
                                        _list_selected_cb, cfdata, key);
          }
     }
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata) 
{
   Evas_Object *hbox, *frame, *icon;

   frame = e_widget_framelist_add(evas, "The Gadget Guardian", 0);

   // ilist
   cfdata->ilist = e_widget_ilist_add(evas, 24, 24, NULL);
   e_widget_size_min_set(cfdata->ilist, 180, 100);
   e_widget_ilist_go(cfdata->ilist);
   e_widget_framelist_object_append(frame, cfdata->ilist);

   // button box
   hbox = e_widget_list_add(evas, 1, 1);

   // allow btn
   cfdata->allow_btn = e_widget_button_add(evas, "Allow", NULL,
                                    _allow_btn_cb, cfdata, NULL);
   e_widget_disabled_set(cfdata->allow_btn, 1);
   icon = edje_object_add(evas);
   if (e_theme_edje_object_set(icon, "base/theme/widgets",
                                "e/widgets/ilist/toggle_end"))
     {
        edje_object_signal_emit(icon, "e,state,checked", "e");
        e_widget_button_icon_set(cfdata->allow_btn, icon);
     }
   e_widget_list_object_append(hbox, cfdata->allow_btn, 1, 1, 0.5);

   // deny btn
   cfdata->deny_btn = e_widget_button_add(evas, "Deny", NULL,
                                   _deny_btn_cb, cfdata, NULL);
   e_widget_disabled_set(cfdata->deny_btn, 1);
   icon = edje_object_add(evas);
   if (e_theme_edje_object_set(icon, "base/theme/widgets",
                                "e/widgets/ilist/toggle_end"))
     {
        edje_object_signal_emit(icon, "e,state,alert", "e");
        e_widget_button_icon_set(cfdata->deny_btn, icon);
     }
   e_widget_list_object_append(hbox, cfdata->deny_btn, 1, 0, 0.5);

   // ask btn
   cfdata->ask_btn = e_widget_button_add(evas, "Ask", NULL,
                                     _ask_btn_cb, cfdata, NULL);
   e_widget_disabled_set(cfdata->ask_btn, 1);
   icon = edje_object_add(evas);
   if (e_theme_edje_object_set(icon, "base/theme/widgets",
                                "e/widgets/ilist/toggle_end"))
     {
        edje_object_signal_emit(icon, "e,state,unchecked", "e");
        e_widget_button_icon_set(cfdata->ask_btn, icon);
     }
   e_widget_list_object_append(hbox, cfdata->ask_btn, 1, 0, 0.5);
   
   e_widget_framelist_object_append_full(frame, hbox,
                                    1, 0, 1, 0, 0.5, 0.5, 25, 25, 999, 999);

   _basic_populate_list(cfdata);
   return frame;
}

static int 
_basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata) 
{
   e_config_save_queue();
   return 1;
}
