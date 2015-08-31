/*
 *Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
 *All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 */

/*************************************************************************//**
 * @ingroup vland
 *
 * @file
 * Main source file for the implementation of vland's OVSDB interface.
 *
 ****************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dynamic-string.h>
#include <vswitch-idl.h>
#include <openhalon-idl.h>
#include <openvswitch/vlog.h>
#include <hash.h>
#include <shash.h>
#include <bitmap.h>
#include <vlan-bitmap.h>

VLOG_DEFINE_THIS_MODULE(vland_ovsdb_if);

#define VALID_VID(x)  ((x)>0 && (x)<4095)

/**************************************************************************//**
 * port_data struct that contains PORT table information for a single port.
 *****************************************************************************/
struct port_data {
    char *name;
    enum ovsrec_port_vlan_mode_e vlan_mode;  /*!< "vlan_mode" column. */
    int native_vid;               /*!< "tag" column - native VLAN ID. */
    bool trunk_all_vlans;         /*!< Indicates whether this port is
                                       implicitly trunking all VLANs
                                       defined in VLAN table. */
    unsigned long *vlans_bitmap;  /*!< 'trunks' column - bitmap of
                                       VLANs in which this port is
                                       a member. */
};

/**************************************************************************//**
 * vlan_data struct that contains VLAN table information for a single VLAN.
 *****************************************************************************/
struct vlan_data {
    const struct ovsrec_vlan *idl_cfg;

    char *name;              /*!< "name" column */
    int vid;                 /*!< "id" column */
    bool any_member_exists;  /*!< True if any PORT is a member of this VLAN. */
    enum ovsrec_vlan_admin_e admin;
    enum ovsrec_vlan_oper_state_e op_state;
    enum ovsrec_vlan_oper_state_reason_e op_state_reason;
};

struct ovsdb_idl *idl;

static unsigned int idl_seqno;

static int system_configured = false;

/* Mapping of all the ports. */
static struct shash all_ports = SHASH_INITIALIZER(&all_ports);

/* Mapping of all the VLANs. */
static struct shash all_vlans = SHASH_INITIALIZER(&all_vlans);

/* Bitmap of all VLANs defined in the system. */
static unsigned long *all_vlans_bitmap;

/* Forward Declarations */
static char * vlan_mode_to_str(enum ovsrec_port_vlan_mode_e mode);
static char * vlan_admin_to_str(enum ovsrec_vlan_admin_e state);
static char * vlan_oper_state_to_str(enum ovsrec_vlan_oper_state_e state);
static char * vlan_oper_state_reason_to_str(enum ovsrec_vlan_oper_state_reason_e reason);
static struct vlan_data * vlan_lookup_by_vid(int vid);
static void update_vlan_membership(struct vlan_data *vlan_ptr);
static int handle_vlan_config(const struct ovsrec_vlan *row, struct vlan_data *vptr);

/**********************************************************************/
/*                               DEBUG                                */
/**********************************************************************/
void
vland_debug_dump(struct ds *ds)
{
    int vid;
    struct shash_node *sh_node;

    ds_put_cstr(ds, "================ Ports ================\n");
    SHASH_FOR_EACH(sh_node, &all_ports) {
        int vid;
        struct port_data *port = sh_node->data;
        ds_put_format(ds, "Port %s:\n", port->name);
        ds_put_format(ds, "  VLAN_mode=%s, native_VID=%d, trunk_all_VLANs=%s\n",
                      vlan_mode_to_str(port->vlan_mode), port->native_vid,
                      (port->trunk_all_vlans ? "true" : "false"));
        ds_put_format(ds, "  VLANs:");
        BITMAP_FOR_EACH_1(vid, VLAN_BITMAP_SIZE, port->vlans_bitmap) {
            ds_put_format(ds, " %d,", vid);
        }
        ds_put_format(ds, "\n");
    }

    ds_put_cstr(ds, "================ VLANs ================\n");
    ds_put_format(ds, "  All VLANs bitmap: ");
    BITMAP_FOR_EACH_1(vid, VLAN_BITMAP_SIZE, all_vlans_bitmap) {
        ds_put_format(ds, " %d,", vid);
    }
    ds_put_format(ds, "\n");

    SHASH_FOR_EACH(sh_node, &all_vlans) {
        struct vlan_data *vl = sh_node->data;
        ds_put_format(ds, "VLAN %d:\n", vl->vid);
        ds_put_format(ds, "  name              :%s\n", vl->name);
        ds_put_format(ds, "  admin             :%s\n", vlan_admin_to_str(vl->admin));
        ds_put_format(ds, "  oper_state        :%s\n", vlan_oper_state_to_str(vl->op_state));
        ds_put_format(ds, "  oper_state_reason :%s\n", vlan_oper_state_reason_to_str(vl->op_state_reason));
    }

} /* vland_debug_dump */

