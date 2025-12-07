import serial
import wave
import struct
import time

SERIAL_PORT = '/dev/cu.usbserial-0001'       # adjust to your port
BAUD = 1000000
SAMPLE_RATE = 8000         # match your ESP sampling rate
SAMPLE_WIDTH = 4           # 4 bytes per sample (int32)
DURATION_SEC = 10           # how many seconds to record
N_SAMPLES = SAMPLE_RATE * DURATION_SEC

ser = serial.Serial(SERIAL_PORT, BAUD, timeout=None)
print("Recording for", DURATION_SEC, "seconds")

raw = ser.read(N_SAMPLES * SAMPLE_WIDTH)
ser.close()

print("Read", len(raw), "bytes")

# convert little‑endian int32 to 16‑bit signed (for WAV)
valid_bytes = (len(raw) // 4) * 4
raw = raw[:valid_bytes]
samples_int32 = struct.unpack('<' + 'i'*(valid_bytes//4), raw)
# optional: downsample / shift to 16-bit
samples_int16 = [int(samp >> 16) for samp in samples_int32]

with wave.open('output.wav', 'wb') as wf:
    wf.setnchannels(1)          # mono
    wf.setsampwidth(2)          # 2 bytes = 16‑bit
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(struct.pack('<' + 'h' * len(samples_int16), *samples_int16))

print("Saved output.wav — play with any media player")
