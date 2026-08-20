import wave

def analyze(filename):
    try:
        with wave.open(filename, 'rb') as f:
            frames = f.getnframes()
            rate = f.getframerate()
            print(f"{filename}: length={frames/rate:.2f}s")
    except Exception as e:
        print(e)

analyze('original_music_dumps/jine_the_cavalry.wav')
analyze('ripped_sound/0x26_Jine_the_Cavalry.wav')
