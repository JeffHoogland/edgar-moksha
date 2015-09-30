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
#include "e_mod_guardian.h"

typedef struct {
   const char *action;
   int action_type;
   int check_state;
   Guardian_Item *inwait_item;
}Guardian_Dialog_Data;

extern Config *edgar_conf;

static Guardian_Item *
_edgar_guardian_allow_set(const char *gadget, const char *action, int allow)
{
   Guardian_Item *item;
   char key[4096];

   if (allow >= EDGAR_GUARDIAN_DENY_LAST)
     return NULL;

   snprintf(key, sizeof(key), "::%s::", gadget);
   if ((allow != EDGAR_GUARDIAN_DENY_ALL) && (allow != EDGAR_GUARDIAN_ALLOW_ALL))
     {
        // we always want an 'header' item (simplify the config list)
        if (!eina_hash_find(edgar_conf->conf_guardian_hash, &key))
          {
             item = E_NEW(Guardian_Item, 1);
             item->allow = EDGAR_GUARDIAN_ASK;
             eina_hash_add(edgar_conf->conf_guardian_hash, key, item);
          }
        snprintf(key, sizeof(key), "%s::%s", gadget, action);
     }

   item = eina_hash_find(edgar_conf->conf_guardian_hash, &key);
   if (!item)
   {
      item = E_NEW(Guardian_Item, 1);
      eina_hash_add(edgar_conf->conf_guardian_hash, key, item);
   }
   item->allow = allow;

   return item;
}

static void
_edgar_guardian_dialog_allow_cb(void *data, E_Dialog *dia)
{
   Instance *inst = data;
   Guardian_Dialog_Data *gdd;

   gdd = e_object_data_get((E_Object *)dia);
   _edgar_guardian_allow_set(inst->gcc->client_class->name, gdd->action,
            gdd->check_state ? EDGAR_GUARDIAN_ALLOW_ALL : EDGAR_GUARDIAN_ALLOW);

   e_object_del((E_Object *)dia);
}

static void
_edgar_guardian_dialog_deny_cb(void *data, E_Dialog *dia)
{
   Instance *inst = data;
   Guardian_Dialog_Data *gdd;

   gdd = e_object_data_get((E_Object *)dia);
   _edgar_guardian_allow_set(inst->gcc->client_class->name, gdd->action,
              gdd->check_state ? EDGAR_GUARDIAN_DENY_ALL : EDGAR_GUARDIAN_DENY);
   e_object_del((E_Object *)dia);
}

static void
_edgar_guardian_dialog_free_cb(void *dia)
{
   Guardian_Dialog_Data *gdd;

   gdd = e_object_data_get(dia);
   if (!gdd) return;

   if (gdd->inwait_item && gdd->inwait_item->allow == EDGAR_GUARDIAN_IN_WAIT)
     gdd->inwait_item->allow = EDGAR_GUARDIAN_ASK;

   if (gdd->action) eina_stringshare_del(gdd->action);
   E_FREE(gdd);
}

