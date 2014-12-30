
#include "precompiled_headers.h"
#include "InternetDownload.h"

InternetDownload::InternetDownload(const tstring& userAgent, const tstring& url) 
    : m_url(url),
      m_hInternet(NULL),
      m_hConnect(NULL),
      m_hHttp(NULL),
      m_error(0)
{
    m_hInternet = ::InternetOpen(userAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL /* proxy*/ , NULL /* proxy bypass */, 0 /* dwflags */);
}

InternetDownload::~InternetDownload() 
{
    if (m_hHttp) {
        ::InternetCloseHandle(m_hHttp);
        m_hHttp = NULL;
    }

    if (m_hConnect) {
        ::InternetCloseHandle(m_hConnect);
        m_hConnect = NULL;
    }

    if (m_hInternet) {
        ::InternetCloseHandle(m_hInternet);
        m_hInternet = NULL;
    }
}

BOOL InternetDownload::request() {

    if (!m_hInternet) {
        return FALSE;
    }

    URL_COMPONENTS urlComponents;
    memset(&urlComponents, 0, sizeof(URL_COMPONENTS));
    urlComponents.dwStructSize = sizeof(URL_COMPONENTS);
    /* Set all the lengths to non-zero, and leave the pointers at zero, then we just get pointers to the 
       url string, with lengths for each section
     */
    urlComponents.dwExtraInfoLength = 1;
    urlComponents.dwHostNameLength = 1;
    urlComponents.dwPasswordLength = 1;
    urlComponents.dwSchemeLength = 1;
    urlComponents.dwUrlPathLength = 1;
    urlComponents.dwUserNameLength = 1;
    urlComponents.nPort = 80;

    /* Make a permanent copy of the url (c_str() returns a temporary object */
    TCHAR *url = new TCHAR[m_url.size() + 1];
    _tcscpy_s(url, m_url.size(), m_url.c_str());

    InternetCrackUrl(url, m_url.size(), 0, &urlComponents);

    
    TCHAR *host = new TCHAR[urlComponents.dwHostNameLength + 1];
    _tcscpy_s(urlComponents.lpszHostName, urlComponents.dwHostNameLength, host);
    host[urlComponents.dwHostNameLength] = _T('\0');


    m_hConnect = InternetConnect(m_hInternet, 
        host,
        urlComponents.nPort, 
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0, /* flags */
        reinterpret_cast<DWORD_PTR>(this));

    delete[] host;

    PCTSTR rgpszAcceptTypes[] = {_T("*/*"), NULL}; 

    TCHAR *path = new TCHAR[urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength + 1];
    _tcscpy_s(path, urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength, path);
    path[urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength] = _T('\0');

    m_hHttp = HttpOpenRequest(
        m_hConnect, 
        _T("GET"),
        path,
        NULL, /* http version - defaults to "HTTP/1.1" */
        NULL, /* referrer - NULL means no referrer header sent */
        rgpszAcceptTypes, /* Accept types */
        0, /* Flags - none required */
        reinterpret_cast<DWORD_PTR>(this));
    
    if (!m_hHttp) {
        m_error = GetLastError();
        return FALSE;
    }

    BOOL sendRequestResult = HttpSendRequest(m_hHttp, 
        NULL, /*lpszHeaders */
        -1L, /* dwHeadersLength */
        NULL, /* lpOptional  (POST data) */
        0);    /* dwOptionalLength */

    if (!sendRequestResult) {
        m_error = GetLastError();
        return FALSE;
    }

    return TRUE;
}


BOOL InternetDownload::getData(writeData_t writeData, void *context) 
{

    if (m_error) {
        return FALSE;
    }

    DWORD bytesAvailable;
    BOOL dataAvailableResponse = InternetQueryDataAvailable(m_hHttp, &bytesAvailable, 0, NULL);

    if (dataAvailableResponse && bytesAvailable) {
        BYTE buffer[4096];
        DWORD bytesRead;
        DWORD bytesToRead = bytesAvailable;
        while (bytesToRead > 0) {
            if (bytesToRead > 4095) {
                bytesToRead = 4095;
            }
            InternetReadFile(m_hHttp, buffer, bytesToRead, &bytesRead);
            
            (*this.*writeData)(buffer, bytesRead, context);

            bytesAvailable -= bytesRead;
            bytesToRead = bytesAvailable;
        }

        return TRUE;
    }

    return FALSE;
}

BOOL InternetDownload::saveToFile(const tstring& filename) {
    if (request()) {
        FILE *fp = _tfopen(filename.c_str(), _T("wb"));
        return getData(&InternetDownload::writeToFile, fp);
    }

    return FALSE;
}

tstring InternetDownload::getContent() {
    if (request()) {
        std::string result;
        BOOL success = getData(&InternetDownload::writeToString, &result);
    
        if (success) {
#ifdef _UNICODE
            int length = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), result.size(), NULL, -1);
            TCHAR *resultWide = new TCHAR[length + 1];
            MultiByteToWideChar(CP_UTF8, 0, result.c_str(), result.size(), resultWide, length + 1);
            tstring strResultWide(resultWide);
            delete[] resultWide;
            return strResultWide;
#else
            return result;
#endif
        }
    }

    return _T("");
}

void InternetDownload::writeToFile(BYTE* buffer, DWORD bufferLength, void *context) {
    fwrite(buffer, bufferLength, 1, reinterpret_cast<FILE*>(context));
}

void InternetDownload::writeToString(BYTE* buffer, DWORD bufferLength, void* context) {
    reinterpret_cast<std::string*>(context)->append(reinterpret_cast<char*>(buffer), static_cast<size_t>(bufferLength));
}