/**********************************************************************/
/*                              Ports                                 */
/**********************************************************************/

/**************************************************************************//**
 * This function parses a port's VLAN related configuration & constructs
 * a bitmap of all VLANs to which this port belongs.  Since all VLAN related
 * columns are optional in a PORT table entry, derive proper default values
 * for any missing data based on OVSDB schema definition.  Save the results
 * in the port_data structure for use later.
 *
 * @param[in] row - a table row entry in OVSDB's PORT table.
 * @param[out] port - port_data structure containing data for this port.
 *****************************************************************************/
static void
construct_vlan_bitmap(const struct ovsrec_port *row, struct port_data *port)
{
    int native_vid = -1;
    bool trunk_all_vlans = false;
    unsigned long *vbmp = NULL;
    enum ovsrec_port_vlan_mode_e vlan_mode;

    /* Get vlan_mode first. */
    if (row->vlan_mode) {
        if (strcmp(row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) == 0) {
            vlan_mode = PORT_VLAN_MODE_ACCESS;
        } else if (strcmp(row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) == 0) {
            vlan_mode = PORT_VLAN_MODE_TRUNK;
        } else if (strcmp(row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) == 0) {
            vlan_mode = PORT_VLAN_MODE_NATIVE_TAGGED;
        } else if (strcmp(row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) == 0) {
            vlan_mode = PORT_VLAN_MODE_NATIVE_UNTAGGED;
        } else {
            /* Should not happen.  Assume TRUNK mode to match bridge.c. */
            VLOG_ERR("Invalid VLAN mode %s", row->vlan_mode);
            vlan_mode = PORT_VLAN_MODE_TRUNK;
        }
    } else {
        /* 'vlan_mode' column is not specified.  Follow default rules:
         *   - If 'tag' contains a value, the port is an access port.
         *   - Otherwise, the port is a trunk port. */
        if (row->tag != NULL) {
            vlan_mode = PORT_VLAN_MODE_ACCESS;
        } else {
            vlan_mode = PORT_VLAN_MODE_TRUNK;
        }
    }

    /* Get native VID from 'tag' column.  Ignore if TRUNK mode. */
    if ((row->tag != NULL) && (vlan_mode != PORT_VLAN_MODE_TRUNK)) {
        native_vid = (int)*row->tag;
    }

    /* Get VLAN membership next. */
    if ((row->n_trunks > 0) && (vlan_mode != PORT_VLAN_MODE_ACCESS)) {
        /* 'trunks' column is not empty, and VLAN mode is one of
         * the TRUNK modes.  Construct bitmap of VLANs from 'trunks'
         * column.  This API will allocate the bitmap. */
        vbmp = vlan_bitmap_from_array(row->trunks, row->n_trunks);

    } else if (vlan_mode == PORT_VLAN_MODE_ACCESS) {
        /* Port is ACCESS mode.  Ignore 'trunks' column & allocate
         * an empty bitmap. */
        vbmp = bitmap_allocate(VLAN_BITMAP_SIZE);

    } else {
        /* 'trunks' column is empty & VLAN mode is one of the
         * TRUNK modes (trunk, native-tagged, or native-untagged).
         * This means all VLANs defined in VLAN table will be
         * configured on this port.*/
        vbmp = vlan_bitmap_clone(all_vlans_bitmap);
        trunk_all_vlans = true;
    }

    /* Finally, add in native VLAN into VLAN bitmap. */
    if (VALID_VID(native_vid)) {
        bitmap_set(vbmp, native_vid, true);
    }

    /* Done. Save new VLAN info. */
    port->vlan_mode = vlan_mode;
    port->native_vid = native_vid;
    port->vlans_bitmap = vbmp;
    port->trunk_all_vlans = trunk_all_vlans;

} /* construct_vlan_bitmap */

