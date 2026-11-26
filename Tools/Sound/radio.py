"""Radio messages and handset sounds: offline Windows speech (System.Speech), then a radio chain in ffmpeg.
Output: out/SW_Radio_*.wav (48 kHz mono 16-bit)."""
import os, subprocess, tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "out")
TMP = os.path.join(ROOT, "radio_tmp")
os.makedirs(OUT, exist_ok=True)
os.makedirs(TMP, exist_ok=True)

SECTORS = {"Front": "front", "Left": "left", "Right": "right", "Rear": "rear"}
TYPES = {"Tanks": "enemy tanks", "Cars": "armored cars", "ATGun": "anti tank gun", "Mixed": "mixed armor"}
LINES = {}
for s_key, sector in SECTORS.items():
    for t_key, kind in TYPES.items():
        LINES["SW_Radio_%s_%s" % (s_key, t_key)] = "Hammer one, contact %s, %s. Over." % (sector, kind)
LINES["SW_Radio_Clear"] = "Hammer one, no contacts around you. Over."


def ff(args):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-y"] + args, capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-1200:])
        raise SystemExit("ffmpeg failed")


def speak(text, path):
    ps = (
        "Add-Type -AssemblyName System.Speech;"
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer;"
        "$s.SelectVoice('Microsoft Zira Desktop'); $s.Rate = 1;"
        "$s.SetOutputToWaveFile('%s');"
        "$s.Speak('%s'); $s.Dispose()" % (path.replace("'", "''"), text.replace("'", "''"))
    )
    subprocess.run(["powershell", "-NoProfile", "-Command", ps], check=True)


# Speech through a handset: narrow band, compressed and a little driven, hiss under it, a squelch burst
# before and the key-up "kssht" after.
CHAIN = (
    "[0:a]aresample=48000,silenceremove=start_periods=1:start_threshold=-45dB,areverse,"
    "silenceremove=start_periods=1:start_threshold=-45dB,areverse,"
    "highpass=f=380,highpass=f=380,lowpass=f=3100,lowpass=f=3100,"
    "acompressor=threshold=-24dB:ratio=6:attack=3:release=60:makeup=8,"
    "volume=1.6,asoftclip=type=atan,equalizer=f=1800:t=q:w=1.2:g=4,"
    "adelay=110|110,apad=pad_dur=0.22[v];"
    "anoisesrc=d=8:c=pink:a=0.018:r=48000,highpass=f=500,lowpass=f=4000[hiss];"
    "anoisesrc=d=0.11:c=white:a=0.35:r=48000,highpass=f=900,lowpass=f=5000,afade=t=out:st=0.03:d=0.08[sq];"
    "[v][hiss]amix=inputs=2:duration=first:normalize=0[vh];"
    "[sq][vh]amix=inputs=2:duration=longest:normalize=0,"
    "areverse,afade=t=in:d=0.04,areverse,alimiter=limit=0.8:level=false,"
    "aformat=sample_fmts=s16:channel_layouts=mono"
)
for name, text in LINES.items():
    raw = os.path.join(TMP, name + "_raw.wav")
    speak(text, raw)
    ff(["-i", raw, "-filter_complex", CHAIN, "-ar", "48000", os.path.join(OUT, name + ".wav")])
    print("made", name)

# Ring while a report waits: an old field-phone buzz, two short bursts, then quiet (the loop gives the rhythm).
ff(["-f", "lavfi", "-i", "sine=f=420:d=1.6:r=48000", "-f", "lavfi", "-i", "sine=f=840:d=1.6:r=48000",
    "-filter_complex",
    "[0][1]amix=inputs=2:normalize=0,volume='if(lt(mod(t,0.8),0.32),1,0)':eval=frame,"
    "volume='0.5+0.5*sin(2*PI*25*t)':eval=frame,asoftclip=type=hard,lowpass=f=2500,highpass=f=300,"
    "atrim=0:1.6,volume=0.35,afade=t=in:d=0.005,aformat=sample_fmts=s16:channel_layouts=mono",
    "-ar", "48000", os.path.join(OUT, "SW_Radio_Call_Loop.wav")])
# Lifting the handset: the line opens with a short rush of static.
ff(["-f", "lavfi", "-i", "anoisesrc=d=0.35:c=white:a=0.5:r=48000", "-af",
    "highpass=f=700,lowpass=f=4500,afade=t=in:d=0.01,afade=t=out:st=0.08:d=0.27,volume=0.6,"
    "aformat=sample_fmts=s16:channel_layouts=mono", "-ar", "48000", os.path.join(OUT, "SW_Radio_Pickup.wav")])
print("made call, pickup")
