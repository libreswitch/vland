/*
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
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
 * @ingroup cli/vtysh
 *
 * @file vlan_vty.c
 * vlan internal range <min_vlan> <max_vlan> (ascending|descending)
 * no vlan internal range
 * shoi vlan internal
 ***************************************************************************/

#include "vtysh/zebra.h"
#include "vtysh/command.h"
#include "vtysh/vty.h"
#include "vtysh/vector.h"
#include "vswitch-idl.h"
#include "openswitch-idl.h"
#include "openswitch-dflt.h"
#include "ovsdb-idl.h"
#include "openvswitch/vlog.h"
#include "vtysh/vtysh.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "vlan_vty.h"
#include "vtysh/utils/vlan_vtysh_utils.h"
#include "vtysh_ovsdb_vlan_context.h"

VLOG_DEFINE_THIS_MODULE(vtysh_vlan_cli);
extern struct ovsdb_idl *idl;

/* qsort comparator function.
 */
int
compare_nodes_by_vlan_id_in_numerical(const void *a_, const void *b_)
{
    const struct shash_node *const *a = a_;
    const struct shash_node *const *b = b_;
    uint i1=0,i2=0;

    sscanf((*a)->name,"%d",&i1);
    sscanf((*b)->name,"%d",&i2);

    if (i1 == i2)
        return 0;
    else if (i1 < i2)
        return -1;
    else
        return 1;
}

/*
 * Sorting function for vlan-id interface
 * on success, returns sorted vlan-id list.
 */
const struct shash_node **
sort_vlan_id(const struct shash *sh)
{
    if (shash_is_empty(sh)) {
        return NULL;
    } else {
        const struct shash_node **nodes;
        struct shash_node *node;

        size_t i, n;

        n = shash_count(sh);
        nodes = xmalloc(n * sizeof *nodes);
        i = 0;
        SHASH_FOR_EACH (node, sh) {
            if(node != NULL)
                nodes[i++] = node;
        }
        ovs_assert(i == n);

        qsort(nodes, n, sizeof *nodes, compare_nodes_by_vlan_id_in_numerical);
        return nodes;
    }
}


/*-----------------------------------------------------------------------------
 | Function: vlan_int_range_add
 | Responsibility: Add a vlan range to Open vSwitch table. This range is used to
 |                 assign VLAN ID's internally to L3 ports to enable L3 support
 |                 on the ASIC.
 | Parameters:
 |      min_vlan: start value of the range.
 |      max_vlan: end value of the range
 |      policy: Assignment policy for internal vlans: ascending (default),
 |              descending
 | Return:
 |      CMD_SUCCESS - Config executed successfully.
 |      CMD_OVSDB_FAILURE - DB failure.
 ------------------------------------------------------------------------------
 */
