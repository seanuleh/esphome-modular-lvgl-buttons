# Notes for Claude

## ESPHome OTA — `upload` does NOT recompile

`esphome upload <config>.yaml` only ships whatever binary is already at
`.esphome/build/<name>/.pioenvs/<name>/firmware.bin`. If you edited YAML and
went straight to `esphome upload`, you will flash a stale binary and the
device will still run the old config — even though the upload reports
"Successfully uploaded program."

Always one of:
- `esphome run <config>.yaml` (compile + upload), or
- `esphome compile <config>.yaml && esphome upload <config>.yaml`

Sanity check before/after a flash:
- `ls -la .esphome/build/<name>/.pioenvs/<name>/firmware.bin` — mtime must be
  newer than the YAML you edited.
- After flash, confirm `desktop-dash/sensor/last_restart/state` over MQTT
  bumped to a fresh timestamp.
