#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "report.h"
#include "debug.h"
#include "config.h"

#define URL_SIZE    256
#define PATH_SIZE   512
#define BUF_SIZE    2560
#define API_URL     "https://api.telegram.org/bot"

typedef enum ReportModeE {
    Markdown,
    MarkdownV2,
    Html,
    Document
} ReportMode;

typedef struct ResponseDataS {
    uint32_t cnt;
    size_t size;
    char buf[BUF_SIZE];
} ResponseData;

typedef struct ReportDataS {
    char msg[BUF_SIZE];
    char doc[PATH_SIZE];
    ReportMode mode;
    uint32_t chatId;
    uint32_t responseTo;
} ReportData;


const char *codename(CURLcode code);

static size_t report_write_chunk (unsigned char *ptr, size_t size, size_t nmemb, void *data) {
    ResponseData *cd = (ResponseData *) (data);
    size_t ret = 0;
    size_t got = size * nmemb;
    if((cd->size + got) < BUF_SIZE) {
        memcpy(&(cd->buf[cd->size]), ptr, got);
        cd->cnt++;
        cd->size += got;
        logTrc("Chunk: %lu, total: %lu", got, cd->size);
        ret = got;
    }
    return ret;
}

static void * report_query (void *ptr) {
    CURL *curl;
    CURLM *multi_handle;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;
    int still_running = 0;
    CURLcode ret;
    ReportData *rd = (ReportData *) (ptr);
    ResponseData cd = {0};
    char url[URL_SIZE + 1] = {0};
    json_t *data = json_object(), *mode;
    switch(rd->mode) {
        case Markdown: mode = json_string("Markdown"); break;
        case MarkdownV2: mode = json_string("MarkdownV2"); break;
        case Html: mode = json_string("HTML"); break;
        default: mode = json_string("Markdown"); break;
    }
    json_object_set_new(data, "chat_id", json_integer(rd->chatId));
    json_object_set_new(data, "text", json_string(rd->msg));
    json_object_set_new(data, "parse_mode", mode);

    if(rd->responseTo) {
        json_t *reply = json_object();
        json_object_set_new(reply, "message_id", json_integer(rd->responseTo));
        json_object_set_new(data, "reply_parameters", reply);
    }
    const char *post = json_dumps(data, JSON_COMPACT);
    int sz = strlen(post);

    struct curl_slist *slist = NULL;
    slist = curl_slist_append(slist, "Content-type: application/json; charset=utf8");

    if (rd->mode == Document) {
        snprintf(url, URL_SIZE, "%s%s/sendDocument?chat_id=%u", API_URL, API_KEY, rd->chatId);
        logTrc("TG_DOC: %s", url);

        curl = curl_easy_init();
        multi_handle = curl_multi_init();
        if (curl && multi_handle) {
            form = curl_mime_init(curl);

            field = curl_mime_addpart(form);
            ret = curl_mime_name(field, "document");
            logTrc("document %s %s", codename(ret), curl_easy_strerror(ret));
            ret = curl_mime_filedata(field, rd->doc);
            logTrc("filedata %s %s", codename(ret), curl_easy_strerror(ret));

            field = curl_mime_addpart(form);
            ret = curl_mime_name(field, "caption");
            logTrc("caption %s %s", codename(ret), curl_easy_strerror(ret));
            ret = curl_mime_data(field, rd->msg, CURL_ZERO_TERMINATED);
            logTrc("data %s %s", codename(ret), curl_easy_strerror(ret));

            if(rd->responseTo) {
                char par[64];
                sprintf(par, "{\"message_id\":%u}", rd->responseTo);
                curl_mime_name(field, "reply_parameters");
                curl_mime_data(field, par, CURL_ZERO_TERMINATED);
            }

            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0);
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cd);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, report_write_chunk);

            curl_multi_add_handle(multi_handle, curl);

            do {
                CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

                if(still_running)
                    /* wait for activity, timeout or "nothing" */
                    mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);
                if(mc)
                    break;
            } while(still_running);

            curl_multi_cleanup(multi_handle);

            /* always cleanup */
            curl_easy_cleanup(curl);

            /* then cleanup the form */
            curl_mime_free(form);

            logTrc ("CURL FIN [chunks=%d, size=%ld]", cd.cnt, cd.size);
            logTrc (cd.buf);

            /* always cleanup */
            curl_easy_cleanup(curl);
            //
        }
    } else {
        logTrc("TG: %s", post);
        snprintf(url, URL_SIZE, "%s%s/sendMessage", API_URL, API_KEY);
        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sz);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cd);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, report_write_chunk);

            ret = curl_easy_perform(curl);
            logTrc ("CURL ret = %d (%s) [chunks=%d, size=%ld]", ret, codename(ret), cd.cnt, cd.size);
            logTrc (cd.buf);

            /* always cleanup */
            curl_easy_cleanup(curl);
        }

    }

    curl_slist_free_all(slist);
    json_decref(data);
    free(ptr);

    return NULL;
}

