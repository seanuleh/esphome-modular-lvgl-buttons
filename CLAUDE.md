# Notes for Claude

## This repo mirrors the ESPHome config dir on `uleh`

This repo is a working mirror of the ESPHome config directory on the home
server `uleh` (192.168.1.99 — SSH host alias `uleh`, user `sean`):

- **Local:**  this repo
- **uleh:**   `/data/esphome/config/` (the `esphome` Docker container's
  `/config`; ESPHome dashboard at esphome.uleh.tv)

Either side can be edited (local repo, or the ESPHome web dashboard on uleh),
so they drift. **Sync is bidirectional and manual — there is no automation.**

Before assuming a file is up to date, diff against uleh:
```bash
ssh uleh 'cat /data/esphome/config/<name>.yaml' | diff - <name>.yaml
```
- **Pull (uleh → local):** `scp uleh:/data/esphome/config/<name>.yaml ./`
- **Push (local → uleh):** `scp <name>.yaml uleh:/data/esphome/config/`

Always diff first and confirm the direction — whichever side was edited most
recently is the source of truth, and overwriting the wrong way loses changes.
The uleh server has its own `~/CLAUDE.md` + `~/docs/smart-home.md`; read those
before operating on the server.

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
