# -*- coding: utf-8 -*-
# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
##########################################################################
# Description: With several vlans configured, bring one vlan up and
#              confirm its state, witch one port assigned to it verify
#              its Reason to ok, then issue the command to set the vlan
#              down and confirm its state. Only one vlan should see a
#              change in its state.
#
# Author:      Jose Pablo Araya
#
# Ported by:   Mauricio Fonseca
#
# Topology:       |Switch|
#
# Success Criteria:  PASS -> Just one of the vlans get the configuration
#                            perfomed
#
#                    FAILED -> If more than one vlan was configured with
#                              same options
#
##########################################################################

"""
OpenSwitch Test for vlan related configurations.
"""

from pytest import mark
from time import sleep


TOPOLOGY = """
# +-------+                    +-------+
# |       |     +--------+     |       |
# |  hs1  <----->  ops1  <----->  hs2  |
# |       |     +--------+     |       |
# +-------+                    +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- ops1:7
ops1:8 -- hs2:1
"""


@mark.gate
def test_ft_vlan_state_reason_transition(topology, step):
    ops1 = topology.get('ops1')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    def findvlan(dut, pvlan):
        dic = dut.libs.vtysh.show_vlan()
        found = False
        for key in dic:
            if dic[key]['vlan_id'] == pvlan:
                    found = True
        return found

    def vlanstatus(dut, pvlan, elem, status):
        dic = dut.libs.vtysh.show_vlan()
        found = False
        for key in dic:
            if dic[key]['vlan_id'] == pvlan:
                if dic[key][elem].lower() == status:
                    found = True
        return found

    step("Step 1- Configure workstations")
    hs1.libs.ip.interface('1', addr='192.168.30.10/24', up=True)
    hs2.libs.ip.interface('1', addr='192.168.30.11/24', up=True)

    step("Step 2- Adding vlans")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        pass
    with ops1.libs.vtysh.ConfigVlan('3') as ctx:
        pass
    with ops1.libs.vtysh.ConfigVlan('4') as ctx:
        pass

    step("Step 3- Verify vlans were configured")
    assert findvlan(ops1, "2")
    assert findvlan(ops1, "3")
    assert findvlan(ops1, "4")

    step("Step 5- Verify vlan reason before enabling it")
    assert vlanstatus(ops1, '2', 'reason', 'admin_down')
    assert vlanstatus(ops1, '3', 'reason', 'admin_down')
    assert vlanstatus(ops1, '4', 'reason', 'admin_down')

    step("Step 6- Enable one of the configured vlans")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    step("Step 7- Verify vlan status")
    assert vlanstatus(ops1, '2', 'status', 'down')
    assert vlanstatus(ops1, '3', 'status', 'down')
    assert vlanstatus(ops1, '4', 'status', 'down')

    step("Step 8- Verify vlan reason")
    assert vlanstatus(ops1, '2', 'reason', 'no_member_port')
    assert vlanstatus(ops1, '3', 'reason', 'admin_down')
    assert vlanstatus(ops1, '4', 'reason', 'admin_down')

    step("Step 9- Add port to vlan")
    with ops1.libs.vtysh.ConfigInterface('7') as ctx:
        ctx.no_routing()
        ctx.vlan_access(2)
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigInterface('8') as ctx:
        ctx.no_routing()
        ctx.vlan_access(2)
        ctx.no_shutdown()

    step("Step 10- Verify vlan status")
    assert vlanstatus(ops1, '2', 'status', 'up')
    assert vlanstatus(ops1, '3', 'status', 'down')
    assert vlanstatus(ops1, '4', 'status', 'down')

    step("Step 11- Verify vlan reason")
    assert vlanstatus(ops1, '2', 'reason', 'ok')
    assert vlanstatus(ops1, '3', 'reason', 'admin_down')
    assert vlanstatus(ops1, '4', 'reason', 'admin_down')

    step("Step 12- Disable vlan")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.shutdown()

    step("Step 13- Verify vlan status down")
    assert vlanstatus(ops1, '2', 'status', 'down')
    assert vlanstatus(ops1, '3', 'status', 'down')
    assert vlanstatus(ops1, '4', 'status', 'down')

    step("Step 14- Verify vlan reason")
    assert vlanstatus(ops1, '2', 'reason', 'admin_down')
    assert vlanstatus(ops1, '3', 'reason', 'admin_down')
    assert vlanstatus(ops1, '4', 'reason', 'admin_down')

    step("Step 15- Verify ping between hosts")
    sleep(5)
    ping = hs1.libs.ping.ping(10, '192.168.30.11')
    assert ping['transmitted'] == ping['errors'] == 10
