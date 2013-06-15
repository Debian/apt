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
#include <signal.h>

char const * const httpcodeToStr(int const httpcode)			/*{{{*/
{
   switch (httpcode)
   {
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
      case 206: return "206 Partial Content";
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
      case 407: return "407 Proxy Authentication Required";
      case 408: return "408 Request Time-out";
      case 409: return "409 Conflict";
      case 410: return "410 Gone";
      case 411: return "411 Length Required";
      case 412: return "412 Precondition Failed";
      case 413: return "413 Request Entity Too Large";
      case 414: return "414 Request-URI Too Large";
      case 415: return "415 Unsupported Media Type";
      case 416: return "416 Requested range not satisfiable";
      case 417: return "417 Expectation Failed";
      case 418: return "418 I'm a teapot";
      // Server error 5xx
      case 500: return "500 Internal Server Error";
      case 501: return "501 Not Implemented";
      case 502: return "502 Bad Gateway";
      case 503: return "503 Service Unavailable";
      case 504: return "504 Gateway Time-out";
      case 505: return "505 HTTP Version not supported";
   }
   return NULL;
}
									/*}}}*/
void addFileHeaders(std::list<std::string> &headers, FileFd &data)	/*{{{*/
{
   std::ostringstream contentlength;
   contentlength << "Content-Length: " << data.FileSize();
   headers.push_back(contentlength.str());

   std::string lastmodified("Last-Modified: ");
   lastmodified.append(TimeRFC1123(data.ModificationTime()));
   headers.push_back(lastmodified);
}
									/*}}}*/
void addDataHeaders(std::list<std::string> &headers, std::string &data)	/*{{{*/
{
   std::ostringstream contentlength;
   contentlength << "Content-Length: " << data.size();
   headers.push_back(contentlength.str());
}
									/*}}}*/
bool sendHead(int const client, int const httpcode, std::list<std::string> &headers)/*{{{*/
{
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
	Success == true && h != headers.end(); ++h)
   {
      Success &= FileFd::Write(client, h->c_str(), h->size());
      if (Success == true)
	 Success &= FileFd::Write(client, "\r\n", 2);
      std::clog << *h << std::endl;
   }
   if (Success == true)
      Success &= FileFd::Write(client, "\r\n", 2);
   std::clog << "<<<<<<<<<<<<<<<<" << std::endl;
   return Success;
}
									/*}}}*/
bool sendFile(int const client, FileFd &data)				/*{{{*/
{
   bool Success = true;
   char buffer[500];
   unsigned long long actual = 0;
   while ((Success &= data.Read(buffer, sizeof(buffer), &actual)) == true)
   {
      if (actual == 0)
	 break;
      if (Success == true)
	 Success &= FileFd::Write(client, buffer, actual);
   }
   if (Success == true)
      Success &= FileFd::Write(client, "\r\n", 2);
   return Success;
}
									/*}}}*/
bool sendData(int const client, std::string const &data)		/*{{{*/
{
   bool Success = true;
   Success &= FileFd::Write(client, data.c_str(), data.size());
   if (Success == true)
      Success &= FileFd::Write(client, "\r\n", 2);
   return Success;
}
									/*}}}*/
void sendError(int const client, int const httpcode, std::string const &request,/*{{{*/
	       bool content, std::string const &error = "")
{
   std::list<std::string> headers;
   std::string response("<html><head><title>");
   response.append(httpcodeToStr(httpcode)).append("</title></head>");
   response.append("<body><h1>").append(httpcodeToStr(httpcode)).append("</h1>");
   if (error.empty() == false)
      response.append("<p><em>Error</em>: ").append(error).append("</p>");
   response.append("This error is a result of the request: <pre>");
   response.append(request).append("</pre></body></html>");
   addDataHeaders(headers, response);
   sendHead(client, httpcode, headers);
   if (content == true)
      sendData(client, response);
}
									/*}}}*/
