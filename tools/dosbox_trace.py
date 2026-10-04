#!/usr/bin/env python3
"""Drive the real DOS game under the DOSBox-X debugger. Guide: docs/dos_trace.md.

  python3 tools/dosbox_trace.py setup WORK --save test-saves-ai/TURN3.SAV [--exe VR_SEED.EXE]
                                          [--patch-cc 0x46ffa]
  python3 tools/dosbox_trace.py start WORK      # Xephyr + DOSBox-X + controller, background
  python3 tools/dosbox_trace.py send WORK file.py [timeout_s]   # exec file.py in the controller
  python3 tools/dosbox_trace.py stop WORK       # kills only the PIDs this tool started

Code sent with `send` runs with `d` (a Dbg), `S` (WORK) and this module's
globals; print() output comes back. State survives between sends.
"""
import contextlib
import io
import json
import os
import re
import shutil
import signal
import struct
import subprocess
import sys
import time
import traceback

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISPLAY = ':57'

CONF = """[sdl]
output=surface
[dosbox]
memsize=16
machine=svga_s3
[cpu]
core=normal
cycles=max
[midi]
mididevice=none
[log]
logfile={work}/dosbox.log
[autoexec]
mount c {work}/C
c:
debugbox opening.exe -g
"""

KEYS = {
    'enter': (0x0d, 0x1c), 'esc': (0x1b, 0x01), 'space': (0x20, 0x39),
    'up': (0, 0x48), 'down': (0, 0x50), 'left': (0, 0x4b), 'right': (0, 0x4d),
}


