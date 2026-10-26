#!/usr/bin/env python3
"""Generate the game's music and sound effects with ElevenLabs (Frank's account,
ELEVENLABS_API_KEY in ~/.claude/env).

Every take is kept in design/audio/eleven/<name>_v<N>.<ext>; nothing is
overwritten. `python3 tools/audio/eleven.py [name ...]` makes a new take of the
named assets (all when none are given). tools/audio/convert.py turns the chosen
takes into the 3DS's raw PCM.
"""
import json
import os
import sys
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'design', 'audio', 'eleven')

MOOD = ('ancient and mysterious, set in Babylon and the city of Ur around 2600 BC, '
        'like a museum at night lit by oil lamps')

# name: (kind, prompt, seconds)
ASSETS = {
    'theme': ('music', 'Instrumental title theme for a video game, ' + MOOD + '. A plucked lyre carries a '
              'haunting, memorable modal melody over a low drone, with a soft frame drum and occasional deep '
              'temple percussion. Slow, around 70 BPM, sparse and atmospheric. It builds gently in the middle '
              'and ends softly so it can loop. No vocals.', 80),
    'ingame': ('music', 'Calm background music for a quiet board game, ' + MOOD + '. Soft plucked lyre motifs, '
               'a gentle low drone and a very light frame drum, with lots of space. Minimal and unobtrusive, '
               'one steady mood with no big builds, so it can loop under gameplay for a long time. Around '
               '70 BPM. No vocals.', 90),
    'win': ('music', 'A short triumphant ancient sting: a rising plucked lyre arpeggio and a soft temple gong, '
            'warm and victorious. No vocals.', 5),
    'lose': ('music', 'A short melancholy ancient phrase: a slow descending lyre line over a low drone, '
             'quiet and resigned. No vocals.', 4),
    'rattle': ('sfx', 'Four small bone tetrahedral dice thrown onto a wooden table, a short dry rattle as they '
               'tumble and settle, close up, small quiet room', 1.0),
    'clack': ('sfx', 'A single flat shell game piece set down firmly on a wooden game board, one crisp short '
              'clack, close up, no reverb', 0.5),
    'tap': ('sfx', 'A very light soft tap of a small game piece touching wood, extremely short and quiet', 0.5),
    'thunk': ('sfx', 'A game piece knocking another game piece off a wooden board: a sharp knock and a short '
              'slide, close up', 0.8),
    'rosette': ('sfx', 'A short shimmering ancient lyre chord with a soft small bell, magical but subtle', 1.6),
    'tick': ('sfx', 'A soft short wooden click, like tapping a small wooden button', 0.5),
}


def api_key():
    key = os.environ.get('ELEVENLABS_API_KEY')
    if not key:
        for line in open(os.path.expanduser('~/.claude/env')):
            line = line.strip()
            if line.startswith('export '):
                line = line[7:]
            if line.startswith('ELEVENLABS_API_KEY='):
                key = line.split('=', 1)[1].strip().strip('"\'')
    if not key:
        sys.exit('no ELEVENLABS_API_KEY')
    return key


def post(url, body):
    req = urllib.request.Request(url, data=json.dumps(body).encode(), method='POST',
                                 headers={'xi-api-key': api_key(), 'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=600) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        sys.exit('%s: HTTP %d %s' % (url, e.code, e.read()[:400].decode(errors='replace')))


def next_path(name, ext):
    os.makedirs(OUT, exist_ok=True)
    v = 1
    while os.path.exists(os.path.join(OUT, '%s_v%d.%s' % (name, v, ext))):
        v += 1
    return os.path.join(OUT, '%s_v%d.%s' % (name, v, ext))


def make(name):
    kind, prompt, secs = ASSETS[name]
    if kind == 'music':
        data = post('https://api.elevenlabs.io/v1/music?output_format=mp3_44100_128',
                    {'prompt': prompt, 'music_length_ms': max(3000, int(secs * 1000)),
                     'model_id': 'music_v2_5', 'force_instrumental': True})
        path = next_path(name, 'mp3')
    else:
        data = post('https://api.elevenlabs.io/v1/sound-generation?output_format=mp3_44100_128',
                    {'text': prompt, 'duration_seconds': secs, 'prompt_influence': 0.5})
        path = next_path(name, 'mp3')
    open(path, 'wb').write(data)
    print('%-8s %7d bytes -> %s' % (name, len(data), os.path.relpath(path, ROOT)))


if __name__ == '__main__':
    for n in sys.argv[1:] or list(ASSETS):
        make(n)