void sendRedirect(int const client, int const httpcode, std::string const &uri,/*{{{*/
		  std::string const &request, bool content)
{
   std::list<std::string> headers;
   std::string response("<html><head><title>");
   response.append(httpcodeToStr(httpcode)).append("</title></head>");
   response.append("<body><h1>").append(httpcodeToStr(httpcode)).append("</h1");
   response.append("<p>You should be redirected to <em>").append(uri).append("</em></p>");
   response.append("This page is a result of the request: <pre>");
   response.append(request).append("</pre></body></html>");
   addDataHeaders(headers, response);
   std::string location("Location: ");
   if (strncmp(uri.c_str(), "http://", 7) != 0)
      location.append("http://").append(LookupTag(request, "Host")).append("/").append(uri);
   else
      location.append(uri);
   headers.push_back(location);
   sendHead(client, httpcode, headers);
   if (content == true)
      sendData(client, response);
}
									/*}}}*/
int filter_hidden_files(const struct dirent *a)				/*{{{*/
{
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
									/*}}}*/
void sendDirectoryListing(int const client, std::string const &dir,	/*{{{*/
			  std::string const &request, bool content)
{
   std::list<std::string> headers;
   std::ostringstream listing;

   struct dirent **namelist;
   int const counter = scandir(dir.c_str(), &namelist, filter_hidden_files, grouped_alpha_case_sort);
   if (counter == -1)
   {
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
      if (S_ISDIR(fs.st_mode))
      {
	 listing << "<tr><td>d</td>"
		 << "<td><a href=\"" << namelist[i]->d_name << "/\">" << namelist[i]->d_name << "</a></td>"
		 << "<td>-</td>";
      }
      else
      {
	 listing << "<tr><td>f</td>"
		 << "<td><a href=\"" << namelist[i]->d_name << "\">" << namelist[i]->d_name << "</a></td>"
		 << "<td>" << SizeToStr(fs.st_size) << "B</td>";
      }
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
bool parseFirstLine(int const client, std::string const &request,	/*{{{*/
		    std::string &filename, bool &sendContent,
		    bool &closeConnection)
{
   if (strncmp(request.c_str(), "HEAD ", 5) == 0)
      sendContent = false;
   if (strncmp(request.c_str(), "GET ", 4) != 0)
   {
      sendError(client, 501, request, true);
      return false;
   }

   size_t const lineend = request.find('\n');
   size_t filestart = request.find(' ');
   for (; request[filestart] == ' '; ++filestart);
   size_t fileend = request.rfind(' ', lineend);
   if (lineend == std::string::npos || filestart == std::string::npos ||
	 fileend == std::string::npos || filestart == fileend)
   {
      sendError(client, 500, request, sendContent, "Filename can't be extracted");
      return false;
   }

   size_t httpstart = fileend;
   for (; request[httpstart] == ' '; ++httpstart);
   if (strncmp(request.c_str() + httpstart, "HTTP/1.1\r", 9) == 0)
      closeConnection = strcasecmp(LookupTag(request, "Connection", "Keep-Alive").c_str(), "Keep-Alive") != 0;
   else if (strncmp(request.c_str() + httpstart, "HTTP/1.0\r", 9) == 0)
      closeConnection = strcasecmp(LookupTag(request, "Connection", "Keep-Alive").c_str(), "close") == 0;
   else
   {
      sendError(client, 500, request, sendContent, "Not a HTTP/1.{0,1} request");
      return false;
   }

   filename = request.substr(filestart, fileend - filestart);
   if (filename.find(' ') != std::string::npos)
   {
      sendError(client, 500, request, sendContent, "Filename contains an unencoded space");
      return false;
   }
   filename = DeQuoteString(filename);

   // this is not a secure server, but at least prevent the obvious …
   if (filename.empty() == true || filename[0] != '/' ||
       strncmp(filename.c_str(), "//", 2) == 0 ||
       filename.find_first_of("\r\n\t\f\v") != std::string::npos ||
       filename.find("/../") != std::string::npos)
   {
      sendError(client, 400, request, sendContent, "Filename contains illegal character (sequence)");
      return false;
   }

   // nuke the first character which is a / as we assured above
   filename.erase(0, 1);
   if (filename.empty() == true)
      filename = ".";
   return true;
}
									/*}}}*/
int main(int const argc, const char * argv[])
{
   CommandLine::Args Args[] = {
      {0, "port", "aptwebserver::port", CommandLine::HasArg},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}
   };

   CommandLine CmdL(Args, _config);
   if(CmdL.Parse(argc,argv) == false)
   {
      _error->DumpErrors();
      exit(1);
   }

   // create socket, bind and listen to it {{{
   // ignore SIGPIPE, this can happen on write() if the socket closes connection
   signal(SIGPIPE, SIG_IGN);
   int sock = socket(AF_INET6, SOCK_STREAM, 0);
   if(sock < 0)
   {
      _error->Errno("aptwerbserver", "Couldn't create socket");
      _error->DumpErrors(std::cerr);
      return 1;
   }

   int const port = _config->FindI("aptwebserver::port", 8080);

   // ensure that we accept all connections: v4 or v6
   int const iponly = 0;
   setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &iponly, sizeof(iponly));
   // to not linger on an address
   int const enable = 1;
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

   struct sockaddr_in6 locAddr;
   memset(&locAddr, 0, sizeof(locAddr));
   locAddr.sin6_family = AF_INET6;
   locAddr.sin6_port = htons(port);
   locAddr.sin6_addr = in6addr_any;

   if (bind(sock, (struct sockaddr*) &locAddr, sizeof(locAddr)) < 0)
   {
      _error->Errno("aptwerbserver", "Couldn't bind");
      _error->DumpErrors(std::cerr);
      return 2;
   }

   std::clog << "Serving ANY file on port: " << port << std::endl;

   listen(sock, 1);
   /*}}}*/

   std::vector<std::string> messages;
   int client;
   while ((client = accept(sock, NULL, NULL)) != -1)
   {
      std::clog << "ACCEPT client " << client
		<< " on socket " << sock << std::endl;

      while (ReadMessages(client, messages))
      {
	 bool closeConnection = false;
	 for (std::vector<std::string>::const_iterator m = messages.begin();
	      m != messages.end() && closeConnection == false; ++m) {
	    std::clog << ">>> REQUEST >>>>" << std::endl << *m
		      << std::endl << "<<<<<<<<<<<<<<<<" << std::endl;
	    std::list<std::string> headers;
	    std::string filename;
	    bool sendContent = true;
	    if (parseFirstLine(client, *m, filename, sendContent, closeConnection) == false)
	       continue;

	    std::string host = LookupTag(*m, "Host", "");
	    if (host.empty() == true)
	    {
	       // RFC 2616 §14.23 requires Host
	       sendError(client, 400, *m, sendContent, "Host header is required");
	       continue;
	    }

	    // string replacements in the requested filename
	    ::Configuration::Item const *Replaces = _config->Tree("aptwebserver::redirect::replace");
	    if (Replaces != NULL)
	    {
	       std::string redirect = "/" + filename;
	       for (::Configuration::Item *I = Replaces->Child; I != NULL; I = I->Next)
		  redirect = SubstVar(redirect, I->Tag, I->Value);
	       redirect.erase(0,1);
	       if (redirect != filename)
	       {
		  sendRedirect(client, 301, redirect, *m, sendContent);
		  continue;
	       }
	    }

	    // deal with the request
	    if (RealFileExists(filename) == true)
	    {
	       FileFd data(filename, FileFd::ReadOnly);
	       std::string condition = LookupTag(*m, "If-Modified-Since", "");
	       if (condition.empty() == false)
	       {
		  time_t cache;
		  if (RFC1123StrToTime(condition.c_str(), cache) == true &&
			cache >= data.ModificationTime())
		  {
		     sendHead(client, 304, headers);
		     continue;
		  }
	       }

	       addFileHeaders(headers, data);
	       sendHead(client, 200, headers);
	       if (sendContent == true)
		  sendFile(client, data);
	    }
	    else if (DirectoryExists(filename) == true)
	    {
	       if (filename == "." || filename[filename.length()-1] == '/')
		  sendDirectoryListing(client, filename, *m, sendContent);
	       else
		  sendRedirect(client, 301, filename.append("/"), *m, sendContent);
	    }
	    else
	       sendError(client, 404, *m, sendContent);
	 }
	 _error->DumpErrors(std::cerr);
	 messages.clear();
	 if (closeConnection == true)
	    break;
      }

      std::clog << "CLOSE client " << client
		<< " on socket " << sock << std::endl;
      close(client);
   }
   return 0;
}
