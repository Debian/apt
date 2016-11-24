#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include "teestream.h"

#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <string>
#include <thread>
#include <vector>

static std::string httpcodeToStr(int const httpcode)			/*{{{*/
{
   switch (httpcode)
   {
      // Informational 1xx
      case 100: return _config->Find("aptwebserver::httpcode::100", "100 Continue");
      case 101: return _config->Find("aptwebserver::httpcode::101", "101 Switching Protocols");
      // Successful 2xx
      case 200: return _config->Find("aptwebserver::httpcode::200", "200 OK");
      case 201: return _config->Find("aptwebserver::httpcode::201", "201 Created");
      case 202: return _config->Find("aptwebserver::httpcode::202", "202 Accepted");
      case 203: return _config->Find("aptwebserver::httpcode::203", "203 Non-Authoritative Information");
      case 204: return _config->Find("aptwebserver::httpcode::204", "204 No Content");
      case 205: return _config->Find("aptwebserver::httpcode::205", "205 Reset Content");
      case 206: return _config->Find("aptwebserver::httpcode::206", "206 Partial Content");
      // Redirections 3xx
      case 300: return _config->Find("aptwebserver::httpcode::300", "300 Multiple Choices");
      case 301: return _config->Find("aptwebserver::httpcode::301", "301 Moved Permanently");
      case 302: return _config->Find("aptwebserver::httpcode::302", "302 Found");
      case 303: return _config->Find("aptwebserver::httpcode::303", "303 See Other");
      case 304: return _config->Find("aptwebserver::httpcode::304", "304 Not Modified");
      case 305: return _config->Find("aptwebserver::httpcode::305", "305 Use Proxy");
      case 307: return _config->Find("aptwebserver::httpcode::307", "307 Temporary Redirect");
      // Client errors 4xx
      case 400: return _config->Find("aptwebserver::httpcode::400", "400 Bad Request");
      case 401: return _config->Find("aptwebserver::httpcode::401", "401 Unauthorized");
      case 402: return _config->Find("aptwebserver::httpcode::402", "402 Payment Required");
      case 403: return _config->Find("aptwebserver::httpcode::403", "403 Forbidden");
      case 404: return _config->Find("aptwebserver::httpcode::404", "404 Not Found");
      case 405: return _config->Find("aptwebserver::httpcode::405", "405 Method Not Allowed");
      case 406: return _config->Find("aptwebserver::httpcode::406", "406 Not Acceptable");
      case 407: return _config->Find("aptwebserver::httpcode::407", "407 Proxy Authentication Required");
      case 408: return _config->Find("aptwebserver::httpcode::408", "408 Request Time-out");
      case 409: return _config->Find("aptwebserver::httpcode::409", "409 Conflict");
      case 410: return _config->Find("aptwebserver::httpcode::410", "410 Gone");
      case 411: return _config->Find("aptwebserver::httpcode::411", "411 Length Required");
      case 412: return _config->Find("aptwebserver::httpcode::412", "412 Precondition Failed");
      case 413: return _config->Find("aptwebserver::httpcode::413", "413 Request Entity Too Large");
      case 414: return _config->Find("aptwebserver::httpcode::414", "414 Request-URI Too Large");
      case 415: return _config->Find("aptwebserver::httpcode::415", "415 Unsupported Media Type");
      case 416: return _config->Find("aptwebserver::httpcode::416", "416 Requested range not satisfiable");
      case 417: return _config->Find("aptwebserver::httpcode::417", "417 Expectation Failed");
      case 418: return _config->Find("aptwebserver::httpcode::418", "418 I'm a teapot");
      // Server error 5xx
      case 500: return _config->Find("aptwebserver::httpcode::500", "500 Internal Server Error");
      case 501: return _config->Find("aptwebserver::httpcode::501", "501 Not Implemented");
      case 502: return _config->Find("aptwebserver::httpcode::502", "502 Bad Gateway");
      case 503: return _config->Find("aptwebserver::httpcode::503", "503 Service Unavailable");
      case 504: return _config->Find("aptwebserver::httpcode::504", "504 Gateway Time-out");
      case 505: return _config->Find("aptwebserver::httpcode::505", "505 HTTP Version not supported");
   }
   return "";
}
									/*}}}*/