static int
del_old_port(struct shash_node *sh_node)
{
    int vid;
    int rc = 0;

    if (sh_node) {
        struct port_data *port = sh_node->data;

        /* Remove this port from the list of all_ports first.
         * This is needed to correctly update VLAN membership. */
        shash_delete(&all_ports, sh_node);

        /* Go through each VLAN that this port is a member of,
         * and update its configuration as necessary. */
        BITMAP_FOR_EACH_1(vid, VLAN_BITMAP_SIZE, port->vlans_bitmap) {
            struct vlan_data *vlan = vlan_lookup_by_vid(vid);
            if (vlan) {
                update_vlan_membership(vlan);
                if (handle_vlan_config(vlan->idl_cfg, vlan)) {
                    rc++;
                }
            }
        }

        // Done.  Free the rest of the structure.
        free(port->name);
        bitmap_free(port->vlans_bitmap);
        free(port);
    }

    return rc;

} /* del_old_port */

static void
add_new_port(const struct ovsrec_port *port_row)
{
    struct port_data *new_port = NULL;

    /* Allocate structure to save state information for this port. */
    new_port = xzalloc(sizeof(struct port_data));

    if (!shash_add_once(&all_ports, port_row->name, new_port)) {
        VLOG_WARN("Port %s specified twice", port_row->name);
        free(new_port);
    } else {
        new_port->name = xstrdup(port_row->name);

        /* Initialize VLANs to NULL for now. */
        new_port->native_vid = -1;
        new_port->vlans_bitmap = NULL;
        new_port->trunk_all_vlans = false;
        new_port->vlan_mode = PORT_VLAN_MODE_ACCESS;

        VLOG_DBG("Created local data for Port %s", port_row->name);
    }

} /* add_new_port */

static int
update_port_cache(void)
{
    struct shash sh_idl_ports;
    const struct ovsrec_port *row;
    struct shash_node *sh_node, *sh_next;
    int rc = 0;

    /* Collect all the ports in the DB. */
    shash_init(&sh_idl_ports);
    OVSREC_PORT_FOR_EACH(row, idl) {
        if (!shash_add_once(&sh_idl_ports, row->name, row)) {
            VLOG_WARN("port %s specified twice", row->name);
        }
    }

    /* Delete old ports. */
    SHASH_FOR_EACH_SAFE(sh_node, sh_next, &all_ports) {
        struct port_data *port = shash_find_data(&sh_idl_ports, sh_node->name);
        if (!port) {
            VLOG_DBG("Found a deleted port %s", sh_node->name);
            if (del_old_port(sh_node)) {
                rc++;
            }
        }
    }

    /* Add new ports. */
    SHASH_FOR_EACH(sh_node, &sh_idl_ports) {
        struct port_data *new_port = shash_find_data(&all_ports, sh_node->name);
        if (!new_port) {
            VLOG_DBG("Found an added port %s", sh_node->name);
            add_new_port(sh_node->data);
        }
    }

    /* Check for changes in the port row entries. */
    SHASH_FOR_EACH(sh_node, &all_ports) {
        const struct ovsrec_port *row = shash_find_data(&sh_idl_ports,
                                                        sh_node->name);
        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(row, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(row, idl_seqno)) {
            struct port_data *port = sh_node->data;
            unsigned long *modified_vlans;
            int vid;

            VLOG_DBG("Received updates for port %s", row->name);

            /* Save old VLAN bitmap first.  If this is a new port,
             * go ahead and allocate a blank bitmap for use later. */
            modified_vlans = port->vlans_bitmap;
            if (modified_vlans == NULL) {
                modified_vlans = bitmap_allocate(VLAN_BITMAP_SIZE);
            }

            /* Update bitmap of VLANs to which this PORT belongs. */
            construct_vlan_bitmap(row, port);

            /* Combine both new & old VLANs since we need to update
             * all of their status. */
            bitmap_or(modified_vlans, port->vlans_bitmap, VLAN_BITMAP_SIZE);
            BITMAP_FOR_EACH_1(vid, VLAN_BITMAP_SIZE, modified_vlans) {
                struct vlan_data *vlan = vlan_lookup_by_vid(vid);
                if (vlan) {
                    update_vlan_membership(vlan);
                    if (handle_vlan_config(vlan->idl_cfg, vlan)) {
                        rc++;
                    }
                }
            }

            /* Done. */
            bitmap_free(modified_vlans);
        }
    }

    /* Destroy the shash of the IDL ports */
    shash_destroy(&sh_idl_ports);

    return rc;

} /* update_port_cache */

