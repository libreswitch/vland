#!/usr/bin/python
#
# Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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


#import pdb; pdb.set_trace()

import os
import sys
import pytest
import subprocess
from opsvsi.docker import *
from opsvsi.opsvsitest import *

PORT_1="1"

class vlandTest( OpsVsiTest ):

    def setupNet(self):
        # if you override this function, make sure to
        # either pass getNodeOpts() into hopts/sopts of the topology that
        # you build or into addHost/addSwitch calls
        topo=SingleSwitchTopo(
            k=1,
            hopts=self.getHostOpts(),
            sopts=self.getSwitchOpts())
        self.net = Mininet(topo=topo,
            switch=VsiOpenSwitch,
            host=Host,
            link=OpsVsiLink, controller=None,
            build=True)
        self.s1 = self.net.switches[0]

    def get_ports(self):
        ports = []
        out = self.s1.cmd("/usr/bin/ovs-vsctl list-ports br0")
        names = out.split("\n")
        for name in names:
            name = name.strip()
            ports.append(name)
        return ports

    def get_vlans(self):
        vlans = []
        out = self.s1.cmd("/usr/bin/ovs-vsctl list-vlans br0")
        names = out.split("\n")
        for name in names:
            name = name.strip()
            vlans.append(name)
        return vlans

    def add_vlan(self, vlan_id):
        out = self.s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 " + vlan_id + \
                " name=" + vlan_id)

    def delete_vlan(self, vlan_id):
        out = self.s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 " + vlan_id)

    def delete_all_vlans(self):
        vlans = self.get_vlans()
        for vlan in vlans:
            self.delete_vlan(vlan)

    def set_vlan_admin(self, vlan_id, state):
        out = self.s1.cmd("/usr/bin/ovs-vsctl set vlan " + vlan_id + \
                " admin=" + state)

    def add_port(self, port, vlan_mode="", tag="", trunks=""):
        modes = ["access", "trunk", "native-tagged", "native-untagged"]
        assert vlan_mode in modes, "Expected suitable vlan_mode value"
        extra = ""
        if vlan_mode != "":
            extra = extra + " vlan_mode=" + vlan_mode
        if tag != "":
            extra = extra + " tag=" + tag
        if trunks != "":
            extra = extra + " trunks=" + trunks
        out = self.s1.cmd("/usr/bin/ovs-vsctl add-port br0 " + port + extra)

    def delete_port(self, port):
        out = self.s1.cmd("/usr/bin/ovs-vsctl del-port br0 " + port)

    def delete_all_ports(self):
        ports = self.get_ports()
        for port in ports:
            self.delete_port(port)

    def set_port_vlan(self, port, vlan):
        out = self.s1.cmd("/usr/bin/ovs-vsctl set port " + port + " tag=" \
                + vlan)

    def set_port_vlans(self, port, vlans):
        out = self.s1.cmd("/usr/bin/ovs-vsctl set port " + port + " trunks=" \
                + vlans)

    def get_vlan(self, vlan_id):
        result = dict()
        out = self.s1.cmd("/usr/bin/ovs-vsctl list vlan " + vlan_id)
        lines = out.split("\n")
        for line in lines:
            if line == "":
                continue
            name_val = line.split(":", 1)
            name = name_val[0].strip()
            val = name_val[1].strip()
            result[name] = val
        return result

    def get_port(self, port):
        result = dict()
        out = self.s1.cmd("/usr/bin/ovs-vsctl list port " + port)
        lines = out.split("\n")
        for line in lines:
            if line == "":
                continue
            name_val = line.split(":", 1)
            name = name_val[0].strip()
            val = name_val[1].strip()
            result[name] = val
        return result