static bool chunkedTransferEncoding(std::list<std::string> const &headers) {
   if (std::find(headers.begin(), headers.end(), "Transfer-Encoding: chunked") != headers.end())
      return true;
   if (_config->FindB("aptwebserver::chunked-transfer-encoding", false) == true)
      return true;
   return false;
}
static void addFileHeaders(std::list<std::string> &headers, FileFd &data)/*{{{*/
{
   if (chunkedTransferEncoding(headers) == false)
   {
      std::ostringstream contentlength;
      contentlength << "Content-Length: " << data.FileSize();
      headers.push_back(contentlength.str());
   }
   if (_config->FindB("aptwebserver::support::last-modified", true) == true)
   {
      std::string lastmodified("Last-Modified: ");
      lastmodified.append(TimeRFC1123(data.ModificationTime(), false));
      headers.push_back(lastmodified);
   }
}
									/*}}}*/
static void addDataHeaders(std::list<std::string> &headers, std::string &data)/*{{{*/
{
   if (chunkedTransferEncoding(headers) == false)
   {
      std::ostringstream contentlength;
      contentlength << "Content-Length: " << data.size();
      headers.push_back(contentlength.str());
   }
}
									/*}}}*/
static bool sendHead(std::ostream &log, int const client, int const httpcode, std::list<std::string> &headers)/*{{{*/
{
   std::string response("HTTP/1.1 ");
   response.append(httpcodeToStr(httpcode));
   headers.push_front(response);
   _config->Set("APTWebserver::Last-Status-Code", httpcode);

   std::stringstream buffer;
   auto const empties = _config->FindVector("aptwebserver::empty-response-header");
   for (auto && e: empties)
      buffer << e << ":" << std::endl;
   _config->Dump(buffer, "aptwebserver::response-header", "%t: %v%n", false);
   std::vector<std::string> addheaders = VectorizeString(buffer.str(), '\n');
   for (std::vector<std::string>::const_iterator h = addheaders.begin(); h != addheaders.end(); ++h)
      headers.push_back(*h);

   std::string date("Date: ");
   date.append(TimeRFC1123(time(NULL), false));
   headers.push_back(date);

   if (chunkedTransferEncoding(headers) == true)
      headers.push_back("Transfer-Encoding: chunked");

   log << ">>> RESPONSE to " << client << " >>>" << std::endl;
   bool Success = true;
   for (std::list<std::string>::const_iterator h = headers.begin();
	Success == true && h != headers.end(); ++h)
   {
      Success &= FileFd::Write(client, h->c_str(), h->size());
      if (Success == true)
	 Success &= FileFd::Write(client, "\r\n", 2);
      log << *h << std::endl;
   }
   if (Success == true)
      Success &= FileFd::Write(client, "\r\n", 2);
   log << "<<<<<<<<<<<<<<<<" << std::endl;
   return Success;
}
									/*}}}*/
static bool sendFile(int const client, std::list<std::string> const &headers, FileFd &data)/*{{{*/
{
   bool Success = true;
   bool const chunked = chunkedTransferEncoding(headers);
   char buffer[500];
   unsigned long long actual = 0;
   while ((Success &= data.Read(buffer, sizeof(buffer), &actual)) == true)
   {
      if (actual == 0)
	 break;

      if (chunked == true)
      {
	 std::string size;
	 strprintf(size, "%llX\r\n", actual);
	 Success &= FileFd::Write(client, size.c_str(), size.size());
	 Success &= FileFd::Write(client, buffer, actual);
	 Success &= FileFd::Write(client, "\r\n", strlen("\r\n"));
      }
      else
	 Success &= FileFd::Write(client, buffer, actual);
   }
   if (chunked == true)
   {
      char const * const finish = "0\r\n\r\n";
      Success &= FileFd::Write(client, finish, strlen(finish));
   }
   if (Success == false)
      std::cerr << "SENDFILE:" << (chunked ? " CHUNKED" : "") << " READ/WRITE ERROR to " << client << std::endl;
   return Success;
}
									/*}}}*/
