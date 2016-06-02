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
# Description: Verify the status of the Vlan change from up to down correctly.
#               The status of a vlan is changed from down to up when a port is
#               added to the vlan and the vlan has been brought up with the
#               respective command.
#
# Author:      Jose Pablo Araya
#
# Topology:       |Switch|
#
# Success Criteria:  PASS -> Vlans status is correctly verifyed
#                            in every scenario
#
#                    FAILED -> If vlan status is wrong showed in one
#                              of the scenarios
#
##########################################################################

"""
OpenSwitch Test for vlan related configurations.
"""


TOPOLOGY = """
# +-------+                    +-------+
# |       |     +--------+     |       |
# |  hs1  <----->  ops1  <----->  hs2  |
# |       |     +--------+     |       |
# +-------+                    +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
[type=host name="Host 1"] hs1

# Links
hs1:1 -- ops1:7
"""


def test_ft_vlan_state_transition(topology, step):  # noqa
    ops1 = topology.get('ops1')

    assert ops1 is not None

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

    def findport_vlan(dut, pvlan, port):
        portsearch = dut.ports[port]
        vlans = dut.libs.vtysh.show_vlan()
        count = False
        for key in vlans:
            if vlans[key]['vlan_id'] == pvlan:
                for elem in vlans[key]['ports']:
                    if elem == portsearch:
                        count = True
        return count

    step("Step 1- Adding vlans")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        pass

    step("Step 2- Verify vlans were configured")
    assert findvlan(ops1, "2")

    step("Step 3- Verify vlan state")
    assert vlanstatus(ops1, '2', 'status', 'down')

    step("Step 4- Add port to vlan")
    with ops1.libs.vtysh.ConfigInterface('7') as ctx:
        ctx.no_routing()
        ctx.vlan_access(2)

    step("Step 5- Verify ports assign correctly")
    assert findport_vlan(ops1, '2', '7')

    step("Step 6- Enable vlan")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    step("Step 7- Verify vlan state")
    assert vlanstatus(ops1, '2', 'status', 'up')