class Test_vland:

    # Create the Mininet topology based on mininet.
    test = vlandTest()

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        # Create initial bridge "br0" to hold ports
        Test_vland.test.s1.cmd("/usr/bin/ovs-vsctl add-br br0")

    def teardown_class(cls):
        # Delete all ports, vlans, and bridge "br0"
        Test_vland.test.delete_all_ports()
        Test_vland.test.delete_all_vlans()
        Test_vland.test.s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        # Stop the Docker containers, and
        # mininet topology
        Test_vland.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    def _verify_data(self, data, hw_vlan_config, oper_state, oper_state_reason):
        # compare actual data to expected data for three key attributes:
        # hw_vlan_config, oper_state, oper_state_reason
        assert data["hw_vlan_config"] == hw_vlan_config, \
                "Unexpected hw_vlan_config"
        assert data["oper_state"] == oper_state, "Unexpected oper_state"
        assert data["oper_state_reason"] == oper_state_reason, \
                "Unexpected oper_state_reason"

    def test_initial_vlan_state(self):
        ########################################################
        # test: verify initial state of vlans
        # 1) Create vlans 100, 200, 300
        # 2) Verify that all VLANs are down
        # 3) Cleanup
        ########################################################
        self.test.add_vlan("100")
        self.test.add_vlan("200")
        self.test.add_vlan("300")

        # verify that they are all in the correct initial state
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, "{}", "down", "admin_down")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "admin_down")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, "{}", "down", "admin_down")

        # cleanup
        self.test.delete_all_vlans()

    def test_vlan_0(self):
        ########################################################
        # test: verify vlan state transitions
        # 1) Create vlan 200
        # 2) Add port referencing vlan 200
        # 3) Delete port
        # 4) Verify vlan is down (admin_down)
        # 5) Set vlan 200 admin up
        # 6) Verify vlan is down (no_member_port)
        # 7) Add port referencing vlan 200
        # 8) Verify vlan is up (ok) and enabled
        # 9) Cleanup
        ########################################################
        # create VLAN 200
        self.test.add_vlan("200")

        # create PORT 1
        self.test.add_port(PORT_1, vlan_mode="access", tag="200")

        # delete PORT 1
        self.test.delete_port(PORT_1)

        # verify admin down
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "admin_down")

        # admin up
        self.test.set_vlan_admin("200", "up")

        # verify no member ports
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")

        # add PORT 1
        self.test.add_port(PORT_1, vlan_mode="trunk", trunks="200")

        # verify ok
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")

        # clean up
        self.test.delete_all_vlans()
        self.test.delete_all_ports()

    def test_vlan_A(self):
        ########################################################
        # test: verify state of vlan with ports and admin=down
        # 1) Add vlans 100, 200, 300
        # 2) Add port referencing vlan 100
        # 3) Verify vlan state down (admin_down)
        # 4) Cleanup
        ########################################################
        self.test.add_vlan("100")
        self.test.add_vlan("200")
        self.test.add_vlan("300")
        self.test.add_port(PORT_1, vlan_mode="access", tag="100")

        # verify state
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, "{}", "down", "admin_down")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "admin_down")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, "{}", "down", "admin_down")

        # cleanup
        self.test.delete_all_ports()
        self.test.delete_all_vlans()

    def test_vlan_B(self):
        ########################################################
        # test: verify state of vlan with no ports and admin=up
        # 1) Create vlans 100, 200, 300
        # 2) Set vlan 200 admin state up
        # 3) Verify 100, 300 down ("admin_down")
        # 4) Verify 200 down (no_member_port)
        # 5) Cleanup
        ########################################################
        self.test.add_vlan("100")
        self.test.add_vlan("200")
        self.test.add_vlan("300")

        # set admin=up for vlan 200
        self.test.set_vlan_admin("200", "up")

        # verify state
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, "{}", "down", "admin_down")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, "{}", "down", "admin_down")

        # cleanup
        self.test.delete_all_ports()
        self.test.delete_all_vlans()

    def test_vlan_C(self):
        ########################################################
        # test: verify state after transition from admin=up -> admin=down
        # 1) Create vlans 100, 200, 300
        # 2) Set vlan 200 admin up
        # 3) Verify vlan 200 down (no_member_port)
        # 4) Add port referencing vlan 200
        # 5) Verify vlan 200 up (ok)
        # 6) Set vlan 200 admin down
        # 7) Verify vlan 200 down (admin_down)
        # 8) Cleanup
        ########################################################
        self.test.add_vlan("100")
        self.test.add_vlan("200")
        self.test.add_vlan("300")

        # set admin=up for vlan 200
        self.test.set_vlan_admin("200", "up")

        # do initial verification
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")

        # add vlan to port
        self.test.add_port(PORT_1, vlan_mode="access", tag="200")

        # do second verification
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")

        # set admin=down for vlan 200
        self.test.set_vlan_admin("200", "down")

        # do final verification
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "admin_down")

        # cleanup
        self.test.delete_all_ports()
        self.test.delete_all_vlans()

    def test_vlan_D(self):
        ########################################################
        # test: verify state with both trunk and default VLAN
        # 1) Create vlans 100, 200, 300
        # 2) Set vlans state up
        # 3) Verify vlans down (no_member_port)
        # 4) Add port referencing vlans 100, 200, 300
        # 5) Verify vlan state up (ok)
        # 6) Remove vlan 200 from port
        # 7) Verify vlan 100, 300 state up (ok)
        # 8) Verify vlan 200 state down (no_member_port)
        # 9) Cleanup
        ########################################################
        self.test.add_vlan("100")
        self.test.add_vlan("200")
        self.test.add_vlan("300")

        # set admin=up for vlan 100,200,300
        self.test.set_vlan_admin("100", "up")
        self.test.set_vlan_admin("200", "up")
        self.test.set_vlan_admin("300", "up")

        # do initial verification
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, "{}", "down", "no_member_port")

        # create port, with multiple VIDs, default VID
        self.test.add_port(PORT_1, vlan_mode="trunk", tag="100", trunks="100,200,300")

        # verification
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")

        # remove vlan 200 from port
        self.test.set_port_vlans(PORT_1, "100,300")

        # verification
        vlan_data = self.test.get_vlan("100")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")
        vlan_data = self.test.get_vlan("200")
        self._verify_data(vlan_data, '{}', "down", "no_member_port")
        vlan_data = self.test.get_vlan("300")
        self._verify_data(vlan_data, '{enable="true"}', "up", "ok")

        # cleanup
        self.test.delete_all_ports()
        self.test.delete_all_vlans()
