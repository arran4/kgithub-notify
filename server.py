import http.server, socketserver, sys
class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.send_response(401)
        self.end_headers()
with socketserver.TCPServer(('', 0), Handler) as httpd:
    print(httpd.server_address[1], flush=True)
    httpd.serve_forever()
