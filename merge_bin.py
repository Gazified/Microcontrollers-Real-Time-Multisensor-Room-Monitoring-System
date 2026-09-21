Import("env")
import os

def merge_bin_action(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    app = os.path.join(build_dir, "firmware.bin")
    merged = os.path.join(build_dir, "firmware_merged.bin")

    py = env.subst("$PYTHONEXE")
    packages_dir = env.subst("$PROJECT_PACKAGES_DIR")
    esptool = os.path.join(packages_dir, "tool-esptoolpy", "esptool.py")

    cmd = f'"{py}" "{esptool}" --chip esp32 merge_bin -o "{merged}" --flash_mode dio --flash_size 4MB 0x1000 "{bootloader}" 0x8000 "{partitions}" 0x10000 "{app}"'
    env.Execute(cmd)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
