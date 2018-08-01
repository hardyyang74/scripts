#!/usr/bin/env python3

import os
import sys
import configparser

def build_kernel(cf, product):
  src_path = cf.get("kernel", "path")
  log_file = os.getcwd() + "/" + product + "/kernel/kernel.log" 

  print('build kernel')
  target_path, tf = os.path.split(log_file)
  if (not os.path.exists(target_path)):
    os.mkdir(target_path)

  os.environ['ARCH'] = 'arm'
  os.environ['CROSS_COMPILE'] = 'arm-fsl-linux-gnueabi-'

  command = "cd " + src_path + "; make uImage -j8 1>" + log_file + " 2>&1" + "; cp arch/arm/boot/uImage " + target_path
  print(command)
  os.system(command)

def build_uboot(cf, product):
  src_path = cf.get("uboot", "path")
  uconfig = cf.get("uboot", "config")
  log_file = os.getcwd() + "/" + product + "/uboot/uboot.log"

  print("build uboot")
  target_path, tf = os.path.split(log_file)
  if (not os.path.exists(target_path)):
    os.mkdir(target_path)

  os.environ['BUILD_DIR'] = 'out'
  command = "cd " + src_path + "; make " + uconfig + ";make -j8 1>" + log_file + " 2>&1" + "; cp out/u-boot.bin " + target_path
  os.system(command)

def build_config(config, *args, **kargs):
  product,extname = os.path.splitext(os.path.basename(config))
  if (not os.path.exists(product)):
    os.mkdir(product)

  cf = configparser.ConfigParser()
  cf.read(config)

  if (not args):
    build_uboot(cf, product)
    build_kernel(cf, product)
    sys.exit(0)

  if ('kernel' in args):
    build_kernel(cf, product)

  if ('uboot' in args):
    build_uboot(cf, product)

if __name__ == "__main__":
  help_str = """parameter is invalid!
    first para is configfile
    ... is module name
    demo: build.py iMX6D.ini kernel or uboot
    """

  if (len(sys.argv) < 2):
    print(help_str)
  elif (not os.path.isfile(sys.argv[1])):
    print("please input config file")
    sys.exit(1)
  else:
    build_config(sys.argv[1], *sys.argv[2:])
