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

"""
OpenSwitch Test for feature testing of vlan trunk. If vlan trunk is allowed
for a VID on an interface then that interface should be able to recieve/send
the tagged packets with vlan-ID VID.
"""

from __future__ import unicode_literals, absolute_import
from __future__ import print_function, division
from time import sleep


TOPOLOGY = """
# +-------+                   +-------+
# |       |     +-------+     |       |
# |  hs1  <----->  sw   <----->  hs2  |
# |       |     +-------+     |       |
# +-------+                   +-------+

# Nodes
[type=openswitch name="Switch 1"] sw
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- sw:3
sw:4 -- hs2:1
"""


def wait_until_interface_up(switch, portlbl, timeout=30, polling_frequency=1):
    """
    Wait until the interface, as mapped by the given portlbl, is marked as up.

    :param switch: The switch node.
    :param str portlbl: Port label that is mapped to the interfaces.
    :param int timeout: Number of seconds to wait.
    :param int polling_frequency: Frequency of the polling.
    :return: None if interface is brought-up. If not, an assertion is raised.
    """
    for i in range(timeout):
        status = switch.libs.vtysh.show_interface(portlbl)
        if status['interface_state'] == 'up':
            break
        sleep(polling_frequency)
    else:
        assert False, (
            'Interface {}:{} never brought-up after '
            'waiting for {} seconds'.format(
                switch.identifier, portlbl, timeout
            )
        )


def vlan_trunk_allowed_positive(dut, hs1, hs2, step):
    """
    In this test two interface of the DUT are configured with vlan
    trunk allowed 2 and 3. Vlan tagged packets of VID 2 and 3 are
    send from one workstation to another.
    """
    dut_port1 = dut.ports['3']
    dut_port2 = dut.ports['4']
    e1 = hs1.ports['1']
    d1 = hs2.ports['1']

    step("Configuring workstation 1")
    # Configure IP and bring UP host 1 interfaces
    hs1.libs.ip.add_link_type_vlan('1', e1 + ".2", 2)
    hs1.libs.ip.sub_interface('1', '2', addr="2.2.2.2/24", up=True)
    hs1.libs.ip.add_link_type_vlan('1', e1 + ".3", 3)
    hs1.libs.ip.sub_interface('1', '3', addr="3.3.3.3/24", up=True)

    step("Configuring workstation 2")
    # Configure IP and bring UP host 2 interfaces
    hs2.libs.ip.add_link_type_vlan('1', d1 + ".2", 2)
    hs2.libs.ip.sub_interface('1', '2', addr="2.2.2.3/24", up=True)
    hs2.libs.ip.add_link_type_vlan('1', d1 + ".3", 3)
    hs2.libs.ip.sub_interface('1', '3', addr="3.3.3.4/24", up=True)

    step("Configure vlans in the switch")
    # Enable vlan 2 and vlan 3 in the switch
    with dut.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()
    with dut.libs.vtysh.ConfigVlan('3') as ctx:
        ctx.no_shutdown()

    step("Configure interfaces with vlan trunk allowed")
    # Configure vlan trunk in switch interfaces and bring them UP
    with dut.libs.vtysh.ConfigInterface(dut_port1) as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_trunk_allowed('2')
        ctx.vlan_trunk_allowed('3')

    with dut.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_trunk_allowed('2')
        ctx.vlan_trunk_allowed('3')

    dut.libs.vtysh.show_running_config()

    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should pass as VLAN 2 & 3 tagged packets are allowed in switch"
         " interfaces")
    # Try to ping from one host to another, send 10 ping packets and check
    # if the number of received packets is atleast 5 to pass the test
    ping = hs1.libs.ping.ping(10, "2.2.2.3")
    assert ping['received'] >= 5, "host2 should be" \
        " reachable from host1 as trunk is allowed for vlan 2"
    ping = hs2.libs.ping.ping(10, "3.3.3.3")
    assert ping['received'] >= 5, "host1 should be" \
        " reachable from host2 as trunk is allowed for vlan 3"

    step("Cleanup the workstations")
    # cleanup
    hs1.libs.ip.remove_link_type_vlan(e1 + ".2")
    hs1.libs.ip.remove_link_type_vlan(e1 + ".3")
    hs2.libs.ip.remove_link_type_vlan(e1 + ".2")
    hs2.libs.ip.remove_link_type_vlan(e1 + ".3")


