"""Keep the native simulator shim aligned with the firmware flash API.

The published simulator currently names a ``WRONG_BOARD`` result that was
removed from INKademic's firmware flasher in favor of ``BAD_CHIP``.  The shim
is generated under ``.pio/libdeps`` by PlatformIO, so patch it at build time
instead of editing generated dependency files in the repository.
"""

from pathlib import Path


Import("env")  # noqa: F821 - SCons injects this at build time


def patch_simulator_firmware(source, target, env):
    simulator_source = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst(
        "$PIOENV"
    ) / "simulator" / "src" / "simulator_firmware.cpp"
    if not simulator_source.exists():
        return

    source_text = simulator_source.read_text()
    stale_case = '  case Result::WRONG_BOARD:\n    return "WRONG_BOARD";\n'
    stale_signature = (
        "Result flashFromSdPath(const char *, ProgressCb onProgress, void *ctx) {"
    )
    if stale_case not in source_text and stale_signature not in source_text:
        patch_simulator_ota(env)
        return

    patched_text = source_text.replace(stale_case, "").replace(
        stale_signature,
        "Result flashFromSdPath(const char *, ProgressCb onProgress, void *ctx, "
        "bool) {",
    )
    simulator_source.write_text(patched_text)
    print(f"Patched simulator flash result compatibility: {simulator_source}")
    patch_simulator_ota(env)


def patch_simulator_ota(env):
    simulator_ota_source = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst(
        "$PIOENV"
    ) / "simulator" / "src" / "simulator_ota.cpp"
    if not simulator_ota_source.exists():
        return

    ota_text = simulator_ota_source.read_text()
    patched_ota_text = ota_text.replace("#ifdef CROSSINK_VERSION", "#ifdef INKADEMIC_VERSION")
    if patched_ota_text != ota_text:
        simulator_ota_source.write_text(patched_ota_text)
        print(f"Patched simulator OTA version guard: {simulator_ota_source}")


patch_simulator_firmware(None, None, env)
