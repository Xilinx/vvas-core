#!/bin/bash

# This script automates the build and deployment of images to the target/Host. If your
# environment is different you may need to make minor changes.
#set -o xtrace
usage()
{
  echo "Usage: e.g. $0 TARGET=<PCIe/Edge> [PLATFORM=U30/V70] [ENABLE_PPE=<0/1> USE_SIMD=<0/1>] [VVAS_CORE_UTILS=<GLIB>] "
  exit 1
}

#FIXME force clean before every build, otherwise old build configuration is used
./clean.sh

for ARGUMENT in "$@"
do
  KEY=$(echo $ARGUMENT | cut -f1 -d=)

  KEY_LENGTH=${#KEY}
  VALUE="${ARGUMENT:$KEY_LENGTH+1}"

  export "$KEY"="$VALUE"
done

if [ ! "$TARGET" ]; then
  echo "TARGET is must for compilation"
  usage
fi

if [ ! "$USE_SIMD" ]; then
  USE_SIMD=0
  echo "Default: Use of SIMD library disabled"
fi

if [ ! "$ENABLE_PPE" ]; then
  ENABLE_PPE=1
  echo "Default: Use of PPE is enabled"
fi

if [ ! "$VVAS_CORE_UTILS" ]; then
  VVAS_CORE_UTILS="GLIB"
  echo "Default: Using GLib utils"
elif [[ ("$VVAS_CORE_UTILS" = "glib") || ("$VVAS_CORE_UTILS" = "GLIB") || ("$VVAS_CORE_UTILS" = "Glib") ]]; then
  VVAS_CORE_UTILS="GLIB"
else
  echo "NON GLIB Utility is not supported"
  usage
fi

echo TARGET = $TARGET
echo USE_SIMD = $USE_SIMD
echo ENABLE_PPE = $ENABLE_PPE
echo VVAS_CORE_UTILS = $VVAS_CORE_UTILS

if [ -f "/opt/xilinx/vvas/include/vvas/config.h" ]; then
  echo -e "\nINFO: Deleting previous build and its configuration\n"
  sudo ./clean_vvas.sh
fi

if [[ ("$TARGET" = "Edge") || ("$TARGET" = "EDGE") || ("$TARGET" = "edge") ]]; then
  echo "Building for Edge"
  TARGET="EDGE"
  PREFIX="/usr"
elif [[ ("$TARGET" = "Pcie") || ("$TARGET" = "PCIE") || ("$TARGET" = "pcie") || ("$TARGET" = "PCIe") ]]; then
  echo "Building for PCIe HOST"
  TARGET="PCIE"
  LIB="lib"
  PREFIX="/opt/xilinx/vvas/"
else
  echo "TARGET is not supported"
  usage
fi

set -e

# Get the current meson version and update the command
# "meson <builddir>" command should be used as "meson setup <builddir>" since 0.64.0
MesonCurrV=`meson --version`
MesonExpecV="0.64.0"

if [ $(echo -e "${MesonCurrV}\n${MesonExpecV}"|sort -rV |head -1) == "${MesonCurrV}" ];
then
MESON="meson setup"
else
MESON="meson"
fi

if [[ "$TARGET" == "EDGE" ]]; then #TARGET == EDGE
  if [ -z "$CC" ]; then
  echo "Cross compilation not set - source environment setup first"
  exit
  fi

  rm -rf install

  # Work-arround the dependancy on meson.native in sysroot
  if [ ! -f $OECORE_NATIVE_SYSROOT/usr/share/meson/meson.native ]; then
    touch $OECORE_NATIVE_SYSROOT/usr/share/meson/meson.native;
  fi

  sed -E 's@<SYSROOT>@'"$SDKTARGETSYSROOT"'@g; s@<NATIVESYSROOT>@'"$OECORE_NATIVE_SYSROOT"'@g' meson.cross.template > meson.cross
  cp meson.cross $OECORE_NATIVE_SYSROOT/usr/share/meson/aarch64-xilinx-linux-meson.cross
  $MESON build --prefix $PREFIX --cross-file $PWD/meson.cross -Denable_ppe=$ENABLE_PPE -Dtracker_use_simd=$USE_SIMD -Dvvas_core_utils=$VVAS_CORE_UTILS
  cd build
  ninja
  DESTDIR=$SDKTARGETSYSROOT ninja install
  DESTDIR=../install ninja install

  cd ../install
  tar -pczvf vvas_base_installer.tar.gz usr
  cd ..

else # TARGET == PCIE

  if [[ (! "$PLATFORM") || ("$PLATFORM" == "V70") || ("$PLATFORM" == "v70") ]]; then
    PCI_PLATFORM="V70"
  else if [[ ("$PLATFORM" == "U30") || ("$PLATFORM" == "u30") ]]; then
    PCI_PLATFORM="U30"
  else
    PCI_PLATFORM="V70"
  fi
  fi

  BASEDIR=$PWD
  source /opt/xilinx/vvas/setup.sh

  # export inside this file will not affect the terminal environment
  # make sure before running the pipe run “source /opt/xilinx/vvas/setup.sh”
  export LD_LIBRARY_PATH=$PWD/install/opt/xilinx/vvas/lib:$LD_LIBRARY_PATH
  export PKG_CONFIG_PATH=$PWD/install/opt/xilinx/vvas/lib/pkgconfig:$PKG_CONFIG_PATH

  $MESON build --prefix $PREFIX --libdir $LIB -Denable_ppe=$ENABLE_PPE -Dtracker_use_simd=$USE_SIMD -Dpci_platform=$PCI_PLATFORM -Dvvas_core_utils=$VVAS_CORE_UTILS
  cd build
  ninja
  sudo ninja install

fi # close TARGET == PCIE
