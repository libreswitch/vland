/*
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
/****************************************************************************
 * @ingroup cli
 *
 * @file vtysh_ovsdb_vlan_context.h
 * Source for registering client callback with vlan context.
 *
 ***************************************************************************/

#ifndef VTYSH_OVSDB_VLAN_CONTEXT_H
#define VTYSH_OVSDB_VLAN_CONTEXT_H

#include "ops-utils.h"

vtysh_ret_val vtysh_vlan_context_clientcallback(void *p_private);
struct feature_sorted_list * vtysh_vlan_context_init(void *p_private);
void vtysh_vlan_context_exit(struct feature_sorted_list * head);
vtysh_ret_val vtysh_intf_context_vlan_clientcallback(void *p_private);
vtysh_ret_val vtysh_vlan_global_context_clientcallback(void *p_private);
#endif /* VTYSH_OVSDB_VLAN_CONTEXT_H */
