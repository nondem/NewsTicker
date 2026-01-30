#!/usr/bin/env python3
"""Fix extractTagValue duplication and add remaining moderate fixes"""

with open('NewsCore.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the duplicate "if (end < 0) return "";"
content = content.replace(
    """    if (end < 0) return "";
    if (end < 0 || end <= start) {""",
    """    if (end < 0 || end <= start) {"""
)

# Fix the closing of extractTagValue (add missing return)
old_func = """String extractTagValue(const String& xml, const char* openTag, const char* closeTag) {
    int start = xml.indexOf(openTag);
    if (start < 0) {
        Serial.print("[WARN] Missing tag: "), Serial.println(openTag);
        return "";
    }
    start += strlen(openTag);
    int end = xml.indexOf(closeTag, start);
    if (end < 0 || end <= start) {
        Serial.print("[WARN] Malformed tag: "), Serial.println(closeTag);
        return "";
    }

time_t parseRSSDate"""

new_func = """String extractTagValue(const String& xml, const char* openTag, const char* closeTag) {
    int start = xml.indexOf(openTag);
    if (start < 0) {
        Serial.print("[WARN] Missing tag: "), Serial.println(openTag);
        return "";
    }
    start += strlen(openTag);
    int end = xml.indexOf(closeTag, start);
    if (end < 0 || end <= start) {
        Serial.print("[WARN] Malformed tag: "), Serial.println(closeTag);
        return "";
    }
    return xml.substring(start, end);
}

time_t parseRSSDate"""

content = content.replace(old_func, new_func)

with open('NewsCore.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("✓ Fixed extractTagValue duplication and added missing return")
