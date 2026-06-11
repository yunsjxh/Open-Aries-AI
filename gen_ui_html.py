import codecs

with open('界面.html', 'r', encoding='utf-8') as f:
    content = f.read()

# Escape for C++ string literal: backslash first, then quote, then newline
escaped = content.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '')

header = '#pragma once\n// Auto-generated from 界面.html — do not edit directly.\nstatic const char UI_HTML[] = "' + escaped + '" ;\n'

with open('ui_html.h', 'w', encoding='utf-8') as f:
    f.write(header)

print('ui_html.h regenerated, length:', len(escaped))
