# -*- coding: utf-8 -*-
#
# Copyright (C) 2016 Hewlett Packard Enterprise Development LP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
##########################################################################
# Description: Verify the functionality of deleting a VLAN and reasigned
#              port to another vlan. The Vlan will be deleted from the
#              end of the VLAN list table.
#              No traffic should pass through other vlans.
#
# Author:      Jose Pablo Araya
#
# Topology:       |workStation|-------|Switch|-------|workStation|
#                                        |
#                                        |
#                                        |
#                                  |workStation|
#
# *Requeriments: workStations must have nmap installed
#
# Success Criteria:  PASS -> Vlans is correctly delete it without affecting
#                            the others
#
#                    FAILED -> If other vlans are affected
#
##########################################################################


"""
OpenSwitch Test for vlan related configurations.
"""

import re
from time import sleep

TOPOLOGY = """
# +-------+                    +-------+
# |       |     +--------+     |       |
# |  hs1  <----->  ops1  <----->  hs2  |
# |       |     +---<----+     |       |
# +-------+         |          +-------+
#                   |
#               +--->----+
#               |        |
#               |   hs3  |
#               |        |
#               +--------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
[type=host name="Host 1" image="openswitch/ubuntutest:latest"] hs1
[type=host name="Host 2" image="openswitch/ubuntutest:latest"] hs2
[type=host name="Host 3" image="openswitch/ubuntutest:latest"] hs3

# Links
hs1:1 -- ops1:7
ops1:8 -- hs2:1
hs3:1 -- ops1:9
"""


def test_ft_vlan_removed_from_middle_of_table(topology, step):
    ops1 = topology.get('ops1')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    hs3 = topology.get('hs3')

    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None
    assert hs3 is not None

    def findvlan(dut, pvlan):
        vlans = dut.libs.vtysh.show_vlan()
        count = 0
        for key in vlans:
            for elem in vlans[key]:
                if elem == "vlan_id" and vlans[key][elem] == pvlan:
                    count += 1
        return count

    def findport_vlan(dut, pvlan, port):
        portsearch = dut.ports[port]
        vlans = dut.libs.vtysh.show_vlan()
        count = 0
        for key in vlans:
            if vlans[key]['vlan_id'] == pvlan:
                for elem in vlans[key]['ports']:
                    if elem == portsearch:
                        count += 1
        return count

    step("Step 1- Adding vlans")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        pass
    with ops1.libs.vtysh.ConfigVlan('3') as ctx:
        pass
    with ops1.libs.vtysh.ConfigVlan('4') as ctx:
        pass

    step("Step 2- Enable vlans")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigVlan('3') as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigVlan('4') as ctx:
        ctx.no_shutdown()

    step("Step 3- Verify vlans were configured")
    assert findvlan(ops1, "2") == 1
    assert findvlan(ops1, "3") == 1
    assert findvlan(ops1, "4") == 1

    step("Step 4- Add ports to vlans")
    with ops1.libs.vtysh.ConfigInterface('7') as ctx:
        ctx.no_routing()
        ctx.vlan_access(2)
    with ops1.libs.vtysh.ConfigInterface('8') as ctx:
        ctx.no_routing()
        ctx.vlan_access(3)
    with ops1.libs.vtysh.ConfigInterface('9') as ctx:
        ctx.no_routing()
        ctx.vlan_access(4)

    step("Step 5- Enable interfaces")
    with ops1.libs.vtysh.ConfigInterface('7') as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigInterface('8') as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigInterface('9') as ctx:
        ctx.no_shutdown()

    step("Step 6- Verify ports assign correctly")
    assert findport_vlan(ops1, '2', '7') == 1
    assert findport_vlan(ops1, '3', '8') == 1
    assert findport_vlan(ops1, '4', '9') == 1

    step("Step 7- Send traffic")
    hs1.libs.ip.interface('1', addr='10.1.1.5/24', up=True)
    hs2.libs.ip.interface('1', addr='10.1.1.6/24', up=True)
    hs3.libs.ip.interface('1', addr='10.1.1.7/24', up=True)
    raw = hs1("nmap -sP 10.1.1.1-254")
    result = re.findall('\(\d*\s\w*\s\w*\)', raw)
    hostnumber = re.findall('\d?', result[0])
    assert hostnumber[1] == '1'

    step("Step 8- Delete middle vlan from table")
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(3)

    step("Step 9- Verify vlan was deleted")
    assert findvlan(ops1, "3") == 0

    step("Step 10- Re-assign port to other vlan")
    with ops1.libs.vtysh.ConfigInterface('8') as ctx:
        ctx.no_routing()
        ctx.vlan_access(2)
    sleep(2)

    step("Step 11- Verify ports re-assign correctly")
    assert findport_vlan(ops1, '2', '8') == 1

    step("Step 12- Send traffic")
    raw = hs1("nmap -sP 10.1.1.1-254")
    result = re.findall('\(\d*\s\w*\s\w*\)', raw)
    hostnumber = re.findall('\d?', result[0])
    assert hostnumber[1] == '2'