/**********************************************************************/
/*                              VLANs                                 */
/**********************************************************************/
/* OPS_TODO: IDL should generate these enum-to-string conversions.  */
static char *
vlan_mode_to_str(enum ovsrec_port_vlan_mode_e mode)
{
    char *rval;

    switch (mode) {
    case PORT_VLAN_MODE_TRUNK:
        rval = OVSREC_PORT_VLAN_MODE_TRUNK;
        break;
    case PORT_VLAN_MODE_NATIVE_TAGGED:
        rval = OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED;
        break;
    case PORT_VLAN_MODE_NATIVE_UNTAGGED:
        rval = OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED;
        break;
    case PORT_VLAN_MODE_ACCESS:
    default:
        rval = OVSREC_PORT_VLAN_MODE_ACCESS;
        break;
    }
    return rval;

} /* vlan_mode_to_str */

static char *
vlan_admin_to_str(enum ovsrec_vlan_admin_e state)
{
    char *rval;

    switch (state) {
    case VLAN_ADMIN_UP:
        rval = OVSREC_VLAN_ADMIN_UP;
        break;
    case VLAN_ADMIN_DOWN:
    default:
        rval = OVSREC_VLAN_ADMIN_DOWN;
        break;
    }
    return rval;

} /* vlan_admin_to_str */

static char *
vlan_oper_state_to_str(enum ovsrec_vlan_oper_state_e state)
{
    char *rval;

    switch (state) {
    case VLAN_OPER_STATE_DOWN:
        rval = OVSREC_VLAN_OPER_STATE_DOWN;
        break;
    case VLAN_OPER_STATE_UP:
        rval = OVSREC_VLAN_OPER_STATE_UP;
        break;
    case VLAN_OPER_STATE_UNKNOWN:
    default:
        rval = OVSREC_VLAN_OPER_STATE_UNKNOWN;
        break;
    }
    return rval;

} /* vlan_oper_state_to_str */

static char *
vlan_oper_state_reason_to_str(enum ovsrec_vlan_oper_state_reason_e reason)
{
    char *rval;

    switch (reason) {
    case VLAN_OPER_STATE_REASON_ADMIN_DOWN:
        rval = OVSREC_VLAN_OPER_STATE_REASON_ADMIN_DOWN;
        break;
    case VLAN_OPER_STATE_REASON_OK:
        rval = OVSREC_VLAN_OPER_STATE_REASON_OK;
        break;
    case VLAN_OPER_STATE_REASON_NO_MEMBER_PORT:
        rval = OVSREC_VLAN_OPER_STATE_REASON_NO_MEMBER_PORT;
        break;
    case VLAN_OPER_STATE_REASON_UNKNOWN:
    default:
        rval = OVSREC_VLAN_OPER_STATE_REASON_UNKNOWN;
        break;
    }
    return rval;

} /* vlan_oper_state_reason_to_str */

