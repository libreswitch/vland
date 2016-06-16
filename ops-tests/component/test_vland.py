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

"""
OpenSwitch Test for vlan related configurations.
"""

# from pytest import mark
import pytest

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
"""

PORT_1 = "1"


def get_ports(dut):
    ports = []
    out = dut("list-ports br0", shell="vsctl")
    names = out.splitlines()
    for name in names:
        name = name.strip()
        ports.append(name)
    return ports


def get_vlans(dut):
    vlans = []
    out = dut("list-vlans br0", shell="vsctl")
    names = out.splitlines()
    for name in names:
        name = name.strip()
        vlans.append(name)
    return vlans


def get_vlan(dut, vlanid):
    result = dict()
    out = dut("list vlan {vlanid}".format(
        **locals()
    ), shell="vsctl")
    lines = out.splitlines()
    for line in lines:
        if line == "":
            continue
        name_val = line.split(":", 1)
        name = name_val[0].strip()
        val = name_val[1].strip()
        result[name] = val
    return result


def add_vlan(dut, vlanid, name):
    out = dut("add-vlan br0 {vlanid} name={name}".format(
        **locals()
    ), shell="vsctl")


def add_port(dut, port, vlan_mode="", tag="", trunks=""):
    modes = ["access", "trunk", "native-tagged", "native-untagged"]
    assert vlan_mode in modes, "Expected suitable vlan_mode value"
    extra = ""
    if vlan_mode != "":
        extra = extra + " vlan_mode=" + vlan_mode
    if tag != "":
        extra = extra + " tag=" + tag
    if trunks != "":
        extra = extra + " trunks=" + trunks
    out = dut("add-port br0 {port}{extra}".format(
        **locals()
    ), shell="vsctl")


def delete_port(dut, port):
    out = dut("del-port br0 {port}".format(**locals()), shell="vsctl")


def set_vlan_admin(dut, vlanid, state):
    out = dut("set vlan {vlanid} admin={state}".format(
        **locals()
    ), shell="vsctl")


def set_port_vlans(dut, port, vlans):
        out = dut("set port {port} trunk={vlans}".format(
            **locals()
        ), shell="vsctl")


def verify_data(data, hw_vlan_config, oper_state, oper_state_reason):
    assert (
        data["hw_vlan_config"] == hw_vlan_config, "Unexpected hw_vlan_config"
    )
    assert data["oper_state"] == oper_state, "Unexpected oper_state"
    assert (
        data["oper_state_reason"] == oper_state_reason,
        "Unexpected oper_state_reason"
    )


def delete_vlan(dut, vlanid):
    out = dut("del-vlan br0 {vlanid}".format(**locals()), shell="vsctl")


def delete_all_ports(dut):
    ports = get_ports(dut)
    for port in ports:
        delete_port(dut, port)


def delete_all_vlans(dut):
    vlans = get_vlans(dut)
    for vlan in vlans:
        delete_vlan(dut, vlan)


@pytest.fixture()
def setup(request, topology):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    ops1("add-br br0", shell="vsctl")

    def cleanup():
        delete_all_ports(ops1)
        delete_all_vlans(ops1)
        ops1("del-br br0", shell="vsctl")

    request.addfinalizer(cleanup)


def test_initial_vlan_state(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    vlans = ["100", "200", "300"]

    step("Step 1- Create vlans 100, 200, 300")
    for vlan in vlans:
        add_vlan(ops1, vlan, vlan)

    step("Step 2- Verify all vlans are down")
    for vlan in vlans:
        verify_data(get_vlan(ops1, vlan), "{}", "down", "admin_down")


def test_vlan_0(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    step("Step 1- Create vlan 200")
    add_vlan(ops1, "200", "200")

    step("Step 2- Add port referencing vlan 200")
    ops1("conf t")
    ops1("interface 1")
    ops1("no routing")
    ops1("vlan access 200")

    step("Step 3- Delete port")
    ops1("routing")
    ops1("exit")
    ops1("vlan 200")
    ops1("shutdown")
    ops1("exit")

    step("Step 4- Verify vlan is down (admin_down)")
    verify_data(get_vlan(ops1, "200"), "{}", "down", "admin_down")

    step("Step 5- Set vlan 200 admin up")
    set_vlan_admin(ops1, "200", "up")

    step("Step 6- Verify vlan is down (no_member_port)")
    verify_data(get_vlan(ops1, "200"), "{}", "down", "no_member_port")

    step("Step 7- Add port referencing vlan 200")
    ops1("interface 1")
    ops1("no routing")
    ops1("vlan trunk allowed 200")

    step("Step 8- Verify vlan is up (ok) and enabled")
    verify_data(get_vlan(ops1, "200"), '{enable=true}', "up", "ok")

    ops1("routing")


def test_vlan_a(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    vlans = ["100", "200", "300"]

    step("Step 1- Add vlans 100, 200, 300")
    for vlan in vlans:
        add_vlan(ops1, vlan, vlan)

    step("Step 2- Add port referencing vlan 100")
    ops1("conf t")
    ops1("interface 1")
    ops1("no routing")
    ops1("vlan access 100")
    ops1("exit")

    step("Step 3- Verify vlan state down (admin_down")
    for vlan in vlans:
        verify_data(get_vlan(ops1, vlan), "{}", "down", "admin_down")


def test_vlan_b(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    vlans = ["100", "200", "300"]

    step("Step 1- Add vlans 100, 200, 300")
    for vlan in vlans:
        add_vlan(ops1, vlan, vlan)

    step("Step 2- Set vlan 200 admin state up")
    set_vlan_admin(ops1, vlans[1], "up")

    step("Step 3- Verify vlans 100, 300 are down (admin_down)")
    verify_data(get_vlan(ops1, vlans[0]), "{}", "down", "admin_down")
    verify_data(get_vlan(ops1, vlans[2]), "{}", "down", "admin_down")

    step("Step 4- Verify vlan 200 is down (no_member_port)")
    verify_data(get_vlan(ops1, vlans[1]), "{}", "down", "no_member_port")


def test_vlan_c(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    vlans = ["100", "200", "300"]

    step("Step 1- Add vlans 100, 200, 300")
    for vlan in vlans:
        add_vlan(ops1, vlan, vlan)

    step("Step 2- Set vlan 200 admin up")
    set_vlan_admin(ops1, vlans[1], "up")

    step("Step 3- Verify vlan 200 down (no_member_port)")
    verify_data(get_vlan(ops1, vlans[1]), "{}", "down", "no_member_port")

    step("Step 4- Add port referencing vlan 200")
    ops1("conf t")
    ops1("interface 1")
    ops1("no routing")
    ops1("vlan access 200")
    ops1("exit")

    step("Step 5- Verify vlan 200 up (ok)")
    verify_data(get_vlan(ops1, vlans[1]), '{enable=true}', "up", "ok")

    step("Step 6- Set vlan 200 admin down")
    set_vlan_admin(ops1, vlans[1], "down")

    step("Step 7- Verify vlan 200 down (admin_down)")
    verify_data(get_vlan(ops1, vlans[1]), "{}", "down", "admin_down")

    ops1("interface 1")
    ops1("routing")
    ops1("exit")

def test_vlan_d(topology, step, setup):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    vlans = ["100", "200", "300"]

    step("Step 1- Add vlans 100, 200, 300")
    for vlan in vlans:
        add_vlan(ops1, vlan, vlan)

    step("Step 2- Set vlans state up")
    for vlan in vlans:
        set_vlan_admin(ops1, vlan, "up")

    step("Step 3- Verify vlans down (no_member_port)")
    for vlan in vlans:
        verify_data(get_vlan(ops1, vlan), "{}", "down", "no_member_port")

    step("Step 4- Add port referencing vlans 100, 200, 300")
    ops1("conf t")
    ops1("interface 1")
    ops1("no routing")
    ops1("vlan access 100")
    ops1("vlan trunk allowed 100")
    ops1("vlan trunk allowed 200")
    ops1("vlan trunk allowed 300")

    step("Step 5- Verify vlan state up (ok)")
    for vlan in vlans:
        verify_data(get_vlan(ops1, vlan), '{enable=true}', "up", "ok")

    step("Step 6- Remove vlan 200 from port")
    ops1("no vlan trunk allowed 200")

    step("Step 7- Verify vlan 100, 300 state up (ok)")
    verify_data(get_vlan(ops1, vlans[0]), '{enable=true}', "up", "ok")
    verify_data(get_vlan(ops1, vlans[2]), '{enable=true}', "up", "ok")

    step("Step 8-  Verify vlan 200 state down (no_member_port)")
    verify_data(get_vlan(ops1, vlans[1]), "{}", "down", "no_member_port")

    ops1("no vlan trunk allowed 100")
    ops1("no vlan trunk allowed 300")
    ops1("routing")
