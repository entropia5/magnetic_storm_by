#!/usr/bin/env python3
"""Check the rendered DOM using the same Qt WebKit engine as production."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
previews = sorted((root / 'bot_screens').glob('preview_*.html'))
assert len(previews) >= 8, 'Generate previews with GEOBOT_KEEP_TEST_IMAGES=1 ./bot_tests first'
script = '''<script>
window.onload = function() {
    var width = document.body.clientWidth;
    var nodes = document.querySelectorAll('h1,.kp-state,.weather-city,.weather-desc,.weather-slot-time,.weather-slot-desc,.weather-slot-meta,.forecast-stat,.hour-cell,.footer');
    var errors = [];
    for (var i=0; i<nodes.length; ++i) {
        var node=nodes[i], rect=node.getBoundingClientRect();
        if (rect.right > width+2 || rect.left < -2 || (node.clientWidth > 0 && node.scrollWidth > node.clientWidth+2)) {
            errors.push(node.className || node.tagName);
        }
    }
    console.log('GEOBOT_LAYOUT=' + JSON.stringify({width: width, errors: errors}));
};
</script>'''
with tempfile.TemporaryDirectory(prefix='belarus-layout-') as working:
    for preview in previews:
        html = preview.read_text().replace('</body>', script + '</body>')
        source = Path(working, 'layout.html')
        source.write_text(html)
        width = '1800' if 'preview_morning_' in preview.name else '1280'
        rendered = subprocess.run(['/usr/bin/timeout', '25s', '/usr/bin/wkhtmltoimage', '--debug-javascript', '--javascript-delay', '100', '--width', width, str(source), str(Path(working, 'layout.jpg'))], capture_output=True, text=True, check=True)
        match = re.search(r'GEOBOT_LAYOUT=(\{[^\n]*\})', rendered.stderr)
        assert match, (preview.name, rendered.stderr)
        report = json.loads(match.group(1))
        assert not report['errors'], (preview.name, report)
        assert report['width'] == int(width), (preview.name, report)
print(f'Qt WebKit layout: {len(previews)} cards passed, no horizontal overflow')
