/* Feel free to use this example code in any way
   you see fit (Public Domain) */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#else
#include <winsock2.h>
#include <direct.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <microhttpd.h>
#include "hoops_license.h"
#include "utilities.h"
#include "ExProcess.h"
#include "HCLuminateBridge.h"
#include <windows.h>

using namespace HC_luminate_bridge;

#ifdef _MSC_VER
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp ((a),(b))
#endif /* !strcasecmp */
#endif /* _MSC_VER */

#if defined(_MSC_VER) && _MSC_VER + 0 <= 1800
   /* Substitution is OK while return value is not used */
#define snprintf _snprintf
#endif

// #define PORT            8888
#define POSTBUFFERSIZE  512
#define MAXCLIENTS      1

static char s_pcWorkingDir[256];
static char s_pcScModelsDir[256];
static char s_pcHtmlRootDir[256];
static char s_pcModelName[256];
static std::map<std::string, std::string> s_mParams;

enum ConnectionType
{
    GET = 0,
    POST = 1
};

static unsigned int nr_of_uploading_clients = 0;
static ExProcess *pExProcess;
static std::map<std::string, HCLuminateBridge*> m_mpLuminateBridge;
HWND hWnd;

/**
 * Information we keep per connection.
 */
struct connection_info_struct
{
    enum ConnectionType connectiontype;

    /**
     * Handle to the POST processing state.
     */
    struct MHD_PostProcessor* postprocessor;

    /**
     * File handle where we write uploaded data.
     */
    FILE* fp;

    /**
     * HTTP response body we will return, NULL if not yet known.
     */
    const char* answerstring;

    /**
     * HTTP status code we will return, 0 for undecided.
     */
    unsigned int answercode;

    const char* filename;

    const char* sessionId;
};

const char* response_busy = "This server is busy, please try again later.";
const char* response_error = "This doesn't seem to be right.";
const char* response_servererror = "Invalid request.";
const char* response_fileioerror = "IO error writing to disk.";
const char* const response_postprocerror = "Error processing POST data";
const char* response_conversionerror = "Conversion error";
const char* response_success = "success";

static enum MHD_Result
sendResponseSuccess(struct MHD_Connection* connection)
{
    struct MHD_Response* response;
    MHD_Result ret = MHD_NO;

    response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static enum MHD_Result
sendResponseText(struct MHD_Connection* connection, const char* responseText, int status_code)
{
    struct MHD_Response* response;
    MHD_Result ret = MHD_NO;

    response = MHD_create_response_from_buffer(strlen(responseText), (void*)responseText, MHD_RESPMEM_PERSISTENT);

    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");
    ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);

    return ret;
}

static enum MHD_Result
sendResponseFloatArr(struct MHD_Connection *connection, std::vector<float> &floatArray)
{
	struct MHD_Response *response;
	MHD_Result ret = MHD_NO;

	if (0 < floatArray.size())
	{
		response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
		std::vector<float>().swap(floatArray);
	}
	else
	{
		return MHD_NO;
	}

	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;
}

template <typename List>
void split(const std::string& s, const std::string& delim, List& result)
{
    result.clear();

    using string = std::string;
    string::size_type pos = 0;

    while (pos != string::npos)
    {
        string::size_type p = s.find(delim, pos);

        if (p == string::npos)
        {
            result.push_back(s.substr(pos));
            break;
        }
        else {
            result.push_back(s.substr(pos, p - pos));
        }

        pos = p + delim.size();
    }
}

bool paramStrToStr(const char *key, std::string &sVal)
{
	sVal = s_mParams[std::string(key)];
	if (sVal.empty()) return false;

	return true;
}

bool paramStrToInt(const char *key, int &iVal)
{
	std::string sVal = s_mParams[std::string(key)];
	if (sVal.empty()) return false;

	iVal = std::atoi(sVal.c_str());

	return true;
}

bool paramStrToDbl(const char *key, double &dVal)
{
	std::string sVal = s_mParams[std::string(key)];
	if (sVal.empty()) return false;

	dVal = std::atof(sVal.c_str());

	return true;
}

bool paramStrToXYZ(const char* key, double*& dXYZ)
{
    std::string sVal = s_mParams[std::string(key)];
    if (sVal.empty()) return false;

    std::vector<std::string> strArr;
    split(sVal.c_str(), ",", strArr);

    int entCnt = strArr.size();

    if (3 != entCnt)
        return false;

    dXYZ = new double[entCnt];

    dXYZ[0] = std::atof(strArr[0].c_str());
    dXYZ[1] = std::atof(strArr[1].c_str());
    dXYZ[2] = std::atof(strArr[2].c_str());

    return true;
}

