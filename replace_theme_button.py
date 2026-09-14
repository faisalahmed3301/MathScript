import re
import sys

def replace_in_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    button_pattern = re.compile(r'<button id="theme-toggle".*?</button>', re.DOTALL)
    
    new_switch = '''<label class="theme-switch" title="Toggle light/dark mode">
            <input type="checkbox" id="theme-toggle">
            <span class="slider"></span>
          </label>'''
    
    new_content = button_pattern.sub(new_switch, content)
    
    with open(filepath, 'w') as f:
        f.write(new_content)

replace_in_file('viewer/graph2d.html')
replace_in_file('viewer/graph3d.html')
replace_in_file('src/graph_export.c')
