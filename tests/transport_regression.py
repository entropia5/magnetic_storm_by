#!/usr/bin/env python3
"""Exercise C++ transport against a local Telegram stub, in an isolated directory."""
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

root = Path(__file__).resolve().parents[1]
requests = []
class TelegramStub(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', '0')))
        scenario, method = self.path.strip('/').split('/')
        requests.append((scenario, method))
        status = 200
        response = {'ok': True, 'result': {'message_id': 100}}
        if scenario == 'force-fail' and method == 'sendPhoto':
            status, response = 429, {'ok': False, 'description': 'Too Many Requests'}
        elif scenario == 'edit-retry' and method == 'editMessageMedia':
            status, response = 500, {'ok': False, 'description': 'Service unavailable'}
        elif scenario == 'edit-missing' and method == 'editMessageMedia':
            status, response = 400, {'ok': False, 'description': 'Bad Request: message to edit not found'}
        elif scenario == 'text-fallback' and method == 'editMessageText':
            status, response = 400, {'ok': False, 'description': 'Bad Request: there is no text in the message to edit'}
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode())
    def log_message(self, *args):
        pass

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), TelegramStub)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='belarus-transport-') as working:
        Path(working, 'templates').symlink_to(root / 'templates', target_is_directory=True)
        environment = dict(os.environ, GEOBOT_TRANSPORT_TEST_URL=f'http://127.0.0.1:{server.server_port}')
        subprocess.run([str(root / 'bot_tests')], cwd=working, env=environment, check=True)
    expected = {
        'force-fail': ['sendPhoto'],
        'force-ok': ['sendPhoto', 'deleteMessage'],
        'edit-missing': ['editMessageMedia', 'sendPhoto', 'deleteMessage'],
        'edit-retry': ['editMessageMedia'],
        'text-fallback': ['editMessageText', 'sendMessage', 'deleteMessage'],
    }
    for scenario, wanted in expected.items():
        actual = [method for case, method in requests if case == scenario]
        assert actual == wanted, (scenario, actual, wanted)
    print('Transport regression: 5 scenarios passed; replacement precedes deletion')
finally:
    server.shutdown()
    server.server_close()
