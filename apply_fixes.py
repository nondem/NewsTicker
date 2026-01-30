#!/usr/bin/env python3
"""Apply moderate/performance fixes to NewsCore.cpp"""

with open('NewsCore.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find and fix extractTagValue (add error checking)
output = []
i = 0
while i < len(lines):
    line = lines[i]
    
    # Look for extractTagValue function
    if 'String extractTagValue(const String& xml' in line:
        # Replace the entire function
        output.append(line)
        i += 1
        # Skip until we find the closing brace
        indent_start = lines[i]
        output.append('    int start = xml.indexOf(openTag);\n')
        i += 1  # Skip old "if (start < 0)"
        output.append('    if (start < 0) {\n')
        output.append('        Serial.print("[WARN] Missing tag: "), Serial.println(openTag);\n')
        output.append('        return "";\n')
        output.append('    }\n')
        i += 1  # Skip old return
        output.append(lines[i])
        i += 1
        output.append(lines[i])
        i += 1
        output.append(lines[i])
        i += 1
        # Now fix the 'if (end < 0)'
        output.append('    if (end < 0 || end <= start) {\n')
        output.append('        Serial.print("[WARN] Malformed tag: "), Serial.println(closeTag);\n')
        output.append('        return "";\n')
        output.append('    }\n')
        i += 1  # Skip old if
        i += 1  # Skip old return
        output.append(lines[i])
        output.append(lines[i+1])
        i += 2
        continue
    
    # Fix 2: Increase WordPress buffer
    if 'int itemMaxLen = isWp ? 3000 : 1500;' in line:
        output.append(line.replace('3000', '4000'))
        i += 1
        continue
    
    # Fix 3: Add per-source error tracking declaration  
    if 'void fetchAndPool(int sourceIdx) {' in line:
        output.append(line)
        i += 1
        output.append(lines[i])  # esp_task_wdt_reset()
        i += 1
        output.append(lines[i])  # Serial.print
        i += 1
        output.append(lines[i])  # Serial.println
        i += 1
        output.append('  \n')
        output.append('  // Per-source error tracking for debugging\n')
        output.append('  static int sourceErrors[30] = {0};\n')
        continue
    
    output.append(line)
    i += 1

with open('NewsCore.cpp', 'w', encoding='utf-8') as f:
    f.writelines(output)

print("✓ Applied: extractTagValue hardening")
print("✓ Applied: WordPress buffer increased to 4000")
print("✓ Applied: Per-source error tracking declaration added")
