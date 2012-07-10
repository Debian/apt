#include <config.h>

#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/init.h>

#include <vector>
#include <string>
#include <list>
#include <sstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <dirent.h>

char const * const httpcodeToStr(int const httpcode) {			/*{{{*/
   switch (httpcode) {
      // Informational 1xx
      case 100: return "100 Continue";
      case 101: return "101 Switching Protocols";
      // Successful 2xx
      case 200: return "200 OK";
      case 201: return "201 Created";
      case 202: return "202 Accepted";
      case 203: return "203 Non-Authoritative Information";
      case 204: return "204 No Content";
      case 205: return "205 Reset Content";
      case 206: return "206 Partial Conent";
      // Redirections 3xx
      case 300: return "300 Multiple Choices";
      case 301: return "301 Moved Permanently";
      case 302: return "302 Found";
      case 303: return "303 See Other";
      case 304: return "304 Not Modified";
      case 305: return "304 Use Proxy";
      case 307: return "307 Temporary Redirect";
      // Client errors 4xx
      case 400: return "400 Bad Request";
      case 401: return "401 Unauthorized";
      case 402: return "402 Payment Required";
      case 403: return "403 Forbidden";
      case 404: return "404 Not Found";
      case 405: return "405 Method Not Allowed";
      case 406: return "406 Not Acceptable";
      case 407: return "Proxy Authentication Required";
      case 408: return "Request Time-out";
      case 409: return "Conflict";
      case 410: return "Gone";
      case 411: return "Length Required";
      case 412: return "Precondition Failed";
      case 413: return "Request Entity Too Large";
      case 414: return "Request-URI Too Large";
      case 415: return "Unsupported Media Type";
      case 416: return "Requested range not satisfiable";
      case 417: return "Expectation Failed";
      // Server error 5xx
      case 500: return "Internal Server Error";
      case 501: return "Not Implemented";
      case 502: return "Bad Gateway";
      case 503: return "Service Unavailable";
      case 504: return "Gateway Time-out";
      case 505: return "HTTP Version not supported";
   }
   return NULL;
}
									/*}}}*/
void addFileHeaders(std::list<std::string> &headers, FileFd &data) {	/*{{{*/
   std::ostringstream contentlength;
   contentlength << "Content-Length: " << data.FileSize();
   headers.push_back(contentlength.str());

   std::string lastmodified("Last-Modified: ");
   lastmodified.append(TimeRFC1123(data.ModificationTime()));
   headers.push_back(lastmodified);
}
									/*}}}*/
void addDataHeaders(std::list<std::string> &headers, std::string &data) {/*{{{*/
   std::ostringstream contentlength;
   contentlength << "Content-Length: " << data.size();
   headers.push_back(contentlength.str());
}
									/*}}}*/
bool sendHead(int const client, int const httpcode, std::list<std::string> &headers) { /*{{{*/
   std::string response("HTTP/1.1 ");
   response.append(httpcodeToStr(httpcode));
   headers.push_front(response);

   headers.push_back("Server: APT webserver");

   std::string date("Date: ");
   date.append(TimeRFC1123(time(NULL)));
   headers.push_back(date);

   std::clog << ">>> RESPONSE >>>" << std::endl;
   bool Success = true;
   for (std::list<std::string>::const_iterator h = headers.begin();
	Success == true && h != headers.end(); ++h) {
      Success &= FileFd::Write(client, h->c_str(), h->size());
      Success &= FileFd::Write(client, "\r\n", 2);
      std::clog << *h << std::endl;
   }
   Success &= FileFd::Write(client, "\r\n", 2);
   std::clog << "<<<<<<<<<<<<<<<<" << std::endl;
   return Success;
}
									/*}}}*/
bool sendFile(int const client, FileFd &data) {				/*{{{*/
   bool Success = true;
   char buffer[500];
   unsigned long long actual = 0;
   while ((Success &= data.Read(buffer, sizeof(buffer), &actual)) == true) {
      if (actual == 0)
	 break;
      Success &= FileFd::Write(client, buffer, actual);
   }
   Success &= FileFd::Write(client, "\r\n", 2);
   return Success;
}
									/*}}}*/
bool sendData(int const client, std::string const &data) {		/*{{{*/
   bool Success = true;
   Success &= FileFd::Write(client, data.c_str(), data.size());
   Success &= FileFd::Write(client, "\r\n", 2);
   return Success;
}
									/*}}}*/
