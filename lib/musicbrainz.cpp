/* --------------------------------------------------------------------------

   MusicBrainz -- The Internet music metadatabase

   Copyright (C) 2000 Robert Kaye
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

     $Id$

----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef WIN32
#include <winsock.h>
#include "config_win32.h"
#endif
#include "musicbrainz.h"
#include "http.h"
#include "errors.h"
#include "diskid.h"

extern "C"
{
   #include "sha1.h"
   #include "base64.h"
   #include "bitcollider.h"
}

const char *scriptUrl = "/cgi-bin/mq.pl";
const char *localCDInfo = "@CDINFO@";
const char *localTOCInfo = "@LOCALCDINFO@";
const char *localAssociateCD = "@CDINFOASSOCIATECD@";
const char *defaultServer = "mm.musicbrainz.org";
const short defaultPort = 80;
const char *rdfUTF8Encoding = "<?xml version=\"1.0\"?>\n";
const char *rdfISOEncoding = 
    "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n";

const char *rdfHeader = 
    "<rdf:RDF xmlns:rdf = \"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"
    "         xmlns:dc  = \"http://purl.org/dc/elements/1.1/\"\n"
    "         xmlns:mq  = \"http://musicbrainz.org/mm/mq-1.0#\"\n"
    "         xmlns:mm  = \"http://musicbrainz.org/mm/mm-2.0#\">\n";

const char *rdfFooter = 
    "</rdf:RDF>\n";

#define DB printf("%s:%d\n",  __FILE__, __LINE__);

MusicBrainz::MusicBrainz(void)
{
    m_rdf = NULL;
    m_server = string(defaultServer);
    m_serverPort = defaultPort;
    m_proxy = "";
    m_useUTF8 = true;
    m_depth = 2;
}

MusicBrainz::~MusicBrainz(void)
{
    delete m_rdf; 
}

// Set the URL and port of the musicbrainz server to use.
bool MusicBrainz::SetServer(const string &serverAddr, short serverPort)
{
    m_server = serverAddr;
    m_serverPort = serverPort;

    return true;
}

// Set the HTTP proxy server address and port if needed.
bool MusicBrainz::SetProxy(const string &proxyAddr, short proxyPort)
{
    m_proxy = proxyAddr;
    m_proxyPort = proxyPort;

    return true;
}

// Set the cdrom device used for cd lookups. In windows, this would
// be a drive specification.
bool MusicBrainz::SetDevice(const string &device)
{
    m_device = device;
    return true;
}

// Set the depth for the queries. The depth of a query determines the
// number of levels of information are returned from the server.
bool MusicBrainz::SetDepth(int depth)
{
    m_depth = depth;
    return true;
}

// Stoopid windows helper functions to init the winsock layer.
#ifdef WIN32
void MusicBrainz::WSAInit(void)
{
    WSADATA sGawdIHateMicrosoft;
    WSAStartup(0x0002,  &sGawdIHateMicrosoft);
}

void MusicBrainz::WSAStop(void)
{
    WSACleanup();
}
#endif

// This function generates the URL needed to access the web cd submission
// pages on MusicBrainz.
bool MusicBrainz::GetWebSubmitURL(string &url)
{
    DiskId id;
    string args;
    Error  ret;

    ret = id.GetWebSubmitURLArgs(m_device, args);
    if (ret != kError_NoErr)
        return false;

    url = string("http://") + string(m_server);
    if (m_serverPort != defaultPort)
    {
       char port[10];

       sprintf(port, ":%d", m_serverPort);
       url += string(port);
    }
    url += string("/cdi/submit.html") + args;

    return true;
}

// A helper function to convert URLs to local file paths
static const char* protocol = "file://";
static Error URLToFilePath(const char* url, char* path, uint32* length)
{
    Error result = kError_InvalidParam;

    assert(path);
    assert(url);
    assert(length);

    if(path && url && length && !strncasecmp(url, protocol, strlen(protocol)))
    {
        result = kError_BufferTooSmall;

        if(*length >= strlen(url) - strlen(protocol) + 1)
        {
            strcpy(path, url + strlen(protocol));
#ifdef WIN32
            if(strlen(path) > 1 && path[1] == '|')
            {
                path[1] = ':';
            }

            for(int32 index = strlen(path) - 1; index >=0; index--)
            {
                if(path[index] == '/')
                    path[index] = '\\';
            }
#endif
            result = kError_NoErr;
        }

        *length = strlen(url) - strlen(protocol) + 1;
    }

    return result;
}

// A helper function to glue the necessary RDF headers and footers to make
// a valid RDF query
void MusicBrainz::MakeRDFQuery(string &rdf)
{
    rdf = (m_useUTF8 ? string(rdfUTF8Encoding) : string(rdfISOEncoding)) +
           string(rdfHeader) + 
           rdf + 
           string(rdfFooter);
}

// The main Query function. This query builds a valid RDF query,
// sends it to the server using http and then creates an RDF parser
// to parse the returned RDF from the server.
bool MusicBrainz::Query(const string &rdfObject, vector<string> *args)
{
    MBHttp   http;
    char   port[20];
    string rdf = rdfObject, url, value;
    Error  ret;

    // Is the given query a placeholder to perform a local query?
    // A cd lookup/associate function requires to have the diskid
    // module to generate an RDF query based on the values read
    // from the current CD.
    if (rdf == string(localCDInfo) ||
        rdf == string(localAssociateCD))
    {
        DiskId id;

        // Generate the local query and then keep trucking
        ret = id.GenerateDiskIdQueryRDF(m_device, rdf, 
                        rdf == string(localAssociateCD));
        if (IsError(ret))
        {
            id.GetLastError(m_error);
            return false;
        }
        //printf("%s\n", rdf.c_str());
    }
    // If the query is a local TOC query, the client just wants to
    // know about the table of contents of the CD, not about the
    // server lookup metadata. In that case, generate the proper
    // RDF from the CD, and then use that as the result of the query
    // so the user can query it to extract the TOC from the CD.
    if (rdf == string(localTOCInfo))
    {
        DiskId id;

        // Generate the TOC query
        ret = id.GenerateDiskIdRDF(m_device, m_response);
        if (IsError(ret))
        {
            id.GetLastError(m_error);
            return false;
        }

        // And now take the query and parse it so the user can query it
        MakeRDFQuery(m_response);

        m_rdf = new RDFExtract(m_response, m_useUTF8);
        if (m_rdf->HasError())
        {
            m_error = string("Internal error.");
            return false;
        }
         
        m_rdf->GetSubjectFromObject(string(MBE_QuerySubject), m_baseURI);
        m_currentURI = m_baseURI;

        // Return, because we don't want to actually query the server
        return true;
    }

    // Substitute the passed in literal strings into the placeholders
    // in the query.
    SubstituteArgs(rdf, args);

    // If there is a proxy, set up the proxy url now
    if (m_proxy.length() > 0)
    {
        sprintf(port, ":%d", m_proxyPort);
        http.SetProxyURL(string("http://") + m_proxy + string(port));
    }

    // Is this a GET or POST query? GET queries start with an http
    // and will specify the URL to retrieve. If a query does not
    // start with an http, then its assumed to be a POST query.
    if (strncmp(rdf.c_str(), "http://", 7) == 0)
    {
        string::size_type pos = 0;

        pos = rdf.find("@URL@", pos);
        if (pos != string::npos)
        {
            sprintf(port, ":%d", m_serverPort);   
            rdf.replace(pos, 5, m_server + string(port));
        }
        url = rdf;
        rdf = string("");
    }
    else
    {
        MakeRDFQuery(rdf);

        sprintf(port, ":%d", m_serverPort);   
        url = string("http://") + m_server + string(port) + string(scriptUrl);
    }

    //printf("  url: %s\n", url.c_str());
    //printf("query: %s\n\n", rdf.c_str());

    // Now use the http module to get/post the request and to download
    // the result.
    ret = http.DownloadToString(url, rdf, m_response); 
    if (IsError(ret))
    { 
        SetError(ret);
        return false;
    }
    //printf("result: %s\n\n", m_response.c_str());

    // Parse the returned RDF
    m_rdf = new RDFExtract(m_response, m_useUTF8);
    if (m_rdf->HasError())
    {
        string err;

        m_rdf->GetError(err);
        m_error = string("The server sent an invalid response. (") +
                  err + string(")");
        return false;
    }

    // Determine the top level node by reverse looking up the mq:result node
    if (!m_rdf->GetSubjectFromObject(string(MBE_QuerySubject), m_baseURI))
    {
        m_error = string("Cannot parse the server response (cannot find "
                         "mq:Result top level URI)");
        return false;
    }

    // Use the baseURI as the starting point in the RDF graph.
    m_currentURI = m_baseURI;

    // See if an error occured. If so, extract the error message and return 
    value = m_rdf->Extract(m_currentURI, string(MBE_GetError));
    if (value.length() != 0)
    {
        m_error = value;
        return false;
    }

    // Extract the status of the query
    value = m_rdf->Extract(m_currentURI, string(MBE_GetStatus));
    if (value.length() == 0)
    {    
        m_error = string("Could not determine the result of the query");
        return false;
    }

    // Return if its not OK or fuzzy. OK means that the query ran successfully
    // and if the query was a CD match query and the match was fuzzy,
    // the status will be Fuzzy.
    if (value != string("OK") && value != string("Fuzzy"))
    {    
        m_error = string("Unknown query status: ") + value;
        return false;
    }

    // We're done -- bail. The user can now use Select/Extract to retrieve
    // data from the RDF
    return true;
}

// returns a error string from the last query
void MusicBrainz::GetQueryError(string &ErrorText)
{
    ErrorText = m_error;
}

// A shortcut function to retrieve a string value from the RDF result.
const string &MusicBrainz::Data(const string &resultName, int Index)
{
    if (!m_rdf)
    {
       m_error = string("The server returned no valid data");
       return m_empty;
    }
    return m_rdf->Extract(m_currentURI, resultName, Index);
}

// A shortcut function to retrieve an integer value from the RDF result.
int MusicBrainz::DataInt(const string &resultName, int Index)
{
    if (!m_rdf)
    {
       m_error = string("The server returned no valid data");
       return -1;
    }
    return atoi(m_rdf->Extract(m_currentURI, resultName, Index).c_str());
}

// This function calls the RDF extract function to retrieve one node
// from the RDF graph. Please see the docs for more details.
bool MusicBrainz::GetResultData(const string &resultName, int Index, 
                                string &data)
{
    if (!m_rdf)
    {
       m_error = string("The server returned no valid data");
       return false;
    }

    data = m_rdf->Extract(m_currentURI, resultName, Index);
    if (data.length() > 0)
        return true;

    m_error = "No data was returned.";
    return false;
}

// Check to see if a given RDF node exists. This function takes the
// same args as the Extract and Select functions
bool MusicBrainz::DoesResultExist(const string &resultName, int Index)
{
    string data;
    string query;

    if (!m_rdf)
       return false;

    data = m_rdf->Extract(m_currentURI, resultName, Index);
    return data.length() > 0;
}

// This is a shorthand version of the Select, which allows passing
// one ordinal. The default value for the ordinal is 0. See 
// the function below and the docs for more details on this function.
bool MusicBrainz::Select(const string &query, int ordinal)
{
    list<int> ordinalList;

    if (m_rdf == NULL)
       return false;

    ordinalList.push_back(ordinal);
    return Select(query, &ordinalList);
}

// The Select function selects a new currentURI. Please check the
// docs for details on this important function.
bool MusicBrainz::Select(const string &queryArg, list<int> *ordinalList)
{
    string newURI, query = queryArg;

    if (m_rdf == NULL)
       return false;

    if (query == string(MBS_Rewind))
    {
        m_currentURI = m_baseURI;
        return true;
    }
    if (query == string(MBS_Back))
    {
        if (m_contextHistory.size() == 0)
            return false;

        m_currentURI = *(m_contextHistory.end() - 1);
        m_contextHistory.erase(m_contextHistory.end(), m_contextHistory.end());
        return true;
    }
   
    newURI = m_rdf->Extract(m_currentURI, query, ordinalList);
    if (newURI.length() == 0)
        return false;

    m_contextHistory.push_back(m_currentURI);
    m_currentURI = newURI;
    return true;
}

// Return the RDF document that the server returned to us.
bool MusicBrainz::GetResultRDF(string &RDFObject)
{
    RDFObject = m_response;
    return true;
}

// This function allows an outside user to set and RDF object and
// then query it using the Extract/Select functions. 
bool MusicBrainz::SetResultRDF(string &rdf)
{
    if (m_rdf)
       delete m_rdf;

    m_rdf = new RDFExtract(rdf, m_useUTF8);
    if (!m_rdf->HasError())
    {
        m_response = rdf;
        return true;
    }
    return false;
}

// some Get???ID queries return complete URLs that can be used
// to retrieve the related content from the MB server. To get just
// the id from the URL, use this function.
void MusicBrainz::GetIDFromURL(const string &url, string &id)
{
    string::size_type pos;

    id = url;
    pos = id.rfind("/", string::npos); 
    if (pos != string::npos)
       pos++;

    id.erase(0, pos); 
}

// Escape the & < and > in the passed rdf string and replace with
// the proper XML entities
const string MusicBrainz::EscapeArg(const string &arg)
{
    string            text;
    string::size_type pos;

    text = arg;

    // Replace all the &
    pos = text.find("&", 0);
    for(;;)
    {
       pos = text.find("&", pos);
       if (pos != string::npos)
           text.replace(pos, 1, string("&amp;"));
       else
           break;
       pos++;
    }

    // Replace all the <
    pos = text.find("<", 0);
    for(;;)
    {
       pos = text.find("<", pos);
       if (pos != string::npos)
           text.replace(pos, 1, string("&lt;"));
       else
           break;
    }

    // Replace all the >
    pos = text.find(">", 0);
    for(;;)
    {
       pos = text.find(">", pos);
       if (pos != string::npos)
           text.replace(pos, 1, string("&gt;"));
       else
           break;
    }

    return text;
}

// This function replaces the @1@, @2@ argument place holders with
// literal values passed in the args vector.
void MusicBrainz::SubstituteArgs(string &rdf, vector<string> *args)
{
    vector<string>::iterator i;
    string::size_type        pos;
    char                     replace[100];
    int                      j;
    string                   arg;

    if (args)
    {
        // Replace all occurances of @##@ with the arguments passed in the
        // args list
        for(i = args->begin(), j = 1; i != args->end(); i++, j++)
        {
            arg = EscapeArg(*i);
            sprintf(replace, "@%d@", j); 
            pos = rdf.find(string(replace), 0);
            if (pos != string::npos)
            {
                if (arg.length() == 0)
                   rdf.replace(pos, strlen(replace), string("__NULL__"));
                else
                   rdf.replace(pos, strlen(replace), arg);
            }
        }
    }
    // If there are fewer args passed than are in the rdf, then
    // replace the remaining tokens with a NULL specifier.
    for(;; j++)
    {
        sprintf(replace, "@%d@", j); 
        pos = rdf.find(string(replace), 0);
        if (pos != string::npos)
        {
            rdf.replace(pos, strlen(replace), "__NULL__");
        }
        else
            break;
    }

    // Replace any depth place holders with the current dept value
    for(;;)
    {
        sprintf(replace, "@DEPTH@", j); 
        pos = rdf.find(string(replace), 0);
        if (pos != string::npos)
        {
            sprintf(replace, "%d", m_depth);
            rdf.replace(pos, 7, string(replace));
        }
        else
            break;
    }
}

// Convert the internal error code to an error string
void MusicBrainz::SetError(Error ret)
{
    switch(ret)
    {
       case kError_CantFindHost:
          m_error = string("Cannot find server: ") + m_server;
          break;
       case kError_ConnectFailed:
          m_error = string("Cannot connect to server: ") + m_server;
          break;
       case kError_IOError:
          m_error = string("Cannot send/receive to/from server.");
          break;
       case kError_InvalidURL:
          m_error = string("Proxy or server URL is invalid.");
          break;
       case kError_WriteFile:
          m_error = string("Cannot write to disk. Disk full?");
          break;
       case kError_HTTPFileNotFound:
          m_error = string("Cannot find musicbrainz pages on server. Check "
                           "your server name and port settings.");
          break;
       case kError_UnknownServerError:
          m_error = string("The server encountered an error processing this "
                           "query.");
          break;
       default:
          char num[10];
          sprintf(num, "%d", ret);
          m_error = string("Internal error: ") + string(num);
          break;
    }
}

// Calculate a Bitzi bitprint and submit it to musicbrainz. The
// bitprint will be used in the musicbrainz metadata acceptance 
// process. The completed bitzi bitprints will be submitted to the
// Bitizi community metadatabase.
bool MusicBrainz::CalculateBitprint(const string &fileName, BitprintInfo *info) 
{
    Bitcollider           *bc;
    BitcolliderSubmission *sub;

    bc = bitcollider_init(0);
    if (!bc)
        return false;

    sub = create_submission(bc);
    if (sub == NULL)
       return false;

    if (!analyze_file(sub, fileName.c_str(), false))
       return false;

    strncpy(info->filename, fileName.c_str(), 255);
    strncpy(info->bitprint, get_attribute(sub, "bitprint"), MB_BITPRINTSIZE);
    strncpy(info->first20, 
            get_attribute(sub, "tag.file.first20"), MB_FIRST20SIZE);
    info->length = atoi(get_attribute(sub, "tag.file.length"));

    if (get_attribute(sub, "tag.mp3.audio_sha1"));
    {
        strncpy(info->audioSha1, 
            get_attribute(sub, "tag.mp3.audio_sha1"), MB_SHA1SIZE);
        info->duration = atoi(get_attribute(sub, "tag.mp3.duration"));
        info->samplerate = atoi(get_attribute(sub, "tag.mp3.samplerate"));
        info->bitrate = atoi(get_attribute(sub, "tag.mp3.bitrate"));
        info->stereo = strcmp(get_attribute(sub, "tag.mp3.stereo"), "y") == 0;
        if (get_attribute(sub, "tag.mp3.vbr"))
           info->vbr = strcmp(get_attribute(sub, "tag.mp3.vbr"), "y") == 0;
        else
           info->vbr = 0;
    }
    delete_submission(sub);
    bitcollider_shutdown(bc);

    return true;
}
