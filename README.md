# vvas-core

## Copyright and license statement
Copyright 2022 Xilinx Inc.

Copyright (C) 2022-2023 Advanced Micro Devices, Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0).

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

vvas-core is a simple C API based interface provided to implement Machine Learning applications. Vitis Video Analytics SDK (VVAS) 3.0 is based on this interface.

## Build and install VVAS essentials for embedded solutions:

A helper script, **./build_install.sh**, is provided in root of this repo to build and install vvas-core.

### For PCIe platform
Example command to build for PCIe platform. Only V70 platform is supported.

./build_install.sh TARGET=PCIe PLATFORM=V70

### For Edge platform
Step 1 : Source sysroot path if not done already
```
	source <sysroot path>/environment-setup-aarch64-xilinx-linux
```
Step 2 : Build
```
	./build_install.sh TARGET=Edge
```
Step 3 : copy vvas-core to embedded board
```
	scp install/vvas_base_installer.tar.gz <board ip>:/
```
Step 4 : Install VVAS on embedded board
```
	cd /
	tar -xvf vvas_base_installer.tar.gz
```
