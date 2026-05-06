from pathlib import Path
import os
import platform
import shutil
import socket
import sys

arch = platform.machine()
homebrew_toolchain = "/opt/homebrew/bin"
has_homebrew_aarch64 = os.path.exists(os.path.join(homebrew_toolchain, "aarch64-linux-gnu-gcc"))
static_lib = "static_lib"

if "CardputerZero" in os.environ:
    if not os.path.exists('build/config/config_tmp.mk'):
        os.makedirs('build/config', exist_ok=True)
        with open('build/config/config_tmp.mk', 'w') as f:
            f.write('CONFIG_V9_5_LV_USE_LINUX_FBDEV=y\n')
            f.write('CONFIG_V9_5_LV_USE_EVDEV=y\n')
            f.write('CONFIG_V9_5_LV_DRAW_SW_ASM_NEON=y\n')
            f.write('CONFIG_V9_5_LV_USE_DRAW_SW_ASM=1\n')
            f.write('CONFIG_TINYALSA_COMPONENT_ENABLED=y\n')
            if has_homebrew_aarch64:
                f.write(f'CONFIG_TOOLCHAIN_PATH="{homebrew_toolchain}"\n')
            f.write('CONFIG_TOOLCHAIN_PREFIX="aarch64-linux-gnu-"\n')
            f.write(f'''CONFIG_TOOLCHAIN_SYSROOT="{os.path.join(sys.path[0], static_lib)}"\n''')
elif arch != "aarch64":
    if not os.path.exists('build/config/config_tmp.mk'):
        os.makedirs('build/config', exist_ok=True)
        with open('build/config/config_tmp.mk', 'w') as f:
            f.write('CONFIG_V9_5_LV_USE_SDL=y\n')
else:
    if not os.path.exists('build/config/config_tmp.mk'):
        os.makedirs('build/config', exist_ok=True)
        with open('build/config/config_tmp.mk', 'w') as f:
            f.write('CONFIG_V9_5_LV_USE_LINUX_FBDEV=y\n')
            f.write('CONFIG_V9_5_LV_USE_EVDEV=y\n')
            f.write('CONFIG_TINYALSA_COMPONENT_ENABLED=y\n')

local_path = Path(os.getcwd())
sdk_path = local_path.parent.parent / 'SDK'
os.environ['SDK_PATH'] = str(sdk_path)
os.environ['EXT_COMPONENTS_PATH'] = str(sdk_path.parent / 'ext_components')


env = SConscript(
    str(sdk_path / 'tools' / 'scons' / 'project.py'),
    variant_dir=os.getcwd(),
    duplicate=0,
)
