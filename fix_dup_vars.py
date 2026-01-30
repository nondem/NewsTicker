#!/usr/bin/env python3
"""Remove duplicate start/end variable declarations"""

with open('NewsCore.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the duplicate declarations
old_cleanup = """  // 3-PHASE CLEANUP
  int start = batchIndex * 6; 
  int end = start + 6;
  
  megaPool.erase"""

new_cleanup = """  // 3-PHASE CLEANUP
  megaPool.erase"""

content = content.replace(old_cleanup, new_cleanup)

with open('NewsCore.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("✓ Removed duplicate start/end variable declarations")
