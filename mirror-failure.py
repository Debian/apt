# File: cgihttpserver-example-1.py

import CGIHTTPServer
import BaseHTTPServer

class Handler(CGIHTTPServer.CGIHTTPRequestHandler):
    #cgi_directories = ["/cgi"]
    def do_POST(self):
	print "do_POST"
        #print self.command
        #print self.path
        #print self.headers
        print self.client_address
        data = self.rfile.read(int(self.headers["content-length"]))
        print data
        self.wfile.write("200 Ok\n");

PORT = 8000

httpd = BaseHTTPServer.HTTPServer(("", PORT), Handler)
print "serving at port", PORT
httpd.serve_forever()

