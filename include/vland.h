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

/************************************************************************//**
 * @defgroup ops-vland VLAN Daemon
 * This module is the OpenSwitch daemon that manages VLANs in an
 * OpenSwitch switch.
 * @{
 *
 * @defgroup vland_public Public Interface
 * Public API for the OpenSwitch VLAN daemon (ops-vland)
 *
 * ops-vland is responsible for managing and reporting status for VLANs
 * configured in an OpenSwitch switch.  In a tradition Open vSwitch,
 * VLANs are configured implicitly via PORT table's "tag" and "trunks"
 * columns.  There is no way to explicitly configure or control
 * individual VLAN behavior.
 *
 * To address this deficiency, OpenSwitch added a new VLAN table to the
 * OVSDB schema so that VLANs are explicitly created by user configuration
 * and can be extended to include per VLAN-specific features in the future.
 *
 * Each VLAN has administrative and operational states based on user
 * configuration and other inputs.  A VLAN can be administratively enabled
 * or disabled via "admin" column, allowing network administrators to
 * quickly shutdown all traffic on a VLAN as needed.  VLANs are only
 * configured in h/w when its operational state is "enabled".  Following
 * is a summary of possible operational states of a VLAN:
 *
 *     OPER_STATE  OPER_STATE_REASON  NOTES
 *     ----------  -----------------  -----
 *
 *     disabled    admin_down         [admin] column is set to "down"
 *                                    by an administrator.
 *
 *     disabled    no_member_port     VLAN has no member port, thus no
 *                                    traffic is flowing through it.
 *
 *     enabled     ok                 VLAN is fine and is configured in h/w.
 *
 *
 * Public APIs
 *
 *     ops-vland: OpenSwitch VLAN daemon
 *     usage: ops-vland [OPTIONS] [DATABASE]
 *     where DATABASE is a socket on which ovsdb-server is listening
 *           (default: "unix:/var/run/openvswitch/db.sock").
 *
 *     Daemon options:
 *       --detach                run in background as daemon
 *       --no-chdir              do not chdir to '/'
 *       --pidfile[=FILE]        create pidfile (default: /var/run/openvswitch/ops-vland.pid)
 *       --overwrite-pidfile     with --pidfile, start even if already running
 *
 *     Logging options:
 *       -vSPEC, --verbose=SPEC   set logging levels
 *       -v, --verbose            set maximum verbosity level
 *       --log-file[=FILE]        enable logging to specified FILE
 *                                (default: /var/log/openvswitch/ops-vland.log)
 *       --syslog-target=HOST:PORT  also send syslog msgs to HOST:PORT via UDP
 *
 *     Other options:
 *       --unixctl=SOCKET        override default control socket name
 *       -h, --help              display this help message
 *
 *
 * Available ovs-apptcl command options are:
 *
 *      coverage/show
 *      exit
 *      list-commands
 *      version
 *      ops-vland/dump
 *      vlog/disable-rate-limit [module]...
 *      vlog/enable-rate-limit  [module]...
 *      vlog/list
 *      vlog/reopen
 *      vlog/set                {spec | PATTERN:destination:pattern}
 *
 *
 * OVSDB elements usage
 *
 *  The following columns are READ by ops-vland:
 *
 *      System:cur_cfg
 *      Port:name
 *      Port:vlan_mode
 *      Port:tag
 *      Port:trunks
 *      VLAN:name
 *      VLAN:id
 *      VLAN:admin
 *
 *  The following columns are WRITTEN by ops-vland:
 *
 *      VLAN:hw_vlan_config
 *      VLAN:oper_state
 *      VLAN:oper_state_reason
 *
 * Linux Files:
 *
 *  The following files are written by ops-vland
 *
 *      /var/run/openvswitch/ops-vland.pid: Process ID for ops-vland
 *      /var/run/openvswitch/ops-vland.<pid>.ctl: Control file for ovs-appctl
 *
 * @{
 *
 * @file
 * Header for OpenSwitch VLAN daemon
 ***************************************************************************/

/** @} end of group vland_public */

#ifndef __VLAND_H__
#define __VLAND_H__

#include <dynamic-string.h>

/**************************************************************************//**
 * @details This function is called by the ops-vland main loop for processing
 * OVSDB change notifications.  It will handle any VLAN configuration
 * changes & push any new config into the h/w and update VLAN status in
 * the OVSDB.
 *****************************************************************************/
extern void vland_run(void);

/**************************************************************************//**
 * @details This function is called by the ops-vland main loop to wait for
 * any OVSDB IDL processing.
 *****************************************************************************/
extern void vland_wait(void);

/**************************************************************************//**
 * @details This function is called during ops-vland start up to initialize
 * the OVSDB IDL interface and cache all necessary tables & columns.
 *
 * @param[in] db_path - name of the OVSDB socket path.
 *****************************************************************************/
extern void vland_ovsdb_init(const char *db_path);

/**************************************************************************//**
 * @details This function is called during ops-vland shutdown to free
 * any data structures & disconnect from the OVSDB IDL interface.
 *****************************************************************************/
extern void vland_ovsdb_exit(void);

/**************************************************************************//**
 * @details This function is called when user invokes ovs-appctl "ops-vland/dump"
 * command.  Prints all ops-vland debug dump information to the console.
 *
 * @param[in] ds - dynamic string into which the output data is written.
 *****************************************************************************/
extern void vland_debug_dump(struct ds *ds);

#endif /* __VLAND_H__ */

/** @} end of group ops-vland */