static bool sendData(int const client, std::list<std::string> const &headers, std::string const &data)/*{{{*/
{
   if (chunkedTransferEncoding(headers) == true)
   {
      unsigned long long const ullsize = data.length();
      std::string size;
      strprintf(size, "%llX\r\n", ullsize);
      char const * const finish = "\r\n0\r\n\r\n";
      if (FileFd::Write(client, size.c_str(), size.length()) == false ||
	    FileFd::Write(client, data.c_str(), ullsize) == false ||
	    FileFd::Write(client, finish, strlen(finish)) == false)
      {
	 std::cerr << "SENDDATA: CHUNK WRITE ERROR to " << client << std::endl;
	 return false;
      }
   }
   else if (FileFd::Write(client, data.c_str(), data.size()) == false)
   {
      std::cerr << "SENDDATA: WRITE ERROR to " << client << std::endl;
      return false;
   }
   return true;
}
									/*}}}*/
static void sendError(std::ostream &log, int const client, int const httpcode, std::string const &request,/*{{{*/
	       bool const content, std::string const &error, std::list<std::string> &headers)
{
   std::string response("<!doctype html><html><head><title>");
   response.append(httpcodeToStr(httpcode)).append("</title><meta charset=\"utf-8\" /></head>");
   response.append("<body><h1>").append(httpcodeToStr(httpcode)).append("</h1>");
   if (httpcode != 200)
      response.append("<p><em>Error</em>: ");
   else
      response.append("<p><em>Success</em>: ");
   if (error.empty() == false)
      response.append(error);
   else
      response.append(httpcodeToStr(httpcode));
   if (httpcode != 200)
      response.append("</p>This error is a result of the request: <pre>");
   else
      response.append("The successfully executed operation was requested by: <pre>");
   response.append(request).append("</pre></body></html>");
   if (httpcode != 200)
   {
      if (_config->FindB("aptwebserver::closeOnError", false) == true)
	 headers.push_back("Connection: close");
   }
   addDataHeaders(headers, response);
   sendHead(log, client, httpcode, headers);
   if (content == true)
      sendData(client, headers, response);
}
static void sendSuccess(std::ostream &log, int const client, std::string const &request,
	       bool const content, std::string const &error, std::list<std::string> &headers)
{
   sendError(log, client, 200, request, content, error, headers);
}
									/*}}}*/
static void sendRedirect(std::ostream &log, int const client, int const httpcode,/*{{{*/
		  std::string const &uri, std::string const &request, bool content)
{
   std::list<std::string> headers;
   std::string response("<!doctype html><html><head><title>");
   response.append(httpcodeToStr(httpcode)).append("</title><meta charset=\"utf-8\" /></head>");
   response.append("<body><h1>").append(httpcodeToStr(httpcode)).append("</h1");
   response.append("<p>You should be redirected to <em>").append(uri).append("</em></p>");
   response.append("This page is a result of the request: <pre>");
   response.append(request).append("</pre></body></html>");
   addDataHeaders(headers, response);
   std::string location("Location: ");
   if (strncmp(uri.c_str(), "http://", 7) != 0 && strncmp(uri.c_str(), "https://", 8) != 0)
   {
      std::string const host = LookupTag(request, "Host");
      unsigned int const httpsport = _config->FindI("aptwebserver::port::https", 4433);
      std::string hosthttpsport;
      strprintf(hosthttpsport, ":%u", httpsport);
      if (host.find(hosthttpsport) != std::string::npos)
	 location.append("https://");
      else
	 location.append("http://");
      location.append(host).append("/");
      if (strncmp("/home/", uri.c_str(), strlen("/home/")) == 0 && uri.find("/public_html/") != std::string::npos)
      {
	 std::string homeuri = SubstVar(uri, "/home/", "~");
	 homeuri = SubstVar(homeuri, "/public_html/", "/");
	 location.append(homeuri);
      }
      else
	 location.append(uri);
   }
   else
      location.append(uri);
   headers.push_back(location);
   sendHead(log, client, httpcode, headers);
   if (content == true)
      sendData(client, headers, response);
}
									/*}}}*/
