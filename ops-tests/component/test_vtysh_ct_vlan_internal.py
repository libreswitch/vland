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

TOPOLOGY = """
#
# +-------+
# |  sw1  |
# +-------+
#

# Nodes
[type=openswitch name="Switch 1"] sw1
"""


def test_vlan_internal_cli(topology, step):
    step("### VLAN CLI validations ###")
    sw1 = topology.get('sw1')

    assert sw1 is not None

    sw1('configure terminal'.format(**locals()))
    ret = sw1('do show vlan internal'.format(**locals()))
    assert 'Internal VLAN range  : 1024-4094' in ret and \
           'Internal VLAN policy : ascending' in ret, \
           'Internal VLAN range at bootup validation failed'

    ret = sw1('vlan internal range 100 10 ascending'.format(**locals()))
    assert 'Invalid VLAN range. End VLAN must be greater ' \
           'or equal to start VLAN' in ret, \
           'Internal VLAN range (start > end) validation failed'

    sw1('vlan internal range 10 10 ascending'.format(**locals()))
    ret = sw1('do show vlan internal'.format(**locals()))
    assert 'Internal VLAN range  : 10-10' in ret and \
           'Internal VLAN policy : ascending', \
           'Internal VLAN range (start = end) validation failed'

    sw1('vlan internal range 10 100 ascending'.format(**locals()))
    ret = sw1('do show vlan internal'.format(**locals()))
    assert 'Internal VLAN range  : 10-100' in ret and \
           'Internal VLAN policy : ascending' in ret, \
           'Ascending Internal VLAN range validation failed'

    sw1('vlan internal range 100 200 descending'.format(**locals()))
    ret = sw1('do show vlan internal'.format(**locals()))
    assert 'Internal VLAN range  : 100-200' in ret and \
           'Internal VLAN policy : descending' in ret, \
           'Descending Internal VLAN range validation failed'

    sw1('no vlan internal range'.format(**locals()))
    ret = sw1('do show vlan internal'.format(**locals()))
    assert 'Internal VLAN range  : 1024-4094' in ret and \
           'Internal VLAN policy : ascending' in ret, \
           'Default Internal VLAN range validation failed'
