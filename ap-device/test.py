import wave
import struct
import time
import socket

ESP_IP = "192.168.4.1"      # default ESP-IDF SoftAP IP
UDP_PORT = 3333
SAMPLE_RATE = 16000         # must match ESP SAMPLE_RATE
SAMPLE_WIDTH = 2            # int16 payload (bytes/sample)
DURATION_SEC = 10           # how many seconds to record
N_SAMPLES = SAMPLE_RATE * DURATION_SEC

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", UDP_PORT))
sock.settimeout(2.0)

# "Hello" so ESP learns our IP:port to stream to.
sock.sendto(b"hello", (ESP_IP, UDP_PORT))
print("Recording UDP for", DURATION_SEC, "seconds from", f"{ESP_IP}:{UDP_PORT}")

need_bytes = N_SAMPLES * SAMPLE_WIDTH
data = bytearray()
expected_seq = None

while len(data) < need_bytes:
    pkt, _ = sock.recvfrom(2048)
    if len(pkt) < 4:
        continue
    seq = struct.unpack_from("<I", pkt, 0)[0]
    payload = pkt[4:]

    if expected_seq is None:
        expected_seq = seq

    # Fill gaps with zeros (best-effort loss concealment).
    if seq > expected_seq:
        missing = seq - expected_seq
        # each packet usually carries 512 samples => 1024 bytes payload, but don't assume;
        # just pad by last payload length * missing for continuity.
        data.extend(b"\x00" * (len(payload) * missing))
    expected_seq = seq + 1

    data.extend(payload)

sock.close()

data = data[:need_bytes]
print("Captured", len(data), "bytes")

samples_int16 = struct.unpack("<" + "h" * (len(data) // 2), data)

with wave.open('output.wav', 'wb') as wf:
    wf.setnchannels(1)          # mono
    wf.setsampwidth(2)          # 2 bytes = 16‑bit
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(struct.pack("<" + "h" * len(samples_int16), *samples_int16))

print("Saved output.wav — play with any media player")
