# Runs inside the esphome-lvprev container (cwd /config). Doc: ~/docs/headless-browser.md
# Compiles $PREVIEW (host + SDL build), runs it under Xvfb and grabs a frame every
# $EVERY seconds, $SHOTS times, starting after $START s. Frames: /config/state<N>.png, all.png.
set -e
: "${PREVIEW:=preview.yaml}" "${START:=4.5}" "${EVERY:=3}" "${SHOTS:=1}" "${CROP:=480x480+0+0}"
name=$(grep -m1 -E '^\s+name:' "$PREVIEW" | awk '{print $2}')
esphome compile "$PREVIEW" >/tmp/c.log 2>&1 || { tail -30 /tmp/c.log; exit 1; }
Xvfb :99 -screen 0 480x480x24 >/dev/null 2>&1 &
sleep 1
export DISPLAY=:99
./.esphome/build/$name/.pioenvs/$name/program >/tmp/run.log 2>&1 &
sleep "$START"
i=0
while [ $i -lt "$SHOTS" ]; do
  xwd -root -silent | convert xwd:- -crop "$CROP" +repage /config/state$i.png
  i=$((i+1)); [ $i -lt "$SHOTS" ] && sleep "$EVERY"
done
convert /config/state*.png -append /config/all.png
