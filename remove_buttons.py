import re

def process(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Remove the live badge
    content = re.sub(r'<span class="live-badge".*?</span>LIVE</span>', '', content, flags=re.DOTALL)
    
    # Remove the reload button
    content = re.sub(r'<button class="text-button" id="reload">Reload graph</button>', '', content)
    
    with open(filepath, 'w') as f:
        f.write(content)

process('viewer/graph2d.html')
process('viewer/graph3d.html')