void sendError(int const client, int const httpcode, std::string const &request, bool content) { /*{{{*/
   std::list<std::string> headers;
   std::string response("<html><head><title>");
   response.append(httpcodeToStr(httpcode)).append("</title></head>");
   response.append("<body><h1>").append(httpcodeToStr(httpcode)).append("</h1");
   response.append("This error is a result of the request: <pre>");
   response.append(request).append("</pre></body></html>");
   addDataHeaders(headers, response);
   sendHead(client, httpcode, headers);
   if (content == true)
      sendData(client, response);
}
									/*}}}*/
// sendDirectoryLisiting						/*{{{*/
int filter_hidden_files(const struct dirent *a) {
   if (a->d_name[0] == '.')
      return 0;
#ifdef _DIRENT_HAVE_D_TYPE
   // if we have the d_type check that only files and dirs will be included
   if (a->d_type != DT_UNKNOWN &&
       a->d_type != DT_REG &&
       a->d_type != DT_LNK && // this includes links to regular files
       a->d_type != DT_DIR)
      return 0;
#endif
   return 1;
}
int grouped_alpha_case_sort(const struct dirent **a, const struct dirent **b) {
#ifdef _DIRENT_HAVE_D_TYPE
   if ((*a)->d_type == DT_DIR && (*b)->d_type == DT_DIR);
   else if ((*a)->d_type == DT_DIR && (*b)->d_type == DT_REG)
      return -1;
   else if ((*b)->d_type == DT_DIR && (*a)->d_type == DT_REG)
      return 1;
   else
#endif
   {
      struct stat f_prop; //File's property
      stat((*a)->d_name, &f_prop);
      int const amode = f_prop.st_mode;
      stat((*b)->d_name, &f_prop);
      int const bmode = f_prop.st_mode;
      if (S_ISDIR(amode) && S_ISDIR(bmode));
      else if (S_ISDIR(amode))
	 return -1;
      else if (S_ISDIR(bmode))
	 return 1;
   }
   return strcasecmp((*a)->d_name, (*b)->d_name);
}
void sendDirectoryListing(int const client, std::string const &dir, std::string const &request, bool content) {
   std::list<std::string> headers;
   std::ostringstream listing;

   struct dirent **namelist;
   int const counter = scandir(dir.c_str(), &namelist, filter_hidden_files, grouped_alpha_case_sort);
   if (counter == -1) {
      sendError(client, 500, request, content);
      return;
   }

   listing << "<html><head><title>Index of " << dir << "</title>"
	   << "<style type=\"text/css\"><!-- td {padding: 0.02em 0.5em 0.02em 0.5em;}"
	   << "tr:nth-child(even){background-color:#dfdfdf;}"
	   << "h1, td:nth-child(3){text-align:center;}"
	   << "table {margin-left:auto;margin-right:auto;} --></style>"
	   << "</head>" << std::endl
	   << "<body><h1>Index of " << dir << "</h1>" << std::endl
	   << "<table><tr><th>#</th><th>Name</th><th>Size</th><th>Last-Modified</th></tr>" << std::endl;
   if (dir != ".")
      listing << "<tr><td>d</td><td><a href=\"..\">Parent Directory</a></td><td>-</td><td>-</td></tr>";
   for (int i = 0; i < counter; ++i) {
      struct stat fs;
      std::string filename(dir);
      filename.append("/").append(namelist[i]->d_name);
      stat(filename.c_str(), &fs);
      listing << "<tr><td>" << ((S_ISDIR(fs.st_mode)) ? 'd' : 'f') << "</td>"
	      << "<td><a href=\"" << namelist[i]->d_name << "\">" << namelist[i]->d_name << "</a></td>";
      if (S_ISDIR(fs.st_mode))
	 listing << "<td>-</td>";
      else
	 listing << "<td>" << SizeToStr(fs.st_size) << "B</td>";
      listing << "<td>" << TimeRFC1123(fs.st_mtime) << "</td></tr>" << std::endl;
   }
   listing << "</table></body></html>" << std::endl;

   std::string response(listing.str());
   addDataHeaders(headers, response);
   sendHead(client, 200, headers);
   if (content == true)
      sendData(client, response);
}
									/*}}}*/