static int filter_hidden_files(const struct dirent *a)			/*{{{*/
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
static int grouped_alpha_case_sort(const struct dirent **a, const struct dirent **b) {
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
static void sendDirectoryListing(std::ostream &log, int const client, std::string const &dir,/*{{{*/
			  std::string const &request, bool content, std::list<std::string> &headers)
{
   struct dirent **namelist;
   int const counter = scandir(dir.c_str(), &namelist, filter_hidden_files, grouped_alpha_case_sort);
   if (counter == -1)
   {
      sendError(log, client, 500, request, content, "scandir failed", headers);
      return;
   }

   std::ostringstream listing;
   listing << "<!doctype html><html><head><title>Index of " << dir << "</title><meta charset=\"utf-8\" />"
	   << "<style type=\"text/css\"><!-- td {padding: 0.02em 0.5em 0.02em 0.5em;}"
	   << "tr:nth-child(even){background-color:#dfdfdf;}"
	   << "h1, td:nth-child(3){text-align:center;}"
	   << "table {margin-left:auto;margin-right:auto;} --></style>"
	   << "</head>" << std::endl
	   << "<body><h1>Index of " << dir << "</h1>" << std::endl
	   << "<table><tr><th>#</th><th>Name</th><th>Size</th><th>Last-Modified</th></tr>" << std::endl;
   if (dir != "./")
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
      listing << "<td>" << TimeRFC1123(fs.st_mtime, true) << "</td></tr>" << std::endl;
   }
   listing << "</table></body></html>" << std::endl;

   std::string response(listing.str());
   addDataHeaders(headers, response);
   sendHead(log, client, 200, headers);
   if (content == true)
      sendData(client, headers, response);
}
									/*}}}*/
