import http.server, socketserver, json
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/user':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'login': 'testuser'}).encode())
        else:
            self.send_response(404)
            self.end_headers()

with socketserver.TCPServer(('', 0), Handler) as httpd:
    print(httpd.server_address[1], flush=True)
    httpd.serve_forever()