int main(int const argc, const char * argv[])
{
   CommandLine::Args Args[] = {
      {0, "simulate-paywall", "aptwebserver::Simulate-Paywall",
       CommandLine::Boolean},
      {0, "port", "aptwebserver::port", CommandLine::HasArg},
      {0,0,0,0}
   };

   CommandLine CmdL(Args, _config);
   if(CmdL.Parse(argc,argv) == false) {
      _error->DumpErrors();
      exit(1);
   }

   // create socket, bind and listen to it {{{
   int sock = socket(AF_INET6, SOCK_STREAM, 0);
   if(sock < 0 ) {
      _error->Errno("aptwerbserver", "Couldn't create socket");
      _error->DumpErrors(std::cerr);
      return 1;
   }

   // get the port
   int const port = _config->FindI("aptwebserver::port", 8080);
   bool const simulate_broken_server = _config->FindB("aptwebserver::Simulate-Paywall", false);

   // ensure that we accept all connections: v4 or v6
   int const iponly = 0;
   setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &iponly, sizeof(iponly));
   // to not linger to an address
   int const enable = 1;
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

   struct sockaddr_in6 locAddr;
   memset(&locAddr, 0, sizeof(locAddr));
   locAddr.sin6_family = AF_INET6;
   locAddr.sin6_port = htons(port);
   locAddr.sin6_addr = in6addr_any;

   if (bind(sock, (struct sockaddr*) &locAddr, sizeof(locAddr)) < 0) {
      _error->Errno("aptwerbserver", "Couldn't bind");
      _error->DumpErrors(std::cerr);
      return 2;
   }

   if (simulate_broken_server) {
      std::clog << "Simulating a broken web server that return nonsense "
                   "for all querries" << std::endl;
   } else {
      std::clog << "Serving ANY file on port: " << port << std::endl;
   }

   listen(sock, 1);
   /*}}}*/

   std::vector<std::string> messages;
   int client;
   while ((client = accept(sock, NULL, NULL)) != -1) {
      std::clog << "ACCEPT client " << client
		<< " on socket " << sock << std::endl;

      while (ReadMessages(client, messages)) {
	 for (std::vector<std::string>::const_iterator m = messages.begin();
	      m != messages.end(); ++m) {
	    std::clog << ">>> REQUEST >>>>" << std::endl << *m
		      << std::endl << "<<<<<<<<<<<<<<<<" << std::endl;
	    std::list<std::string> headers;
	    bool sendContent = true;
	    if (strncmp(m->c_str(), "HEAD ", 5) == 0)
	       sendContent = false;
	    if (strncmp(m->c_str(), "GET ", 4) != 0)
	       sendError(client, 501, *m, true);

	    std::string host = LookupTag(*m, "Host", "");
	    if (host.empty() == true) {
	       // RFC 2616 §14.23 Host
	       sendError(client, 400, *m, sendContent);
	       continue;
	    }

	    size_t const filestart = m->find(' ', 5);
	    std::string filename = m->substr(5, filestart - 5);
	    if (filename.empty() == true)
	       filename = ".";

	    if (simulate_broken_server == true) {
	       std::string data("ni ni ni\n");
	       addDataHeaders(headers, data);
	       sendHead(client, 200, headers);
	       sendData(client, data);
	    }
	    else if (RealFileExists(filename) == true) {
	       FileFd data(filename, FileFd::ReadOnly);
	       std::string condition = LookupTag(*m, "If-Modified-Since", "");
	       if (condition.empty() == false) {
		  time_t cache;
		  if (RFC1123StrToTime(condition.c_str(), cache) == true &&
		      cache >= data.ModificationTime()) {
		     sendHead(client, 304, headers);
		     continue;
		  }
	       }
	       addFileHeaders(headers, data);
	       sendHead(client, 200, headers);
	       if (sendContent == true)
		  sendFile(client, data);
	    }
	    else if (DirectoryExists(filename) == true) {
	       sendDirectoryListing(client, filename, *m, sendContent);
	    }
	    else
	       sendError(client, 404, *m, false);
	 }
	 _error->DumpErrors(std::cerr);
	 messages.clear();
      }

      std::clog << "CLOSE client " << client
		<< " on socket " << sock << std::endl;
      close(client);
   }
   return 0;
}
