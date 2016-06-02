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
# Description:       Verify that a VID out of the 802.1Q range or reserved VID
#                    cannot be set and
#                    also verifies that a VID which already exists cannot not
#                    be set again.
#
# Author:            Jose Pablo Araya
#
# Ported by:         Mauricio Fonseca
#
# Topology:          |Switch|
#
# Success Criteria:  PASS -> Vlan out of range and repeated not configured
#
#                    FAILED -> Vlan out of range or repeated were configured
#
##########################################################################

"""
OpenSwitch Test for vlan related configurations.
"""

from pytest import mark

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1

# FIXME remove this host when bug is fixed
[type=host name="Host 1"] hs1

# Links
# FIXME remove this link when bug is fixed
ops1:7 -- hs1:1
"""


@mark.gate
def test_ft_vlan_id_validation(topology, step):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    def getvlan(dut):
        vlans = dut.libs.vtysh.show_vlan()
        return vlans

    def findvlan(dut, pvlan):
        dic = getvlan(dut)
        count = 0
        for key in dic:
            for elem in dic[key]:
                if elem == "vlan_id" and dic[key][elem] == pvlan:
                    count += 1
        return count

    step("Step 1- Invalid vlan not configured")
    print("Trying to configure an invalid VLAN")
    ops1('config terminal')
    result = ops1('vlan 4095')
    ops1('end')
    # FIXME when error check is enabled
    assert result == '% Unknown command.'
    print("Verify VLAN was not configured")
    assert findvlan(ops1, "4095") == 0

    step("Step 2- Verify no previous vlans are configured")
    countv = getvlan(ops1)
    if len(countv) == 0:
        print("No Vlans are configured")
    else:
        assert len(countv) > 0

    step("Step 3- Add vlan")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    step("Step 4- Verify vlan was configured")
    assert findvlan(ops1, "2") == 1

    step("Step 5- Try to add repeated vlan")
    with ops1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    step("Step 6- Validate vlan repeated not configured")
    assert findvlan(ops1, "2") == 1
