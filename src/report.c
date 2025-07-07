#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "report.h"
#include "debug.h"
#include "config.h"

#define URL_SIZE    256
#define BUF_SIZE    2560
#define API_URL     "https://api.telegram.org/bot"

typedef enum ReportModeE {
    Markdown,
    MarkdownV2,
    Html
} ReportMode;

typedef struct ResponseDataS {
    uint32_t cnt;
    size_t size;
    char buf[BUF_SIZE];
} ResponseData;

typedef struct ReportDataS {
    char msg[BUF_SIZE];
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

        ret = curl_easy_perform(curl);
        (void)ret;
        logTrc ("CURL ret = %d [chunks=%d, size=%ld]", ret, cd.cnt, cd.size);
        logTrc (cd.buf);

        /* always cleanup */
        curl_easy_cleanup(curl);

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