class Dbg:
    """pexpect wrapper around the DOSBox-X curses debugger (TERM=dumb)."""

    def __init__(self, work):
        import pexpect
        self.pexpect = pexpect
        self.work = work
        env = dict(os.environ, DISPLAY=DISPLAY, SDL_AUDIODRIVER='dummy', TERM='dumb')
        self.p = pexpect.spawn('dosbox-x', ['-conf', os.path.join(work, 'dbx.conf'), '-nomenu'],
                               env=env, dimensions=(24, 80), encoding='latin-1',
                               timeout=30, cwd=work)
        self.p.logfile_read = open(os.path.join(work, 'debugger.out'), 'w')
        self.p.expect('TYPE HELP', timeout=60)
        self.drain(1)

    # ---- debugger primitives -------------------------------------------
    def drain(self, t=0.3):
        try:
            self.p.expect(self.pexpect.TIMEOUT, timeout=t)
        except self.pexpect.EOF:
            pass
        return self.p.before or ''

    def cmd(self, c, t=0.3):
        self.p.send(c + '\r')
        return self.drain(t)

    def run_until_break(self, t=30):
        """RUN, wait for the next breakpoint. False = still running after t s;
        the debugger then accepts no commands until something breaks."""
        self.p.send('RUN\r')
        self.p.expect(r'\(Running\)', timeout=10)
        i = self.p.expect([r'I-> ', self.pexpect.TIMEOUT], timeout=t)
        self.drain(0.03)
        return i == 0

    def step(self, n):
        for _ in range(n):
            if not self.run_until_break():
                return False
        return True

    def regs(self, *names):
        out = self.cmd('EV ' + ' '.join(names), 0.15).split('is:')[-1]
        m = re.search(r'LOG: ([0-9a-fA-F ]+)', out)
        return [int(x, 16) for x in m.group(1).split()]

    def mem(self, seg, off, n):
        fn = os.path.join(self.work, 'MEMDUMP.BIN')
        if os.path.exists(fn):
            os.remove(fn)
        self.cmd('MEMDUMPBIN %x:%x %x' % (seg, off, n), 0.2)
        for _ in range(100):
            if os.path.exists(fn) and os.path.getsize(fn) >= n:
                break
            time.sleep(0.05)
        return open(fn, 'rb').read()

    def word(self, seg, off, signed=True):
        return struct.unpack('<h' if signed else '<H', self.mem(seg, off, 2))[0]

    # ---- screen and keyboard -------------------------------------------
    def window(self):
        """(id, title) of the DOSBox-X window on the Xephyr display. The title
        carries the running program (OPENING / VICEROY)."""
        out = subprocess.run(['xwininfo', '-root', '-tree', '-display', DISPLAY],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            m = re.match(r'\s*(0x[0-9a-f]+) "(DOSBox-X[^"]*)"', line)
            if m:
                return m.group(1), m.group(2)
        return None, ''

    def shot(self, name='cur.png'):
        """Screenshot of the game window only: 640x400 = 2x the VGA screen.
        (MEMDUMPBIN of A000 reads zeros; this is the only way to see it.)"""
        wid, _ = self.window()
        path = os.path.join(self.work, name)
        subprocess.run(['import', '-window', wid or 'root', path],
                       env=dict(os.environ, DISPLAY=DISPLAY), check=False)
        return path

    def _gray(self, path):
        from PIL import Image
        return Image.open(path).convert('L')

    def _mean(self, im, box):
        from PIL import ImageStat
        return ImageStat.Stat(im.crop(box)).mean[0]

    def _highlight(self, im, x0, y0, dy, n):
        """Index of the one dark (highlighted) row, or None. Menus draw the
        selection as a dark bar: ~38-39 against 46-95 for the other rows
        (wood grain behind the load list makes the others noisy)."""
        m = [self._mean(im, (x0, y0 + k * dy - 2, x0 + 10, y0 + k * dy + 2)) for k in range(n)]
        lo = sorted(m)
        return m.index(lo[0]) if lo[0] < 42 and lo[1] - lo[0] >= 6 else None

    def _matches(self, im, ref_name, x, y, thr=5):
        """Mean abs difference of a reference crop (tools/dosbox_trace_ref/,
        window coordinates) against the same box of `im`."""
        from PIL import Image, ImageChops
        ref = Image.open(os.path.join(REPO, 'tools', 'dosbox_trace_ref', ref_name)).convert('L')
        box = im.crop((x, y, x + ref.size[0], y + ref.size[1]))
        return sum(ImageChops.difference(box, ref).getdata()) / (ref.size[0] * ref.size[1]) < thr

    def menu_row(self, im):
        """Main menu (5 rows) selection, or None when the menu is not up."""
        if not self._matches(im, 'menu_header.png', 160, 188):
            return None
        return self._highlight(im, 440, 219, 16, 5)

    def list_row(self, im):
        """Load-game list (10 slots) selection, or None."""
        if not self._matches(im, 'list_header.png', 118, 132):
            return None
        return self._highlight(im, 505, 156, 12, 10)

    def on_map(self, im):
        return self._matches(im, 'map_menubar.png', 0, 0, thr=8)

    def keys(self, *names):
        """Stuff the BIOS keyboard buffer (the game reads INT 16)."""
        data = []
        for k in names:
            data += list(KEYS[k] if isinstance(k, str) else k)
        self.cmd('SM 0040:001a 1e 00 %x 00' % (0x1e + len(data)))
        self.cmd('SM 0040:001e ' + ' '.join('%x' % b for b in data))

    def until(self, pred, every=20, tries=400):
        """Run `every` breaks at a time until pred(screen image) is truthy.
        An all-black screen gets a Space now and then: start-up sits on a
        black screen until a key arrives."""
        for i in range(tries):
            if not self.step(every):
                raise RuntimeError('game stopped polling INT 16 (debugger left running)')
            im = self._gray(self.shot())
            r = pred(im)
            if r is not None and r is not False:
                return r
            if i % 10 == 9 and im.getextrema()[1] < 20:
                self.keys('space')
        raise RuntimeError('condition never met; see %s/cur.png' % self.work)

    def select(self, row_fn, target):
        """Move a highlighted menu to `target` one Down at a time, verifying
        each step (keys sent during a redraw are dropped)."""
        cur = self.until(row_fn)
        moved = lambda im: (lambda r: r if r is not None and r != cur else None)(row_fn(im))
        while cur != target:
            for _ in range(5):
                self.keys('down')
                try:
                    cur = self.until(moved, tries=15)
                    break
                except RuntimeError:
                    continue
            else:
                raise RuntimeError('Down had no effect')

    def load_slot(self, slot=0):
        """Main menu -> LOAD Game -> COLONYnn.SAV -> map, with BPINT 16 as the
        pump. Returns the map screenshot path."""
        self.cmd('BPINT 16')
        self.select(self.menu_row, 3)
        self.keys('enter')
        self.select(self.list_row, slot)
        # Enter loads; "Loaded COLONYnn.SAV successfully." then needs another.
        for _ in range(10):
            self.keys('enter')
            try:
                self.until(self.on_map, tries=15)
                return self.shot('map.png')
            except RuntimeError:
                continue
        raise RuntimeError('map never appeared')

    # ---- overlay code --------------------------------------------------
    def trap_overlay(self, entry_off, first_byte):
        """Call at a BPINT 3 stop caused by a --patch-cc byte at overlay offset
        entry_off. Restores the byte, rewinds IP, returns the overlay CS (or
        None if this break is something else)."""
        cs, ip = self.regs('CS', 'IP')
        if ip not in (entry_off, entry_off + 1) or self.mem(cs, entry_off, 1) != b'\xcc':
            return None
        self.cmd('SM %x:%x %x' % (cs, entry_off, first_byte))
        if ip != entry_off:
            self.cmd('SR IP %x' % entry_off)
        return cs

    def unit(self, ds, idx):
        """DOS unit record (DS:3144 + 0x1c*idx). Indices shift when a unit
        is deleted mid-turn; match by xy/type, not by save index."""
        r = self.mem(ds, 0x3144 + idx * 0x1c, 0x1c)
        return {'x': r[0], 'y': r[1], 'type': r[2], 'nation': r[3] & 0xf,
                'spent': r[5], 'order': r[8], 'goto': (r[9], r[10]), 'facing': r[0xb],
                'raw': r.hex()}


# ---- process management (PID files only; never pkill -f) ------------------
def _pids(work):
    path = os.path.join(work, 'pids.json')
    return json.load(open(path)) if os.path.exists(path) else {}


def _save_pids(work, pids):
    json.dump(pids, open(os.path.join(work, 'pids.json'), 'w'))


def _alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def cmd_setup(a):
    work = os.path.abspath(a.work)
    game = os.path.join(work, 'C')
    if os.path.exists(game):
        shutil.rmtree(game)
    shutil.copytree(os.path.join(REPO, 'COLONIZE'), game)
    if a.exe != 'VICEROY.EXE':
        shutil.copy(os.path.join(game, a.exe), os.path.join(game, 'VICEROY.EXE'))
    if a.save:
        shutil.copy(a.save, os.path.join(game, 'COLONY00.SAV'))
    for off in a.patch_cc:
        with open(os.path.join(game, 'VICEROY.EXE'), 'r+b') as f:
            f.seek(int(off, 0))
            print('patch %s: %02x -> cc' % (off, f.read(1)[0]))
            f.seek(int(off, 0))
            f.write(b'\xcc')
    open(os.path.join(work, 'dbx.conf'), 'w').write(CONF.format(work=work))
    print('ready:', work)


def cmd_start(a):
    work = os.path.abspath(a.work)
    pids = _pids(work)
    if pids.get('ctl') and _alive(pids['ctl']):
        sys.exit('already running (stop first)')
    if not (pids.get('xephyr') and _alive(pids['xephyr'])):
        x = subprocess.Popen(['Xephyr', DISPLAY, '-screen', '800x500', '-title', 'dosbox-trace'],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             start_new_session=True)
        pids['xephyr'] = x.pid
        time.sleep(2)
    for f in ('ready', 'cmd.py', 'res.txt'):
        if os.path.exists(os.path.join(work, f)):
            os.remove(os.path.join(work, f))
    c = subprocess.Popen([sys.executable, os.path.abspath(__file__), '_serve', work],
                         stdout=open(os.path.join(work, 'ctl.log'), 'w'),
                         stderr=subprocess.STDOUT, start_new_session=True)
    pids['ctl'] = c.pid
    _save_pids(work, pids)
    for _ in range(120):
        if os.path.exists(os.path.join(work, 'ready')):
            print('started; debugger stopped at opening.exe entry')
            return
        time.sleep(0.5)
    sys.exit('controller did not come up; see %s/ctl.log' % work)


def cmd_serve(a):
    work = a.work
    d = Dbg(work)
    pids = _pids(work)
    pids['dosbox'] = d.p.pid
    _save_pids(work, pids)
    g = dict(globals(), d=d, S=work)
    open(os.path.join(work, 'ready'), 'w').write('1')
    cmd = os.path.join(work, 'cmd.py')
    while True:
        if not os.path.exists(cmd):
            time.sleep(0.1)
            continue
        code = open(cmd).read()
        os.remove(cmd)
        buf = io.StringIO()
        try:
            with contextlib.redirect_stdout(buf):
                exec(code, g)
        except Exception:
            buf.write(traceback.format_exc())
        tmp = os.path.join(work, 'res.tmp')
        open(tmp, 'w').write(buf.getvalue())
        os.rename(tmp, os.path.join(work, 'res.txt'))


def cmd_send(a):
    work = os.path.abspath(a.work)
    res = os.path.join(work, 'res.txt')
    if os.path.exists(res):
        os.remove(res)
    shutil.copy(a.file, os.path.join(work, 'cmd.py'))
    end = time.time() + a.timeout
    while time.time() < end:
        if os.path.exists(res):
            sys.stdout.write(open(res).read())
            return
        time.sleep(0.2)
    print('TIMEOUT: still running; result will land in %s' % res)


def cmd_stop(a):
    work = os.path.abspath(a.work)
    for name, pid in _pids(work).items():
        if _alive(pid):
            os.kill(pid, signal.SIGKILL)   # SIGTERM opens DOSBox-X's quit dialog
            print('killed', name, pid)
    _save_pids(work, {})


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('setup')
    s.add_argument('work')
    s.add_argument('--save')
    s.add_argument('--exe', default='VR_SEED.EXE')
    s.add_argument('--patch-cc', action='append', default=[], metavar='FILE_OFFSET')
    s.set_defaults(fn=cmd_setup)
    for name, fn in (('start', cmd_start), ('stop', cmd_stop), ('_serve', cmd_serve)):
        p = sub.add_parser(name)
        p.add_argument('work')
        p.set_defaults(fn=fn)
    p = sub.add_parser('send')
    p.add_argument('work')
    p.add_argument('file')
    p.add_argument('timeout', nargs='?', type=float, default=540)
    p.set_defaults(fn=cmd_send)
    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
