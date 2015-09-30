/*  Copyright (C) 2008 Davide Andreoli (see AUTHORS)
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

#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H


/* Increment for Major Changes */
#define MOD_CONFIG_FILE_EPOCH      3
/* Increment for Minor Changes (ie: user doesn't need a new config) */
#define MOD_CONFIG_FILE_GENERATION 1
#define MOD_CONFIG_FILE_VERSION \
   ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

/* Contain whole module conf */
typedef struct {
   int version;
   E_Module *module;
   E_Config_Dialog *cfd;
   Eina_Hash *conf_items_hash;
}Config;

/* This struct used to hold config for individual gadget */
typedef struct {
   const char *id;
   Eina_Hash *data_hash;
}Config_Item;

/* This struct used to hold data for every gadgets */
typedef struct {
   const char *val_str;
   int val_int;
   double val_float;
}Data_Item;


EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);


#endif
