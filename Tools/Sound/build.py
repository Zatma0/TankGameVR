"""Cut, layer and master the game sounds from the CC0 recordings (ffmpeg). Output: 48 kHz 16-bit WAV in out/."""
import glob, json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(ROOT, "raw")
OUT = os.path.join(ROOT, "out")
os.makedirs(OUT, exist_ok=True)
USED = {}


def src(sound_id):
    hits = glob.glob(os.path.join(RAW, "*", "%d_*" % sound_id))
    assert hits, sound_id
    return hits[0]


def ff(args):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-y"] + args, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(r.stderr[-1500:])
        raise SystemExit("ffmpeg failed")
    return r.stderr


def level(path):
    # astats works in float, so peaks above full scale are seen too.
    err = ff(["-i", path, "-af", "astats=metadata=0:measure_perchannel=none", "-f", "null", "-"])
    peak = float(re.findall(r"Peak level dB: (-?[0-9.]+|-inf)", err)[-1].replace("-inf", "-120"))
    mean = float(re.findall(r"RMS level dB: (-?[0-9.]+|-inf)", err)[-1].replace("-inf", "-120"))
    return peak, mean


def onset_ms(path):
    """Milliseconds of near silence before the sound starts."""
    err = ff(["-i", path, "-af", "silencedetect=noise=-36dB:d=0.001", "-f", "null", "-"])
    starts = re.findall(r"silence_start: (-?[0-9.]+)", err)
    ends = re.findall(r"silence_end: (-?[0-9.]+)", err)
    if starts and float(starts[0]) <= 0.001 and ends:
        return float(ends[0]) * 1000.0
    return 0.0


def render(name, inputs, graph, channels=1, peak=-1.0, mean=None, tail_fade=0.08):
    """inputs: list of sound ids or ('lavfi', 'expr'). graph ends in [out]."""
    args = []
    ids = []
    for item in inputs:
        if isinstance(item, tuple):
            args += ["-f", "lavfi", "-i", item[1]]
        else:
            args += ["-i", src(item)]
            ids.append(item)
    tmp = os.path.join(OUT, "_tmp.wav")
    layout = "mono" if channels == 1 else "stereo"
    ff(args + ["-filter_complex", graph + ";[out]aresample=48000,aformat=sample_fmts=flt:channel_layouts=%s[fin]" % layout,
               "-map", "[fin]", "-c:a", "pcm_f32le", tmp])
    p, m = level(tmp)
    gain = (mean - m) if mean is not None else (peak - p)
    final = os.path.join(OUT, name + ".wav")
    # Loops are leveled by their average and must not clip; one-shots by their peak.
    post = "volume=%.2fdB,alimiter=limit=0.89:attack=2:release=80:level=false" % gain
    if mean is None:
        # One-shots start on the first sample that matters, so they land on the frame that triggers them.
        post = "silenceremove=start_periods=1:start_threshold=-46dB:start_silence=0.001," + post
    ff(["-i", tmp, "-af", post, "-ar", "48000", "-c:a", "pcm_s16le", final])
    os.remove(tmp)
    USED[name] = ids
    dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", final], capture_output=True, text=True).stdout)
    p2, m2 = level(final)
    print("%-26s %5.2f s  peak %5.1f dB  mean %5.1f dB  starts after %4.1f ms  %s" % (name, dur, p2, m2, onset_ms(final), ids))


def seg(index, start, end, more="", label=None):
    label = label or "s%d" % index
    chain = "[%d:a]aresample=48000,atrim=start=%s:end=%s,asetpts=PTS-STARTPTS" % (index, start, end)
    if more:
        chain += "," + more
    return chain + "[%s]" % label


def pitch(ratio):
    return "asetrate=%d,aresample=48000" % round(48000 * ratio)