static struct vlan_data *
vlan_lookup_by_vid(int vid)
{
    struct shash_node *sh_node;

    SHASH_FOR_EACH(sh_node, &all_vlans) {
        struct vlan_data *vlan = sh_node->data;
        if (vlan->vid == vid) {
            return vlan;
        }
    }
    return NULL;

} /* vlan_lookup_by_vid */

static void
parse_vlan_data(const struct ovsrec_vlan *data, struct vlan_data *vlan_ptr)
{
    /* Save a pointer to the IDL data for use later. */
    vlan_ptr->idl_cfg = data;
    vlan_ptr->name = xstrdup(data->name);
    vlan_ptr->vid = data->id;
    vlan_ptr->any_member_exists = false;
    vlan_ptr->admin = VLAN_ADMIN_DOWN;

    /* Initialize oper_state to unknown. */
    vlan_ptr->op_state = VLAN_OPER_STATE_UNKNOWN;
    vlan_ptr->op_state_reason = VLAN_OPER_STATE_REASON_UNKNOWN;

} /* parse_vlan_data */

/**************************************************************************//**
 * This function updates a VLAN's "any_member_exists" attribute by looping
 * through all ports configured in the system that references this VLAN,
 * whether explicitly via "tag" or "trunks" column, or implicitly via
 * trunking all VLANs defined in the VLAN table.  Also adds this VLAN to
 * a port's "vlans_bitmap" if it is implicitly trunking all VLANs.
 *
 * @param[in] vlan_ptr - vlan_data structure containing data for this VLAN.
 *****************************************************************************/
static void
update_vlan_membership(struct vlan_data *vlan_ptr)
{
    bool found = false;
    struct shash_node *sh_node;
    struct port_data *port;

    SHASH_FOR_EACH(sh_node, &all_ports) {
        port = sh_node->data;

        /* Add this new VLAN to any port that's implicitly trunking all VLANs. */
        if (port->trunk_all_vlans) {
            found = true;
            bitmap_set(port->vlans_bitmap, vlan_ptr->vid, true);

        } else if (bitmap_is_set(port->vlans_bitmap, vlan_ptr->vid)) {
            found = true;
            /* Do not exit here.  We need to update all other
             * ports that may be implicitly trunking all VLANs. */
        }
    }

    vlan_ptr->any_member_exists = found;

} /* update_vlan_membership */

/**************************************************************************//**
 * FUNCTION: calc_vlan_op_state_n_reason()
 *
 * This function determines a VLAN's operational state & reasons.
 *
 * Following is a complete summary of the different operational states and
 * the associated reasons for a VLAN, listed in order of priority, with
 * the highest priority values listed first.
 *
 * Note that if multiple reasons apply to a VLAN, only the highest priority
 * reason is displayed.  E.g., if a VLAN has invalid VID, and its admin
 * state is set to "down" by an administrator, then [op_state_reason]
 * will only show "admin_down". It becomes "invalid VLAN ID" after its
 * admin state is set to "up".
 *
 *     OP STATE  OP STATE REASON  NOTES
 *     --------  ---------------  -----
 *     disabled  admin_down       [admin] column is set to "down"
 *                                by an administrator.
 *
 *     disabled  no_member_port   VLAN has no member port, thus no
 *                                traffic is flowing through it.
 *
 *     NOTE: All new checks should be added above the following
 *
 *     enabled   ok               VLAN is fine and is configured in h/w.
 *
 * @param[in] new_vlan - vlan_data structure containing data for this VLAN.
 * @param[out] new_state - newly calculated oper_state for this VLAN.
 * @param[out] new_reason - newly calculated oper_state_reason for this VLAN.
 *****************************************************************************/
static void
calc_vlan_op_state_n_reason(struct vlan_data *new_vlan,
                            enum ovsrec_vlan_oper_state_e *new_state,
                            enum ovsrec_vlan_oper_state_reason_e *new_reason)
{
    enum ovsrec_vlan_oper_state_e state;
    enum ovsrec_vlan_oper_state_reason_e reason;