bool paramStrToChr(const char *key, char &cha)
{
	std::string sVal = s_mParams[std::string(key)];
	if (sVal.empty()) return false;

	cha = sVal.c_str()[0];

	return true;
}

static enum MHD_Result
iterate_post(void* coninfo_cls,
    enum MHD_ValueKind kind,
    const char* key,
    const char* filename,
    const char* content_type,
    const char* transfer_encoding,
    const char* data,
    uint64_t off,
    size_t size)
{
    struct connection_info_struct* con_info = (connection_info_struct*)coninfo_cls;
    FILE* fp;
    (void)kind;               /* Unused. Silent compiler warning. */
    (void)content_type;       /* Unused. Silent compiler warning. */
    (void)transfer_encoding;  /* Unused. Silent compiler warning. */
    (void)off;                /* Unused. Silent compiler warning. */

    // if (0 = strcmp(key, "file"))
    // {
    //     con_info->answerstring = response_servererror;
    //     con_info->answercode = MHD_HTTP_BAD_REQUEST;
    //     return MHD_YES;
    // }

    if (0 == strcmp(key, "file"))
    {
        if (!con_info->fp)
        {
            if (0 != con_info->answercode)   /* something went wrong */
                return MHD_YES;

            char filePath[FILENAME_MAX], workingDir[FILENAME_MAX];

            char lowext[64], extype[64];
            getLowerExtention(filename, lowext, extype);
            sprintf(s_pcModelName, "model.%s", lowext);

#ifndef _WIN32
            sprintf(filePath, "%s%s/%s", s_pcWorkingDir, con_info->sessionId, s_pcModelName);
#else
            sprintf(filePath, "%s%s\\%s", s_pcWorkingDir, con_info->sessionId, s_pcModelName);
#endif

            // Prepare working dir
            sprintf(workingDir, "%s%s", s_pcWorkingDir, con_info->sessionId);
#ifndef _WIN32
            if (0 != mkdir(workingDir, 0777))
#else
            if (0 != mkdir(workingDir))
#endif
            {
                con_info->answerstring = response_fileioerror;
                con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
                return MHD_YES;
            } 

            /* NOTE: This is technically a race with the 'fopen()' above,
            but there is no easy fix, short of moving to open(O_EXCL)
            instead of using fopen(). For the example, we do not care. */
            con_info->fp = fopen(filePath, "ab");

            if (!con_info->fp)
            {
                con_info->answerstring = response_fileioerror;
                con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
                return MHD_YES;
            }
        }

        if (size > 0)
        {
            if (!fwrite(data, sizeof(char), size, con_info->fp))
            {
                con_info->answerstring = response_fileioerror;
                con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
                return MHD_YES;
            }
        }
    }
    else if (size > 0)
    {
        s_mParams.insert(std::make_pair(std::string(key), std::string(data)));
        printf("key: %s, data: %s\n", key, data);
    }

    return MHD_YES;
}


static void
request_completed(void* cls,
    struct MHD_Connection* connection,
    void** con_cls,
    enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;
    (void)cls;         /* Unused. Silent compiler warning. */
    (void)connection;  /* Unused. Silent compiler warning. */
    (void)toe;         /* Unused. Silent compiler warning. */

    if (NULL == con_info)
        return;

    if (con_info->connectiontype == POST)
    {
        if (NULL != con_info->postprocessor)
        {
            MHD_destroy_post_processor(con_info->postprocessor);
            nr_of_uploading_clients--;
        }

        if (con_info->fp)
            fclose(con_info->fp);
    }

    free(con_info);
    *con_cls = NULL;
}


