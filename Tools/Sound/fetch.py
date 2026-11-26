"""Collect CC0 candidates from Freesound search pages: public HQ previews plus who made them."""
import html, json, os, re, sys, time, urllib.parse, urllib.request

ROOT = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(ROOT, "raw")
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36"
CC0 = 'license:"Creative Commons 0"'

# category: (queries, min seconds, max seconds, how many to keep)
WANT = {
    "cannon": (["tank cannon shot", "artillery cannon fire", "cannon blast", "tank gun firing"], 0.8, 9.0, 8),
    "explosion": (["large explosion", "bomb explosion debris", "explosion distant boom", "tank explosion"], 1.5, 12.0, 8),
    "metal_impact": (["heavy metal impact", "bullet hit metal", "metal clang hit", "armor hit ricochet"], 0.2, 4.0, 8),
    "engine": (["tank engine idle", "diesel engine idle loop", "heavy diesel engine running", "truck engine idle"], 4.0, 90.0, 7),
    "tracks": (["tank tracks", "tank driving", "bulldozer tracks moving", "tracked vehicle"], 4.0, 90.0, 6),
    "servo": (["servo motor", "electric motor whine", "hydraulic motor loop", "turret rotation"], 1.0, 40.0, 7),
    "breech": (["heavy metal latch", "breech close", "heavy bolt action", "metal door slam heavy"], 0.2, 4.0, 8),
    "casing": (["shell casing drop", "metal pipe drop", "large shell casing"], 0.3, 5.0, 6),
    "slide": (["metal slide scrape", "loading artillery shell", "metal object slide"], 0.3, 5.0, 6),
    "fire": (["fire crackling loop", "fire burning", "campfire crackle"], 4.0, 90.0, 6),
    "extinguisher": (["fire extinguisher", "co2 spray", "compressed air hiss"], 0.8, 25.0, 7),
    "launcher": (["grenade launcher shot", "mortar launch", "smoke grenade launcher", "tube thump"], 0.2, 4.0, 7),
    "autocannon": (["autocannon", "heavy machine gun single shot", "20mm cannon"], 0.2, 6.0, 7),
    "switch": (["toggle switch click", "mechanical button press", "metal switch"], 0.05, 1.5, 6),
    "dirt": (["explosion dirt debris", "impact dirt ground", "debris falling"], 0.4, 6.0, 5),
}


def get(url, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=40) as r:
        data = r.read()
    return data if binary else data.decode("utf-8", "replace")


def search(query):
    url = "https://freesound.org/search/?" + urllib.parse.urlencode({"q": query, "f": CC0, "s": "Rating (highest first)"})
    page = get(url)
    out = []
    for tag in re.findall(r'<div[^>]*class="bw-player"[^>]*>', page):
        a = dict(re.findall(r'data-([a-z\-]+)="([^"]*)"', tag))
        if "sound-id" in a and "ogg" in a:
            out.append({
                "id": int(a["sound-id"]), "user": html.unescape(a.get("username", "")), "title": html.unescape(a.get("title", "")),
                "seconds": float(a.get("duration", "0") or 0), "rate": float(a.get("samplerate", "0") or 0),
                "downloads": int(a.get("num-downloads", "0") or 0), "preview": a["ogg"].replace("-lq.ogg", "-hq.ogg"),
            })
    return out


def main(only=None):
    meta_path = os.path.join(RAW, "candidates.json")
    meta = json.load(open(meta_path, encoding="utf-8")) if os.path.exists(meta_path) else {}
    for cat, (queries, lo, hi, keep) in WANT.items():
        if only and cat not in only:
            continue
        found = {}
        for q in queries:
            try:
                for s in search(q):
                    if lo <= s["seconds"] <= hi and s["rate"] >= 44000:
                        found.setdefault(s["id"], dict(s, query=q))
            except Exception as ex:
                print("  search failed", q, ex)
            time.sleep(0.6)
        picks = sorted(found.values(), key=lambda s: -s["downloads"])[:keep]
        os.makedirs(os.path.join(RAW, cat), exist_ok=True)
        got = []
        for s in picks:
            name = "%d_%s.ogg" % (s["id"], re.sub(r"[^A-Za-z0-9]+", "_", s["title"])[:40].strip("_"))
            path = os.path.join(RAW, cat, name)
            try:
                if not os.path.exists(path):
                    data = get(s["preview"], binary=True)
                    open(path, "wb").write(data)
                    time.sleep(0.3)
                s["file"] = name
                s["bytes"] = os.path.getsize(path)
                s["license"] = "CC0"
                s["url"] = "https://freesound.org/people/%s/sounds/%d/" % (s["user"], s["id"])
                got.append(s)
            except Exception as ex:
                print("  download failed", s["id"], ex)
        meta[cat] = got
        print("%-13s %d of %d candidates, %.1f MB" % (cat, len(got), len(found), sum(s["bytes"] for s in got) / 1e6))
        json.dump(meta, open(meta_path, "w", encoding="utf-8"), indent=1)


if __name__ == "__main__":
    main(set(sys.argv[1:]) or None)