    /* Default to operationally disabled. */
    state  = VLAN_OPER_STATE_DOWN;
    reason = VLAN_OPER_STATE_REASON_UNKNOWN;

    /* Check for admin state first. */
    if (new_vlan->admin == VLAN_ADMIN_DOWN) {
        reason = VLAN_OPER_STATE_REASON_ADMIN_DOWN;

    /* Check if any port is configured for this VLAN. */
    } else if (!new_vlan->any_member_exists) {
        reason = VLAN_OPER_STATE_REASON_NO_MEMBER_PORT;

    /* If we get here, everything's fine. */
    } else {
        state  = VLAN_OPER_STATE_UP;
        reason = VLAN_OPER_STATE_REASON_OK;
    }

    VLOG_DBG("new_state=%s, new_reason=%s",
             vlan_oper_state_to_str(state),
             vlan_oper_state_reason_to_str(reason));

    /* Set the return values. */
    *new_state  = state;
    *new_reason = reason;

} /* calc_vlan_op_state_n_reason */

/**************************************************************************//**
 * This function handles a VLAN's updated configuration.  First, calculate
 * the VLAN's new "oper_state" and "oper_state_reason".  If there's any
 * change, update "hw_vlan_config" column accordingly in order to drive
 * the VLAN configuration into h/w.  Finally, update VLAN status columns
 * in the OVSDB.
 *
 * @param[in] row - a table row entry in OVSDB's VLAN table.
 * @param[out] vptr - vlan_data structure containing data for this VLAN.
 *****************************************************************************/
static int
handle_vlan_config(const struct ovsrec_vlan *row, struct vlan_data *vptr)
{
    struct smap hw_cfg_smap;
    enum ovsrec_vlan_oper_state_e new_state;
    enum ovsrec_vlan_oper_state_reason_e new_reason;

    VLOG_DBG("%s entry: name=%s, vid=%d, op_state=%s, op_state_reason=%s",
             __FUNCTION__, vptr->name, vptr->vid,
             vlan_oper_state_to_str(vptr->op_state),
             vlan_oper_state_reason_to_str(vptr->op_state_reason));

    if (smap_get(&row->internal_usage, VLAN_INTERNAL_USAGE_L3PORT)) {
        VLOG_DBG("%s: %s is used internally for L3 interface. Skip config",
                 __FUNCTION__, row->name);
        return 0;
    }

    /* Update VLAN's op state & reason, and update h/w
     * config & status elements as appropriate. */
    calc_vlan_op_state_n_reason(vptr, &new_state, &new_reason);

    if ((new_state == vptr->op_state) &&
        (new_reason == vptr->op_state_reason)) {
        return 0;
    }

    /* Save new state information. */
    vptr->op_state = new_state;
    vptr->op_state_reason = new_reason;

    if (VLAN_OPER_STATE_UP == vptr->op_state) {
        /* State is up.  Update hw_vlan_config to push
         * VLAN configuration info into h/w. */
        smap_init(&hw_cfg_smap);
        smap_add(&hw_cfg_smap, "enable", VLAN_HW_CONFIG_MAP_ENABLE_TRUE);
        ovsrec_vlan_set_hw_vlan_config(row, &hw_cfg_smap);

    } else {
        /* State is down.  Delete hw_vlan_config. */
        smap_init(&hw_cfg_smap);
        ovsrec_vlan_set_hw_vlan_config(row, &hw_cfg_smap);
    }

    /* Update VLAN status. */
    ovsrec_vlan_set_oper_state(row, vlan_oper_state_to_str(vptr->op_state));
    ovsrec_vlan_set_oper_state_reason(row, vlan_oper_state_reason_to_str(vptr->op_state_reason));

    /* Return non-zero to indicate need to update row data in OVSDB. */
    return 1;

} /* handle_vlan_config */