static enum MHD_Result
answer_to_connection(void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls)
{
    (void)cls;               /* Unused. Silent compiler warning. */
    (void)url;               /* Unused. Silent compiler warning. */
    (void)version;           /* Unused. Silent compiler warning. */
    
    if (NULL == *con_cls)
    {
        /* First call, setup data structures */
        struct connection_info_struct* con_info;
        
        if (nr_of_uploading_clients >= MAXCLIENTS)
            return sendResponseText(connection, response_busy, MHD_HTTP_OK);

        con_info = (connection_info_struct*)malloc(sizeof(struct connection_info_struct));
        if (NULL == con_info)
            return MHD_NO;
        con_info->answercode = 0;   /* none yet */
        con_info->fp = NULL;
        s_mParams.clear();

        printf("--- New %s request for %s using version %s\n", method, url, version);
        con_info->sessionId = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "session_id");
        printf("Session ID: %s\n", con_info->sessionId);

        if (0 == strcasecmp(method, MHD_HTTP_METHOD_POST))
        {
            con_info->postprocessor =
                MHD_create_post_processor(connection,
                    POSTBUFFERSIZE,
                    &iterate_post,
                    (void*)con_info);

            if (NULL == con_info->postprocessor)
            {
                free(con_info);
                return MHD_NO;
            }

            nr_of_uploading_clients++;

            con_info->connectiontype = POST;
        }
        else
        {
            con_info->connectiontype = GET;
        }

        *con_cls = (void*)con_info;

        return MHD_YES;
    }

    if (0 == strcasecmp(method, MHD_HTTP_METHOD_GET))
    {
        return sendResponseSuccess(connection);
    }

    if (0 == strcasecmp(method, MHD_HTTP_METHOD_POST))
    {
        struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;

        if (0 != *upload_data_size)
        {
            /* Upload not yet done */
            if (0 != con_info->answercode)
            {
                /* we already know the answer, skip rest of upload */
                *upload_data_size = 0;
                return MHD_YES;
            }
            if (MHD_YES !=
                MHD_post_process(con_info->postprocessor,
                    upload_data,
                    *upload_data_size))
            {
                con_info->answerstring = response_postprocerror;
                con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
            }
            *upload_data_size = 0;

            return MHD_YES;
        }
        /* Upload finished */
        if (0 == strcmp(url, "/Clear"))
        {
            // Delete SC model
            char scDir[FILENAME_MAX];
            sprintf(scDir, "%s%s", s_pcScModelsDir, con_info->sessionId);

#ifndef _WIN32
            delete_files(scDir);
            delete_dirs(scDir);
#else
            wchar_t wscDir[_MAX_FNAME];
            size_t iRet;
            mbstowcs_s(&iRet, wscDir, _MAX_FNAME, scDir, _MAX_FNAME);
            delete_dirs(wscDir);
#endif

            // Delete ModelFile
            pExProcess->DeleteModelFile(con_info->sessionId);

            // Delete Luminate bridge
            if (0 < m_mpLuminateBridge.count(con_info->sessionId))
            {
                m_mpLuminateBridge[con_info->sessionId]->shutdown();
                delete m_mpLuminateBridge[con_info->sessionId];

                m_mpLuminateBridge.erase(con_info->sessionId);
            }

            con_info->answerstring = response_success;
            con_info->answercode = MHD_HTTP_OK;
            return sendResponseText(connection, con_info->answerstring, con_info->answercode);
        }
        else if (0 == strcmp(url, "/SetOptions"))
        {
            int entityIds;
            if (!paramStrToInt("entityIds", entityIds)) return MHD_NO;

            int sewModel;
			if (!paramStrToInt("sewModel", sewModel)) return MHD_NO;

            double sewingTol;
            if (!paramStrToDbl("sewingTol", sewingTol)) return MHD_NO;

            pExProcess->SetOptions((bool) entityIds, (bool)sewModel, sewingTol);

            con_info->answerstring = response_success;
            con_info->answercode = MHD_HTTP_OK;
            return sendResponseText(connection, con_info->answerstring, con_info->answercode);
        }
        else if (0 == strcmp(url, "/FileUpload"))
        {
            if (NULL != con_info->fp)
            {
                fclose(con_info->fp);
                con_info->fp = NULL;
            }
            if (0 == con_info->answercode)
            {
                /* No errors encountered, declare success */
                // Convert to SC
                char basename[256], filePath[FILENAME_MAX], scPath[FILENAME_MAX], workingDir[FILENAME_MAX], scDir[FILENAME_MAX];
                getBaseName(s_pcModelName, basename);

#ifndef _WIN32
                sprintf(filePath, "%s%s/%s", s_pcWorkingDir, con_info->sessionId, s_pcModelName);
                sprintf(scPath, "%s%s/%s",s_pcScModelsDir ,con_info->sessionId, basename);
#else
                sprintf(filePath, "%s%s\\%s", s_pcWorkingDir, con_info->sessionId, s_pcModelName);
                sprintf(scPath, "%s%s\\%s", s_pcScModelsDir, con_info->sessionId, basename);
#endif
                sprintf(workingDir, "%s%s", s_pcWorkingDir, con_info->sessionId);
                sprintf(scDir, "%s%s", s_pcScModelsDir, con_info->sessionId);
                
#ifndef _WIN32
                if (0 != mkdir(scDir, 0777))
#else
                if (0 != mkdir(scDir))
#endif
                {
                    con_info->answerstring = response_fileioerror;
                    con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
                }

                printf("converting...\n");

                std::vector<float> floatArr = pExProcess->LoadFile(con_info->sessionId, filePath, scPath);
                con_info->answerstring = response_success;
                con_info->answercode = MHD_HTTP_OK;

                // Delete uploaded file and working dir
#ifndef _WIN32
                delete_files(workingDir);
                delete_dirs(workingDir);
#else
                wchar_t wscDir[_MAX_FNAME];
                size_t iRet;
                mbstowcs_s(&iRet, wscDir, _MAX_FNAME, workingDir, _MAX_FNAME);
                delete_dirs(wscDir);
#endif
                return sendResponseFloatArr(connection, floatArr);
            }
        }
        else if (0 == strcmp(url, "/PrepareRendering"))
        {
            double width, height;
            if (!paramStrToDbl("width", width));
            if (!paramStrToDbl("height", height));

            double* target, * up, * position;
            if (!paramStrToXYZ("target", target)) return MHD_NO;
            if (!paramStrToXYZ("up", up)) return MHD_NO;
            if (!paramStrToXYZ("position", position)) return MHD_NO;

            int projection;
            if (!paramStrToInt("projection", projection));

            double cameraW, cameraH;
            if (!paramStrToDbl("cameraW", cameraW));
            if (!paramStrToDbl("cameraH", cameraH));

            if (0 < m_mpLuminateBridge.count(con_info->sessionId))
            {
                m_mpLuminateBridge[con_info->sessionId]->shutdown();
                delete m_mpLuminateBridge[con_info->sessionId];

                m_mpLuminateBridge.erase(con_info->sessionId);
            }

            HCLuminateBridge* pHCLuminateBridge = new HCLuminateBridge();
            m_mpLuminateBridge[con_info->sessionId] = pHCLuminateBridge;

            CameraInfo cameraInfo = pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

            std::string filepath = "";
            if (pHCLuminateBridge->initialize(HOOPS_LICENSE, hWnd, width, height, filepath, cameraInfo))
                pHCLuminateBridge->draw();

            con_info->answerstring = response_success;
            con_info->answercode = MHD_HTTP_OK;
            return sendResponseText(connection, con_info->answerstring, con_info->answercode);

        }
        else if (0 == strcmp(url, "/Raytracing"))
        {
            double width, height;
            if (!paramStrToDbl("width", width));
            if (!paramStrToDbl("height", height));

            double *target, *up, *position;
            if (!paramStrToXYZ("target", target)) return MHD_NO;
            if (!paramStrToXYZ("up", up)) return MHD_NO;
            if (!paramStrToXYZ("position", position)) return MHD_NO;

            int projection;
            if (!paramStrToInt("projection", projection));

            double cameraW, cameraH;
            if (!paramStrToDbl("cameraW", cameraW));
            if (!paramStrToDbl("cameraH", cameraH));

            std::vector<MeshPropaties> aMeshProps = pExProcess->GetModelMesh(con_info->sessionId);

            if (0 == m_mpLuminateBridge.count(con_info->sessionId))
            {
                con_info->answerstring = response_error;
                con_info->answercode = MHD_HTTP_OK;
            }
            else
            {
                HCLuminateBridge* pHCLuminateBridge = m_mpLuminateBridge[con_info->sessionId];
                
                CameraInfo cameraInfo = pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

                pHCLuminateBridge->syncScene(aMeshProps, cameraInfo);

                con_info->answerstring = response_success;
                con_info->answercode = MHD_HTTP_OK;
            }

            return sendResponseText(connection, con_info->answerstring, con_info->answercode);
        }
        else if (0 == strcmp(url, "/Draw"))
        {
            std::vector<float> floatArr;
            
            if (0 == m_mpLuminateBridge.count(con_info->sessionId))
            {
                con_info->answerstring = response_error;
                con_info->answercode = MHD_HTTP_OK;
            }
            else
            {
                HCLuminateBridge* pHCLuminateBridge = m_mpLuminateBridge[con_info->sessionId];

                pHCLuminateBridge->draw();

                FrameStatistics statistics = pHCLuminateBridge->getFrameStatistics();

                floatArr.push_back(statistics.renderingIsDone);
                floatArr.push_back(statistics.renderingProgress);
                floatArr.push_back(statistics.remainingTimeMilliseconds);

                char filePath[FILENAME_MAX];
                sprintf(filePath, "%s%s.png", s_pcHtmlRootDir, con_info->sessionId);
                pHCLuminateBridge->saveImg(filePath);

                con_info->answerstring = response_success;
                con_info->answercode = MHD_HTTP_OK;
            }
            return sendResponseFloatArr(connection, floatArr);
        }
        else if (0 == strcmp(url, "/SyncCamera"))
        {
            if (0 == m_mpLuminateBridge.count(con_info->sessionId))
            {
                con_info->answerstring = response_error;
                con_info->answercode = MHD_HTTP_OK;
            }
            else
            {
                HCLuminateBridge* pHCLuminateBridge = m_mpLuminateBridge[con_info->sessionId];
                
                double* target, * up, * position;
                if (!paramStrToXYZ("target", target)) return MHD_NO;
                if (!paramStrToXYZ("up", up)) return MHD_NO;
                if (!paramStrToXYZ("position", position)) return MHD_NO;

                int projection;
                if (!paramStrToInt("projection", projection));

                double cameraW, cameraH;
                if (!paramStrToDbl("cameraW", cameraW));
                if (!paramStrToDbl("cameraH", cameraH));

                CameraInfo cameraInfo = pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

                pHCLuminateBridge->setSyncCamera(true, cameraInfo);

                con_info->answerstring = response_success;
                con_info->answercode = MHD_HTTP_OK;
            }

            return sendResponseText(connection, con_info->answerstring, con_info->answercode);
        }
    }

    return MHD_NO;
}

