#!/usr/bin/env python3
"""Add runtime statistics and source-level error tracking"""

with open('NewsCore.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

output = []
i = 0

while i < len(lines):
    line = lines[i]
    
    # Add per-source statistics after global declarations
    if '// --- GLOBAL STORAGE ---' in line:
        output.append(line)
        i += 1
        output.append(lines[i])  # std::vector<Story> megaPool;
        i += 1
        output.append(lines[i])  # std::vector<int> playbackQueue;
        i += 1
        output.append(lines[i])  # int failureCount = 0;
        i += 1
        output.append(lines[i])  # bool lastSyncFailed = false;
        i += 1
        # Add new stats tracking
        output.append('\n')
        output.append('// --- SOURCE-LEVEL STATISTICS ---\n')
        output.append('struct SourceStats {\n')
        output.append('  int fetched = 0;      // Total items fetched\n')
        output.append('  int accepted = 0;    // Items added to pool\n')
        output.append('  int duplicates = 0;  // Duplicate rejections\n')
        output.append('  int parseErrors = 0; // Parse/validation failures\n')
        output.append('  int consecutiveFails = 0; // Consecutive failures\n')
        output.append('  unsigned long lastFetchMs = 0; // Last fetch timestamp\n')
        output.append('};\n')
        output.append('SourceStats sourceStats[30] = {};\n')
        output.append('\n')
        continue
    
    # Enhance refreshNewsData to reset stats at start of batch
    if 'void refreshNewsData(int batchIndex) {' in line:
        output.append(line)
        i += 1
        while i < len(lines) and 'Serial.print("Fetch Start. Batch:' not in lines[i]:
            output.append(lines[i])
            i += 1
        output.append(lines[i])
        i += 1
        output.append('  \n')
        output.append('  // Reset stats for this batch\n')
        output.append('  int start = batchIndex * 6;\n')
        output.append('  int end = start + 6;\n')
        output.append('  for(int i = start; i < end; i++) {\n')
        output.append('      sourceStats[i].fetched = 0;\n')
        output.append('      sourceStats[i].accepted = 0;\n')
        output.append('      sourceStats[i].duplicates = 0;\n')
        output.append('      sourceStats[i].parseErrors = 0;\n')
        output.append('  }\n')
        output.append('  \n')
        continue
    
    output.append(line)
    i += 1

with open('NewsCore.cpp', 'w', encoding='utf-8') as f:
    f.writelines(output)

print("✓ Added per-source statistics tracking structure")
print("✓ Added batch stats reset at start of refreshNewsData()")
