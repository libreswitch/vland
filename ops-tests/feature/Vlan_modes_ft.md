# Feature Test Cases for VLAN Modes

- [Objective](#objective)
- [Requirements](#requirements)
- [Setup](#setup)
	- [Topology](#topology)
	- [Test setup](#test-setup)
- [VLAN Access port behavior](#vlan-access-port-behavior)
	- [Verifying VLAN access mode](#verifying-vlan-access-mode)
	- [Verify the negative reachability-1 of VLAN access port](#verify-the-negative-reachability-1-of-vlan-access-port)
	- [Verify the negative reachability-2 of VLAN access](#verify-the-negative-reachability-2-of-vlan-access)
- [VLAN trunk allowed port behavior](#vlan-trunk-allowed-port-behavior)
	- [Verify L2 VLAN trunk port mode](#verify-l2-vlan-trunk-port-mode)
	- [Verify the negative reachability-1 of VLAN trunk allowed](#verify-the-negative-reachability-1-of-vlan-trunk-allowed)
	- [Verify the negative reachability-2 of VLAN trunk allowed](#verify-the-negative-reachability-2-of-vlan-trunk-allowed)
- [VLAN trunk native port behavior](#vlan-trunk-native-port-behavior)
	- [Verify the L2 VLAN trunk native untag mode](#verify-the-l2-vlan-trunk-native-untag-mode)
	- [Verify the negative reachability of VLAN trunk native](#verify-the-negative-reachability-of-vlan-trunk-native)
- [VLAN trunk native tag port behavior](#vlan-trunk-native-tag-port-behavior)
	- [Verify L2 VLAN trunk native tag mode](#verify-l2-vlan-trunk-native-tag-mode)
	- [Verify the negative reachability of VLAN trunk native tag](#verify-the-negative-reachability-of-vlan-trunk-native-tag)


## Objective
Verify the L2 reachability of VLAN modes such as access, trunk, native untag, and native tag.

## Requirements
The requirements for these test cases are:
 - OpenSwitch
 - Host1 and Host2

## Setup
### Topology

```
 +-------+                   +-------+
 |       |     +-------+     |       |
 |  hs1  <----->  sw1  <----->  hs2  |
 |       |     +-------+     |       |
 +-------+                   +-------+
```

### Test setup
Connect OpenSwitch interface 1 to eth1 on Host1.
Connect OpenSwitch interface 2 to eth1 on Host2.

## VLAN access port behavior
### Verifying VLAN access mode
#### Objective
Verify L2 reachability of VLAN access mode.

#### Description
1. In Host1 assign eth1 with IP add 2.2.2.2/24, and make it up.
2. In Host2 assign eth1 with IP add 2.2.2.3/24, and make it up.
3. Create VLAN2 and make it no shut.
4. Create two layer2 interfaces and make them up.
5. Make the port as VLAN2 access port
6. Ping 2.2.2.3 from Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping succeeds.

##### Test fail criteria
Ping fails.

### Verify the negative reachability-1 of VLAN access port

#### Objective
Verify the negative-reachability of VLAN access mode.

#### Description
1. In Host1 assign eth1 with IP add 2.2.2.2/24, and make it up.
2. In Host2 assign eth1 with IP add 2.2.2.3/24, and make it up.
3. Create VLAN2 ,VLAN3, and make them up
4. Create two layer2 interfaces and make them up.
5. Make port1 as VLAN2 access port and port2 as VLAN3 access port.
6. Ping 2.2.2.3 from the host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.

### Verify the negative reachability-2 of VLAN access

#### Objective
Verify the negative reachability of VLAN access mode.

#### Description
1. In Host1 create eth1.1 with IP add 2.2.2.2/24, assign VLAN encapsulation 2, and make it up.
2. In Host2 assign eth1.2 with IP add 2.2.2.3/24, assign VLAN encapsulation 2, and make it up.
3. Create VLAN2 and make it up.
4. Create two layer2 interfaces, and make them up.
5. Make ports as VLAN2 access port.
6. Ping 2.2.2.3 from the Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.

## VLAN trunk allowed port behavior

### Verify L2 VLAN trunk port mode
#### Objective
Verify the L2 reachability of VLAN trunk port.

#### Description
1. In Host1 create eth1.2 with IP add 2.2.2.2/24, assign VLAN encapsulation 2, and make it up.
2. In Host1 create eth1.3 with IP add 3.3.3.3/24, assign VLAN encapsulation 3, and make it up.
3. In Host2 create eth1.2 with IP add 2.2.2.3/24, assign VLAN encapsulation 2, and make it up.
4. In Host2 create eth1.3 with IP add 3.3.3.4/24, assign VLAN encapsulation 3, and make it up.
5. Create VLAN2, VLAN3, and make them up.
6. Create two layer2 interfaces, and make them up.
7. Make  port1 and port2 as VLAN trunk allowed 2 and 3.
8. Ping 2.2.2.3 from the Host1.
9. Ping 3.3.3.4 from the Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping succeeds.

##### Test fail criteria
Ping fails.

### Verify the negative reachability-1 of VLAN trunk allowed

#### Objective
Verify the negative-reachability of VLAN trunk allowed port.

#### Description
1. In Host1 create eth1 with IP add 2.2.2.2/24, and make it up.
2. In Host2 create eth1.2 with IP add 2.2.2.3/24, assign VLAN encapsulation 2, and make it up.
3. Create VLAN2, VLAN3, and make them up.
4. Create two layer2 interfaces, and make them up.
5. Make  port1 as VLAN2 trunk port.
6. Make port2 as VLAN trunk allowed 2 and 3.
7. Ping 2.2.2.3 from the Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.

### Verify the negative reachability-2 of VLAN trunk allowed

#### Objective
Verify the negative-reachability of VLAN trunk allowed port.
#### Description
1. In Host1 create eth1.2 with IP add 2.2.2.2/24 ,assign vlan encapsulation 2 and make it up.
2. In Host2 create eth1.3 with IP add 2.2.2.3/24, assign vlan encapsulation 3 and make it up.
3. Create VLAN2, VLAN3, and make them up.
4. Create two layer2 interfaces, and make them up.
5. Make port1 as VLAN2 trunk port.
6. Make port2 as VLAN3 trunk port.
6. Ping 2.2.2.3 from Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.

## VLAN trunk native port behavior

### Verify the L2 VLAN trunk native untag mode

#### Objective
Verify the L2 reachability of VLAN trunk native port.
#### Description
1. In Host1 create eth1.2 with Ip add 2.2.2.2/24, assign VLAN encapsulation 2, and make it up.
2. In Host2 assign eth1 with IP add 2.2.2.3/24 and make it up.
3. Create VLAN2 and make it no shut.
4. Create two layer2 interfaces, and make them up.
5. Make port 1 as "vlan trunk allowed 2" and port 2 as "vlan trunk native 2".
6. Ping 2.2.2.3 from Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping succeeds.

##### Test fail criteria
Ping fails.

### Verify the negative reachability of VLAN trunk native

#### Objective
Verify the negative-reachability of vlan trunk native port.
#### Description
1. In Host1 create eth1.2 with IP add 2.2.2.2/24, assign VLAN encapsulation 3, and make it up.
2. In Host2 create eth1.2 with IP add 2.2.2.3/24, assign VLAN encapsulation 2, and make it up.
3. Create VLAN2 and make them up.
4. Create two layer2 interfaces, and make them up.
5. Make port 1 as "vlan trunk allowed 2" and port 2 as "vlan trunk native 2".
6. Ping 2.2.2.3 from Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.

## VLAN trunk native tag port behavior

### Verify L2 VLAN trunk native tag mode
#### Objective
Verify the negative-reachability of VLAN trunk native tag port.
#### Description
1. In Host1 assign eth1 with IP add 2.2.2.2/24, and make it up.
2. In Host2 create eth1.2 with IP add 2.2.2.3/24, assign VLAN encapsulation 2, and make it up.
3. Create vlan 2 and make it up.
4. Create two layer2 interfaces, and make them up.
5. Make port 1 as "vlan trunk native 2", port 2 as "vlan trunk allowed 2", and VLAN "trunk native tag".
6. Ping 2.2.2.3 from the Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping succeeds.

##### Test fail criteria
Ping fails.

### Verify the negative reachability of VLAN trunk native tag

#### Objective
Verify the negative-reachability of VLAN trunk native tag mode.
#### Description
1. In Host1 assign eth1 with IP add 2.2.2.2/24, and make it up.
2. In Host2 create eth1 with IP add 2.2.2.3/24, and make it up.
3. Create VLAN2 and make it up.
4. Create two layer2 interfaces, and make them up.
5. Make port 1 as "vlan trunk native 2", port 2 as "vlan trunk allowed 2", and VLAN "trunk native tag".
6. Ping 2.2.2.3 from the Host1.

#### Test result criteria
Ping result.

##### Test pass criteria
Ping fails.

##### Test fail criteria
Ping succeeds.