def loop_graph(index, start, length, cross, more=""):
    """Seamless loop: the end of the take is cross-faded into its beginning."""
    a, b = start, start + length
    g = "[%d:a]aresample=48000%s,asplit=3[l0][l1][l2];" % (index, ("," + more) if more else "")
    g += "[l0]atrim=start=%s:end=%s,asetpts=PTS-STARTPTS[body];" % (a + cross, b)
    g += "[l1]atrim=start=%s:end=%s,asetpts=PTS-STARTPTS[tail];" % (b, b + cross)
    g += "[l2]atrim=start=%s:end=%s,asetpts=PTS-STARTPTS[head];" % (a, a + cross)
    g += "[tail][head]acrossfade=d=%s:c1=tri:c2=tri[x];[body][x]concat=n=2:v=0:a=1" % cross
    return g


LIMIT = "alimiter=limit=0.9:attack=1:release=80:level=false"
PUNCH = "acompressor=threshold=-20dB:ratio=3:attack=4:release=140:makeup=3"
SUB = ("lavfi", "aevalsrc='sin(2*PI*%s*(1-exp(-%s*t))/%s)*exp(-%s*t)':s=48000:d=%s")


def sub(f0=95, drop=5.0, decay=4.5, dur=1.2):
    return ("lavfi", SUB[1] % (f0, drop, drop, decay, dur))


def tone(freq, dur, shape="sine"):
    return ("lavfi", "sine=f=%s:d=%s:r=48000" % (freq, dur))


def noise(dur, color="white"):
    return ("lavfi", "anoisesrc=c=%s:d=%s:r=48000:a=0.6" % (color, dur))