static bool parseFirstLine(std::ostream &log, int const client, std::string const &request,/*{{{*/
		    std::string &filename, std::string &params, bool &sendContent,
		    bool &closeConnection, std::list<std::string> &headers)
{
   if (strncmp(request.c_str(), "HEAD ", 5) == 0)
      sendContent = false;
   if (strncmp(request.c_str(), "GET ", 4) != 0)
   {
      sendError(log, client, 501, request, true, "", headers);
      return false;
   }

   size_t const lineend = request.find('\n');
   size_t filestart = request.find(' ');
   for (; request[filestart] == ' '; ++filestart);
   size_t fileend = request.rfind(' ', lineend);
   if (lineend == std::string::npos || filestart == std::string::npos ||
	 fileend == std::string::npos || filestart == fileend)
   {
      sendError(log, client, 500, request, sendContent, "Filename can't be extracted", headers);
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
      sendError(log, client, 500, request, sendContent, "Not a HTTP/1.{0,1} request", headers);
      return false;
   }

   filename = request.substr(filestart, fileend - filestart);
   if (filename.find(' ') != std::string::npos)
   {
      sendError(log, client, 500, request, sendContent, "Filename contains an unencoded space", headers);
      return false;
   }

   std::string host = LookupTag(request, "Host", "");
   if (host.empty() == true)
   {
      // RFC 2616 §14.23 requires Host
      sendError(log, client, 400, request, sendContent, "Host header is required", headers);
      return false;
   }
   host = "http://" + host;

   // Proxies require absolute uris, so this is a simple proxy-fake option
   std::string const absolute = _config->Find("aptwebserver::request::absolute", "uri,path");
   if (strncmp(host.c_str(), filename.c_str(), host.length()) == 0 && APT::String::Startswith(filename, "/_config/") == false)
   {
      if (absolute.find("uri") == std::string::npos)
      {
	 sendError(log, client, 400, request, sendContent, "Request is absoluteURI, but configured to not accept that", headers);
	 return false;
      }

      // strip the host from the request to make it an absolute path
      filename.erase(0, host.length());

      std::string const authConf = _config->Find("aptwebserver::proxy-authorization", "");
      std::string auth = LookupTag(request, "Proxy-Authorization", "");
      if (authConf.empty() != auth.empty())
      {
	 if (auth.empty())
	    sendError(log, client, 407, request, sendContent, "Proxy requires authentication", headers);
	 else
	    sendError(log, client, 407, request, sendContent, "Client wants to authenticate to proxy, but proxy doesn't need it", headers);
	return false;
      }
      if (authConf.empty() == false)
      {
	 char const * const basic = "Basic ";
	 if (strncmp(auth.c_str(), basic, strlen(basic)) == 0)
	 {
	    auth.erase(0, strlen(basic));
	    if (auth != authConf)
	    {
	       sendError(log, client, 407, request, sendContent, "Proxy-Authentication doesn't match", headers);
	       return false;
	    }
	 }
	 else
	 {
	    std::list<std::string> headers;
	    headers.push_back("Proxy-Authenticate: Basic");
	    sendError(log, client, 407, request, sendContent, "Unsupported Proxy-Authentication Scheme", headers);
	    return false;
	 }
      }
   }
   else if (absolute.find("path") == std::string::npos && APT::String::Startswith(filename, "/_config/") == false)
   {
      sendError(log, client, 400, request, sendContent, "Request is absolutePath, but configured to not accept that", headers);
      return false;
   }

   if (APT::String::Startswith(filename, "/_config/") == false)
   {
      std::string const authConf = _config->Find("aptwebserver::authorization", "");
      std::string auth = LookupTag(request, "Authorization", "");
      if (authConf.empty() != auth.empty())
      {
	 if (auth.empty())
	    sendError(log, client, 401, request, sendContent, "Server requires authentication", headers);
	 else
	    sendError(log, client, 401, request, sendContent, "Client wants to authenticate to server, but server doesn't need it", headers);
	 return false;
      }
      if (authConf.empty() == false)
      {
	 char const * const basic = "Basic ";
	 if (strncmp(auth.c_str(), basic, strlen(basic)) == 0)
	 {
	    auth.erase(0, strlen(basic));
	    if (auth != authConf)
	    {
	       sendError(log, client, 401, request, sendContent, "Authentication doesn't match", headers);
	       return false;
	    }
	 }
	 else
	 {
	    headers.push_back("WWW-Authenticate: Basic");
	    sendError(log, client, 401, request, sendContent, "Unsupported Authentication Scheme", headers);
	    return false;
	 }
      }
   }

   size_t paramspos = filename.find('?');
   if (paramspos != std::string::npos)
   {
      params = filename.substr(paramspos + 1);
      filename.erase(paramspos);
   }

   filename = DeQuoteString(filename);

   // this is not a secure server, but at least prevent the obvious …
   if (filename.empty() == true || filename[0] != '/' ||
       strncmp(filename.c_str(), "//", 2) == 0 ||
       filename.find_first_of("\r\n\t\f\v") != std::string::npos ||
       filename.find("/../") != std::string::npos)
   {
      std::list<std::string> headers;
      sendError(log, client, 400, request, sendContent, "Filename contains illegal character (sequence)", headers);
      return false;
   }

   // nuke the first character which is a / as we assured above
   filename.erase(0, 1);
   if (filename.empty() == true)
      filename = "./";
   // support ~user/ uris to refer to /home/user/public_html/ as a kind-of special directory
   else if (filename[0] == '~')
   {
      // /home/user is actually not entirely correct, but good enough for now
      size_t dashpos = filename.find('/');
      if (dashpos != std::string::npos)
      {
	 std::string home = filename.substr(1, filename.find('/') - 1);
	 std::string pubhtml = filename.substr(filename.find('/') + 1);
	 filename = "/home/" + home + "/public_html/" + pubhtml;
      }
      else
	 filename = "/home/" + filename.substr(1) + "/public_html/";
   }

   // if no filename is given, but a valid directory see if we can use an index or
   // have to resort to a autogenerated directory listing later on
   if (DirectoryExists(filename) == true)
   {
      std::string const directoryIndex = _config->Find("aptwebserver::directoryindex");
      if (directoryIndex.empty() == false && directoryIndex == flNotDir(directoryIndex) &&
	    RealFileExists(filename + directoryIndex) == true)
	 filename += directoryIndex;
   }

   return true;
}
									/*}}}*/
static bool handleOnTheFlyReconfiguration(std::ostream &log, int const client,/*{{{*/
      std::string const &request, std::vector<std::string> parts, std::list<std::string> &headers)
{
   size_t const pcount = parts.size();
   for (size_t i = 0; i < pcount; ++i)
      parts[i] = DeQuoteString(parts[i]);
   if (pcount == 4 && parts[1] == "set")
   {
      _config->Set(parts[2], parts[3]);
      sendSuccess(log, client, request, true, "Option '" + parts[2] + "' was set to '" + parts[3] + "'!", headers);
      return true;
   }
   else if (pcount == 4 && parts[1] == "find")
   {
      std::string response = _config->Find(parts[2], parts[3]);
      addDataHeaders(headers, response);
      sendHead(log, client, 200, headers);
      sendData(client, headers, response);
      return true;
   }
   else if (pcount == 3 && parts[1] == "find")
   {
      if (_config->Exists(parts[2]) == true)
      {
	 std::string response = _config->Find(parts[2]);
	 addDataHeaders(headers, response);
	 sendHead(log, client, 200, headers);
	 sendData(client, headers, response);
	 return true;
      }
      sendError(log, client, 404, request, true, "Requested Configuration option doesn't exist", headers);
      return false;
   }
   else if (pcount == 3 && parts[1] == "clear")
   {
      _config->Clear(parts[2]);
      sendSuccess(log, client, request, true, "Option '" + parts[2] + "' was cleared.", headers);
      return true;
   }

   sendError(log, client, 400, request, true, "Unknown on-the-fly configuration request", headers);
   return false;
}
									/*}}}*/
