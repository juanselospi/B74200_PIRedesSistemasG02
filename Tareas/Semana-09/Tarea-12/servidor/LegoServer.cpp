#include "LegoServer.h"
#include "Logger.h"

#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <vector>
#include <fstream>

/**
 * Constructor
 */
LegoServer::LegoServer(const std::string &bindIp, int port)
    : serverSocket('s', false), bindIp(bindIp), port(port)
{
}

/**
 * Read the full HTTP request headers
 */
std::string LegoServer::readHttpRequest(VSocket *client)
{

   std::string request = "";
   char buffer[1024];
   size_t bytesRead = 0;

   while ((bytesRead = client->Read(buffer, sizeof(buffer) - 1)) > 0)
   {
      buffer[bytesRead] = '\0';
      request += buffer;

      if (request.find("\r\n\r\n") != std::string::npos)
      {
         break;
      }
   }

   return request;
}

/**
 * Extract path from first request line
 */
std::string LegoServer::getPathFromRequest(const std::string &request)
{

   std::istringstream stream(request);
   std::string method;
   std::string path;
   std::string version;

   stream >> method >> path >> version;

   if (method != "GET")
   {
      throw std::runtime_error("HTTP method not supported");
   }

   if (path.empty() || version.empty())
   {
      throw std::runtime_error("Invalid HTTP request");
   }

   return path;
}

/**
 * Extract query parameter from path
 */
std::string LegoServer::getQueryParam(const std::string &path, const std::string &key)
{

   size_t qpos = path.find('?');
   if (qpos == std::string::npos)
   {
      return "";
   }

   std::string query = path.substr(qpos + 1);
   std::string pattern = key + "=";

   size_t start = query.find(pattern);
   if (start == std::string::npos)
   {
      return "";
   }

   start += pattern.size();
   size_t end = query.find('&', start);

   if (end == std::string::npos)
   {
      return query.substr(start);
   }

   return query.substr(start, end - start);
}

/**
 * Build full HTTP response
 */
std::string LegoServer::buildHttpResponse(const std::string &body,
                                          const std::string &status,
                                          const std::string &contentType)
{

   std::ostringstream response;

   response << "HTTP/1.1 " << status << "\r\n";
   response << "Content-Type: " << contentType << "\r\n";
   response << "Content-Length: " << body.size() << "\r\n";
   response << "Connection: close\r\n";
   response << "\r\n";
   response << body;

   return response.str();
}

std::string LegoServer::handleBitacora()
{
   std::ifstream file("bitacora.log");

   std::ostringstream html;
   html << "<html><head>";
   html << "<meta charset=\"UTF-8\">";
   html << "<title>Log</title>";
   html << "<style>";
   html << "body{font-family:Arial;margin:30px;background:#f4f4f4;}";
   html << "h1{color:#333;}";
   html << "table{border-collapse:collapse;width:100%;background:white;}";
   html << "th,td{border:1px solid #ccc;padding:8px;text-align:left;}";
   html << "th{background:#222;color:white;}";
   html << "tr:nth-child(even){background:#f2f2f2;}";
   html << ".error{color:red;font-weight:bold;}";
   html << ".ok{color:green;font-weight:bold;}";
   html << "</style>";
   html << "</head><body>";

   html << "<h1>Server Log</h1>";

   if (!file.is_open())
   {
      html << "<p class=\"error\">bitacora.log does not exist or could not be opened.</p>";
   }
   else
   {
      html << "<table>";
      html << "<tr><th>Entry</th></tr>";

      std::string line;
      while (std::getline(file, line))
      {
         html << "<tr><td>" << line << "</td></tr>";
      }

      html << "</table>";
   }

   html << "</body></html>";

   return html.str();
}

/*
 * Build client log response
 */
std::string LegoServer::handleBitacoraCliente()
{
   std::ifstream file("../cliente/bitacora.log");

   std::ostringstream html;
   html << "<html><head><meta charset=\"UTF-8\">";
   html << "<title>Client Log</title>";
   html << "<style>";
   html << "body{font-family:Arial;margin:30px;background:#f4f4f4;}";
   html << "table{border-collapse:collapse;width:100%;background:white;}";
   html << "th,td{border:1px solid #ccc;padding:8px;text-align:left;}";
   html << "th{background:#222;color:white;}";
   html << "</style></head><body>";

   html << "<h1>Client Log</h1>";

   if (!file.is_open())
   {
      html << "<p>Could not open ../cliente/bitacora.log</p>";
   }
   else
   {
      html << "<table><tr><th>Entry</th></tr>";
      std::string line;
      while (std::getline(file, line))
      {
         html << "<tr><td>" << line << "</td></tr>";
      }
      html << "</table>";
   }

   html << "</body></html>";
   return html.str();
}

