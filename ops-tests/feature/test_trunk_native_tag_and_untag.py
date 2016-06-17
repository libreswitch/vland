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
OpenSwitch Test for ping between the workstations with native configuration.
"""

from __future__ import unicode_literals, absolute_import
from __future__ import print_function, division

from time import sleep


TOPOLOGY = """
# +-------+                   +-------+
# |       |     +-------+     |       |
# |  hs1  <----->  sw1  <----->  hs2  |
# |       |     +-------+     |       |
# +-------+                   +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
sw1:if01 -- hs1:p1
sw1:if02 -- hs2:q1
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


def vlan_trunk_native_untag(sw1, hs1, hs2, step):
    """
    This test case will verify, port configured with vlan trunk
    native will send always
    untaged packets.
    """

    dut_port1 = sw1.ports["if01"]
    dut_port2 = sw1.ports["if02"]
    e1 = hs1.ports["p1"]
    d1 = hs2.ports["q1"]

    # Configure IP and bring UP host 1 interfaces
    hs1.libs.ip.add_link_type_vlan('p1', e1 + ".2", 2)
    hs1.libs.ip.sub_interface('p1', '2', addr="2.2.2.2/24", up=True)

    # Configure IP and bring UP host 2 interfaces
    hs2.libs.ip.interface('q1', addr="2.2.2.3/24", up=True)

    # Creating vlan
    with sw1.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    # Configure vlan on switch interfaces
    with sw1.libs.vtysh.ConfigInterface(dut_port1) as ctx:
        ctx.no_shutdown()
        ctx.no_routing()
        ctx.vlan_trunk_allowed('2')

    with sw1.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_shutdown()
        ctx.no_routing()
        sleep(2)
        ctx.vlan_trunk_native('2')

    # Wait until interfaces are up
    for switch, portlbl in [(sw1, dut_port1), (sw1, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    # show the running conifguration to ensure that switch configured properly
    sw1.libs.vtysh.show_running_config()

    sleep(5)
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['received'] >= 5

    # Negative reachability test
    hs2.libs.ip.remove_ip('q1', addr="2.2.2.3/24")
    hs2.libs.ip.add_link_type_vlan('q1', d1 + ".2", 2)
    hs2.libs.ip.sub_interface('q1', '2', addr="2.2.2.3/24", up=True)
    sleep(2)
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['loss_pc'] == 100
    step('Ping not success !!!!negative reachability passed.')


def vlan_trunk_native_tag(sw1, hs1, hs2, step):
    """
    This test case will verify, port configured with vlan tr native will send
    always tagged packets.
    """
    dut_port1 = sw1.ports["if01"]
    dut_port2 = sw1.ports["if02"]
    e1 = hs1.ports["p1"]
    d1 = hs2.ports["q1"]

    #Removing the initial cofiguration from host1 and host2
    hs1.libs.ip.remove_ip(e1 + '.2', addr="2.2.2.2/24")
    hs2.libs.ip.remove_ip(d1 + '.2', addr="2.2.2.3/24")

    # Configure IP and bring UP host 1 interfaces
    hs1.libs.ip.interface('p1', addr="2.2.2.2/24", up=True)

    # Configure IP and bring UP host 2 interfaces
    hs2.libs.ip.sub_interface('q1', '2', addr="2.2.2.3/24", up=True)

    #Configure vlan on switch interfaces
    with sw1.libs.vtysh.ConfigInterface(dut_port1) as ctx:
        ctx.no_vlan_trunk_allowed(2)
        ctx.vlan_trunk_native('2')

    with sw1.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.vlan_trunk_allowed(2)
        ctx.vlan_trunk_native_tag()

    # show the running conifguration to ensure that switch configured properly
    sw1.libs.vtysh.show_running_config()

    sleep(5)
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['received'] >= 5

    # Negative reachability test
    hs2.libs.ip.remove_ip(d1 + '.2', addr="2.2.2.3/24")
    hs2.libs.ip.interface('q1', addr="2.2.2.3/24", up=True)
    sleep(2)
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['loss_pc'] == 100
    step('Ping not success !!!!negative reachability passed.')


def test_ping(topology, step):
    """
    Ping between the hosts in native configuration .
    """
    sw1 = topology.get('sw1')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw1 is not None
    assert hs1 is not None
    assert hs2 is not None

    step('####Reachbility test for vlan trunk native.######')
    vlan_trunk_native_untag(sw1, hs1, hs2, step)
    step('####Reachbility test for vlan trunk native tag.######')
    vlan_trunk_native_tag(sw1, hs1, hs2, step)
