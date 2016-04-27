/*
 * Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002 Kunihiro Ishiguro
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
/****************************************************************************
 * @ingroup cli
 *
 * @file vtysh_ovsdb_vlan_context.c
 * Source for registering client callback with vlan context.
 *
 ***************************************************************************/

#include "vtysh/zebra.h"
#include "vtysh/vty.h"
#include "vtysh/vector.h"
#include "vswitch-idl.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "vtysh/utils/vlan_vtysh_utils.h"
#include "vtysh_ovsdb_vlan_context.h"
#include "vlan_vty.h"
#include "smap.h"

static struct shash sorted_vlan_id;

struct feature_sorted_list *
vtysh_vlan_context_init(void *p_private)
{
   vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
   const struct ovsrec_vlan *vlan_row;
   const struct shash_node **nodes;
   struct feature_sorted_list *sorted_list = NULL;
   int count;
   char vlan_id_str[15];

   shash_init(&sorted_vlan_id);

   OVSREC_VLAN_FOR_EACH(vlan_row, p_msg->idl)
   {
       sprintf(vlan_id_str, "%ld", vlan_row->id);
       shash_add(&sorted_vlan_id, vlan_id_str, (void *)vlan_row);
   }

   nodes = sort_vlan_id(&sorted_vlan_id);
   count = shash_count(&sorted_vlan_id);

   sorted_list = (struct feature_sorted_list *)
                 malloc (sizeof(struct feature_sorted_list));
   if (sorted_list != NULL) {
       sorted_list->nodes = nodes;
       sorted_list->count = count;
   }

   return sorted_list;
}

void
vtysh_vlan_context_exit(struct feature_sorted_list *list)
{
   shash_destroy(&sorted_vlan_id);
   free(list->nodes);
   free(list);
}

/*-----------------------------------------------------------------------------
| Function : vtysh_vlan_context_clientcallback
| Responsibility : client callback routine
| Parameters :
|     void *p_private: void type object typecast to required
| Return : e_vtysh_ok on success else e_vtysh_error
-----------------------------------------------------------------------------*/

vtysh_ret_val
vtysh_vlan_context_clientcallback(void *p_private)
{
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const struct ovsrec_vlan *vlan_row = (struct ovsrec_vlan *)p_msg->feature_row;

  if (!check_if_internal_vlan(vlan_row)) {
        vtysh_ovsdb_cli_print(p_msg, "%s %d", "vlan", vlan_row->id);
        if (strcmp(vlan_row->admin, OVSREC_VLAN_ADMIN_UP) == 0) {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "no shutdown");
        }
        if (vlan_row->description != NULL) {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%s", "", "description ",
                                                   vlan_row->description);
        }
   }

  return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_ovsdb_intftable_parse_vlan
| Responsibility : Used for VLAN related config
| Parameters :
|     const char *if_name           : Name of interface
|     vtysh_ovsdb_cbmsg_ptr p_msg   : Used for idl operations
| Return : e_vtysh_ok on success else e_vtysh_error
-----------------------------------------------------------------------------*/
static vtysh_ret_val
vtysh_ovsdb_intftable_parse_vlan(const char *if_name,
                                 vtysh_ovsdb_cbmsg_ptr p_msg)
{
  const struct ovsrec_port *port_row;
  int i;

  port_row = port_lookup(if_name, p_msg->idl);
  if (port_row == NULL)
  {
    return e_vtysh_ok;
  }
  if (port_row->vlan_mode == NULL)
  {
    return e_vtysh_ok;
  }
  else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) == 0)
  {
    if(port_row->n_tag == 1)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan access ",
                            *port_row->tag);
    }
  }
  else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) == 0)
  {
    for (i = 0; i < port_row->n_trunks; i++)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                            port_row->trunks[i]);
    }
  }
  else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED)
           == 0)
  {
    if (port_row->n_tag == 1)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk native ",
                            *port_row->tag);
    }
    for (i = 0; i < port_row->n_trunks; i++)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                            port_row->trunks[i]);
    }
  }
  else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED)
           == 0)
  {
    if (port_row->n_tag == 1)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk native ",
                            *port_row->tag);
    }
    vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "vlan trunk native tag");
    for (i = 0; i < port_row->n_trunks; i++)
    {
      vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                            port_row->trunks[i]);
    }
  }

  return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_vlan_global_context_clientcallback
| Responsibility : Verify if internal vlan range is changed
| Parameters :
|     void *p_private: void type object typecast to required
| Return : e_vtysh_ok on success else e_vtysh_error
-----------------------------------------------------------------------------*/
vtysh_ret_val
vtysh_vlan_global_context_clientcallback(void *p_private)
{
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const struct ovsrec_system *system_row;
  uint16_t min_internal_vlan_id, max_internal_vlan_id;
  const char* vlan_policy;

  system_row = ovsrec_system_first(p_msg->idl);
  if (system_row == NULL) {
     vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "Failed to get row information of system table\n");
  }
  else {
      min_internal_vlan_id = smap_get_int(
                                 &system_row->other_config,
                                 SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN,
                                 INTERNAL_VLAN_ID_INVALID);
      max_internal_vlan_id = smap_get_int(
                                 &system_row->other_config,
                                 SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN,
                                 INTERNAL_VLAN_ID_INVALID);
      vlan_policy = smap_get(&system_row->other_config,
                            SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY);
      if (vlan_policy == NULL) {
          vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                                     "Failed to fetch internal vlan policy "
                                     "from system table\n");
          return e_vtysh_ok;
      }

      if (min_internal_vlan_id != DEFAULT_INTERNAL_VLAN_MIN_VID_VALUE ||
          max_internal_vlan_id != DEFAULT_INTERNAL_VLAN_MAX_VID_VALUE ||
          strncmp(vlan_policy,
               SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY_ASCENDING_DEFAULT,
               VLAN_POLICY_STR_LEN)) {
          vtysh_ovsdb_cli_print(p_msg, "vlan internal range %d %d %s",
                                         min_internal_vlan_id,
                                         max_internal_vlan_id,
                                         vlan_policy);
      }
  }
  return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_intf_context_vlan_clientcallback
| Responsibility : Verify if interface is VLAN and parse vlan related config
| Parameters :
|     void *p_private: void type object typecast to required
| Return : e_vtysh_ok on success else e_vtysh_error
-----------------------------------------------------------------------------*/
vtysh_ret_val
vtysh_intf_context_vlan_clientcallback(void *p_private)
{
  const struct ovsrec_port *port_row;
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const struct ovsrec_interface *ifrow = NULL;

  ifrow = (struct ovsrec_interface *)p_msg->feature_row;
  port_row = port_lookup(ifrow->name, p_msg->idl);
  if (!port_row) {
    return e_vtysh_ok;
  }
  if (!check_iface_in_vrf(ifrow->name)) {
    if (!p_msg->disp_header_cfg) {
      vtysh_ovsdb_cli_print(p_msg, "interface %s", ifrow->name);
      p_msg->disp_header_cfg = true;
    }
    vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "no routing");
    vtysh_ovsdb_intftable_parse_vlan(ifrow->name, p_msg);
  }
  return e_vtysh_ok;
}