def build():
    # ---------------------------------------------------------------- main gun
    # Our gun from outside: howitzer crack + the low body of a tank shot + a long cannon tail + sub thump.
    render("SW_Cannon_Fire", [161343, 127845, 448002, sub(100, 5.5, 4.0, 1.4)],
           seg(0, 0, 0.8, "afade=t=out:st=0.55:d=0.25,treble=g=3:f=4000,volume=2.2", "crack") + ";"
           + seg(1, 0, 3.6, "bass=g=4:f=80,afade=t=out:st=2.2:d=1.4,volume=0.8", "body") + ";"
           + seg(2, 0, 4.2, "highpass=f=180,afade=t=in:d=0.02,afade=t=out:st=2.4:d=1.8,volume=0.45", "tail") + ";"
           + "[3:a]afade=t=out:st=0.7:d=0.7,volume=0.8[sub];"
           + "[crack][body][tail][sub]amix=inputs=4:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # The same shot from inside the hull: dull pressure thump, the recoil clank and a short metal ring.
    render("SW_Cannon_Interior", [127845, 191095, 351371, sub(70, 4.0, 5.0, 1.0)],
           seg(0, 0, 1.6, "lowpass=f=700,bass=g=6:f=70,afade=t=out:st=0.7:d=0.9", "thump") + ";"
           + seg(1, 2.05, 2.75, pitch(0.7) + ",adelay=70:all=1,volume=5.0,aecho=0.8:0.5:35|70:0.3|0.18", "clank") + ";"
           + seg(2, 0.03, 0.42, pitch(0.45) + ",lowpass=f=3000,adelay=20:all=1,volume=0.35,afade=t=out:st=0.5:d=0.4", "ring") + ";"
           + "[3:a]afade=t=out:st=0.5:d=0.5,volume=0.9[sub];"
           + "[thump][clank][ring][sub]amix=inputs=4:normalize=0," + LIMIT + "[out]", channels=2)

    # Recoil over: the breech drops open.
    render("SW_Breech_Open", [191095],
           seg(0, 0.28, 1.0, pitch(0.8) + ",volume=6.0,afade=t=out:st=0.5:d=0.3", "latch") + ";[latch]" + LIMIT + "[out]", peak=-3.0)

    # The case stub hitting the floor (played on the frame it lands).
    render("SW_Casing_Drop", [133101],
           seg(0, 1.50, 3.2, pitch(0.85) + ",afade=t=out:st=1.5:d=0.5", "stub") + ";[stub]" + LIMIT + "[out]", peak=-3.0)

    # A round pushed into the chamber: the scrape.
    render("SW_Shell_Ram", [530075],
           seg(0, 0, 0.72, "aresample=48000," + pitch(0.75) + ",afade=t=in:d=0.03,afade=t=out:st=0.72:d=0.22", "scrape") + ";[scrape]" + LIMIT + "[out]", peak=-3.0)

    # ... and the moment it seats.
    render("SW_Shell_Seat", [392240],
           seg(0, 0, 0.5, "lowpass=f=1800,afade=t=out:st=0.25:d=0.2", "seat") + ";[seat]" + LIMIT + "[out]", peak=-4.0)

    # The wedge slams shut: heavy thunk with the latch on top.
    render("SW_Breech_Close", [392240, 191095, sub(85, 6.0, 9.0, 0.5)],
           seg(0, 0, 0.9, pitch(0.8) + ",bass=g=5:f=110,afade=t=out:st=0.6:d=0.45", "thunk") + ";"
           + seg(1, 2.05, 2.75, pitch(0.9) + ",volume=4.5,adelay=15:all=1", "latch") + ";"
           + "[2:a]volume=0.6[sub];"
           + "[thunk][latch][sub]amix=inputs=3:normalize=0," + PUNCH + "," + LIMIT + "[out]")

    # Trigger pulled on an empty gun.
    render("SW_Dry_Fire", [275630, 351371],
           seg(0, 0.09, 0.45, pitch(0.7) + ",volume=6.0", "click") + ";"
           + seg(1, 0.03, 0.3, pitch(1.6) + ",volume=0.08,adelay=10:all=1", "tick") + ";"
           + "[click][tick]amix=inputs=2:normalize=0," + LIMIT + "[out]", peak=-6.0)

    # ---------------------------------------------------------------- where the rounds land
    # AP on armour: a hard steel clang and a sharp crack, no fireball.
    render("SW_Impact_AP", [267893, 351371, 220062],
           seg(0, 0.24, 1.25, pitch(0.72) + ",bass=g=4:f=140,afade=t=out:st=0.9:d=0.45", "clang") + ";"
           + seg(1, 0.03, 0.42, pitch(0.8) + ",volume=0.8,afade=t=out:st=0.3:d=0.2", "ping") + ";"
           + seg(2, 0.06, 0.55, "highpass=f=900,volume=0.5,afade=t=out:st=0.2:d=0.3", "crack") + ";"
           + "[clang][ping][crack]amix=inputs=3:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # HE: fireball, deep boom, debris coming down.
    render("SW_Explosion_HE", [366086, 523365, 703247, sub(80, 3.0, 2.6, 1.8)],
           seg(0, 0, 5.0, "silenceremove=start_periods=1:start_threshold=-40dB,bass=g=5:f=70,afade=t=out:st=2.6:d=2.2", "boom") + ";"
           + seg(1, 0, 4.0, "silenceremove=start_periods=1:start_threshold=-40dB,highpass=f=120,volume=0.7,afade=t=out:st=1.8:d=2.0", "blast") + ";"
           + seg(2, 0, 2.6, "silenceremove=start_periods=1:start_threshold=-40dB,adelay=450:all=1,volume=0.5,afade=t=out:st=2.3:d=0.7", "debris") + ";"
           + "[3:a]afade=t=out:st=0.9:d=0.9,volume=0.9[sub];"
           + "[boom][blast][debris][sub]amix=inputs=4:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # A round into the ground: thud and falling dirt.
    render("SW_Impact_Dirt", [703248, 220062],
           seg(0, 0, 2.3, "silenceremove=start_periods=1:start_threshold=-40dB,afade=t=out:st=1.5:d=0.7", "dirt") + ";"
           + seg(1, 0.06, 1.0, "lowpass=f=500,volume=0.6,afade=t=out:st=0.4:d=0.5", "thud") + ";"
           + "[dirt][thud]amix=inputs=2:normalize=0," + LIMIT + "[out]")

    # An enemy vehicle blowing up: explosion with torn metal in it.
    render("SW_Enemy_Destroyed", [220062, 267893, 703247, sub(75, 3.5, 3.0, 1.5)],
           seg(0, 0.06, 4.4, "bass=g=4:f=80,afade=t=out:st=2.4:d=1.9", "boom") + ";"
           + seg(1, 0.24, 1.25, pitch(0.5) + ",volume=0.7,adelay=60:all=1", "metal") + ";"
           + seg(2, 0, 2.6, "silenceremove=start_periods=1:start_threshold=-40dB,adelay=700:all=1,volume=0.45", "debris") + ";"
           + "[3:a]afade=t=out:st=0.8:d=0.7,volume=0.8[sub];"
           + "[boom][metal][debris][sub]amix=inputs=4:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # ---------------------------------------------------------------- our hull
    # A heavy round on our own armour, heard from inside: a bell-like bang through the whole hull.
    render("SW_Hull_Hit", [267893, 392240, 351371, sub(90, 5.0, 5.0, 0.9)],
           seg(0, 0.24, 1.25, pitch(0.6) + ",lowpass=f=5000,aecho=0.8:0.6:40|85:0.35|0.2,afade=t=out:st=1.2:d=0.6", "bang") + ";"
           + seg(1, 0, 0.9, pitch(0.7) + ",bass=g=6:f=90,volume=0.8", "thud") + ";"
           + seg(2, 0.03, 0.42, pitch(0.38) + ",volume=0.7,afade=t=out:st=0.5:d=0.6", "ring") + ";"
           + "[3:a]volume=0.7[sub];"
           + "[bang][thud][ring][sub]amix=inputs=4:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]", channels=2)

    # Autocannon rounds rattling on the armour.
    render("SW_Hull_Hit_Light", [351371, 30932],
           seg(0, 0.03, 0.42, pitch(0.9) + ",afade=t=out:st=0.25:d=0.17", "ping") + ";"
           + seg(1, 0.02, 0.5, "lowpass=f=4000,volume=0.5,afade=t=out:st=0.3:d=0.2", "tick") + ";"
           + "[ping][tick]amix=inputs=2:normalize=0," + LIMIT + "[out]", peak=-4.0)

    # ---------------------------------------------------------------- smoke
    # Eight launcher tubes going off within a third of a second.
    pops = []
    for index, (delay, ratio, vol) in enumerate([(0, 0.82, 1.0), (45, 0.9, 0.9), (85, 0.78, 0.95), (130, 0.86, 0.85), (170, 0.8, 0.9), (215, 0.92, 0.8), (255, 0.84, 0.85), (300, 0.88, 0.8)]):
        pops.append("[p%d]%s,adelay=%d:all=1,volume=%s[q%d]" % (index, pitch(ratio), delay, vol, index))
    render("SW_Smoke_Salvo", [162402],
           "[0:a]aresample=48000,atrim=0:0.5,asetpts=PTS-STARTPTS,bass=g=5:f=120,lowpass=f=5000,asplit=8" + "".join("[p%d]" % i for i in range(8)) + ";"
           + ";".join(pops) + ";" + "".join("[q%d]" % i for i in range(8)) + "amix=inputs=8:normalize=0," + PUNCH + "," + LIMIT + "[out]")

    # A grenade bursting into smoke: a soft pop and a hiss that dies away.
    render("SW_Smoke_Burst", [155516, 162402],
           seg(0, 0.2, 1.7, "highpass=f=500,lowpass=f=7000,afade=t=in:d=0.04,afade=t=out:st=0.35:d=1.1", "hiss") + ";"
           + seg(1, 0, 0.4, pitch(0.6) + ",lowpass=f=1500,volume=0.6", "pop") + ";"
           + "[hiss][pop]amix=inputs=2:normalize=0," + LIMIT + "[out]", peak=-3.0)

    render("SW_Switch_Click", [275630], seg(0, 0.09, 0.5, "volume=4.0", "c") + ";[c]" + LIMIT + "[out]", peak=-6.0)

    # ---------------------------------------------------------------- enemy guns
    render("SW_Enemy_Cannon", [448002, 127845],
           seg(0, 0, 4.0, "bass=g=3:f=90,afade=t=out:st=2.0:d=1.9", "shot") + ";"
           + seg(1, 0, 2.0, "volume=0.45,afade=t=out:st=1.0:d=1.0", "low") + ";"
           + "[shot][low]amix=inputs=2:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # The AT gun: sharper and higher than a tank gun.
    render("SW_ATGun_Fire", [161343, 448002],
           seg(0, 0, 0.8, pitch(1.12) + ",treble=g=4:f=3500", "crack") + ";"
           + seg(1, 0, 3.0, pitch(1.15) + ",highpass=f=120,volume=0.6,afade=t=out:st=1.3:d=1.3", "tail") + ";"
           + "[crack][tail]amix=inputs=2:normalize=0," + PUNCH + "," + LIMIT + ",afade=t=in:d=0.002[out]")

    # One autocannon round (the game fires the burst itself, round by round).
    render("SW_Autocannon_Shot", [854186, 448002],
           seg(0, 0.03, 0.26, "bass=g=3:f=150,afade=t=out:st=0.16:d=0.07", "shot") + ";"
           + seg(1, 0.05, 0.9, "highpass=f=400,volume=0.18,afade=t=in:d=0.03,afade=t=out:st=0.3:d=0.55", "tail") + ";"
           + "[shot][tail]amix=inputs=2:normalize=0," + LIMIT + ",afade=t=in:d=0.001[out]", peak=-2.0)

    # ---------------------------------------------------------------- loops
    render("SW_Engine_Loop", [146326], loop_graph(0, 8.0, 6.0, 1.0, "bass=g=4:f=70,lowpass=f=6000") + "[out]", channels=2, mean=-19.0)
    render("SW_Tracks_Loop", [329800], loop_graph(0, 3.0, 5.0, 0.8, "highpass=f=250,treble=g=2:f=3000") + "[out]", channels=2, mean=-20.0)
    render("SW_Enemy_Engine_Loop", [186935], loop_graph(0, 0.5, 4.0, 0.6, "bass=g=3:f=90") + "[out]", mean=-20.0)
    # Turret drive: the recorded turret turn is a second long, so it is stretched into a steady whine with a motor under it.
    render("SW_Turret_Loop", [675726, 418881],
           loop_graph(0, 2.0, 3.0, 0.6, pitch(0.55) + ",lowpass=f=4500") + "[servo];"
           + "[1:a]aresample=48000,atrim=0.15:0.95,asetpts=PTS-STARTPTS,aloop=loop=4:size=38400,atrim=0:3.0,afade=t=in:d=0.2,afade=t=out:st=2.6:d=0.4,volume=1.5[turn];"
           + "[servo][turn]amix=inputs=2:normalize=0[out]", mean=-22.0)
    render("SW_Fire_Loop", [563764], loop_graph(0, 6.0, 6.0, 1.0, "highpass=f=120") + "[out]", mean=-21.0)
    render("SW_Extinguisher_Loop", [155516], loop_graph(0, 0.6, 2.4, 0.5, "highpass=f=300") + "[out]", mean=-19.0)

    # ---------------------------------------------------------------- cockpit electronics (made here, not recorded)
    box = "bandpass=f=1800:width_type=o:w=2.2,asoftclip=type=tanh"
    # Target locked: two quick blips.
    render("SW_Lock_Beep", [tone(1250, 0.06), tone(1250, 0.06)],
           "[0:a]afade=t=in:d=0.004,afade=t=out:st=0.045:d=0.015[a];[1:a]afade=t=in:d=0.004,afade=t=out:st=0.045:d=0.015,adelay=95:all=1[b];"
           "[a][b]amix=inputs=2:normalize=0," + box + "[out]", peak=-9.0)
    # Gun laid on the target: one higher, longer tone.
    render("SW_On_Target", [tone(1700, 0.16), tone(2550, 0.16)],
           "[0:a]volume=1.0[a];[1:a]volume=0.25[b];[a][b]amix=inputs=2:normalize=0,afade=t=in:d=0.004,afade=t=out:st=0.09:d=0.07," + box + "[out]", peak=-9.0)
    # Round seated and breech shut: the ready lamp.
    render("SW_Gun_Ready", [tone(880, 0.07), tone(1320, 0.11)],
           "[0:a]afade=t=in:d=0.004,afade=t=out:st=0.05:d=0.02[a];[1:a]afade=t=in:d=0.004,afade=t=out:st=0.07:d=0.04,adelay=80:all=1[b];"
           "[a][b]amix=inputs=2:normalize=0," + box + "[out]", peak=-10.0)
    # Warning horn for "an AT gun is laying on you" and "fire on board": a harsh two-tone that loops.
    render("SW_Warning_Loop", [tone(980, 0.5), tone(740, 0.5), tone(1960, 0.5)],
           "[0:a]volume='if(lt(mod(t,0.5),0.22),1,0)':eval=frame[a];[1:a]volume='if(between(mod(t,0.5),0.25,0.47),1,0)':eval=frame[b];[2:a]volume=0.12[c];"
           "[a][b][c]amix=inputs=3:normalize=0,asoftclip=type=hard:threshold=0.5,bandpass=f=1400:width_type=o:w=2.5[out]", mean=-18.0)
    # Radio: a new wave is coming.
    render("SW_Wave_Start", [noise(0.5), tone(660, 0.12), tone(990, 0.2)],
           "[0:a]bandpass=f=2200:width_type=o:w=1.5,afade=t=in:d=0.01,afade=t=out:st=0.08:d=0.1,atrim=0:0.2,volume=0.5[sq];"
           "[1:a]afade=t=in:d=0.005,afade=t=out:st=0.09:d=0.03,adelay=180:all=1[a];[2:a]afade=t=in:d=0.005,afade=t=out:st=0.12:d=0.08,adelay=320:all=1[b];"
           "[sq][a][b]amix=inputs=3:normalize=0," + box + "[out]", peak=-9.0)
    # Radio: wave cleared, three rising notes.
    render("SW_Wave_Clear", [tone(660, 0.1), tone(880, 0.1), tone(1320, 0.26)],
           "[0:a]afade=t=in:d=0.005,afade=t=out:st=0.07:d=0.03[a];[1:a]afade=t=in:d=0.005,afade=t=out:st=0.07:d=0.03,adelay=120:all=1[b];"
           "[2:a]afade=t=in:d=0.005,afade=t=out:st=0.12:d=0.14,adelay=240:all=1[c];[a][b][c]amix=inputs=3:normalize=0," + box + "[out]", peak=-9.0)
    # Tank knocked out: power dying away under static.
    render("SW_Game_Over", [("lavfi", "aevalsrc='sin(2*PI*220*(1-exp(-1.6*t))/1.6)*exp(-0.9*t)':s=48000:d=1.8"), noise(1.8, "pink")],
           "[0:a]asoftclip=type=tanh,volume=0.9[a];[1:a]bandpass=f=1500:width_type=o:w=2,volume=0.25,afade=t=in:d=0.2,afade=t=out:st=0.9:d=0.9[n];"
           "[a][n]amix=inputs=2:normalize=0,afade=t=out:st=1.2:d=0.6[out]", peak=-8.0)


if __name__ == "__main__":
    build()
    json.dump(USED, open(os.path.join(OUT, "used.json"), "w"), indent=1)
    print(len(USED), "sounds")
