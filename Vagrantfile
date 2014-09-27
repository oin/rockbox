# -*- mode: ruby -*-
# vi: set ft=ruby :

VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "hashicorp/precise32"
  config.ssh.forward_x11 = true
  config.vm.provision :shell, path: "vagrant-bootstrap.sh"
  config.vm.network :private_network, ip: '192.168.50.50'
  config.vm.synced_folder '.', '/vagrant', nfs: true
  config.vm.provider "virtualbox" do |v|
	v.customize ["modifyvm", :id, "--cpuexecutioncap", "90"]
	v.customize ["modifyvm", :id, "--cpus", 1]
	v.customize ["modifyvm", :id, "--audio", "coreaudio", "--audiocontroller", "ac97"]
  end
end