static void * handleClient(int const client, size_t const id)		/*{{{*/
{
   auto logfilepath = _config->FindFile("aptwebserver::logfiles");
   if (logfilepath.empty() == false)
      strprintf(logfilepath, "%s.client-%lu.log", logfilepath.c_str(), id);
   else
      logfilepath = "/dev/null";
   std::ofstream logfile(logfilepath);
   basic_teeostream<char> log(std::clog, logfile);

   log << "ACCEPT client " << client << std::endl;
   bool closeConnection = false;
   while (closeConnection == false)
   {
      std::vector<std::string> messages;
      if (ReadMessages(client, messages) == false)
	 break;

      std::list<std::string> headers;
      for (std::vector<std::string>::const_iterator m = messages.begin();
	    m != messages.end() && closeConnection == false; ++m) {
	 // if we announced a closing in previous response, do the close now
	 if (std::find(headers.begin(), headers.end(), std::string("Connection: close")) != headers.end())
	 {
	    closeConnection = true;
	    break;
	 }
	 headers.clear();

	 log << ">>> REQUEST from " << client << " >>>" << std::endl << *m
	    << std::endl << "<<<<<<<<<<<<<<<<" << std::endl;
	 std::string filename;
	 std::string params;
	 bool sendContent = true;
	 if (parseFirstLine(log, client, *m, filename, params, sendContent, closeConnection, headers) == false)
	    continue;

	 // special webserver command request
	 if (filename.length() > 1 && filename[0] == '_')
	 {
	    std::vector<std::string> parts = VectorizeString(filename, '/');
	    if (parts[0] == "_config")
	    {
	       handleOnTheFlyReconfiguration(log, client, *m, parts, headers);
	       continue;
	    }
	 }

	 // string replacements in the requested filename
	 ::Configuration::Item const *Replaces = _config->Tree("aptwebserver::redirect::replace");
	 if (Replaces != NULL)
	 {
	    std::string redirect = "/" + filename;
	    for (::Configuration::Item *I = Replaces->Child; I != NULL; I = I->Next)
	       redirect = SubstVar(redirect, I->Tag, I->Value);
	    if (redirect.empty() == false && redirect[0] == '/')
	       redirect.erase(0,1);
	    if (redirect != filename)
	    {
	       sendRedirect(log, client, _config->FindI("aptwebserver::redirect::httpcode", 301), redirect, *m, sendContent);
	       continue;
	    }
	 }

	 ::Configuration::Item const *Overwrite = _config->Tree("aptwebserver::overwrite");
	 if (Overwrite != NULL)
	 {
	    for (::Configuration::Item *I = Overwrite->Child; I != NULL; I = I->Next)
	    {
	       regex_t *pattern = new regex_t;
	       int const res = regcomp(pattern, I->Tag.c_str(), REG_EXTENDED | REG_ICASE | REG_NOSUB);
	       if (res != 0)
	       {
		  char error[300];
		  regerror(res, pattern, error, sizeof(error));
		  sendError(log, client, 500, *m, sendContent, error, headers);
		  continue;
	       }
	       if (regexec(pattern, filename.c_str(), 0, 0, 0) == 0)
	       {
		  filename = _config->Find("aptwebserver::overwrite::" + I->Tag + "::filename", filename);
		  if (filename[0] == '/')
		     filename.erase(0,1);
		  regfree(pattern);
		  break;
	       }
	       regfree(pattern);
	    }
	 }

	 // deal with the request
	 unsigned int const httpsport = _config->FindI("aptwebserver::port::https", 4433);
	 std::string hosthttpsport;
	 strprintf(hosthttpsport, ":%u", httpsport);
	 if (_config->FindB("aptwebserver::support::http", true) == false &&
	       LookupTag(*m, "Host").find(hosthttpsport) == std::string::npos)
	 {
	    sendError(log, client, 400, *m, sendContent, "HTTP disabled, all requests must be HTTPS", headers);
	    continue;
	 }
	 else if (RealFileExists(filename) == true)
	 {
	    FileFd data(filename, FileFd::ReadOnly);
	    std::string condition = LookupTag(*m, "If-Modified-Since", "");
	    if (_config->FindB("aptwebserver::support::modified-since", true) == true && condition.empty() == false)
	    {
	       time_t cache;
	       if (RFC1123StrToTime(condition.c_str(), cache) == true &&
		     cache >= data.ModificationTime())
	       {
		  sendHead(log, client, 304, headers);
		  continue;
	       }
	    }

	    if (_config->FindB("aptwebserver::support::range", true) == true)
	       condition = LookupTag(*m, "Range", "");
	    else
	       condition.clear();
	    if (condition.empty() == false && strncmp(condition.c_str(), "bytes=", 6) == 0)
	    {
	       std::string ranges = ',' + _config->Find("aptwebserver::response-header::Accept-Ranges") + ',';
	       ranges.erase(std::remove(ranges.begin(), ranges.end(), ' '), ranges.end());
	       if (ranges.find(",bytes,") == std::string::npos)
	       {
		  // we handle it as an error here because we are a test server - a real one should just ignore it
		  sendError(log, client, 400, *m, sendContent, "Client does range requests we don't support", headers);
		  continue;
	       }

	       time_t cache;
	       std::string ifrange;
	       if (_config->FindB("aptwebserver::support::if-range", true) == true)
		  ifrange = LookupTag(*m, "If-Range", "");
	       bool validrange = (ifrange.empty() == true ||
		     (RFC1123StrToTime(ifrange.c_str(), cache) == true &&
		      cache <= data.ModificationTime()));

	       // FIXME: support multiple byte-ranges (APT clients do not do this)
	       if (condition.find(',') == std::string::npos)
	       {
		  size_t start = 6;
		  unsigned long long filestart = strtoull(condition.c_str() + start, NULL, 10);
		  // FIXME: no support for last-byte-pos being not the end of the file (APT clients do not do this)
		  size_t dash = condition.find('-') + 1;
		  unsigned long long fileend = strtoull(condition.c_str() + dash, NULL, 10);
		  unsigned long long filesize = data.FileSize();
		  if ((fileend == 0 || (fileend == filesize && fileend >= filestart)) &&
			validrange == true)
		  {
		     if (filesize > filestart)
		     {
			data.Skip(filestart);
                        // make sure to send content-range before conent-length
                        // as regression test for LP: #1445239
			std::ostringstream contentrange;
			contentrange << "Content-Range: bytes " << filestart << "-"
			   << filesize - 1 << "/" << filesize;
			headers.push_back(contentrange.str());
			std::ostringstream contentlength;
			contentlength << "Content-Length: " << (filesize - filestart);
			headers.push_back(contentlength.str());
			sendHead(log, client, 206, headers);
			if (sendContent == true)
			   sendFile(client, headers, data);
			continue;
		     }
		     else
		     {
			if (_config->FindB("aptwebserver::support::content-range", true) == true)
			{
			   std::ostringstream contentrange;
			   contentrange << "Content-Range: bytes */" << filesize;
			   headers.push_back(contentrange.str());
			}
			sendError(log, client, 416, *m, sendContent, "", headers);
			continue;
		     }
		  }
	       }
	    }

	    addFileHeaders(headers, data);
	    sendHead(log, client, 200, headers);
	    if (sendContent == true)
	       sendFile(client, headers, data);
	 }
	 else if (DirectoryExists(filename) == true)
	 {
	    if (filename[filename.length()-1] == '/')
	       sendDirectoryListing(log, client, filename, *m, sendContent, headers);
	    else
	       sendRedirect(log, client, 301, filename.append("/"), *m, sendContent);
	 }
	 else
	    sendError(log, client, 404, *m, sendContent, "", headers);
      }

      // if we announced a closing in the last response, do the close now
      if (std::find(headers.begin(), headers.end(), std::string("Connection: close")) != headers.end())
	 closeConnection = true;

      if (_error->PendingError() == true)
	 break;
      _error->DumpErrors(std::cerr);
   }
   _error->DumpErrors(std::cerr);
   close(client);
   log << "CLOSE client " << client << std::endl;
   return NULL;
}
									/*}}}*/

