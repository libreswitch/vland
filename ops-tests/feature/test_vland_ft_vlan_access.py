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
OpenSwitch Test for functionality testing of vlan access mode.
"""

from __future__ import unicode_literals, absolute_import
from __future__ import print_function, division
from time import sleep


TOPOLOGY = """
# +-------+                   +-------+
# |       |     +-------+     |       |
# |  hs1  <----->   sw  <----->  hs2  |
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


def configure_hosts(hs1, hs2, step):
    """
    Configure the workstations by assigning IP to the interfaces.
    """
    step("Configure IP in host1")
    # Configure IP and bring UP host 1 interface
    hs1.libs.ip.interface('1', addr="2.2.2.2/24", up=True)

    step("Configure IP in host2")
    # Configure IP and bring UP host 2 interface
    hs2.libs.ip.interface('1', addr="2.2.2.3/24", up=True)


def unconfigure_hosts(hs1, hs2, step):
    """
    Clean up the workstation by removing the IP from interfaces.
    """
    step("Remove the IP from host1")
    hs1.libs.ip.remove_ip('1', addr="2.2.2.2/24")

    step("Remove the IP from host2")
    hs2.libs.ip.remove_ip('1', addr="2.2.2.3/24")


def vlan_access_positive(dut, hs1, hs2, step):
    """
    In this testcase reachability is tested when vlan access mode
    is configured on the interfaces of a switch.
    """
    dut_port1 = dut.ports["3"]
    dut_port2 = dut.ports["4"]

    # Configure hosts
    configure_hosts(hs1, hs2, step)

    step("Configure vlan 2 in switch")
    with dut.libs.vtysh.ConfigVlan('2') as ctx:
        ctx.no_shutdown()

    step("Configure switch interfaces with vlan access configuration")
    # Configure IP and bring UP switch 1 interfaces
    with dut.libs.vtysh.ConfigInterface(dut_port1) as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access('2')

    with dut.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access('2')

    dut.libs.vtysh.show_running_config()
    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should succeed as the switch's interfaces will allow untagged"
         " packets to pass between " + dut_port1 + " and " + dut_port2)
    # Try to ping from one host to another, send 10 ping packets and check
    # if the number of received packets is atleast 5 to pass the test
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['received'] >= 5, "Ping should have "\
        "worked when vlan access 2 is enabled on both the interfaces"

    # Cleanup
    unconfigure_hosts(hs1, hs2, step)


def vlan_access_negative(dut, hs1, hs2, step):
    """
    In this testcase reachability is tested when two different vlans are
    configured as access for two interfaces
    """
    dut_port1 = dut.ports["3"]
    dut_port2 = dut.ports["4"]

    # Configure hosts
    configure_hosts(hs1, hs2, step)

    step("Configure vlan 3 in switch")
    # Configure and bring UP switch interfaces
    with dut.libs.vtysh.ConfigVlan('3') as ctx:
        ctx.no_shutdown()

    step("Configure switch interface with vlan 3 as access")
    with dut.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_vlan_access('2')
        ctx.vlan_access('3')

    dut.libs.vtysh.show_running_config()
    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should fail as different vlans are configured access for the"
         " two interfaces")
    # Try to ping from host1 to host2, verify that non of the ICMP requests
    # are getting any replies
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['loss_pc'] == 100, "ping should not happen as switch doesn't"\
        " have vlan 2 access on " + dut_port2

    # Cleanup
    unconfigure_hosts(hs1, hs2, step)


def vlan_access_tagged_pkt(dut, hs1, hs2, step):
    """
    In this testcase reachability is tested when tagged packets are sent
    in vlan access configuration
    """
    dut_port1 = dut.ports["3"]
    dut_port2 = dut.ports["4"]
    e1 = hs1.ports['1']
    d1 = hs2.ports['1']

    # Configure hosts
    step("Configuring workstation 1")
    # Configure IP and bring UP host 1 interfaces
    hs1.libs.ip.add_link_type_vlan('1', e1 + ".2", 2)
    hs1.libs.ip.sub_interface('1', '2', addr="2.2.2.2/24", up=True)

    step("Configuring workstation 2")
    # Configure IP and bring UP host 2 interfaces
    hs2.libs.ip.add_link_type_vlan('1', d1 + ".2", 2)
    hs2.libs.ip.sub_interface('1', '2', addr="2.2.2.3/24", up=True)

    # Configure Switch interface
    step("Configure switch interface with vlan 2 as access")
    with dut.libs.vtysh.ConfigInterface(dut_port2) as ctx:
        ctx.no_vlan_access('3')
        ctx.vlan_access('2')

    dut.libs.vtysh.show_running_config()
    # Wait until interfaces are up
    for switch, portlbl in [(dut, dut_port1), (dut, dut_port2)]:
        wait_until_interface_up(switch, portlbl)

    sleep(5)
    step("Ping should fail as switch does not allow tagged packets on"
         " vlan access ports")
    # Try to ping from host1 to host2, verify that non of the ICMP requests
    # are getting any replies
    ping = hs1.libs.ping.ping(10, '2.2.2.3')
    assert ping['loss_pc'] == 100, "ping should not happen as switch should"\
        " not allow tagged packets on vlan access ports"


def test_vlan_access(topology, step):
    """
    This testcase validates the basic functionality of vlan access mode
    configuration on switch.
    """
    sw = topology.get('sw')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw is not None
    assert hs1 is not None
    assert hs2 is not None

    step("####### 1. Test case to validate vlan access positive scenario ###")
    vlan_access_positive(sw, hs1, hs2, step)
    step("####### 2. Test case to validate vlan access negative scenario ###")
    vlan_access_negative(sw, hs1, hs2, step)
    step("####### 3. Test case to validate vlan access when tagged "
         "packets are sent ###")
    vlan_access_tagged_pkt(sw, hs1, hs2, step)