HWND GetConsoleHwnd()
{
#define MY_BUFSIZE 1024 // Buffer size for console window titles.
    HWND hwndFound;         // This is what is returned to the caller.
    wchar_t pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                        // WindowTitle.
    wchar_t pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                        // WindowTitle.

    // Fetch current window title.

    GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

    // Format a "unique" NewWindowTitle.

    wsprintf(pszNewWindowTitle, L"%d/%d",
        GetTickCount(),
        GetCurrentProcessId());

    // Change current window title.

    SetConsoleTitle(pszNewWindowTitle);

    // Ensure window title has been updated.

    Sleep(40);

    // Look for NewWindowTitle.

    hwndFound = FindWindow(NULL, pszNewWindowTitle);

    // Restore original window title.

    SetConsoleTitle(pszOldWindowTitle);

    return(hwndFound);
}

int
main(int argc, char** argv)
{
    if (argc != 2) {
        printf("%s PORT\n",
            argv[0]);
        return 1;
    }

    int iPort = atoi(argv[1]);
    printf("Bind to %d port\n", iPort);

    // Get working dir
    GetEnvironmentVariablePath("EX_SERVER_WORKING_DIR", s_pcWorkingDir, true);
    if (0 == strlen(s_pcWorkingDir))
        return 1;
    printf("EX_SERVER_WORKING_DIR=%s\n", s_pcWorkingDir);

    // Get SC model dir
    GetEnvironmentVariablePath("SC_MODELS_DIR", s_pcScModelsDir, true);
    if (0 == strlen(s_pcScModelsDir))
        return 1;
    printf("SC_MODELS_DIR=%s\n", s_pcScModelsDir);

    // Get html root dir
    GetEnvironmentVariablePath("HTML_ROOT_DIR", s_pcHtmlRootDir, true);
    if (0 == strlen(s_pcHtmlRootDir))
        return 1;
    printf("HTML_ROOT_DIR=%s\n", s_pcHtmlRootDir);


    pExProcess = new ExProcess();
    if (!pExProcess->Init())
    {
        printf("HOOPS Exchange loading Failed.\n");
        return 1;
    }
    printf("HOOPS Exchange Loaded.\n");

    hWnd = GetConsoleHwnd();

    bool licenseIsActive;
    RED_RC rc = setLicense(HOOPS_LICENSE, licenseIsActive);
    if (rc != RED_OK || !licenseIsActive)
        return false;
    printf("HOOPS Luminate Initialized.\n");

    struct MHD_Daemon* daemon;

    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD,
        iPort, NULL, NULL,
        &answer_to_connection, NULL,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed,
        NULL,
        MHD_OPTION_END);
    if (NULL == daemon)
    {
        fprintf(stderr,
            "Failed to start daemon.\n");
        return 1;
    }

    bool bFlg = true;
    while (bFlg)
    {
        if (getchar())
            bFlg = false;
    }
    MHD_stop_daemon(daemon);

    delete pExProcess;

    return 0;
}