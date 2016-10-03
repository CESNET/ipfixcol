# <a name="top"></a>Installing IPFIXcol using Ansible

If you are familiar with Ansible, you can use it to easily install IPFIXcol on any target machine. Provided playbook allows you to separately install:
* dependencies for IPFIXcol
* IPFIXcol base
* any IPFIXcol plugin (requires base first)
* IPFIXcol tools (e.g. fbitdump)

The Ansible orchestration was tested on following systems:
* Debian Jessie (8.5)
* Ubuntu Xenial (16.04 LTS)
* CentOS 7 (7.2.1511)
* Fedora 24

If you want to orchestrate latest Fedora systems, make sure your Ansible version contains the `dnf` module (versions >= 1.9.4). Tested version is 2.1.0.

## <a name="howto"></a>Howto use IPFIXcol playbook

Installing IPFIXcol with Ansible is fairly simple. First, put target machines into the hosts file. The default target is localhost.
```
[ipfixcol-hosts]
localhost
```
To install the entire collector including all (exceptions apply) plugins and tools, use
```
ansible-playbook -i hosts ipfixcol.yml
```
To see what this command would do, use `--check` switch with `ansible-playbook`

If you want only dependencies for basic IPFIXcol without plugins, use
```
ansible-playbook -i hosts ipfixcol.yml --tags dependencies
```
There is a tag for each plugin and its dependency. LibFastbit library can be installed separately as well.

## <a name="libfastbit"></a>LibFastbit compilation

The LibFastbit is a fairly large beast and takes some time to compile. Therefore, we usually install it using binaries, which are pre-compiled for all supported systems (see above). However, if you are installing on unsupported an distribution or you just want to build from sources, you can force this by using `--extra-vars "build_fastbit_compile=true"`