int main(int const argc, const char * argv[])
{
   CommandLine::Args Args[] = {
      {'p', "port", "aptwebserver::port", CommandLine::HasArg},
      {0, "request-absolute", "aptwebserver::request::absolute", CommandLine::HasArg},
      {0, "authorization", "aptwebserver::authorization", CommandLine::HasArg},
      {0, "proxy-authorization", "aptwebserver::proxy-authorization", CommandLine::HasArg},
      {0, "logfiles", "aptwebserver::logfiles", CommandLine::HasArg},
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
   // we don't care for our slaves, so ignore their death
   signal(SIGCHLD, SIG_IGN);

   int sock = socket(AF_INET6, SOCK_STREAM, 0);
   if(sock < 0)
   {
      _error->Errno("aptwerbserver", "Couldn't create socket");
      _error->DumpErrors(std::cerr);
      return 1;
   }

   int port = _config->FindI("aptwebserver::port", 8080);

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

   if (port == 0)
   {
      struct sockaddr_in6 addr;
      socklen_t addrlen = sizeof(sockaddr_in6);
      if (getsockname(sock, (struct sockaddr*) &addr, &addrlen) != 0)
	 _error->Errno("getsockname", "Could not get chosen port number");
      else
	 port = ntohs(addr.sin6_port);
   }
   std::string const portfilename = _config->Find("aptwebserver::portfile", "");
   if (portfilename.empty() == false)
   {
      FileFd portfile(portfilename, FileFd::WriteOnly | FileFd::Create | FileFd::Empty);
      std::string portcontent;
      strprintf(portcontent, "%d", port);
      portfile.Write(portcontent.c_str(), portcontent.size());
      portfile.Sync();
   }
   _config->Set("aptwebserver::port::http", port);

   FileFd pidfile;
   if (_config->FindB("aptwebserver::fork", false) == true)
   {
      std::string const pidfilename = _config->Find("aptwebserver::pidfile", "aptwebserver.pid");
      int const pidfilefd = GetLock(pidfilename);
      if (pidfilefd < 0 || pidfile.OpenDescriptor(pidfilefd, FileFd::WriteOnly) == false)
      {
	 _error->Errno("aptwebserver", "Couldn't acquire lock on pidfile '%s'", pidfilename.c_str());
	 _error->DumpErrors(std::cerr);
	 return 3;
      }

      pid_t child = fork();
      if (child < 0)
      {
	 _error->Errno("aptwebserver", "Forking failed");
	 _error->DumpErrors(std::cerr);
	 return 4;
      }
      else if (child != 0)
      {
	 // successfully forked: ready to serve!
	 std::string pidcontent;
	 strprintf(pidcontent, "%d", child);
	 pidfile.Write(pidcontent.c_str(), pidcontent.size());
	 pidfile.Sync();
	 if (_error->PendingError() == true)
	 {
	    _error->DumpErrors(std::cerr);
	    return 5;
	 }
	 std::cout << "Successfully forked as " << child << std::endl;
	 return 0;
      }
   }

   std::clog << "Serving ANY file on port: " << port << std::endl;

   int const slaves = _config->FindI("aptwebserver::slaves", SOMAXCONN);
   std::cerr << "SLAVES: " << slaves << std::endl;
   listen(sock, slaves);
   /*}}}*/

   _config->CndSet("aptwebserver::response-header::Server", "APT webserver");
   _config->CndSet("aptwebserver::response-header::Accept-Ranges", "bytes");
   _config->CndSet("aptwebserver::directoryindex", "index.html");

   size_t id = 0;
   while (true)
   {
      int client = accept(sock, NULL, NULL);
      if (client == -1)
      {
	 if (errno == EINTR)
	    continue;
	 _error->Errno("accept", "Couldn't accept client on socket %d", sock);
	 _error->DumpErrors(std::cerr);
	 return 6;
      }

      std::thread t(handleClient, client, ++id);
      t.detach();
   }
   pidfile.Close();

   return 0;
}