EAPI Eina_Bool
edgar_guardian_is_allowed(Instance *inst, int type, const char *action)
{
   Guardian_Item *allow;
   Guardian_Dialog_Data *gdd;
   E_Dialog *dia;
   char buf[4096];
   int w, h;

   // just to be safe, in case something goes wrong loading from config
   if (!edgar_conf->conf_guardian_hash)
      edgar_conf->conf_guardian_hash = eina_hash_string_superfast_new(NULL);

   // check in the guardian hash
   if (action && inst && inst->gcc && inst->gcc->client_class &&
       inst->gcc->client_class->name)
   {
      snprintf(buf, sizeof(buf), "::%s::", inst->gcc->client_class->name);
      allow = eina_hash_find(edgar_conf->conf_guardian_hash, &buf);
      if (!allow || allow->allow == EDGAR_GUARDIAN_ASK)
      {
         snprintf(buf, sizeof(buf), "%s::%s", inst->gcc->client_class->name, action);
         allow = eina_hash_find(edgar_conf->conf_guardian_hash, &buf);
      }
      DBG("Guardian, can exec this? '%s'", buf);

      if (allow && allow->allow != EDGAR_GUARDIAN_ASK)
      {
         switch (allow->allow)
         {
            case EDGAR_GUARDIAN_ALLOW:
            case EDGAR_GUARDIAN_ALLOW_ALL:
               DBG("Yes, you can! go on.");
               return EINA_TRUE;

            case EDGAR_GUARDIAN_DENY:
            case EDGAR_GUARDIAN_DENY_ALL:
            case EDGAR_GUARDIAN_IN_WAIT:
            default:
               DBG("NO! permission denied. (reason: %d)", allow->allow);
               return EINA_FALSE;
         }
      }
   }

   // or ask the user what to do
   gdd = E_NEW(Guardian_Dialog_Data, 1);
   if (!gdd) return EINA_FALSE;
   gdd->action = eina_stringshare_add(action);
   gdd->action_type = type;

   if (type == EDGAR_GUARDIAN_TYPE_EACTION)
      snprintf(buf, sizeof(buf),
         "The gadget \"%s\"<br>"
         "want to execute the enlightenment action:<br>"
         "<br><b>%s</b><br><br>"
         "Do I have to allow the execution?", inst->ci->id ,action);
   else if (type == EDGAR_GUARDIAN_TYPE_COMMAND)
      snprintf(buf, sizeof(buf),
         "The gadget \"%s\"<br>"
         "want to execute the shell command:<br>"
         "<br><b>%s</b><br><br>"
         "Do I have to allow the execution?", inst->ci->id ,action);

   else if (type == EDGAR_GUARDIAN_TYPE_GETURL)
      snprintf(buf, sizeof(buf),
         "The gadget \"%s\"<br>"
         "want to access the net at the url:<br>"
         "<br><b>%s</b><br><br>"
         "Do I have to allow the access?", inst->ci->id ,action);


   // dialog
#ifdef HAVE_E19
   E_Comp *con;
   con = e_manager_current_get()->comp;
#else
   E_Container *con;
   con = e_container_current_get(e_manager_current_get());
#endif
   dia = e_dialog_new(con, "can exec", "class");
   e_dialog_resizable_set(dia, EINA_TRUE);
   e_dialog_title_set(dia, "The gadget guardian");
   e_dialog_button_add(dia, "Allow", NULL, _edgar_guardian_dialog_allow_cb, inst);
   e_dialog_button_add(dia, "Deny", NULL, _edgar_guardian_dialog_deny_cb, inst);
   e_object_data_set((E_Object*)dia, gdd);
   e_object_free_attach_func_set((E_Object*)dia, _edgar_guardian_dialog_free_cb);

   // vbox
   Evas_Object *vbox;
   vbox = e_widget_list_add(dia->win->evas, 0, 0);

   // label
   Evas_Object *text;
   text = e_widget_textblock_add(dia->win->evas);
   e_widget_textblock_markup_set(text, buf);
   e_widget_size_min_set(text, 260 * e_scale, 90 * e_scale); // hmmm, :/
   e_widget_list_object_append(vbox, text, 1, 1, 0.0);
   
   // check
   Evas_Object *chk;
   chk = e_widget_check_add(dia->win->evas, "Allow or deny all the actions from this gadget", &gdd->check_state); //TODO i18n
   e_widget_list_object_append(vbox, chk, 1, 1, 0.0);

   // resize & show the dialog
   e_widget_size_min_get(vbox, &w, &h);
   e_dialog_content_set(dia, vbox, w, h);
   e_dialog_show(dia);

   // set to deny (in_wait), until the user choose or the dialog will be closed
   gdd->inwait_item = _edgar_guardian_allow_set(inst->gcc->client_class->name,
                                                action, EDGAR_GUARDIAN_IN_WAIT);
   return EINA_FALSE;
}