int send_report(uint32_t chat, char *msg, uint32_t responseTo) {
    if(!chat) return -1;
    if(!msg) return -2;
    pthread_t pid;
    ReportData *rep = malloc(sizeof(ReportData));
    strcpy(rep->msg, msg);
    rep->chatId = chat;
    rep->responseTo = responseTo;
    rep->mode = Markdown;
    return pthread_create(&pid, NULL, report_query, rep);
}

int send_document(uint32_t chat, char *path, char *caption, uint32_t responseTo) {
    if(!chat) return -1;
    if(!path) return -2;
    if(!caption) return -3;
    pthread_t pid;
    ReportData *rep = malloc(sizeof(ReportData));
    strcpy(rep->msg, caption);
    strcpy(rep->doc, path);
    rep->chatId = chat;
    rep->responseTo = responseTo;
    rep->mode = Document;
    return pthread_create(&pid, NULL, report_query, rep);
/*
    private static function postFile($chat_id, $filepath, $filename, $caption = '') {
        $strUrl = self::API_URL . TELEGRAM_KEY . "/sendDocument?chat_id={$chat_id}" ;
        $ch = curl_init($strUrl);
        curl_setopt($ch, CURLOPT_HEADER, false);
        curl_setopt($ch, CURLOPT_POST, true);
        // curl_setopt(
        //     $ch,
        //     CURLOPT_HTTPHEADER,
        //     array('Content-type: application/json; charset=utf8')
        // );

        $finfo = finfo_file(finfo_open(FILEINFO_MIME_TYPE), $filepath);
        $cFile = new CURLFile($filepath, $finfo, $filename);

        $data = ['document'  => $cFile ];
        if($caption) {
            $data['caption'] = $caption;
        }

        curl_setopt($ch, CURLOPT_POSTFIELDS,     $data);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
        curl_setopt($ch, CURLOPT_TIMEOUT,        50);
        $res = curl_exec($ch);
        $err = curl_error($ch);
        curl_close($ch);
        $ret = false;
        $mid = 0;
        if($res) {
            $ret = json_decode($res);
        }
        if(!empty($ret) && is_object($ret)) {
            //$ok  = property_exists($ret, 'ok') ? $ret->ok : false;
            if(property_exists($ret, 'result')) {
                if(property_exists($ret->result, 'message_id')) {
                    $mid = $ret->result->message_id;
                }
            }
        }
        self::$error  = $err;
        self::$result = $res;
        self::$return = $ret;
        return $mid;
    }
*/
}

