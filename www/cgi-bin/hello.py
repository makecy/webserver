#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html")
print("")
print("<html><body>")
print("<h1>Hello from Python CGI!</h1>")
print(f"<p>Request Method: {os.environ.get('REQUEST_METHOD', 'Unknown')}</p>")
print(f"<p>Request URI: {os.environ.get('REQUEST_URI', 'Unknown')}</p>")
print(f"<p>Server Software: {os.environ.get('SERVER_SOFTWARE', 'Unknown')}</p>")
print(f"<p>Query String: {os.environ.get('QUERY_STRING', 'None')}</p>")

if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print(f"<p>POST Data: {post_data}</p>")

print("</body></html>")