def vlan_trunk_allowed_negative(dut, hs1, hs2, step):
    """
    In this testcase reachability is tested between two hosts when switch
    when switch doesn't allow tagged packets on one of the interfaces.
    """
    dut_port1 = dut.ports['3']
    dut_port2 = dut.ports['4']
    e1 = hs1.ports['1']

    step("Configure workstations")
    # Configure IP and bring UP host interfaces
    hs1.libs.ip.add_link_type_vlan('1', e1 + ".2", 2)
    hs1.libs.ip.sub_interface('1', '2', addr="2.2.2.2/24", up=True)

    hs2.libs.ip.interface('1', addr="2.2.2.3/24", up=True)

    step("Configure switch")
    # Configure and bring UP switch 1 interfaces
    with dut.libs.vtysh.ConfigInterface(dut_port1) as ctx:
        ctx.no_vlan_trunk_allowed('3')

    with dut.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_vlan_trunk_allowed('2')

    dut.libs.vtysh.show_running_config()

    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should fail as tagged packets are expected on a normal"
         " interface")
    # Try to ping from host1 to host2, verify that non of the ICMP requests
    # are getting any replies
    ping = hs1.libs.ping.ping(10, "2.2.2.3")
    assert ping['loss_pc'] == 100, "ping should not happen as switch doesn't"\
        " allow vlan 2 tagged packets on " + dut_port2

    step("Cleanup the workstations")
    # Cleanup
    hs1.libs.ip.remove_link_type_vlan(e1 + ".2")
    hs2.libs.ip.remove_ip('1', addr="2.2.2.3/24")


def vlan_no_trunk_allowed(dut, hs1, hs2, step):
    """
    This test verifies that even though we have sub-interfaces with
    vlan id 2 on both interfaces, if the switch interfaces don't support
    vlan trunk allowed for vlan id 2 on both the interfaces then packet
    transfer is not possible.
    """
    dut_port1 = dut.ports['3']
    dut_port2 = dut.ports['4']
    e1 = hs1.ports['1']
    d1 = hs2.ports['1']

    step("Create a sub-interface with vlan id 2 in wrkstn 1")
    # Configure IP and bring UP host 1 interfaces
    hs1.libs.ip.add_link_type_vlan('1', e1 + ".2", 2)
    hs1.libs.ip.sub_interface('1', '2', addr="2.2.2.2/24", up=True)

    step("Create a sub-interface with vlan id 3 in wrkstn 2")
    # Configure IP and bring UP host 2 interfaces
    hs2.libs.ip.add_link_type_vlan('1', d1 + ".3", 3)
    hs2.libs.ip.sub_interface('1', '3', addr="2.2.2.3/24", up=True)

    dut.libs.vtysh.show_running_config()

    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should fail as vlan id 2 tagged packets are not allowed in"
         " Switch interface " + dut_port2)
    # Try to ping from host1 to host2, verify that non of the ICMP requests
    # are getting any replies
    ping = hs1.libs.ping.ping(10, "2.2.2.3")
    assert ping['loss_pc'] == 100, "ping should not happen as switch doesn't"\
        " allow vlan 2 tagged packets on " + dut_port2


def test_vlan_trunk_allowed(topology, step):
    """
    Testcases to validate the working of trunk interface.
    """
    sw = topology.get('sw')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw is not None
    assert hs1 is not None
    assert hs2 is not None

    step("##### 1. Test for vlan trunk interface positive scenario #########")
    vlan_trunk_allowed_positive(sw, hs1, hs2, step)
    step("##### 2. Test for vlan trunk interface negative scenario #########")
    vlan_trunk_allowed_negative(sw, hs1, hs2, step)
    step("##### 3. Test for no vlan trunk interface #########################")
    vlan_no_trunk_allowed(sw, hs1, hs2, step)
