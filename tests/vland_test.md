# vland Test Cases

[TOC]

## Test Initial Conditions
### Objective
Verify that newly created VLANs have the correct configuration and state.
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description ###
1. Add VLANs 100, 200, and 300
2. Verify that all three VLANs have no hw\_vlan\_config data, are in the "down" state, and the state reason is "admin\_down"
3. Delete all VLANs
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Test VLAN State Transitions
### Objective
Verify that VLAN state transitions correctly for admin state and member port changes.
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description
1. Add VLAN 200
2. Create port 1 (attached to interface 1) with access mode in VLAN 200
3. Delete port 1
4. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "admin"down"
5. Set VLAN 200 admin "up"
6. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "no_member_port"
7. Create port 1 (attached to interface 1) with trunk mode and trunks 200
8. Verify that VLAN 200 has hw\_vlan\_config "enable=true", is in the "up" state, and the state reason is "ok"
9. Delete all VLANs
10. Delete all ports
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Test Admin Down State
### Objective
Verify that VLANs with admin=down have state reason "admin_down"
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description
1. Add VLANs 100, 200, and 300
2. Create port 1 (attached to interface 1) with access mode in VLAN 100
3. Verify that all VLANs have no hw\_vlan\_config data, are in the "down" state, and the state reason is "admin\_down"
4. Delete all ports
5. Delete all VLANs
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Test Admin Up State
### Objective
Verify that VLANs with admin=up have state reason depending on port membership
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description
1. Add VLANs 100, 200, and 300
2. Set VLAN 200 admin=up
3. Verify that VLANs 100 and 300 have no hw\_vlan\_config data, are in the "down" state, and the state reason is "admin\_down"
4. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "no\_member\_port"
4. Delete all ports
5. Delete all VLANs
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Test Admin Up to Down State Transition
### Objective
Verify that VLANs with admin=up changing to admin=down result in correct state
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description
1. Add VLANs 100, 200, and 300
2. Set VLAN 200 admin=up
3. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "no\_member\_port"
4. Create port 1 (attached to interface 1) with access mode in VLAN 200
5. Verify that VLAN 200 has hw\_vlan\_config "enable=true", is in the "up" state, and the state reason is "ok"
6. Set VLAN 200 admin=down
7. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "admin\_down"
8. Delete all ports
9. Delete all VLANs
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Test Trunk VLANs
### Objective
Verify that trunk-mode ports are recorded as members of VLANs appropriately
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[s1]
```
### Description
1. Add VLANs 100, 200, and 300
2. Set VLAN 100, 200, and 300 admin=up
3. Verify that all VLANs have no hw\_vlan\_config data, are in the "down" state, and the state reason is "no\_member\_port"
4. Create port 1 (attached to interface 1) with trunk mode with VLANS 100, 200, 300
5. Verify that all VLANs have hw\_vlan\_config "enable=true", is in the "up" state, and the state reason is "ok"
6. Change port 1 to have VLANs 100, 300 (remove 200)
7. Verify that VLAN 200 has no hw\_vlan\_config data, is in the "down" state, and the state reason is "no\_member\_port"
8. Verify that VLANs 100, 300 have hw\_vlan\_config "enable=true", is in the "up" state, and the state reason is "ok"
9. Delete all ports
10. Delete all VLANs
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.
