"""Read-only endurance test for a USB service device. Logs exclude credentials/CSRF."""
import argparse
import base64
import csv
import datetime as dt
import io
import json
import os
from pathlib import Path
import statistics
import threading
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True)
    parser.add_argument('--until', help='UTC ISO end time, e.g. 2026-11-05T05:40:00Z')
    parser.add_argument('--seconds', type=float, default=600)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--port', help='Optional serial monitor; opening does not reset the ESP32')
    parser.add_argument('--version', default='0.4.1')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    summary_path = args.output / 'summary.json'
    if summary_path.exists():
        parser.error('Use a new output directory for each run')
    started = time.monotonic()
    duration = args.seconds
    if args.until:
        until = dt.datetime.fromisoformat(args.until.replace('Z', '+00:00'))
        duration = (until - dt.datetime.now(dt.timezone.utc)).total_seconds()
    if duration <= 0:
        parser.error('End time must be in the future')
    if os.name == 'nt':
        # Temporary, per-thread sleep inhibition; the display may turn off.
        # No persistent Windows power-plan settings are changed.
        import ctypes
        ctypes.windll.kernel32.SetThreadExecutionState(0x80000001)
    end = started + duration
    headers = {}
    password = os.environ.get('TDLOGGER_ADMIN_PASSWORD')
    if password:
        headers['Authorization'] = 'Basic ' + base64.b64encode(('admin:' + password).encode()).decode()
    events = []
    metrics = []
    latencies = []
    requests = failures = consecutive = 0
    baseline_csv = None
    baseline = latest = None
    last_csv = last_graph = -1e9
    stop = threading.Event()
    serial_thread = serial_port = None
    def utc():
        return dt.datetime.now(dt.timezone.utc).isoformat()
    def event(message):
        if not events or events[-1]['message'] != message:
            events.append({'at': utc(), 'message': message})
            print(message, flush=True)
    def get(path):
        nonlocal requests, failures
        requests += 1
        start = time.monotonic()
        try:
            request = urllib.request.Request(args.url.rstrip('/') + path, headers=headers)
            with urllib.request.urlopen(request, timeout=8) as response:
                data = response.read(8 * 1024 * 1024)
            latencies.append(time.monotonic() - start)
            return data
        except Exception:
            failures += 1
            raise
    def read_serial():
        buffer = bytearray()
        with (args.output / 'serial.log').open('w', encoding='utf-8') as file:
            while not stop.is_set():
                buffer.extend(serial_port.read(serial_port.in_waiting or 1))
                while b'\n' in buffer:
                    line, _, rest = buffer.partition(b'\n'); buffer[:] = rest
                    text = line.decode('utf-8', 'replace').strip()
                    if text.startswith(('sample ', 'T-Display ', 'rst:', 'Guru ', 'Brownout', 'OTA:')):
                        file.write(utc() + ' ' + text + '\n'); file.flush()
    if args.port:
        import serial
        serial_port = serial.Serial(port=None, baudrate=115200, timeout=.2)
        serial_port.dtr = False; serial_port.rts = False; serial_port.port = args.port
        serial_port.open()
        serial_thread = threading.Thread(target=read_serial, daemon=True); serial_thread.start()
    def save(running):
        elapsed = time.monotonic() - started
        report = {'running': running, 'pid': os.getpid(), 'updated_utc': utc(), 'duration_s': round(elapsed, 1),
                  'target_duration_s': round(duration, 1), 'requests': requests, 'request_failures': failures,
                  'consecutive_status_failures': consecutive, 'events': events,
                  'responses_max_s': round(max(latencies, default=0), 3),
                  'responses_median_s': round(statistics.median(latencies), 3) if latencies else None,
                  'status_samples': len(metrics), 'initial': baseline, 'latest': latest}
        if baseline and latest:
            report['new_records'] = latest['record_count'] - baseline['record_count']
            report['heap_change_bytes'] = latest['free_heap_bytes'] - baseline['free_heap_bytes']
        tmp = summary_path.with_suffix('.tmp'); tmp.write_text(json.dumps(report, indent=2), encoding='utf-8'); tmp.replace(summary_path)
    try:
        with (args.output / 'metrics.jsonl').open('w', encoding='utf-8') as output:
            while time.monotonic() < end and not (args.output / 'stop').exists():
                cycle = time.monotonic()
                try:
                    s = json.loads(get('/api/status'))
                    assert all(isinstance(s.get(key), int) for key in
                               ['session', 'record_count', 'free_heap_bytes', 'interval_s']), 'Missing runtime metrics'
                    keep = ['version', 'session', 'uptime_s', 'elapsed_s', 'epoch', 'time_known', 'sensor_ok',
                            'sensor_type', 'interval_s', 'next_sample_in_s', 'last_write_age_s', 'record_count',
                            'valid_record_count', 'used_bytes', 'free_heap_bytes', 'min_free_heap_bytes',
                            'largest_free_block_bytes', 'loop_stack_free_min_bytes', 'reset_reason', 'storage_error', 'ota_error']
                    latest = {key: s.get(key) for key in keep}
                    if baseline is None: baseline = dict(latest)
                    if s['version'] != args.version: event('Unexpected firmware version: ' + s['version'])
                    if s['session'] != baseline['session']: event('Unexpected reset/session change: ' + str(s['session']))
                    if not s['sensor_ok']: event('Sensor unavailable')
                    if s['storage_error'] or s['ota_error']: event('Device error: ' + (s['storage_error'] or s['ota_error']))
                    if s['last_write_age_s'] is None or s['last_write_age_s'] > s['interval_s'] + 5: event('Scheduled write is late')
                    if s['free_heap_bytes'] < 40000 or s['largest_free_block_bytes'] < 20000: event('Low free memory')
                    if s['loop_stack_free_min_bytes'] < 1024: event('Low loop stack reserve')
                    metrics.append(latest)
                    output.write(json.dumps({'at': utc(), **latest}) + '\n'); output.flush()
                    consecutive = 0
                    if cycle - last_graph >= 60:
                        history = json.loads(get('/api/history?hours=8'))
                        assert history['session'] == s['session'] and history['time_basis'] == 'session_elapsed'
                        assert len(history['bins']) == 480
                        for b in history['bins']:
                            if b is not None: assert len(b) == 7 and b[4] == s['session'] and 300 <= b[5] <= b[6] <= 1100
                        last_graph = cycle
                    if cycle - last_csv >= 600:
                        data = get('/api/export.csv').decode('utf-8')
                        rows = list(csv.DictReader(io.StringIO(data)))
                        if baseline_csv is None: baseline_csv = list(rows)
                        if rows[:len(baseline_csv)] != baseline_csv: event('Previously exported records changed or disappeared')
                        for row in rows:
                            if row['sensor_ok'] == '1' and row['sensor_type'] == 'BMP085/180':
                                assert row['humidity_pct'] == '' and row['humidity_available'] == '0'
                        own = [int(r['elapsed_s']) for r in rows if int(r['session']) == baseline['session']]
                        # Existing pre-test stalls are excluded from timing assessment.
                        new_gaps = [b-a for a,b in zip(own,own[1:]) if a >= baseline['elapsed_s']]
                        if any(abs(g-s['interval_s']) > 3 for g in new_gaps): event('Unexpected spacing between newly recorded samples')
                        (args.output / 'latest.csv').write_text(data, encoding='utf-8')
                        last_csv = cycle
                except Exception as error:
                    consecutive += 1
                    event('Check failed: ' + type(error).__name__ + ': ' + str(error))
                save(True)
                stop.wait(min(max(0, 15 - (time.monotonic()-cycle)), max(0, end-time.monotonic())))
        if baseline and latest and latest['record_count'] - baseline['record_count'] < max(0, int((time.monotonic()-started)/baseline['interval_s'])-2):
            event('Fewer samples recorded than expected')
        if not baseline: event('No successful status response')
        if (args.output / 'stop').exists(): event('Run stopped before deadline')
        if baseline:
            try:
                data = get('/api/export.csv').decode('utf-8')
                rows = list(csv.DictReader(io.StringIO(data)))
                if baseline_csv is not None and rows[:len(baseline_csv)] != baseline_csv:
                    event('Final CSV changed or lost previously exported records')
                (args.output / 'latest.csv').write_text(data, encoding='utf-8')
            except Exception as error:
                event('Final CSV failed: ' + type(error).__name__ + ': ' + str(error))
    finally:
        stop.set()
        if serial_thread: serial_thread.join(timeout=2)
        if serial_port: serial_port.close()
        save(False)
        if os.name == 'nt':
            ctypes.windll.kernel32.SetThreadExecutionState(0x80000000)
    print('Completed; events=%d requests=%d failures=%d' % (len(events), requests, failures), flush=True)
    return 1 if events else 0

if __name__ == '__main__':
    raise SystemExit(main())
