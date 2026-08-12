import signal
import threading
import time

import ff_pymedia as ff


def dump_output_channels(module):
    print(f"{module.getName()} output channels:")
    for channel in module.getOutputMediaChannels():
        extra_size = channel.extra_data.getActiveSize() if channel.extra_data else 0
        if channel.media_type == ff.BUFFER_TYPE_VIDEO:
            image = channel.image_para
            print(
                f"  Channel[{channel.id}] {channel.name}: video, "
                f"codec {channel.codec}, format {image.v4l2Fmt}, "
                f"size {image.width}x{image.height}, "
                f"stride {image.hstride}x{image.vstride}, extra {extra_size} bytes"
            )
        elif channel.media_type == ff.BUFFER_TYPE_AUDIO:
            sample = channel.sample_info
            print(
                f"  Channel[{channel.id}] {channel.name}: audio, "
                f"codec {channel.codec}, format {sample.fmt}, "
                f"channels {sample.channels}, sample rate {sample.sample_rate}, "
                f"samples {sample.nb_samples}, extra {extra_size} bytes"
            )
        else:
            print(
                f"  Channel[{channel.id}] {channel.name}: "
                f"type {channel.media_type}, codec {channel.codec}, "
                f"extra {extra_size} bytes"
            )


class RunMonitor:
    def __init__(self, max_frames=0, duration=0.0, report_every=100, verbose=False):
        self.max_frames = max_frames
        self.duration = duration
        self.report_every = report_every
        self.verbose = verbose
        self.stop_event = threading.Event()
        self.lock = threading.Lock()
        self.frames = 0
        self.bytes = 0
        self.eos = False
        self.abnormal = False
        self.start_time = time.monotonic()

    def reset(self):
        with self.lock:
            self.frames = 0
            self.bytes = 0
            self.eos = False
            self.abnormal = False
            self.start_time = time.monotonic()
        self.stop_event.clear()

    def output_callback(self, name, queue_size, buffer):
        with self.lock:
            self.frames += 1
            self.bytes += buffer.getActiveSize()
            frames = self.frames
            total_bytes = self.bytes
            elapsed = max(time.monotonic() - self.start_time, 1e-9)
        if self.verbose:
            print(
                f"{name}: queue {queue_size}, channel {buffer.getMediaChannelId()}, "
                f"type {buffer.getMediaBufferType()}, codec {buffer.getMediaCodec()}, "
                f"bytes {buffer.getActiveSize()}, pts {buffer.getPUstimestamp()}, "
                f"dts {buffer.getDUstimestamp()}"
            )
        if self.report_every and frames % self.report_every == 0:
            print(
                f"{name}: {frames} buffers, {total_bytes} bytes, "
                f"{elapsed:.3f}s, {frames / elapsed:.2f} buffers/s, queue {queue_size}"
            )
        if self.max_frames and frames >= self.max_frames:
            self.stop_event.set()

    def status_callback(self, name, status):
        print(f"{name} status changed to {status}")
        if status == ff.EOS:
            self.eos = True
            self.stop_event.set()
        elif status == ff.ABNORMAL:
            self.abnormal = True
            self.stop_event.set()

    def wait(self):
        while not self.stop_event.wait(0.02):
            if self.duration > 0 and time.monotonic() - self.start_time >= self.duration:
                self.stop_event.set()

    def install_signal_handlers(self):
        def stop_handler(_signum, _frame):
            self.stop_event.set()

        signal.signal(signal.SIGINT, stop_handler)
        signal.signal(signal.SIGTERM, stop_handler)

    def print_summary(self, label):
        elapsed = max(time.monotonic() - self.start_time, 1e-9)
        print(
            f"{label} summary: {self.frames} buffers, {self.bytes} bytes, "
            f"{elapsed:.3f}s, {self.frames / elapsed:.2f} buffers/s, "
            f"eos {self.eos}, abnormal {self.abnormal}"
        )