static void
add_new_vlan(struct shash_node *sh_node)
{
    struct vlan_data *new_vlan = NULL;
    const struct ovsrec_vlan *vlan_row = sh_node->data;

    /* Allocate structure to save state information for this VLAN. */
    new_vlan = xzalloc(sizeof(struct vlan_data));

    if (!shash_add_once(&all_vlans, vlan_row->name, new_vlan)) {
        VLOG_WARN("VLAN %d specified twice", (int)vlan_row->id);
        free(new_vlan);
    } else {
        /* Parse OVSDB data into internal format. */
        parse_vlan_data(vlan_row, new_vlan);

        /* Save VLAN in global bitmap. */
        bitmap_set(all_vlans_bitmap, new_vlan->vid, true);

        /* Check if any member port exists for this VLAN. */
        update_vlan_membership(new_vlan);

        VLOG_DBG("Created local data for VLAN %d", (int)vlan_row->id);
    }
} /* add_new_vlan */

static void
del_old_vlan(struct shash_node *sh_node)
{
    if (sh_node) {
        struct vlan_data *vl = sh_node->data;
        struct port_data *port;
        struct shash_node *port_node;

        /* Remove this VLAN from any port that's implicitly trunking all VLANs. */
        SHASH_FOR_EACH(port_node, &all_ports) {
            port = port_node->data;
            if (port->trunk_all_vlans) {
                bitmap_set(port->vlans_bitmap, vl->vid, false);
            }
        }
        bitmap_set(all_vlans_bitmap, vl->vid, false);
        free(vl->name);
        free(vl);
        shash_delete(&all_vlans, sh_node);
    }

} /* del_old_vlan */

static int
update_vlan_cache(void)
{
    struct vlan_data *new_vlan;
    struct shash sh_idl_vlans;
    const struct ovsrec_vlan *row;
    struct shash_node *sh_node, *sh_next;
    int rc = 0;

    /* Collect all the VLANs in the DB. */
    shash_init(&sh_idl_vlans);
    OVSREC_VLAN_FOR_EACH(row, idl) {
        if (!shash_add_once(&sh_idl_vlans, row->name, row)) {
            VLOG_WARN("VLAN %s (%d) specified twice", row->name, (int)row->id);
        }
    }

    /* Delete old VLANs. */
    SHASH_FOR_EACH_SAFE(sh_node, sh_next, &all_vlans) {
        new_vlan = shash_find_data(&sh_idl_vlans, sh_node->name);
        if (!new_vlan) {
            VLOG_DBG("Found a deleted VLAN %s", sh_node->name);
            del_old_vlan(sh_node);
        }
    }

    /* Add new VLANs. */
    SHASH_FOR_EACH(sh_node, &sh_idl_vlans) {
        new_vlan = shash_find_data(&all_vlans, sh_node->name);
        if (!new_vlan) {
            VLOG_DBG("Found an added VLAN %s", sh_node->name);
            add_new_vlan(sh_node);
        }
    }

    /* Check for changes in the VLAN row entries. */
    SHASH_FOR_EACH(sh_node, &all_vlans) {
        const struct ovsrec_vlan *row = shash_find_data(&sh_idl_vlans,
                                                        sh_node->name);
        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(row, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(row, idl_seqno)) {

            struct vlan_data *vptr = sh_node->data;

            /* The only thing that should change is optional 'admin' column. */
            vptr->admin = VLAN_ADMIN_DOWN;
            if (row->admin &&
                strcmp(OVSREC_VLAN_ADMIN_UP, row->admin) == 0) {
                vptr->admin = VLAN_ADMIN_UP;
            }

            /* Handle VLAN config update. */
            if (handle_vlan_config(row, vptr)) {
                rc++;
            }
        }
    }

    /* Destroy the shash of the IDL vlans */
    shash_destroy(&sh_idl_vlans);

    return rc;

} /* update_vlan_cache */

/**********************************************************************/
/*                              OVSDB                                 */
/**********************************************************************/

/* Create a connection to the OVSDB at db_path and create a DB cache
 * for this daemon. */
