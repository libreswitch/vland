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
 * @ingroup ops-vland
 *
 * @file
 * Main source file for the implementation of the OpenSwitch VLAN daemon.
 *
 *   1. During start up, read Port and VLAN related configuration
 *      data and determine the operational status of the VLANs.
 *
 *   2. Dynamically configure hardware based on operational
 *      state changes as needed.
 *
 *   3. During operations, receive administrative
 *      configuration changes and apply to the hardware.
 *
 ****************************************************************************/

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <command-line.h>
#include <compiler.h>
#include <daemon.h>
#include <dirs.h>
#include <dynamic-string.h>
#include <fatal-signal.h>
#include <ovsdb-idl.h>
#include <poll-loop.h>
#include <unixctl.h>
#include <util.h>
#include <openvswitch/vconn.h>
#include <openvswitch/vlog.h>
#include <vswitch-idl.h>
#include <openswitch-idl.h>
#include <hash.h>
#include <shash.h>
#include  <diag_dump.h>

#include "vland.h"

VLOG_DEFINE_THIS_MODULE(ops_vland);

#define VLAND_PID_FILE        "/var/run/openvswitch/ops-vland.pid"

static void
vland_unixctl_dump(struct unixctl_conn *conn, int argc OVS_UNUSED,
                   const char *argv[] OVS_UNUSED, void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    vland_debug_dump(&ds);

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);

} /* vland_unixctl_dump */

static void
vland_init(const char *db_path)
{
    /* Initialize IDL through a new connection to the DB. */
    vland_ovsdb_init(db_path);

    /* Initialize diagnostic dump for l2vlan */
    INIT_DIAG_DUMP_BASIC(l2vlan_diag_dump_callback);

    /* Register ovs-appctl commands for this daemon. */
    unixctl_command_register("ops-vland/dump", "", 0, 0, vland_unixctl_dump, NULL);

} /* vland_init */

static void
vland_exit(void)
{
    vland_ovsdb_exit();

} /* vland_exit */

static void
usage(void)
{
    printf("%s: OpenSwitch VLAN daemon\n"
           "usage: %s [OPTIONS] [DATABASE]\n"
           "where DATABASE is a socket on which ovsdb-server is listening\n"
           "      (default: \"unix:%s/db.sock\").\n",
           program_name, program_name, ovs_rundir());
    daemon_usage();
    vlog_usage();
    printf("\nOther options:\n"
           "  --unixctl=SOCKET        override default control socket name\n"
           "  -h, --help              display this help message\n");
    exit(EXIT_SUCCESS);

} /* usage */

static char *
parse_options(int argc, char *argv[], char **unixctl_pathp)
{
    enum {
        OPT_UNIXCTL = UCHAR_MAX + 1,
        VLOG_OPTION_ENUMS,
        DAEMON_OPTION_ENUMS,
    };
    static const struct option long_options[] = {
        {"help",        no_argument, NULL, 'h'},
        {"unixctl",     required_argument, NULL, OPT_UNIXCTL},
        DAEMON_LONG_OPTIONS,
        VLOG_LONG_OPTIONS,
        {NULL, 0, NULL, 0},
    };
    char *short_options = long_options_to_short_options(long_options);

    for (;;) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage();

        case OPT_UNIXCTL:
            *unixctl_pathp = optarg;
            break;

        VLOG_OPTION_HANDLERS
        DAEMON_OPTION_HANDLERS

        case '?':
            exit(EXIT_FAILURE);

        default:
            abort();
        }
    }
    free(short_options);

    argc -= optind;
    argv += optind;

    switch (argc) {
    case 0:
        return xasprintf("unix:%s/db.sock", ovs_rundir());

    case 1:
        return xstrdup(argv[0]);

    default:
        VLOG_FATAL("at most one non-option argument accepted; "
                   "use --help for usage");
    }

} /* parse_options */

static void
vland_unixctl_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                   const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;
    unixctl_command_reply(conn, NULL);

} /* vland_unixctl_exit */

int
main(int argc, char *argv[])
{
    char *appctl_path = NULL;
    struct unixctl_server *appctl;
    char *ovsdb_sock;
    bool exiting;
    int retval;

    set_program_name(argv[0]);
    proctitle_init(argc, argv);
    fatal_ignore_sigpipe();

    /* Parse commandline args and get the name of the OVSDB socket. */
    ovsdb_sock = parse_options(argc, argv, &appctl_path);

    /* Initialize the metadata for the IDL cache. */
    ovsrec_init();

    /* Fork and return in child process; but don't notify parent of
     * startup completion yet. */
    daemonize_start();

    /* Create UDS connection for ovs-appctl. */
    retval = unixctl_server_create(appctl_path, &appctl);
    if (retval) {
        exit(EXIT_FAILURE);
    }

    /* Register the ovs-appctl "exit" command for this daemon. */
    unixctl_command_register("exit", "", 0, 0, vland_unixctl_exit, &exiting);

    /* Create the IDL cache of the DB at ovsdb_sock. */
    vland_init(ovsdb_sock);
    free(ovsdb_sock);

    /* Notify parent of startup completion. */
    daemonize_complete();

    /* Enable asynch log writes to disk. */
    vlog_enable_async();

    VLOG_INFO_ONCE("%s (OpenSwitch VLAN Daemon) started", program_name);

    exiting = false;
    while (!exiting) {
        vland_run();
        unixctl_server_run(appctl);

        vland_wait();
        unixctl_server_wait(appctl);
        if (exiting) {
            poll_immediate_wake();
        } else {
            poll_block();
        }
    }

    vland_exit();
    unixctl_server_destroy(appctl);

    VLOG_INFO("%s (OpenSwitch VLAN Daemon) exiting", program_name);

    return 0;

} /* main */
