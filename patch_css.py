import re

with open('viewer/graph3d.css', 'r') as f:
    css = f.read()

# Remove .theme-toggle-btn styles
css = re.sub(r'\.theme-toggle-btn\b.*?\}', '', css, flags=re.DOTALL)
css = re.sub(r'\[data-theme="[^"]*"\] \.theme-toggle-btn[^{]*\{[^}]*\}', '', css, flags=re.DOTALL)

theme_switch_css = '''
/* Theme Toggle Slider Switch */
.theme-switch {
  position: relative;
  display: inline-block;
  width: 44px;
  height: 24px;
  margin-left: 10px;
}

.theme-switch input { 
  opacity: 0;
  width: 0;
  height: 0;
}

.theme-switch .slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #3b82f6;
  transition: .3s;
  border-radius: 24px;
}

.theme-switch .slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  transition: .3s;
  border-radius: 50%;
}

[data-theme="light"] .theme-switch .slider {
  background-color: #ff7e67;
}

[data-theme="light"] .theme-switch .slider:before {
  transform: translateX(20px);
}
'''

css += theme_switch_css

with open('viewer/graph3d.css', 'w') as f:
    f.write(css)
