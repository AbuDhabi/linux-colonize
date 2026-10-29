#!/usr/bin/env python3
"""Client for build/debug/play_driver (headless interactive play; command list in
tools/play_driver_main.c's header).

  python3 tools/play.py start DIR [--load SAVE] [--seed N] [--live-ticks]   launch the driver in the background
  python3 tools/play.py send DIR CMD [CMD ...]                run commands (or read them from stdin)
  python3 tools/play.py stop DIR

`send` appends the commands plus a sync marker to DIR/cmd.txt (the driver tails it),
waits for the marker in DIR/out.txt (the driver runs as DIR/bin/play_driver, so
opencol.log lands in DIR/bin; start rotates the previous one), prints the driver output of this batch and
turns each SHOT .ppm into a 2x .png. Run from the repo root.
"""
import os
import shutil
import subprocess
import sys
import time


def convert(ppm):
    from PIL import Image
    im = Image.open(ppm)
    im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
    png = ppm[:-4] + '.png'
    im.save(png)
    os.remove(ppm)
    return png


def wait_marker(out, marker, timeout):
    end = time.time() + timeout
    while time.time() < end:
        text = open(out).read() if os.path.exists(out) else ''
        if marker in text:
            return text
        time.sleep(0.05)
    sys.exit(f'timeout waiting for {marker!r}; tail of {out}:\n' + text[-2000:])


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    op, d = sys.argv[1], sys.argv[2]
    cmd, out, seq_file = (os.path.join(d, n) for n in ('cmd.txt', 'out.txt', 'seq.txt'))
    if op == 'start':
        os.makedirs(d, exist_ok=True)
        open(cmd, 'w').close()
        open(out, 'w').close()
        with open(seq_file, 'w') as f:
            f.write('0')
        # A private copy of the binary: opencol.log lands next to it, in DIR/bin.
        bindir = os.path.join(d, 'bin')
        os.makedirs(bindir, exist_ok=True)
        log = os.path.join(bindir, 'opencol.log')
        if os.path.exists(log):
            n = 1
            while os.path.exists(os.path.join(bindir, f'opencol.{n}.log')):
                n += 1
            os.rename(log, os.path.join(bindir, f'opencol.{n}.log'))
        shutil.copy2('build/debug/play_driver', bindir)
        args = ' '.join(sys.argv[3:])
        subprocess.Popen(
            f'exec tail -n +1 -F -s 0.05 {cmd} 2>/dev/null | exec {bindir}/play_driver {d} {args} > {out} 2>&1',
            shell=True, start_new_session=True,
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        text = wait_marker(out, '@@ ready', 60)
        print(text, end='')
        return
    if op == 'stop':
        with open(cmd, 'a') as f:
            f.write('quit\n')
        subprocess.run(['pkill', '-f', f'tail -n \\+1 -F -s 0.05 {cmd}'])
        return
    if op != 'send':
        sys.exit(__doc__)
    lines = sys.argv[3:] or [l.rstrip('\n') for l in sys.stdin]
    seq = int(open(seq_file).read() or 0) + 1
    with open(seq_file, 'w') as f:
        f.write(str(seq))
    start = len(open(out).read())
    marker = f'@@ b{seq}\n'
    with open(cmd, 'a') as f:
        f.write(''.join(l + '\n' for l in lines) + f'sync b{seq}\n')
    text = wait_marker(out, marker, 900)[start:]
    for line in text.splitlines():
        if line.startswith('SHOT ') and line.endswith('.ppm') and os.path.exists(line[5:]):
            line = 'SHOT ' + convert(line[5:])
        if line != marker.strip():
            print(line)


if __name__ == '__main__':
    main()