std::string LegoServer::handleIndex() {

   std::cout << "Fetching figures index..." << std::endl;
   Logger::log("SERVER", "RESPONSE", "FIGURES_LIST");

   std::vector<std::string> figures = fileSystem.getFigureNames();

   std::ostringstream html;

   html << "<html><head>";
   html << "<meta charset=\"UTF-8\">";
   html << "<title>Lego Figures</title>";
   html << "<style>";
   html << "body{font-family:Arial;margin:30px;background:#f4f4f4;color:#222;}";
   html << "h1{font-size:38px;margin-bottom:10px;}";
   html << ".card{background:white;padding:25px;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,.12);}";
   html << "select{width:100%;padding:12px;font-size:18px;border-radius:8px;border:1px solid #ccc;margin-top:15px;}";
   html << ".fig{padding:10px;margin:8px 0;background:#e8e8e8;border-radius:6px;font-size:18px;}";
   html << ".fig a{text-decoration:none;color:#222;font-weight:bold;}";
   html << ".fig a:hover{color:#007BFF;}";
   html << "</style>";
   html << "</head><body>";

   html << "<div class=\"card\">";
   html << "<h1>Available Lego Figures</h1>";

   html << "<SELECT NAME=\"figures\">";

   if (figures.empty()) {
      html << "<OPTION value=\"None\">No figures available</OPTION>";
   } else {
      for (const auto &fig : figures) {
         html << "<OPTION value=\"" << fig << "\">" << fig << "</OPTION>";
      }
   }

   html << "</SELECT>";

   if (!figures.empty()) {
      for (const auto &fig : figures) {
         html << "<div class=\"fig\">";
         html << "<a href=\"/lego/list.php?figure=" << fig << "&part=1\">"
              << fig << " - Part 1</a><br>";
         html << "<a href=\"/lego/list.php?figure=" << fig << "&part=2\">"
              << fig << " - Part 2</a>";
         html << "</div>";
      }
   }

   html << "</div>";
   html << "</body></html>";

   return html.str();
}


/**
 * Build /lego/list.php response
 */
std::string LegoServer::handleList( const std::string & path ) {

   std::string figure = getQueryParam( path, "figure" );
   std::string part = getQueryParam( path, "part" );

   std::vector<Piece> pieces = fileSystem.getPieces( figure, part );

   if ( pieces.empty() ) {
      Logger::log("SERVER", "RESPONSE", "FIGURE_NOT_FOUND figure=" + figure + " segment=" + part);
   } else {
      Logger::log("SERVER", "RESPONSE", "FIGURE_FOUND figure=" + figure + " segment=" + part);
   }

   std::ostringstream html;
   int total = 0;

   html << "<html><head>";
   html << "<meta charset=\"UTF-8\">";
   html << "<title>Lego Pieces</title>";
   html << "<style>";
   html << "body{font-family:Arial;margin:30px;background:#f4f4f4;color:#222;}";
   html << "h1{font-size:38px;margin-bottom:5px;}";
   html << "h2{font-size:20px;font-weight:normal;color:#555;margin-top:0;}";
   html << ".card{background:white;padding:25px;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,.12);}";
   html << "table{border-collapse:collapse;width:100%;margin-top:20px;background:white;}";
   html << "th,td{border:1px solid #ccc;padding:12px;text-align:left;font-size:18px;}";
   html << "th{background:#222;color:white;}";
   html << "tr:nth-child(even){background:#f2f2f2;}";
   html << ".total{font-weight:bold;background:#e8e8e8;}";
   html << ".empty{color:#b00020;font-weight:bold;font-size:18px;}";
   html << "</style>";
   html << "</head><body>";

   html << "<div class=\"card\">";
   html << "<h1>Pieces List</h1>";
   html << "<h2>Figure: " << figure << " | Segment: " << part << "</h2>";

   if (pieces.empty()) {
      html << "<p class=\"empty\">No pieces found for this figure.</p>";
   }

   html << "<table border=\"1\">";
   html << "<tr><th>CANTIDAD</th><th>DESCRIPTION</th></tr>";

   for ( const auto & piece : pieces ) {
      html << "<tr>";
      html << "<td>" << piece.quantity << "</td>";
      html << "<td>" << piece.description << "</td>";
      html << "</tr>";
      total += piece.quantity;
   }

   html << "<tr class=\"total\">";
   html << "<td>Total pieces to build this figure</td>";
   html << "<td>" << total << "</td>";
   html << "</tr>";

   html << "</table>";

   html << "</div>";
   html << "</body></html>";

   return html.str();
}


/**
 * Process request and choose route
 */
std::string LegoServer::processRequest(const std::string &request)
{

   std::string path = getPathFromRequest(request);

   if (path == "/lego/index.php")
   {
      return buildHttpResponse(handleIndex());
   }

   if (path.find("/lego/list.php") == 0)
   {
      return buildHttpResponse(handleList(path));
   }

   if (path == "/log")
   {
      return buildHttpResponse(handleBitacora());
   }

   if (path == "/log/client")
   {
      return buildHttpResponse(handleBitacoraCliente());
   }

   return buildHttpResponse(
       "<html><body><h1>404 Not Found</h1></body></html>",
       "404 Not Found");
}

/**
 * Worker function: attend one client
 */
void LegoServer::handleClient(VSocket *client)
{

   try
   {
      std::string request = readHttpRequest(client);
      std::string response = processRequest(request);
      client->Write(response.c_str());
   }
   catch (const std::exception &e)
   {
      Logger::log("SERVIDOR", "ERROR", std::string("Error procesando solicitud: ") + e.what());
      try
      {
         std::string body =
             std::string("<html><body><h1>500 Internal Server Error</h1><p>") +
             e.what() +
             "</p></body></html>";

         std::string response = buildHttpResponse(body, "500 Internal Server Error");
         client->Write(response.c_str());
      }
      catch (...)
      {
      }
   }

   try
   {
      client->Close();
   }
   catch (...)
   {
   }

   delete client;
}

/**
 * Main server loop
 */
void LegoServer::run()
{

   serverSocket.Bind(bindIp.c_str(), port);
   serverSocket.MarkPassive(10);

   std::cout << "Servidor escuchando en " << bindIp << ":" << port << std::endl;

   while (true)
   {
      VSocket *client = serverSocket.AcceptConnection();

      std::thread worker(&LegoServer::handleClient, this, client);
      worker.detach();
   }
}