const char *codename(CURLcode code) {
    switch(code) {
        case CURLE_OK: return "CURLE_OK"; break;
        case CURLE_UNSUPPORTED_PROTOCOL: return "CURLE_UNSUPPORTED_PROTOCOL"; break;
        case CURLE_FAILED_INIT: return "CURLE_FAILED_INIT"; break;
        case CURLE_URL_MALFORMAT: return "CURLE_URL_MALFORMAT"; break;
        case CURLE_NOT_BUILT_IN: return "CURLE_NOT_BUILT_IN"; break;
        case CURLE_COULDNT_RESOLVE_PROXY: return "CURLE_COULDNT_RESOLVE_PROXY"; break;
        case CURLE_COULDNT_RESOLVE_HOST: return "CURLE_COULDNT_RESOLVE_HOST"; break;
        case CURLE_COULDNT_CONNECT: return "CURLE_COULDNT_CONNECT"; break;
        case CURLE_WEIRD_SERVER_REPLY: return "CURLE_WEIRD_SERVER_REPLY"; break;
        case CURLE_REMOTE_ACCESS_DENIED: return "CURLE_REMOTE_ACCESS_DENIED"; break;
        case CURLE_FTP_ACCEPT_FAILED: return "CURLE_FTP_ACCEPT_FAILED"; break;
        case CURLE_FTP_WEIRD_PASS_REPLY: return "CURLE_FTP_WEIRD_PASS_REPLY"; break;
        case CURLE_FTP_ACCEPT_TIMEOUT: return "CURLE_FTP_ACCEPT_TIMEOUT"; break;
        case CURLE_FTP_WEIRD_PASV_REPLY: return "CURLE_FTP_WEIRD_PASV_REPLY"; break;
        case CURLE_FTP_WEIRD_227_FORMAT: return "CURLE_FTP_WEIRD_227_FORMAT"; break;
        case CURLE_FTP_CANT_GET_HOST: return "CURLE_FTP_CANT_GET_HOST"; break;
        case CURLE_HTTP2: return "CURLE_HTTP2"; break;
        case CURLE_FTP_COULDNT_SET_TYPE: return "CURLE_FTP_COULDNT_SET_TYPE"; break;
        case CURLE_PARTIAL_FILE: return "CURLE_PARTIAL_FILE"; break;
        case CURLE_FTP_COULDNT_RETR_FILE: return "CURLE_FTP_COULDNT_RETR_FILE"; break;
        case CURLE_OBSOLETE20: return "CURLE_OBSOLETE20"; break;
        case CURLE_QUOTE_ERROR: return "CURLE_QUOTE_ERROR"; break;
        case CURLE_HTTP_RETURNED_ERROR: return "CURLE_HTTP_RETURNED_ERROR"; break;
        case CURLE_WRITE_ERROR: return "CURLE_WRITE_ERROR"; break;
        case CURLE_OBSOLETE24: return "CURLE_OBSOLETE24"; break;
        case CURLE_UPLOAD_FAILED: return "CURLE_UPLOAD_FAILED"; break;
        case CURLE_READ_ERROR: return "CURLE_READ_ERROR"; break;
        case CURLE_OUT_OF_MEMORY: return "CURLE_OUT_OF_MEMORY"; break;
        case CURLE_OPERATION_TIMEDOUT: return "CURLE_OPERATION_TIMEDOUT"; break;
        case CURLE_OBSOLETE29: return "CURLE_OBSOLETE29"; break;
        case CURLE_FTP_PORT_FAILED: return "CURLE_FTP_PORT_FAILED"; break;
        case CURLE_FTP_COULDNT_USE_REST: return "CURLE_FTP_COULDNT_USE_REST"; break;
        case CURLE_OBSOLETE32: return "CURLE_OBSOLETE32"; break;
        case CURLE_RANGE_ERROR: return "CURLE_RANGE_ERROR"; break;
        case CURLE_HTTP_POST_ERROR: return "CURLE_HTTP_POST_ERROR"; break;
        case CURLE_SSL_CONNECT_ERROR: return "CURLE_SSL_CONNECT_ERROR"; break;
        case CURLE_BAD_DOWNLOAD_RESUME: return "CURLE_BAD_DOWNLOAD_RESUME"; break;
        case CURLE_FILE_COULDNT_READ_FILE: return "CURLE_FILE_COULDNT_READ_FILE"; break;
        case CURLE_LDAP_CANNOT_BIND: return "CURLE_LDAP_CANNOT_BIND"; break;
        case CURLE_LDAP_SEARCH_FAILED: return "CURLE_LDAP_SEARCH_FAILED"; break;
        case CURLE_OBSOLETE40: return "CURLE_OBSOLETE40"; break;
        case CURLE_FUNCTION_NOT_FOUND: return "CURLE_FUNCTION_NOT_FOUND"; break;
        case CURLE_ABORTED_BY_CALLBACK: return "CURLE_ABORTED_BY_CALLBACK"; break;
        case CURLE_BAD_FUNCTION_ARGUMENT: return "CURLE_BAD_FUNCTION_ARGUMENT"; break;
        case CURLE_OBSOLETE44: return "CURLE_OBSOLETE44"; break;
        case CURLE_INTERFACE_FAILED: return "CURLE_INTERFACE_FAILED"; break;
        case CURLE_OBSOLETE46: return "CURLE_OBSOLETE46"; break;
        case CURLE_TOO_MANY_REDIRECTS: return "CURLE_TOO_MANY_REDIRECTS"; break;
        case CURLE_UNKNOWN_OPTION: return "CURLE_UNKNOWN_OPTION"; break;
        case CURLE_TELNET_OPTION_SYNTAX: return "CURLE_SETOPT_OPTION_SYNTAX"; break;
        // case CURLE_SETOPT_OPTION_SYNTAX: return "CURLE_SETOPT_OPTION_SYNTAX"; break;
        case CURLE_OBSOLETE50: return "CURLE_OBSOLETE50"; break;
        case CURLE_OBSOLETE51: return "CURLE_OBSOLETE51"; break;
        case CURLE_GOT_NOTHING: return "CURLE_GOT_NOTHING"; break;
        case CURLE_SSL_ENGINE_NOTFOUND: return "CURLE_SSL_ENGINE_NOTFOUND"; break;
        case CURLE_SSL_ENGINE_SETFAILED: return "CURLE_SSL_ENGINE_SETFAILED"; break;
        case CURLE_SEND_ERROR: return "CURLE_SEND_ERROR"; break;
        case CURLE_RECV_ERROR: return "CURLE_RECV_ERROR"; break;
        case CURLE_OBSOLETE57: return "CURLE_OBSOLETE57"; break;
        case CURLE_SSL_CERTPROBLEM: return "CURLE_SSL_CERTPROBLEM"; break;
        case CURLE_SSL_CIPHER: return "CURLE_SSL_CIPHER"; break;
        case CURLE_PEER_FAILED_VERIFICATION: return "CURLE_PEER_FAILED_VERIFICATION"; break;
        case CURLE_BAD_CONTENT_ENCODING: return "CURLE_BAD_CONTENT_ENCODING"; break;
        // case CURLE_OBSOLETE62: return "CURLE_OBSOLETE62"; break;
        case CURLE_FILESIZE_EXCEEDED: return "CURLE_FILESIZE_EXCEEDED"; break;
        case CURLE_USE_SSL_FAILED: return "CURLE_USE_SSL_FAILED"; break;
        case CURLE_SEND_FAIL_REWIND: return "CURLE_SEND_FAIL_REWIND"; break;
        case CURLE_SSL_ENGINE_INITFAILED: return "CURLE_SSL_ENGINE_INITFAILED"; break;
        case CURLE_LOGIN_DENIED: return "CURLE_LOGIN_DENIED"; break;
        case CURLE_TFTP_NOTFOUND: return "CURLE_TFTP_NOTFOUND"; break;
        case CURLE_TFTP_PERM: return "CURLE_TFTP_PERM"; break;
        case CURLE_REMOTE_DISK_FULL: return "CURLE_REMOTE_DISK_FULL"; break;
        case CURLE_TFTP_ILLEGAL: return "CURLE_TFTP_ILLEGAL"; break;
        case CURLE_TFTP_UNKNOWNID: return "CURLE_TFTP_UNKNOWNID"; break;
        case CURLE_REMOTE_FILE_EXISTS: return "CURLE_REMOTE_FILE_EXISTS"; break;
        case CURLE_TFTP_NOSUCHUSER: return "CURLE_TFTP_NOSUCHUSER"; break;
        // case CURLE_OBSOLETE75: return "CURLE_OBSOLETE75"; break;
        // case CURLE_OBSOLETE76: return "CURLE_OBSOLETE76"; break;
        case CURLE_SSL_CACERT_BADFILE: return "CURLE_SSL_CACERT_BADFILE"; break;
        case CURLE_REMOTE_FILE_NOT_FOUND: return "CURLE_REMOTE_FILE_NOT_FOUND"; break;
        case CURLE_SSH: return "CURLE_SSH"; break;
        case CURLE_SSL_SHUTDOWN_FAILED: return "CURLE_SSL_SHUTDOWN_FAILED"; break;
        case CURLE_AGAIN: return "CURLE_AGAIN"; break;
        case CURLE_SSL_CRL_BADFILE: return "CURLE_SSL_CRL_BADFILE"; break;
        case CURLE_SSL_ISSUER_ERROR: return "CURLE_SSL_ISSUER_ERROR"; break;
        case CURLE_FTP_PRET_FAILED: return "CURLE_FTP_PRET_FAILED"; break;
        case CURLE_RTSP_CSEQ_ERROR: return "CURLE_RTSP_CSEQ_ERROR"; break;
        case CURLE_RTSP_SESSION_ERROR: return "CURLE_RTSP_SESSION_ERROR"; break;
        case CURLE_FTP_BAD_FILE_LIST: return "CURLE_FTP_BAD_FILE_LIST"; break;
        case CURLE_CHUNK_FAILED: return "CURLE_CHUNK_FAILED"; break;
        case CURLE_NO_CONNECTION_AVAILABLE: return "CURLE_NO_CONNECTION_AVAILABLE"; break;
        case CURLE_SSL_PINNEDPUBKEYNOTMATCH: return "CURLE_SSL_PINNEDPUBKEYNOTMATCH"; break;
        case CURLE_SSL_INVALIDCERTSTATUS: return "CURLE_SSL_INVALIDCERTSTATUS"; break;
        case CURLE_HTTP2_STREAM: return "CURLE_HTTP2_STREAM"; break;
        case CURLE_RECURSIVE_API_CALL: return "CURLE_RECURSIVE_API_CALL"; break;
        case CURLE_AUTH_ERROR: return "CURLE_AUTH_ERROR"; break;
        case CURLE_HTTP3: return "CURLE_HTTP3"; break;
        case CURLE_QUIC_CONNECT_ERROR: return "CURLE_QUIC_CONNECT_ERROR"; break;
        case CURLE_PROXY: return "CURLE_PROXY"; break;
        // case CURLE_SSL_CLIENTCERT: return "CURLE_SSL_CLIENTCERT"; break;
        // case CURLE_UNRECOVERABLE_POLL: return "CURLE_UNRECOVERABLE_POLL"; break;
        default: break;
    }
    return "<unknown>";
}