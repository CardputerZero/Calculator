from pathlib import Path
import os
import platform
import sys

local_path = Path(os.getcwd())
workspace_root = local_path.parent.parent
sdk_path = Path(os.environ.get("CARDPUTERZERO_SDK_PATH", workspace_root / "SDK")).resolve()
ext_components_path = Path(os.environ.get(
    "CARDPUTERZERO_EXT_COMPONENTS_PATH", workspace_root / "ext_components"
)).resolve()
version = (ext_components_path / "cp0_lvgl" / "sdk_version.txt").read_text(encoding="utf-8").strip()
static_lib_path = sdk_path / "github_source" / f"static_lib_{version}"

if os.environ.get("CardputerZero", "") == "y":
    os.environ["CONFIG_DEFAULT_FILE"] = "linux_x86_cross_cp0_config_defaults.mk"
elif os.environ.get("CONFIG_DEFAULT_FILE") is None and platform.machine() == "x86_64":
    os.environ["CONFIG_DEFAULT_FILE"] = "config_defaults.mk"

selected_config_path = Path(os.environ["CONFIG_DEFAULT_FILE"]).resolve()
selected_config = str(selected_config_path) + "\n" + selected_config_path.read_text(encoding="utf-8")
if "cross" in os.environ["CONFIG_DEFAULT_FILE"]:
    selected_config += f"\nstatic-lib={static_lib_path}\n"
selection_path = Path("build") / "config" / "selected-defaults.txt"
previous_config = selection_path.read_text() if selection_path.exists() else ""
if previous_config != selected_config:
    for generated_name in ("global_config.mk", "global_config.h", "lvgl_config.h"):
        generated_path = Path("build") / "config" / generated_name
        if generated_path.exists():
            generated_path.unlink()
    selection_path.parent.mkdir(parents=True, exist_ok=True)
    selection_path.write_text(selected_config)

cross_package_enabled = "cross" in os.environ["CONFIG_DEFAULT_FILE"]
config_tmp_path = Path("build") / "config" / "config_tmp.mk"
if cross_package_enabled:
    config_tmp_content = (
        f'CONFIG_TOOLCHAIN_SYSROOT="{static_lib_path.as_posix()}"\n'
        "CONFIG_V9_5_LV_USE_LINUX_FBDEV=y\n"
        "CONFIG_V9_5_LV_USE_EVDEV=y\n"
        "CONFIG_V9_5_LV_DRAW_SW_ASM_NEON=y\n"
        "CONFIG_V9_5_LV_USE_DRAW_SW_ASM=1\n"
    )
    if not config_tmp_path.exists() or config_tmp_path.read_text() != config_tmp_content:
        config_tmp_path.parent.mkdir(parents=True, exist_ok=True)
        config_tmp_path.write_text(config_tmp_content)
else:
    config_tmp_content = "CONFIG_V9_5_LV_USE_SDL=y\n"
    if not config_tmp_path.exists() or config_tmp_path.read_text() != config_tmp_content:
        config_tmp_path.parent.mkdir(parents=True, exist_ok=True)
        config_tmp_path.write_text(config_tmp_content)

os.environ["SDK_PATH"] = str(sdk_path)
os.environ["EXT_COMPONENTS_PATH"] = str(ext_components_path)


env = SConscript(
    str(sdk_path / 'tools' / 'scons' / 'project.py'),
    variant_dir=os.getcwd(),
    duplicate=0,
)

if cross_package_enabled:
    update = False
    if not static_lib_path.exists():
        update = True
    else:
        try:
            with open(str(static_lib_path / "version"), "r") as f:
                if version != f.read().strip():
                    update = True
        except Exception:
            update = True

    if update:
        with open(env["PROJECT_TOOL_S"]) as f:
            exec(f.read())
        down_url = (
            "https://github.com/CardputerZero/M5CardputerZero-UserDemo/"
            "releases/download/{}/sdk_bsp.tar.gz"
        ).format(version)
        check_wget_down(down_url, f"static_lib_{version}.tar.gz")