static int vlan_int_range_add(const char *min_vlan,
                              const char *max_vlan,
                              const char *policy)
{
    const struct ovsrec_system *const_row = NULL;
    struct smap other_config;
    struct ovsdb_idl_txn *status_txn = NULL;

    status_txn = cli_do_config_start();

    if (NULL == status_txn) {
        /* OPS_TODO: Generic comment. "Macro'ize" it in lib and use*/
        VLOG_ERR("[%s:%d]: Failed to create OVSDB transaction\n", __FUNCTION__, __LINE__);
        cli_do_config_abort(NULL);
        return CMD_OVSDB_FAILURE;
    }

    /* There will be only one row in System table */
    const_row = ovsrec_system_first(idl);

    if (!const_row) {
        /* OPS_TODO: Generic comment. "Macro'ize" it in lib and use */
        VLOG_ERR("[%s:%d]: Failed to retrieve a row from System table\n",
                    __FUNCTION__, __LINE__);

        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    /* Copy a const type to writeable other_config type and eliminate GCC
     * warnings */
    smap_clone(&other_config, &const_row->other_config);

    smap_replace(&other_config, SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN, min_vlan);
    smap_replace(&other_config, SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN, max_vlan);
    smap_replace(&other_config, SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY, policy);

    ovsrec_system_set_other_config(const_row, &other_config);

    smap_destroy(&other_config);

    if (cli_do_config_finish(status_txn)) {
        return CMD_SUCCESS;
    } else {
        return CMD_OVSDB_FAILURE;
    }
}

static struct cmd_node vlan_interface_node =
{
  VLAN_INTERFACE_NODE,
  "%s(config-if-vlan)# ",
};


static struct cmd_node vlan_node =
{
  VLAN_NODE,
  "%s(config-vlan)# ",
};

DEFUN (vtysh_interface_vlan,
       vtysh_interface_vlan_cmd,
       "interface vlan VLANID",
       "Select an interface to configure\n"
        VLAN_STR
       "Vlan id within <1-4094> and should not be an internal vlan\n")
{
   vty->node = VLAN_INTERFACE_NODE;
   static char vlan_if[MAX_IFNAME_LENGTH];

   VLANIF_NAME(vlan_if, argv[0]);

   if (!verify_ifname(vlan_if)) {
       vty->node = CONFIG_NODE;
       return CMD_ERR_NOTHING_TODO;
   }

   VLOG_DBG("%s vlan interface = %s\n", __func__, vlan_if);

   if (create_vlan_interface(vlan_if) == CMD_OVSDB_FAILURE) {
       vty->node = CONFIG_NODE;
       return CMD_ERR_NOTHING_TODO;
   }
   vty->index = vlan_if;

   return CMD_SUCCESS;
}

DEFUN (no_vtysh_interface_vlan,
       no_vtysh_interface_vlan_cmd,
       "no interface vlan VLANID",
       NO_STR
       INTERFACE_NO_STR
       "VLAN interface\n"
       "Vlan id within <1-4094> and should not be an internal vlan\n")
{
   vty->node = CONFIG_NODE;
   static char vlan_if[MAX_IFNAME_LENGTH];

   VLANIF_NAME(vlan_if, argv[0]);

   if ((verify_ifname(vlan_if) == 0)) {
       return CMD_OVSDB_FAILURE;
   }

   VLOG_DBG("%s: vlan interface = %s\n", __func__, vlan_if);

   if (delete_vlan_interface(vlan_if) == CMD_OVSDB_FAILURE) {
       return CMD_OVSDB_FAILURE;
   }
   vty->index = vlan_if;

   return CMD_SUCCESS;
}

DEFUN(vtysh_vlan,
    vtysh_vlan_cmd,
    "vlan <1-4094>",
    VLAN_STR
    "VLAN identifier\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    const struct ovsrec_bridge *bridge_row = NULL;
    const struct ovsrec_bridge *default_bridge_row = NULL;
    const struct ovsrec_port *port_row = NULL;
    bool vlan_found = false;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct ovsrec_vlan **vlans = NULL;
    int i = 0;
    int vlan_id = atoi(argv[0]);
    static char vlan[5] = { 0 };
    static char vlan_name[9] = { 0 };
    static char vlan_if[MAX_IFNAME_LENGTH];
    int64_t tag = (int64_t)vlan_id;
    snprintf(vlan, 5, "%s", argv[0]);
    snprintf(vlan_name, 9, "%s%s", "VLAN", argv[0]);

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row != NULL)
    {
        OVSREC_VLAN_FOR_EACH(vlan_row, idl)
        {
            if (vlan_row->id == vlan_id)
            {
                vlan_found = true;
                break;
            }
        }
    }

    if (vlan_found && check_if_internal_vlan(vlan_row))
    {
        /* Check for internal VLAN.
         * No configuration is allowed on internal VLANs. */
        vty_out(vty, "VLAN%ld is used as an internal VLAN. "
                "No further configuration allowed.%s", vlan_row->id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    if (!vlan_found)
    {
        status_txn = cli_do_config_start();

        if (status_txn == NULL)
        {
            VLOG_DBG("Transaction creation failed by cli_do_config_start().Function=%s, Line=%d", __func__, __LINE__);
            cli_do_config_abort(status_txn);
            vty_out(vty, "Failed to create the vlan%s", VTY_NEWLINE);
            return CMD_SUCCESS;
        }

        vlan_row = ovsrec_vlan_insert(status_txn);
        ovsrec_vlan_set_id(vlan_row, vlan_id);
        ovsrec_vlan_set_name(vlan_row, vlan_name);
        ovsrec_vlan_set_admin(vlan_row, OVSREC_VLAN_ADMIN_DOWN);
        ovsrec_vlan_set_oper_state(vlan_row, OVSREC_VLAN_OPER_STATE_DOWN);
        ovsrec_vlan_set_oper_state_reason(vlan_row, OVSREC_VLAN_OPER_STATE_REASON_ADMIN_DOWN);

        default_bridge_row = ovsrec_bridge_first(idl);
        if (default_bridge_row != NULL)
        {
            OVSREC_BRIDGE_FOR_EACH(bridge_row, idl)
            {
                if (strcmp(bridge_row->name, DEFAULT_BRIDGE_NAME) == 0)
                {
                    default_bridge_row = (struct ovsrec_bridge*)bridge_row;
                    break;
                }
            }

            if (default_bridge_row == NULL)
            {
                VLOG_DBG("Couldn't find default bridge. Function=%s, Line=%d", __func__, __LINE__);
                cli_do_config_abort(status_txn);
                vty_out(vty, "Failed to create the vlan%s", VTY_NEWLINE);
                return CMD_SUCCESS;
            }
        }

        vlans = xmalloc(sizeof(*default_bridge_row->vlans) *
            (default_bridge_row->n_vlans + 1));
        for (i = 0; i < default_bridge_row->n_vlans; i++)
        {
            vlans[i] = default_bridge_row->vlans[i];
        }
        vlans[default_bridge_row->n_vlans] = CONST_CAST(struct ovsrec_vlan*,vlan_row);
        ovsrec_bridge_set_vlans(default_bridge_row, vlans,
            default_bridge_row->n_vlans + 1);
        /* Checking for interface vlan if found add as a member of the vlan.*/
        VLANIF_NAME(vlan_if, argv[0]);
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
           if (strcmp(port_row->name, vlan_if) == 0) {
               ovsrec_port_set_tag(port_row, &tag, 1);
               ovsrec_port_set_vlan_mode(port_row, NULL);
           }
        }


        status = cli_do_config_finish(status_txn);
        free(vlans);
        if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
        {
            vty->node = VLAN_NODE;
            vty->index = (char *) vlan;
        }
        else
        {
            VLOG_DBG("Transaction failed to create vlan. Function:%s, LINE:%d", __func__, __LINE__);
            vty_out(vty, "Failed to create the vlan%s", VTY_NEWLINE);
            return CMD_SUCCESS;
        }
    }
    else
    {
        vty->node = VLAN_NODE;
        vty->index = (char *) vlan;
        return CMD_SUCCESS;
    }
    return CMD_SUCCESS;
}

DEFUN(vtysh_no_vlan,
    vtysh_no_vlan_cmd,
    "no vlan <2-4094>",
    NO_STR
    VLAN_STR
    "VLAN Identifier\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_interface *ifrow= NULL;
    const struct ovsrec_bridge *bridge_row = NULL;
    const struct ovsrec_bridge *default_bridge_row = NULL;
    bool vlan_found = false;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct ovsrec_vlan **vlans = NULL;
    int i = 0, n = 0;
    int vlan_id = atoi(argv[0]);

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row != NULL)
    {
        OVSREC_VLAN_FOR_EACH(vlan_row, idl)
        {
            if (vlan_row->id == vlan_id)
            {
                vlan_found = true;
                break;
            }
        }
    }

    if (vlan_found)
    {
        if (check_if_internal_vlan(vlan_row))
        {
            /* Check for internal VLAN.
             * No deletion is allowed on internal VLANs. */
            vty_out(vty, "VLAN%ld is used as an internal VLAN. "
                    "Deletion not allowed.%s", vlan_row->id, VTY_NEWLINE);
            return CMD_SUCCESS;
        }

        status_txn = cli_do_config_start();

        if (status_txn == NULL)
        {
            VLOG_DBG("Trasaction creation failed by cli_do_config_start().Function=%s, Line=%d", __func__, __LINE__);
            cli_do_config_abort(status_txn);
            vty_out(vty, "Failed to create the vlan%s", VTY_NEWLINE);
            return CMD_SUCCESS;
        }

        default_bridge_row = ovsrec_bridge_first(idl);
        if (default_bridge_row != NULL)
        {
            OVSREC_BRIDGE_FOR_EACH(bridge_row, idl)
            {
                if (strcmp(bridge_row->name, DEFAULT_BRIDGE_NAME) == 0)
                {
                    default_bridge_row = (struct ovsrec_bridge*)bridge_row;
                    break;
                }
            }

            if (default_bridge_row == NULL)
            {
                VLOG_DBG("Couldn't find default bridge. Function=%s, Line=%d", __func__, __LINE__);
                cli_do_config_abort(status_txn);
                vty_out(vty, "Failed to create the vlan%s", VTY_NEWLINE);
                return CMD_SUCCESS;
            }
        }

        vlans = xmalloc(sizeof(*default_bridge_row->vlans) *
            (default_bridge_row->n_vlans - 1));
        for (i = n = 0; i < default_bridge_row->n_vlans; i++)
        {
            if (vlan_row != default_bridge_row->vlans[i])
            {
                vlans[n++] = default_bridge_row->vlans[i];
            }
        }
        ovsrec_bridge_set_vlans(default_bridge_row, vlans,
            default_bridge_row->n_vlans - 1);

        vlan_found = 0;
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            int64_t* trunks = NULL;
            int trunk_count = port_row->n_trunks;
            for (i = 0; i < port_row->n_trunks; i++)
            {
                if (vlan_id == port_row->trunks[i])
                {
                    vlan_found = 1;
                    trunks = xmalloc(sizeof *port_row->trunks * (port_row->n_trunks - 1));
                    for (i = n = 0; i < port_row->n_trunks; i++)
                    {
                        if (vlan_id != port_row->trunks[i])
                        {
                            trunks[n++] = port_row->trunks[i];
                        }
                    }
                    trunk_count = port_row->n_trunks - 1;
                    ovsrec_port_set_trunks(port_row, trunks, trunk_count);
                    free(trunks);
                    break;
                }
            }
            if (port_row->n_tag == 1 && *port_row->tag == vlan_id) {
                vlan_found = 1;
            }

            if (vlan_found)
            {
                int64_t* tag = NULL;
                int tag_count = 0;
                if ( trunk_count ) {
                    ovsrec_port_set_vlan_mode(port_row, OVSREC_PORT_VLAN_MODE_TRUNK);
                    ovsrec_port_set_tag(port_row, tag, tag_count);
                } else {
                      OVSREC_INTERFACE_FOR_EACH(ifrow, idl) {
                         if (strcmp(ifrow->name, port_row->name) == 0) {
                              break;
                         }
                      }
                      if ((ifrow != NULL) &&
                         (strcmp(ifrow->type, OVSREC_INTERFACE_TYPE_SYSTEM) == 0)) {
                          ovsrec_port_set_vlan_mode(port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
                          tag = xmalloc(sizeof *port_row->tag);
                          tag_count = 1;
                          tag[0] = DEFAULT_VLAN;
                          ovsrec_port_set_tag(port_row, tag, tag_count);
                          free(tag);
                      }
                  }
            }
            vlan_found = 0;
        }

        ovsrec_vlan_delete(vlan_row);

        status = cli_do_config_finish(status_txn);
        free(vlans);
        if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
        {
            return CMD_SUCCESS;
        }
        else
        {
            VLOG_DBG("Transaction failed to delete vlan. Function:%s, LINE:%d", __func__, __LINE__);
            vty_out(vty, "Failed to delete the vlan%s", VTY_NEWLINE);
            return CMD_SUCCESS;
        }
    }
    else
    {
        vty_out(vty, "Couldn't find the VLAN %d. Make sure it's configured%s", vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

/* vlan internal configuration command */
DEFUN  (cli_vlan_int_range_add,
        cli_vlan_int_range_add_cmd,
        "vlan internal range <2-4094> <2-4094> (ascending|descending)",
        VLAN_STR
        VLAN_INT_STR
        VLAN_INT_RANGE_STR
        "Start VLAN, between 2 and 4094\n"
        "End VLAN, between Start VLAN and 4094\n"
        "Assign VLANs in ascending order (Default)\n"
        "Assign VLANs in descending order\n")
{
    uint16_t    min_vlan, max_vlan;
    char vlan_policy_str[VLAN_POLICY_STR_LEN];

    /* argv[0] = min/start VLAN ID
     * argv[1] = max/end VLAN ID
     * argv[2] = ascending or descending
     */
    min_vlan = atoi(argv[0]);
    max_vlan = atoi(argv[1]);

    /* argv[2] contains user input as is (so, partial word is possible). Convert
     * it to full policy name */
    if (*argv[2] == 'a') {
        strncpy(vlan_policy_str,
                SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY_ASCENDING_DEFAULT,
                VLAN_POLICY_STR_LEN);
    } else {
        strncpy(vlan_policy_str,
                SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY_DESCENDING,
                VLAN_POLICY_STR_LEN);
    }

    /* invalid range, log an error and notify user (in CLI) */
    if (max_vlan < min_vlan) {
        vty_out(vty, "Invalid VLAN range. End VLAN must be greater or equal to start VLAN.\n");
        return CMD_SUCCESS;
    }

    return vlan_int_range_add(argv[0], argv[1], vlan_policy_str);
}

/*-----------------------------------------------------------------------------
 | Function: vlan_int_range_del
 | Responsibility: Remove an internal vlan range to Open vSwitch table. Instead, insert a
 |                 default range. VLAN in this range are assigned to interfaces
 |                 in ascending order, by default.
 | Parameters:
 | Return:
 |      CMD_SUCCESS - Config executed successfully.
 |      CMD_OVSDB_FAILURE - DB failure.
 ------------------------------------------------------------------------------
 */
static int vlan_int_range_del(uint16_t min_vlan_in,
                              uint16_t max_vlan_in)
{
    const struct ovsrec_system *const_row = NULL;
    struct smap other_config;
    char min_vlan[VLAN_ID_LEN], max_vlan[VLAN_ID_LEN];
    struct ovsdb_idl_txn *status_txn = NULL;
    uint16_t min_vlan_db, max_vlan_db;

    status_txn = cli_do_config_start();

    if (NULL == status_txn) {
        VLOG_ERR("[%s:%d]: Failed to create OVSDB transaction\n", __FUNCTION__, __LINE__);
        cli_do_config_abort(NULL);
        return CMD_OVSDB_FAILURE;
    }

    const_row = ovsrec_system_first(idl);

    if (!const_row) {
        VLOG_ERR("[%s:%d]: Failed to retrieve a row from System table\n",
                    __FUNCTION__, __LINE__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    /* Copy a const data type to writeable type, to 'honor' smap_clone
     * symantics and avoid GCC warnigns */
    smap_clone(&other_config, &const_row->other_config);

    min_vlan_db  = smap_get_int(&const_row->other_config,
                                SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN,
                                INTERNAL_VLAN_ID_INVALID);

    max_vlan_db  = smap_get_int(&const_row->other_config,
                                SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN,
                                INTERNAL_VLAN_ID_INVALID);

    /* Checking if the input VLAN range is same as the one configured, or
     * there is no range provided as an input */
    if((min_vlan_db == min_vlan_in && max_vlan_db == max_vlan_in) ||
       (min_vlan_in == 0 && max_vlan_in == 0)) {
        snprintf(min_vlan, VLAN_ID_LEN, "%d",
                 DFLT_SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN_ID);

        smap_replace(&other_config,
                     SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN,
                     min_vlan);

        snprintf(max_vlan, VLAN_ID_LEN, "%d",
                 DFLT_SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN_ID);

        smap_replace(&other_config,
                     SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN,
                     max_vlan);

        smap_replace(&other_config,
               SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY,
               SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY_ASCENDING_DEFAULT);
    } else {
        vty_out(vty, "Invalid internal VLAN range specified.Configured "
                     "internal VLAN range: %d-%d%s",
                      min_vlan_db, max_vlan_db, VTY_NEWLINE);
    }

    ovsrec_system_set_other_config(const_row, &other_config);

    smap_destroy(&other_config);

    if (cli_do_config_finish(status_txn)) {
        return CMD_SUCCESS;
    } else {
        return CMD_OVSDB_FAILURE;
    }
}

/* Deleting vlan internal configuration. Default config takes effect */
DEFUN  (cli_vlan_int_range_del,
        cli_vlan_int_range_del_cmd,
        "no vlan internal range",
        NO_STR
        VLAN_STR
        VLAN_INT_STR
        VLAN_INT_RANGE_STR)
{
    return vlan_int_range_del(0, 0);
}

/* Deleting vlan internal configuration. Default config takes effect */
DEFUN  (cli_vlan_int_range_del_arg,
        cli_vlan_int_range_del_cmd_arg,
        "no vlan internal range <2-4094> <2-4094> (ascending|descending)",
        NO_STR
        VLAN_STR
        VLAN_INT_STR
        VLAN_INT_RANGE_STR
        "Start VLAN, between 2 and 4094\n"
        "End VLAN, between Start VLAN and 4094\n"
        "Assign VLANs in ascending order (Default)\n"
        "Assign VLANs in descending order\n")
{
    uint16_t min_vlan, max_vlan;

    min_vlan = atoi(argv[0]);
    max_vlan = atoi(argv[1]);

    /* invalid range, log an error and notify user (in CLI) */
    if (max_vlan < min_vlan) {
        vty_out(vty, "Invalid VLAN range. End VLAN must be greater or equal "
                      "to start VLAN.%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    return vlan_int_range_del(min_vlan, max_vlan);
}


/*-----------------------------------------------------------------------------
 | Function: show_vlan_int_range
 | Responsibility: Handle 'show vlan internal' command
 | Parameters:
 | Return:
 |      CMD_SUCCESS - Config executed successfully.
 |      CMD_OVSDB_FAILURE - DB failure.
 ------------------------------------------------------------------------------
 */
static int show_vlan_int_range()
{
    const struct ovsrec_system *const_row = NULL;
    const char *policy;
    uint16_t   min_vlan, max_vlan;

    struct shash sorted_vlan_port;
    const struct shash_node **nodes;
    int idx, count;

    /* VLAN info on port */
    const struct ovsrec_port *port_row = NULL;
    const char *port_vlan_str;

    const_row = ovsrec_system_first(idl);

    if (!const_row) {
        VLOG_ERR("[%s:%d]: Failed to retrieve a row from System table\n",
                    __FUNCTION__, __LINE__);
        return CMD_OVSDB_FAILURE;
    }

    /* Get values associated with internal vlan. */
    min_vlan  = smap_get_int(&const_row->other_config,
                            SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN,
                            INTERNAL_VLAN_ID_INVALID);

    max_vlan  = smap_get_int(&const_row->other_config,
                            SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN,
                            INTERNAL_VLAN_ID_INVALID);

    policy    = smap_get(&const_row->other_config,
                         SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY);

    if ((min_vlan == INTERNAL_VLAN_ID_INVALID) ||
        (max_vlan == INTERNAL_VLAN_ID_INVALID) ||
        (NULL == policy)) {
        /* Internal VLAN range is not explicitly configured. Use default
         * values */
        min_vlan = DFLT_SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN_ID;
        max_vlan = DFLT_SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN_ID;
        policy   = SYSTEM_OTHER_CONFIG_MAP_INTERNAL_VLAN_POLICY_ASCENDING_DEFAULT;
    }

    vty_out(vty, "\nInternal VLAN range  : %d-%d\n", min_vlan, max_vlan);
    vty_out(vty, "Internal VLAN policy : %s\n", policy);
    vty_out(vty, "------------------------\n");

    vty_out(vty, "Assigned Interfaces:\n");
    vty_out(vty, "\t%-4s\t\t%-16s\n", "VLAN","Interface");
    vty_out(vty, "\t%-4s\t\t%-16s\n", "----","---------");

    shash_init(&sorted_vlan_port);

    OVSREC_PORT_FOR_EACH(port_row, idl) {
        port_vlan_str = smap_get(&port_row->hw_config, PORT_HW_CONFIG_MAP_INTERNAL_VLAN_ID);

        if (port_vlan_str == NULL) {
            continue;
        } else {
            shash_add(&sorted_vlan_port, port_vlan_str, (void *)port_row);
        }
    }

    count = shash_count(&sorted_vlan_port);
    nodes = sort_vlan_id(&sorted_vlan_port);
    for (idx = 0; idx < count; idx++) {
        port_row = (const struct ovsrec_port *)nodes[idx]->data;
         port_vlan_str = smap_get(&port_row->hw_config, PORT_HW_CONFIG_MAP_INTERNAL_VLAN_ID);
        vty_out(vty, "\t%-4s\t\t%-16s\n", port_vlan_str, port_row->name);
    }

    shash_destroy(&sorted_vlan_port);
    free(nodes);

    return CMD_SUCCESS;
}

DEFUN  (cli_show_vlan_int_range,
        cli_show_vlan_int_range_cmd,
        "show vlan internal",
        SHOW_STR
        SHOW_VLAN_STR
        SHOW_VLAN_INT_STR)
{
    return show_vlan_int_range();
}


DEFUN(cli_vlan_admin,
    cli_vlan_admin_cmd,
    "shutdown",
    "Disable the VLAN\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) vty->index);

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_VLAN_SHUTDOWN_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    if (vlan_id == DEFAULT_VLAN )
    {
        VLOG_DBG("Shutdown not permitted in DEFAULT_VLAN_%d."
                 " Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, "Shutdown not permitted in DEFAULT_VLAN_%d.\n", vlan_id);
        return CMD_SUCCESS;

    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            break;
        }
    }

    ovsrec_vlan_set_admin(vlan_row, OVSREC_VLAN_ADMIN_DOWN);
    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to shutdown vlan. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_VLAN_SHUTDOWN_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_no_vlan_admin,
    cli_no_vlan_admin_cmd,
    "no shutdown",
    NO_STR
    "Disable the VLAN\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) vty->index);

    if (NULL == status_txn )
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_VLAN_NO_SHUTDOWN_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            break;
        }
    }

    ovsrec_vlan_set_admin(vlan_row, OVSREC_VLAN_ADMIN_UP);
    status = cli_do_config_finish(status_txn);
    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to enable vlan. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_VLAN_NO_SHUTDOWN_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_vlan_access,
    cli_intf_vlan_access_cmd,
    "vlan access <1-4094>",
    VLAN_STR
    "Access configuration\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_interface * tmp_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int i = 0, found_vlan = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_ACCESS_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    /* Check for internal vlan use. */
    OVSREC_INTERFACE_FOR_EACH(tmp_row, idl) {
        if (check_internal_vlan(atoi(argv[0])) == 0)
        {
            vty_out(vty, "Error : Vlan ID is an internal vlan.%s", VTY_NEWLINE);
	    cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (NULL == port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (NULL == vlan_port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to set access VLAN. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (NULL == vlan_row)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            found_vlan = 1;
            break;
        }
    }

    if (0 == found_vlan)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
    int64_t* trunks = NULL;
    int trunk_count = 0;
    ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    int64_t* tag = NULL;
    tag = xmalloc(sizeof *vlan_port_row->tag);
    tag[0] = vlan_id;
    int tag_count = 1;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set access vlan %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_ACCESS_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_no_vlan_access,
    cli_intf_no_vlan_access_cmd,
    "no vlan access <2-4094>",
    NO_STR
    VLAN_STR
    "Access configuration\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    const struct ovsrec_vlan *vlan_row = NULL;
    enum ovsdb_idl_txn_status status;
    int i = 0;
    int vlan_id = 0;
    bool found_vlan = 0;

    if(argc > 0 && argv[0]!=NULL)
    {
        vlan_id = atoi((char *) argv[0]);
    }

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_ACCESS_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (NULL == vlan_row)
    {
        vty_out(vty, "VLAN %d not found.%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if(vlan_id != 0)
    {
        OVSREC_VLAN_FOR_EACH(vlan_row, idl)
        {
            if (vlan_row->id == vlan_id)
            {
                found_vlan = 1;
                break;
            }
        }

        if (!found_vlan)
        {
            vty_out(vty, "VLAN %d is not configured.%s", vlan_id, VTY_NEWLINE);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (NULL == port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (NULL == vlan_port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to remove access VLAN. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (NULL == vlan_port_row->vlan_mode || (NULL != vlan_port_row->vlan_mode &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) != 0))
    {
        vty_out(vty, "Interface is not in access mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_id != 0 && vlan_port_row->tag[0] != vlan_id)
    {
        vty_out(vty, "VLAN %d is not configured in interface access mode.%s",
                 vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
    int64_t* trunks = NULL;
    int trunk_count = 0;
    ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    int64_t* tag = xmalloc(sizeof *port_row->tag);
    int tag_count = 1;

    tag[0] = DEFAULT_VLAN;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove access vlan. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_ACCESS_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_no_vlan_access_val,
    cli_intf_no_vlan_access_cmd_val,
    "no vlan access",
    NO_STR
    VLAN_STR
    "Access configuration\n")
{
    return cli_intf_no_vlan_access(self, vty, vty_flags, argc, argv);
}

DEFUN(cli_intf_vlan_trunk_allowed,
    cli_intf_vlan_trunk_allowed_cmd,
    "vlan trunk allowed <A:1-4094>",
    VLAN_STR
    TRUNK_STR
    "Allowed VLANs on the trunk port\n"
    "VLAN identifier range. [2, 2-10 or 2,3,4 or 2,3-10]\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct range_list *temp_to_free, *temp_to_display, *list = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    const struct ovsrec_system *const_row = NULL;
    enum ovsdb_idl_txn_status status;

    int vlan_id = 0;
    int max_vlan = 0;
    int min_vlan = 0;

    char *in = xmalloc ((strlen(argv[0]) + 1) * sizeof (char));
    strncpy(in, argv[0], strlen(argv[0]) + 1);
    list = cmd_get_range_value(in, 0);
    free(in);

    if (list == NULL)
        return CMD_ERR_NO_MATCH;

    temp_to_display = temp_to_free = list;
    status_txn = cli_do_config_start();
    const_row = ovsrec_system_first(idl);

    if (!const_row) {
        VLOG_ERR("[%s:%d]: Failed to retrieve a row from System table\n",
                    __FUNCTION__, __LINE__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    min_vlan = smap_get_int(&const_row->other_config,
                    SYSTEM_OTHER_CONFIG_MAP_MIN_INTERNAL_VLAN, -1);

    max_vlan = smap_get_int(&const_row->other_config,
                    SYSTEM_OTHER_CONFIG_MAP_MAX_INTERNAL_VLAN, -1);

    while (list != NULL)
    {
        int i = 0, found_vlan = 0;
        vlan_id = atoi(list->value);

        if (NULL == status_txn)
        {
            VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
            cli_do_config_abort(status_txn);
            vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
            return CMD_SUCCESS;
        }
          /* Check for internal vlan use. */
        if (check_internal_vlan(vlan_id) == 0)
        {
            vty_out(vty, "Error : Vlan ID-%d is an internal vlan.%s",vlan_id, VTY_NEWLINE);
            list = list->link;
            continue;
        }


        char *ifname = (char *) vty->index;

        if ((vlan_id >= min_vlan) && (vlan_id <= max_vlan))
        {
            vty_out(vty, "Unable to set VLAN. VLAN %s is part of internal VLAN.%s",
                         (list->value), VTY_NEWLINE);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
        {
            if (strcmp(intf_row->name, ifname) == 0)
            {
                break;
            }
        }

        port_row = ovsrec_port_first(idl);
        if (NULL == port_row)
        {
            vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
        }
        else
        {
            OVSREC_PORT_FOR_EACH(port_row, idl)
            {
                for (i = 0; i < port_row->n_interfaces; i++)
                {
                    if (port_row->interfaces[i] == intf_row)
                    {
                        if (strcmp(port_row->name, ifname) != 0)
                        {
                            vty_out(vty, "Can't configure VLAN, interface is "
                                         "part of LAG %s.%s", port_row->name,
                                          VTY_NEWLINE);
                            cli_do_config_abort(status_txn);
                            return CMD_SUCCESS;
                        }
                        else
                        {
                            vlan_port_row = port_row;
                            break;
                        }
                    }
                }
            }
        }

        if (NULL == vlan_port_row )
        {
            vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
        }

        if (!check_iface_in_bridge(ifname))
        {
            vty_out(vty, "Failed to set allowed trunk VLAN. Disable routing on"
                         " the interface %s.%s", ifname, VTY_NEWLINE);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        vlan_row = ovsrec_vlan_first(idl);
        if (NULL == vlan_row)
        {
            vty_out(vty, "VLAN %d not found for interface %s, aborting all the"
                         " VLAN's ", vlan_id, ifname);
            while (temp_to_display->link != NULL)
            {
                vty_out (vty, "%s, ", (temp_to_display->value));
                temp_to_display = temp_to_display->link;
            }
            vty_out (vty, "%s configurations.%s", (temp_to_display->value), VTY_NEWLINE);
            temp_to_display = NULL;
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        OVSREC_VLAN_FOR_EACH(vlan_row, idl)
        {
            if (vlan_row->id == vlan_id)
            {
                found_vlan = 1;
                break;
            }
        }

        if (0 == found_vlan)
        {
            vty_out(vty, "VLAN %d not found for interface %s, aborting all the"
                         " VLAN's ", vlan_id, ifname);
            while (temp_to_display->link != NULL)
            {
                vty_out (vty, "%s, ", (temp_to_display->value));
                temp_to_display = temp_to_display->link;
            }
            vty_out (vty, "%s configurations.%s", (temp_to_display->value), VTY_NEWLINE);
            temp_to_display = NULL;
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        if (strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
                 strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
        {
            ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
        }

        int64_t* trunks = NULL;
        for (i = 0; i < vlan_port_row->n_trunks; i++)
        {
            if (vlan_id == vlan_port_row->trunks[i])
            {
                vty_out(vty, "The VLAN %d is already allowed on the interface"
                             "%s.%s", vlan_id, ifname, VTY_NEWLINE);
                status = cli_do_config_finish(status_txn);
                if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
                {
                    return CMD_SUCCESS;
                }
                else
                {
                    VLOG_DBG("Transaction failed to set allowed trunk VLAN %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
                    vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
                    return CMD_SUCCESS;
                }
            }
        }

        trunks = xmalloc(sizeof *vlan_port_row->trunks * (vlan_port_row->n_trunks + 1));
        for (i = 0; i < vlan_port_row->n_trunks; i++)
        {
            trunks[i] = vlan_port_row->trunks[i];
        }
        trunks[vlan_port_row->n_trunks] = vlan_id;
        int trunk_count = vlan_port_row->n_trunks + 1;
        ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);

        list = list->link;
        free(trunks);
    }
    status = cli_do_config_finish(status_txn);

    temp_to_free = cmd_free_memory_range_list(temp_to_free);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set allowed trunk VLAN %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_no_vlan_trunk_allowed,
    cli_intf_no_vlan_trunk_allowed_cmd,
    "no vlan trunk allowed <1-4094>",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Allowed vlans on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int i = 0, n = 0;
    bool is_vlan_found = false;
    char *ifname = (char *) vty->index;
    int64_t* trunks = NULL;
    int trunk_count = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, "Failed to remove trunk VLAN%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }


    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (NULL == port_row)
    {
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (NULL == vlan_port_row)
    {
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to remove trunk VLAN. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        vty_out(vty, "The interface is not in trunk mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    trunk_count = vlan_port_row->n_trunks;
    for (i = 0; i < vlan_port_row->n_trunks; i++)
    {
        if (vlan_id == vlan_port_row->trunks[i])
        {
            is_vlan_found = true;
            trunks = xmalloc(sizeof *vlan_port_row->trunks * (vlan_port_row->n_trunks - 1));

            for (i = n = 0; i < vlan_port_row->n_trunks; i++)
            {
                if (vlan_id != vlan_port_row->trunks[i])
                {
                    trunks[n++] = vlan_port_row->trunks[i];
                }
            }
            trunk_count = vlan_port_row->n_trunks - 1;
            ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
            free(trunks);
            break;
        }
    }

    if (is_vlan_found == false)
    {
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) == 0)
    {
        if (0 == trunk_count)
        {
            trunks = NULL;
            ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
            ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);

            int64_t* tag = xmalloc(sizeof *port_row->tag);
            int tag_count = 1;

            tag[0] = DEFAULT_VLAN;
            ovsrec_port_set_tag(vlan_port_row, tag, tag_count);
            free(tag);

        }
    }

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove trunk VLAN. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_vlan_trunk_native,
    cli_intf_vlan_trunk_native_cmd,
    "vlan trunk native <1-4094>",
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
     const struct ovsrec_interface * tmp_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int i = 0, found_vlan = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_ERROR,vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

     /* Check for internal vlan use. */
    OVSREC_INTERFACE_FOR_EACH(tmp_row, idl) {
        if (check_internal_vlan(vlan_id) == 0)
        {
            vty_out(vty, "Error : Vlan ID-%d is an internal vlan.%s",vlan_id, VTY_NEWLINE);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (NULL == vlan_port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to add native vlan. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (NULL == vlan_row)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            found_vlan = 1;
            break;
        }
    }

    if (found_vlan == 0)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (NULL == vlan_port_row->vlan_mode)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    }
    else if (strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    }

    int64_t* tag = NULL;
    tag = xmalloc(sizeof *vlan_port_row->tag);
    tag[0] = vlan_id;
    int tag_count = 1;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set native vlan %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_no_vlan_trunk_native,
    cli_intf_no_vlan_trunk_native_cmd,
    "no vlan trunk native [<1-4094>]",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port* vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int i = 0;
    int vlan_id = 0;
    if(argc > 0 && argv[0] != NULL)
    {
        vlan_id = atoi((char *) argv[0]);
    }

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (NULL == vlan_port_row)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to remove native VLAN. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if ((NULL != vlan_port_row) && vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        vty_out(vty, "The interface is not in native mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }


    if (vlan_id != 0 && vlan_port_row->tag[0] != vlan_id)
    {
        vty_out(vty, "VLAN %d is not the native vlan in this interface.%s",
                vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }


    int64_t* trunks = NULL;
    int64_t* tag = NULL;
    int trunk_count = 0;
    int tag_count = 1;
    tag = xmalloc(sizeof *vlan_port_row->tag);
    trunk_count = vlan_port_row->n_trunks;
    tag[0] = DEFAULT_VLAN;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);
    if (trunk_count)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_TRUNK);
    }
    else
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
        ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    }
    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove native VLAN. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
    free(tag);
}

DEFUN(cli_intf_vlan_trunk_native_tag,
    cli_intf_vlan_trunk_native_tag_cmd,
    "vlan trunk native tag",
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "Tag configuration on the trunk port\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int i = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (vlan_port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to set native VLAN tagging. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) == 0)
    {
        vty_out(vty, "The interface is in access mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED);

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set native VLAN tagging. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_intf_no_vlan_trunk_native_tag,
    cli_intf_no_vlan_trunk_native_tag_cmd,
    "no vlan trunk native tag",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_interface *intf_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int i = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *ifname = (char *) vty->index;

    OVSREC_INTERFACE_FOR_EACH(intf_row, idl)
    {
        if (strcmp(intf_row->name, ifname) == 0)
        {
            break;
        }
    }

    port_row = ovsrec_port_first(idl);
    if (port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }
    else
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_interfaces; i++)
            {
                if (port_row->interfaces[i] == intf_row)
                {
                    if (strcmp(port_row->name, ifname) != 0)
                    {
                        vty_out(vty, "Can't configure VLAN, interface is part of LAG %s.%s", port_row->name, VTY_NEWLINE);
                        cli_do_config_abort(status_txn);
                        return CMD_SUCCESS;
                    }
                    else
                    {
                        vlan_port_row = port_row;
                        break;
                    }
                }
            }
        }
    }

    if (vlan_port_row == NULL)
    {
        vlan_port_row = port_check_and_add(ifname, true, true, status_txn);
    }

    if (!check_iface_in_bridge(ifname))
    {
        vty_out(vty, "Failed to remove native VLAN tagging. Disable routing on the interface.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0)
    {
        vty_out(vty, "The interface is not in native-tagged mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove native VLAN tagging. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_vlan_access,
    cli_lag_vlan_access_cmd,
    "vlan access <1-4094>",
    VLAN_STR
    "Access Configuration\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int found_vlan = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_ACCESS_ERROR,vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to set access VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            found_vlan = 1;
            break;
        }
    }

    if (found_vlan == 0)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
    int64_t* trunks = NULL;
    int trunk_count = 0;
    ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    int64_t* tag = NULL;
    tag = xmalloc(sizeof *vlan_port_row->tag);
    tag[0] = vlan_id;
    int tag_count = 1;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set access VLAN %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_ACCESS_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_no_vlan_access,
    cli_lag_no_vlan_access_cmd,
    "no vlan access [<2-4094>]",
    NO_STR
    VLAN_STR
    "Access configuration\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_ACCESS_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to remove access VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if(vlan_port_row == NULL)
    {
        vty_out(vty, "Failed to find port entry.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) != 0)
    {
        vty_out(vty, "The LAG is not in access mode%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, NULL);
    int64_t* trunks = NULL;
    int trunk_count = 0;
    ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    int64_t* tag = xmalloc(sizeof *port_row->tag);
    int tag_count = 1;

    tag[0] = DEFAULT_VLAN;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove access VLAN. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_ACCESS_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_vlan_trunk_allowed,
    cli_lag_vlan_trunk_allowed_cmd,
    "vlan trunk allowed <1-4094>",
    VLAN_STR
    TRUNK_STR
    "Allowed vlans on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int i = 0, found_vlan = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id,VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to remove access VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            found_vlan = 1;
            break;
        }
    }

    if (found_vlan == 0)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if (vlan_port_row->vlan_mode == NULL)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_TRUNK);
    }
    else if (strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_TRUNK);
    }

    int64_t* trunks = NULL;
    for (i = 0; i < vlan_port_row->n_trunks; i++)
    {
        if (vlan_id == vlan_port_row->trunks[i])
        {
            vty_out(vty, "The VLAN is already allowed on the LAG.%s", VTY_NEWLINE);
            status = cli_do_config_finish(status_txn);
            if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
            {
                return CMD_SUCCESS;
            }
            else
            {
                VLOG_DBG("Transaction failed to set allowed trunk VLAN %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
                vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
                return CMD_SUCCESS;
            }
        }
    }

    trunks = xmalloc(sizeof *vlan_port_row->trunks * (vlan_port_row->n_trunks + 1));
    for (i = 0; i < vlan_port_row->n_trunks; i++)
    {
        trunks[i] = vlan_port_row->trunks[i];
    }
    trunks[vlan_port_row->n_trunks] = vlan_id;
    int trunk_count = vlan_port_row->n_trunks + 1;
    ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);

    status = cli_do_config_finish(status_txn);
    free(trunks);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set allowed trunk VLAN %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_no_vlan_trunk_allowed,
    cli_lag_no_vlan_trunk_allowed_cmd,
    "no vlan trunk allowed <1-4094>",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Allowed vlans on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int i = 0, n = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_ALLOWED_ERROR,vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to remove trunk VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if(vlan_port_row == NULL)
    {
        vty_out(vty, "Failed to find port entry.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        vty_out(vty, "The LAG is not in trunk mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    int64_t* trunks = NULL;
    int trunk_count = vlan_port_row->n_trunks;
    for (i = 0; i < vlan_port_row->n_trunks; i++)
    {
        if (vlan_id == vlan_port_row->trunks[i])
        {
            trunks = xmalloc(sizeof *vlan_port_row->trunks * (vlan_port_row->n_trunks - 1));
            for (i = n = 0; i < vlan_port_row->n_trunks; i++)
            {
                if (vlan_id != vlan_port_row->trunks[i])
                {
                    trunks[n++] = vlan_port_row->trunks[i];
                }
            }
            trunk_count = vlan_port_row->n_trunks - 1;
            ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
            break;
        }
    }

    if (strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) == 0)
    {
        if (trunk_count == 0)
        {
            trunks = NULL;
            ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
            ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);

            int64_t* tag = xmalloc(sizeof *port_row->tag);
            int tag_count = 1;

            tag[0] = DEFAULT_VLAN;
            ovsrec_port_set_tag(vlan_port_row, tag, tag_count);
            free(tag);
        }
    }

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove trunk vlan. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_ALLOWED_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_vlan_trunk_native,
    cli_lag_vlan_trunk_native_cmd,
    "vlan trunk native <1-4094>",
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "VLAN identifier\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    int vlan_id = atoi((char *) argv[0]);
    int found_vlan = 0;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to add native VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            found_vlan = 1;
            break;
        }
    }

    if (found_vlan == 0)
    {
        vty_out(vty, "VLAN %d not found%s", vlan_id, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if(vlan_port_row == NULL)
    {
        vty_out(vty, "Failed to find port entry.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode == NULL)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    }
    else if (strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    }

    int64_t* tag = NULL;
    tag = xmalloc(sizeof *vlan_port_row->tag);
    tag[0] = vlan_id;
    int tag_count = 1;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);

    status = cli_do_config_finish(status_txn);
    free(tag);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set native vlan %d. Function:%s, Line:%d", vlan_id, __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_ERROR, vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_no_vlan_trunk_native,
    cli_lag_no_vlan_trunk_native_cmd,
    "no vlan trunk native",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port* vlan_port_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to remove native VLAN. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0 &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) != 0)
    {
        vty_out(vty, "The LAG is not in native mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    int64_t* trunks = NULL;
    int tag_count = 1;
    int trunk_count = 0;
    int64_t* tag = xmalloc(sizeof *vlan_port_row->tag);
    tag[0] = DEFAULT_VLAN;
    ovsrec_port_set_tag(vlan_port_row, tag, tag_count);
    trunk_count = vlan_port_row->n_trunks;
    if (trunk_count)
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_TRUNK);
    }
    else
    {
        ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
        ovsrec_port_set_trunks(vlan_port_row, trunks, trunk_count);
    }
    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove native VLAN. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
    free(tag);
}

DEFUN(cli_lag_vlan_trunk_native_tag,
    cli_lag_vlan_trunk_native_tag_cmd,
    "vlan trunk native tag",
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "Tag configuration on the trunk port\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to set native VLAN tagging. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if(vlan_port_row == NULL)
    {
        vty_out(vty, "Failed to find port entry.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) == 0)
    {
        vty_out(vty, "The LAG is in access mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED);

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to set native VLAN tagging. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_lag_no_vlan_trunk_native_tag,
    cli_lag_no_vlan_trunk_native_tag_cmd,
    "no vlan trunk native tag",
    NO_STR
    VLAN_STR
    TRUNK_STR
    "Native VLAN on the trunk port\n"
    "Tag configuration on the trunk port\n")
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_port *vlan_port_row = NULL;
    struct ovsdb_idl_txn *status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;

    if (NULL == status_txn)
    {
        VLOG_ERR("Failed to create transaction. Function:%s, Line:%d", __func__, __LINE__);
        cli_do_config_abort(status_txn);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    char *lagname = (char *) vty->index;
    if (!check_port_in_bridge(lagname))
    {
        vty_out(vty, "Failed to remove native VLAN tagging. Disable routing on the LAG.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if (strcmp(port_row->name, lagname) == 0)
        {
            vlan_port_row = port_row;
            break;
        }
    }

    if (vlan_port_row->vlan_mode != NULL &&
        strcmp(vlan_port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) != 0)
    {
        vty_out(vty, "The LAG is not in native-tagged mode.%s", VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    ovsrec_port_set_vlan_mode(vlan_port_row, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED);
    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED)
    {
        return CMD_SUCCESS;
    }
    else
    {
        VLOG_DBG("Transaction failed to remove native VLAN tagging. Function:%s, Line:%d", __func__, __LINE__);
        vty_out(vty, OVSDB_INTF_VLAN_REMOVE_TRUNK_NATIVE_TAG_ERROR, VTY_NEWLINE);
        return CMD_SUCCESS;
    }
}

DEFUN(cli_show_vlan_summary,
    cli_show_vlan_summary_cmd,
    "show vlan summary",
    SHOW_STR
    SHOW_VLAN_STR
    "The summary of VLANs\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    int i = 0;

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "Number of existing VLANs: 0%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        i++;
    }

    vty_out(vty, "Number of existing VLANs: %d%s", i, VTY_NEWLINE);
    return CMD_SUCCESS;
}

DEFUN(cli_show_vlan,
    cli_show_vlan_cmd,
    "show vlan",
    SHOW_STR
    SHOW_VLAN_STR)
{
    const struct ovsrec_vlan *vlan_row = NULL;
    const struct ovsrec_port *port_row = NULL;
    struct shash sorted_vlan_id;
    struct shash sorted_interfaces;
    const struct shash_node **nodes;
    const struct shash_node **ports;
    int idx, count, i;
    int port_count = 0;
    char *str;
    str = xmalloc(sizeof(char) * sizeof(long int));
    const char *l3_port;

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "No vlan is configured%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    vty_out(vty, "%s", VTY_NEWLINE);
    vty_out(vty, "--------------------------------------------------------------------------------------%s", VTY_NEWLINE);
    vty_out(vty, "VLAN    Name            Status   Reason         Reserved       Interfaces%s", VTY_NEWLINE);
    vty_out(vty, "--------------------------------------------------------------------------------------%s", VTY_NEWLINE);

    shash_init(&sorted_vlan_id);

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        sprintf(str, "%ld", vlan_row->id);
        shash_add(&sorted_vlan_id, str, (void *)vlan_row);
    }

    OVSREC_PORT_FOR_EACH(port_row, idl)
    {
        if(port_row->tag != 0 || port_row->n_trunks != 0)
            port_count++;
    }

    nodes = sort_vlan_id(&sorted_vlan_id);
    count = shash_count(&sorted_vlan_id);
    for (idx = 0; idx < count; idx++)
    {
        shash_init(&sorted_interfaces);
        vlan_row = (const struct ovsrec_vlan *)nodes[idx]->data;
        struct ovsrec_port **port_nodes = NULL;
        int n = 0;
        char vlan_id[5] = { 0 };
        snprintf(vlan_id, 5, "%ld", vlan_row->id);
        vty_out(vty, "%-8s", vlan_id);
        vty_out(vty, "%-16s", vlan_row->name);
        vty_out(vty, "%-9s", vlan_row->oper_state);
        vty_out(vty, "%-15s", vlan_row->oper_state_reason);
        if(!smap_is_empty(&vlan_row->internal_usage))
            vty_out(vty, "%-15s", "l3port");
        else
            vty_out(vty, "%-15s", "");
        int count = 0, print_tag = 0;
        port_row = ovsrec_port_first(idl);

        port_nodes = xmalloc(port_count * sizeof (struct ovsrec_port));
        if (port_row != NULL)
        {
            OVSREC_PORT_FOR_EACH(port_row, idl)
            {
                print_tag = 0;
                for (i = 0; i < port_row->n_trunks; i++)
                {
                    if (vlan_row->id == port_row->trunks[i])
                    {
                        if (port_row->n_tag == 1 && *port_row->tag == vlan_row->id)
                        {
                            print_tag = 1;
                        }
                        port_nodes[n++] = (struct ovsrec_port *)port_row;
                        break;
                    }
                }
                if (print_tag == 0 && port_row->n_tag == 1 && *port_row->tag == vlan_row->id)
                {
                    port_nodes[n++] = (struct ovsrec_port *)port_row;
                }
            }
        }

        if ((l3_port = smap_get(&vlan_row->internal_usage, VLAN_INTERNAL_USAGE_L3PORT)) != NULL)
        {
            vty_out(vty, "%s", l3_port);
        }

        if(n > 0)
        {
            for(i = 0; i < n; i++)
             {
                 shash_add(&sorted_interfaces,
                          (const char *)port_nodes[i]->name, (void *)port_row);
             }
             ports = sort_interface(&sorted_interfaces);
             count = shash_count(&sorted_interfaces);

             for(int id = 0; id < count; id++)
             {
                 if(id != count-1)
                     vty_out(vty, "%s, ", ports[id]->name);
                 else
                     vty_out(vty, "%s", ports[id]->name);
             }
             free(ports);
        }
        shash_destroy(&sorted_interfaces);
        free(port_nodes);
        vty_out(vty, "%s", VTY_NEWLINE);
    }
    shash_destroy(&sorted_vlan_id);
    free(nodes);
    free(str);

    return CMD_SUCCESS;
}

DEFUN(cli_show_vlan_id,
    cli_show_vlan_id_cmd,
    "show vlan <1-4094>",
    SHOW_STR
    SHOW_VLAN_STR
    "VLAN identifier\n")
{
    const struct ovsrec_vlan *vlan_row = NULL;
    const struct ovsrec_vlan *temp_vlan_row = NULL;
    const struct ovsrec_port *port_row = NULL;
    int vlan_id = atoi((char*) argv[0]);
    int i = 0;
    const char *l3_port;

    vlan_row = ovsrec_vlan_first(idl);
    if (vlan_row == NULL)
    {
        vty_out(vty, "No vlan is configured%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    OVSREC_VLAN_FOR_EACH(vlan_row, idl)
    {
        if (vlan_row->id == vlan_id)
        {
            temp_vlan_row = vlan_row;
            break;
        }
    }

    if (temp_vlan_row == NULL)
    {
        vty_out(vty, "VLAN %d has not been configured%s", vlan_id, VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    vty_out(vty, "%s", VTY_NEWLINE);
    vty_out(vty, "--------------------------------------------------------------------------------------%s", VTY_NEWLINE);
    vty_out(vty, "VLAN    Name            Status   Reason         Reserved       Interfaces%s", VTY_NEWLINE);
    vty_out(vty, "--------------------------------------------------------------------------------------%s", VTY_NEWLINE);


    vty_out(vty, "%-8s", argv[0]);
    vty_out(vty, "%-16s", temp_vlan_row->name);
    vty_out(vty, "%-9s", temp_vlan_row->oper_state);
    vty_out(vty, "%-15s", temp_vlan_row->oper_state_reason);
    if(!smap_is_empty(&temp_vlan_row->internal_usage))
        vty_out(vty, "%-15s", "l3port");
    else
        vty_out(vty, "%-15s", "");
    int count = 0, print_tag = 0;
    port_row = ovsrec_port_first(idl);
    if (port_row != NULL)
    {
        OVSREC_PORT_FOR_EACH(port_row, idl)
        {
            for (i = 0; i < port_row->n_trunks; i++)
            {
                if (vlan_row->id == port_row->trunks[i])
                {
                    if (port_row->n_tag == 1 && *port_row->tag == vlan_row->id)
                    {
                        print_tag = 1;
                    }
                    if (count == 0)
                    {
                        vty_out(vty, "%s", port_row->name);
                        count++;
                    }
                    else
                    {
                        vty_out(vty, ", %s", port_row->name);
                    }
                }
            }
            if (print_tag == 0 && port_row->n_tag == 1 && *port_row->tag == vlan_row->id)
            {
                if (count == 0)
                {
                    vty_out(vty, "%s", port_row->name);
                    count++;
                }
                else
                {
                    vty_out(vty, ", %s", port_row->name);
                }
            }
        }

        if ((l3_port = smap_get(&vlan_row->internal_usage, VLAN_INTERNAL_USAGE_L3PORT)) != NULL)
            {
                vty_out(vty, "%s", l3_port);
            }

    }

    vty_out(vty, "%s", VTY_NEWLINE);

    return CMD_SUCCESS;
}

static void
vlan_ovsdb_init()
{
    ovsdb_idl_add_table(idl, &ovsrec_table_vlan);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_id);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_admin);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_description);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_hw_vlan_config);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_oper_state);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_oper_state_reason);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_internal_usage);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_external_ids);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_other_config);
}


/* Initialize ops-vland cli node.
 */
void cli_pre_init(void)
{
    vtysh_ret_val retval = e_vtysh_error;

    install_node (&vlan_node, NULL);
    install_node (&vlan_interface_node, NULL);
    vtysh_install_default (VLAN_NODE);
    vtysh_install_default (VLAN_INTERFACE_NODE);
    vlan_ovsdb_init();

    retval = install_show_run_config_context(e_vtysh_global_vlan_context,
                                     &vtysh_vlan_global_context_clientcallback,
                                     NULL, NULL);
    if(e_vtysh_ok != retval)
    {
        vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "Vlan golbal context unable to add vlan global" \
                           " client callback");
        assert(0);
        return;
    }
    retval = e_vtysh_error;

    retval = install_show_run_config_context(e_vtysh_vlan_context,
                                         &vtysh_vlan_context_clientcallback,
                                         &vtysh_vlan_context_init, &vtysh_vlan_context_exit);
    if(e_vtysh_ok != retval)
    {
        vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                              "Unable to add vlan context callback");
        assert(0);
    }
    return;
}

/* Initialize ops-vland cli element.
 */
void cli_post_init(void)
{
    vtysh_ret_val retval = e_vtysh_error;

    install_element (CONFIG_NODE, &no_vtysh_interface_vlan_cmd);
    install_element (VLAN_INTERFACE_NODE, &vtysh_exit_interface_cmd);
    install_element (VLAN_INTERFACE_NODE, &vtysh_end_all_cmd);
    install_element (CONFIG_NODE, &vtysh_vlan_cmd);
    install_element(CONFIG_NODE, &vtysh_no_vlan_cmd);
    install_element (CONFIG_NODE, &vtysh_interface_vlan_cmd);
    install_element(CONFIG_NODE, &cli_vlan_int_range_add_cmd);
    install_element(CONFIG_NODE, &cli_vlan_int_range_del_cmd);
    install_element(CONFIG_NODE, &cli_vlan_int_range_del_cmd_arg);
    install_element(ENABLE_NODE, &cli_show_vlan_int_range_cmd);
    install_element(ENABLE_NODE, &cli_show_vlan_summary_cmd);
    install_element(ENABLE_NODE, &cli_show_vlan_cmd);
    install_element(ENABLE_NODE, &cli_show_vlan_id_cmd);

    install_element(VLAN_NODE, &config_exit_cmd);
    install_element(VLAN_NODE, &config_end_cmd);
    install_element(VLAN_NODE, &cli_vlan_admin_cmd);
    install_element(VLAN_NODE, &cli_no_vlan_admin_cmd);

    install_element(INTERFACE_NODE, &cli_intf_vlan_access_cmd);
    install_element(INTERFACE_NODE, &cli_intf_no_vlan_access_cmd);
    install_element(INTERFACE_NODE, &cli_intf_no_vlan_access_cmd_val);
    install_element(INTERFACE_NODE, &cli_intf_vlan_trunk_allowed_cmd);
    install_element(INTERFACE_NODE, &cli_intf_no_vlan_trunk_allowed_cmd);
    install_element(INTERFACE_NODE, &cli_intf_vlan_trunk_native_cmd);
    install_element(INTERFACE_NODE, &cli_intf_no_vlan_trunk_native_cmd);
    install_element(INTERFACE_NODE, &cli_intf_vlan_trunk_native_tag_cmd);
    install_element(INTERFACE_NODE, &cli_intf_no_vlan_trunk_native_tag_cmd);

    install_element(LINK_AGGREGATION_NODE, &cli_lag_vlan_access_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_no_vlan_access_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_vlan_trunk_allowed_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_no_vlan_trunk_allowed_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_vlan_trunk_native_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_no_vlan_trunk_native_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_vlan_trunk_native_tag_cmd);
    install_element(LINK_AGGREGATION_NODE, &cli_lag_no_vlan_trunk_native_tag_cmd);

    retval = install_show_run_config_subcontext(e_vtysh_interface_context,
                                     e_vtysh_interface_context_vlan,
                                     &vtysh_intf_context_vlan_clientcallback,
                                     NULL, NULL);
    if(e_vtysh_ok != retval)
    {
        vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "Interface context unable to add vlan client callback");
        assert(0);
        return;
    }

    return;
}
