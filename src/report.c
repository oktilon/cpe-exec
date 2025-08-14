#include <curl/curl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
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
    CURLcode ret;
    ReportData *rd = (ReportData *) (ptr);
    ResponseData cd = {0};
    char url[URL_SIZE + 1] = {0};
    FILE *fd = NULL;
    struct stat file_info;
    json_t *data = json_object(), *mode;
    switch(rd->mode) {
        case Markdown: mode = json_string("Markdown"); break;
        case MarkdownV2: mode = json_string("MarkdownV2"); break;
        case Html: mode = json_string("HTML"); break;
        default: mode = json_string("Markdown"); break;
    }
    json_object_set_new(data, "chat_id", json_integer(rd->chatId));
    if (rd->mode == Document) {
        fd = fopen(rd->doc, "rb");
        if (fd) {
            if(fstat(fileno(fd), &file_info) != 0) {
                logErr("Can't stat file[%s]", rd->doc);
                fclose(fd);
                fd = NULL;
            }
        } else {
            logErr("Cant open doc[%s]", rd->doc);
        }
    } else {
        json_object_set_new(data, "text", json_string(rd->msg));
        json_object_set_new(data, "parse_mode", mode);
    }
    if(rd->responseTo) {
        json_t *reply = json_object();
        json_object_set_new(reply, "message_id", json_integer(rd->responseTo));
        json_object_set_new(data, "reply_parameters", reply);
    }
    const char *post = json_dumps(data, JSON_COMPACT);
    int sz = strlen(post);
    logTrc("TG: %s", post);

    snprintf(url, URL_SIZE, "%s%s/sendMessage", API_URL, API_KEY);

    struct curl_slist *slist = NULL;
    slist = curl_slist_append(slist, "Content-type: application/json; charset=utf8");

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
        if (fd) {
            logTrc("Upload file %p sz:%lu", fd, file_info.st_size);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_READDATA, fd);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
        }

        ret = curl_easy_perform(curl);
        (void)ret;
        logTrc ("CURL ret = %d [chunks=%d, size=%ld]", ret, cd.cnt, cd.size);
        logTrc (cd.buf);

        /* always cleanup */
        curl_easy_cleanup(curl);
        if (fd) {
            fclose(fd);
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