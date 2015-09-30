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
#include "e_mod_edgar.h"


EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION , "Edgar" };


static E_Config_DD *conf_edd = NULL;
E_Config_DD *conf_item_edd = NULL;
E_Config_DD *conf_data_edd = NULL;

Config *edgar_conf = NULL;

static void
_edgar_conf_free(Config *cfg)
{
       /* Cleanup our item list */
   //~ while (edgar_conf->conf_items)  TODO CLEANUP THE HASH like  mixer
     //~ {
        //~ Config_Item *ci = NULL;

        //~ ci = edgar_conf->conf_items->data;
        //~ edgar_conf->conf_items = eina_list_remove_list(edgar_conf->conf_items,
                                                       //~ edgar_conf->conf_items);
        //~ if (ci->id) eina_stringshare_del(ci->id);
        //~ E_FREE(ci);
     //~ }
   E_FREE(cfg);
}

EAPI void *
e_modapi_init(E_Module *m)
{
   /* Define EET Data Storage */
   conf_data_edd = E_CONFIG_DD_NEW("Data_Item", Data_Item);
   E_CONFIG_VAL(conf_data_edd, Data_Item, val_str, STR);
   E_CONFIG_VAL(conf_data_edd, Data_Item, val_int, INT);
   E_CONFIG_VAL(conf_data_edd, Data_Item, val_float, DOUBLE);
   
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
   E_CONFIG_VAL(conf_item_edd, Config_Item, id, STR);
   E_CONFIG_HASH(conf_item_edd, Config_Item, data_hash, conf_data_edd);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
   E_CONFIG_VAL(conf_edd, Config, version, INT);
   //E_CONFIG_VAL(conf_edd, Config, switch1, UCHAR); /* our var from header */
   //E_CONFIG_LIST(conf_edd, Config, conf_items, conf_item_edd); /* the list */
   E_CONFIG_HASH(conf_edd, Config, conf_items_hash, conf_item_edd); /* the hash of items */

   /* Tell E to find any existing module data. First run ? */
   edgar_conf = e_config_domain_load("module.edgar", conf_edd);
   if (edgar_conf)
   {
       // Chech if a new configuration is needed
       // TODO i18n
      if (!e_util_module_config_check(("Gadget loader"), 
                                    edgar_conf->version,
                                    MOD_CONFIG_FILE_VERSION))
        {
            _edgar_conf_free(edgar_conf);
            edgar_conf = NULL;
        }
    }

   /* if we don't have a config yet create a default one */
   if (!edgar_conf)
   {
      edgar_conf = E_NEW(Config, 1);
      edgar_conf->version = MOD_CONFIG_FILE_VERSION;
      edgar_conf->conf_items_hash = eina_hash_string_superfast_new(NULL);
      e_config_save_queue();
   }

   edgar_conf->module = m;
   edgar_init();
   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m) 
{
   edgar_shutdown();

   /* Cleanup the main config structure */
   _edgar_conf_free(edgar_conf);

   /* Clean EET */
   E_CONFIG_DD_FREE(conf_data_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   E_CONFIG_DD_FREE(conf_edd);
   
   return 1;
}

EAPI int 
e_modapi_save(E_Module *m) 
{
   e_config_domain_save("module.edgar", conf_edd, edgar_conf);
   return 1;
}