void
vland_ovsdb_init(const char *db_path)
{
    /* Initialize IDL through a new connection to the DB. */
    idl = ovsdb_idl_create(db_path, &ovsrec_idl_class, false, true);
    idl_seqno = ovsdb_idl_get_seqno(idl);
    ovsdb_idl_set_lock(idl, "ops_vland");
    ovsdb_idl_verify_write_only(idl);

    /* Cache Open_vSwitch table. */
    ovsdb_idl_add_table(idl, &ovsrec_table_open_vswitch);
    ovsdb_idl_add_column(idl, &ovsrec_open_vswitch_col_cur_cfg);

    /* Cache Port and VLAN tables and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_port);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_vlan_mode);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_tag);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_trunks);

    ovsdb_idl_add_table(idl, &ovsrec_table_vlan);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_id);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_admin);
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_internal_usage);

    /* These VLAN columns are write-only for VLAND. */
    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_hw_vlan_config);
    ovsdb_idl_omit_alert(idl, &ovsrec_vlan_col_hw_vlan_config);

    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_oper_state);
    ovsdb_idl_omit_alert(idl, &ovsrec_vlan_col_oper_state);

    ovsdb_idl_add_column(idl, &ovsrec_vlan_col_oper_state_reason);
    ovsdb_idl_omit_alert(idl, &ovsrec_vlan_col_oper_state_reason);

    /* Initialize global VLANs bitmap. */
    all_vlans_bitmap = bitmap_allocate(VLAN_BITMAP_SIZE);

} /* vland_ovsdb_init */

void
vland_ovsdb_exit(void)
{
    shash_destroy_free_data(&all_ports);
    shash_destroy_free_data(&all_vlans);
    ovsdb_idl_destroy(idl);

} /* vland_ovsdb_exit */

static int
vland_reconfigure(void)
{
    int rc = 0;
    unsigned int new_idl_seqno = ovsdb_idl_get_seqno(idl);

    if (new_idl_seqno == idl_seqno) {
        /* There was no change in the DB. */
        return 0;
    }

    /* Update Ports table cache. */
    if (update_port_cache()) {
        rc++;
    }

    /* Update VLANs table cache. */
    if (update_vlan_cache()) {
        rc++;
    }

    /* Update IDL sequence # after we've handled everything. */
    idl_seqno = new_idl_seqno;

    return rc;

} /* vland_reconfigure */

static inline void
vland_chk_for_system_configured(void)
{
    const struct ovsrec_open_vswitch *ovs_vsw = NULL;

    if (system_configured) {
        /* Nothing to do if we're already configured. */
        return;
    }

    ovs_vsw = ovsrec_open_vswitch_first(idl);

    if (ovs_vsw && ovs_vsw->cur_cfg > (int64_t) 0) {
        system_configured = true;
        VLOG_INFO("System is now configured (cur_cfg=%d).",
                  (int)ovs_vsw->cur_cfg);
    }

} /* vland_chk_for_system_configured */

void
vland_run(void)
{
    struct ovsdb_idl_txn *txn;

    /* Process a batch of messages from OVSDB. */
    ovsdb_idl_run(idl);

    if (ovsdb_idl_is_lock_contended(idl)) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

        VLOG_ERR_RL(&rl, "Another vland process is running, "
                    "disabling this process until it goes away");

        return;
    } else if (!ovsdb_idl_has_lock(idl)) {
        return;
    }

    /* Update the local configuration and push any changes to the DB.
     * Only do this after the system has been configured by CFGD, i.e.
     * table Open_vSwitch "cur_cfg" > 1. */
    vland_chk_for_system_configured();
    if (system_configured) {
        txn = ovsdb_idl_txn_create(idl);
        if (vland_reconfigure()) {
            /* Some OVSDB write needs to happen. */
            ovsdb_idl_txn_commit_block(txn);
        }
        ovsdb_idl_txn_destroy(txn);
    }

    return;

} /* vland_run */

void
vland_wait(void)
{
    ovsdb_idl_wait(idl);

} /* vland_wait */
