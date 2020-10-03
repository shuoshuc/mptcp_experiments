# MPTCP Experiments

The goal of this experiment is to set up an MPTCP connection between 2 machines, and observe the protocol behaviors such as handshake, subflow creation etc.
To achieve that, there are several steps we need to take, as described below.

## [Step 1] Create a 2-VM topology.
We need a topology that MPTCP can run on. So we create 2 VMs in VirtualBox, VM-1 has 3 NICs and VM-2 has 2 NICs.

For VM-1, 1 NIC is in NAT mode used for Internet connectivity and 2 NICs are in host-only adapter mode connected to _vboxnet1_.

For VM-2, 1 NIC is in NAT mode and the other NIC is in host-only adapter mode also connected to _vboxnet1_.

_vboxnet1_ should have a range of **20.20.20.0/24** and a gateway of **20.20.20.1** on the host.

In later experiment, we will start an MPTCP server program on VM-2 and have VM-1 connect to it via the 2 NICs connected to _vboxnet1_.

## [Step 2] Install VM.
In this step, we install the 2 VMs with Ubuntu server 20.04.1 LTS.

After installation, remember to update & upgrade.
> $ sudo apt update

> $ sudo apt upgrade

## [Step 3] Compile and upgrade kernel.
We will be using 5.8.13 mainline kernel for the experiment since it contains MPTCP. All the following should be done on both VMs.

First, some preparation needs to be done: install building environment.

> $ sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev gdebi

5.8 kernel requires pahole (dwarves) version >= 1.16 but the one provided by Ubuntu 20.04 is only 1.15. So we need to download a higher version from Ubuntu 20.10 repo. (There is no compatibility issue.)

> $ wget http://archive.ubuntu.com/ubuntu/pool/universe/d/dwarves-dfsg/dwarves_1.17-1_amd64.deb

> $ sudo gdebi dwarves_1.17-1_amd64.deb

Of course, we need to download kernel source code as well.

> $ wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.8.13.tar.xz

> $ tar xvf linux-5.8.13.tar.xz

> $ cd linux-5.8.13/

If there is any non-standard kernel config that needs to be preserved, we can copy it into the folder as a start point for the new kernel config.

> $ cp -v /boot/config-$(uname -r) .config

Then, we can use a GUI interface to tweak the configs, especially enable MPTCP.

> $ make menuconfig

In the menu, follow _Networking support -> Networking Options -> MPTCP: Multipath TCP_. Make sure that _MPTCP: Multipath TCP_ option and _MPTCP: IPv6 support for Multipath TCP_ are both selected and marked with a star. This will make sure MPTCP is compiled into the kernel instead of as a module. Now, let's compile and install the new kernel.

> $ make -j $(nproc)

> $ sudo make modules_install

> $ sudo make install

Reboot both VMs, they should be running a 5.8.13 kernel.

## [Step 4] Configure VM IPs.
As mentioned in **[Step 1]**, the NICs connected to _vboxnet1_ need to be assigned IP addresses. The 2 NICs of VM-1 are assigned **20.20.20.10** and **20.20.20.11**. The only NIC of VM-2 is assigned **20.20.20.20**. Below are the commands to set them.

> $ sudo vi /etc/netplan/00-installer-config.yaml

Note that the fresh-installed VMs should only have one _yaml_ config under _/etc/netplan/_, but it could have a different name. For VM-1, open the file and specify the interface IPs like this. _enp0s8_ and _enp0s9_ are the two NICs.

```
# This is the network config written by 'subiquity'
network:
  ethernets:
    enp0s3:
      dhcp4: true
    enp0s8:
      dhcp4: false
      addresses: [20.20.20.10/24]
      gateway4: 20.20.20.1
    enp0s9:
      dhcp4: false
      addresses: [20.20.20.11/24]
      gateway4: 20.20.20.1
  version: 2
```

For VM-2, the config looks like this. _enp0s8_ is the NIC.

```
# This is the network config written by 'subiquity'
network:
  ethernets:
    enp0s3:
      dhcp4: true
    enp0s8:
      dhcp4: false
      addresses: [20.20.20.20/24]
      gateway4: 20.20.20.1
  version: 2
```

Save on exit and then run this command to apply the changes immediately.

> $ sudo netplan apply
