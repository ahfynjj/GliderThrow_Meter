/**
 * @file    esp_mad_task_http_server.c
 * @author  Alain Désandré - alain.desandre@wanadoo.fr
 * @date    November 18th 2018
 * @brief   This file include all the code to deal with a simple http server.
 *
 * @details HTTP server deal with the various http request from the clients.
 *			At the fist reception of an http get from one client, the server
 *			respond with the requested uri (see WebsiteFiles/ for the various uri).
 *          esp_mad_task_http_server deals with :
 *              - nvs intialisation
 *              - dhcp server intialisation
 *              - wifi driver initialisation in soft AP mode
 *              - when AP station is started, the http server is launched and the uri handles are setup
 *          Uris are embedded in the .rodata DRAM segment (see CMakeList.txt)
 *          Main HTML page is WebsiteFiles/esp.html and used bootstrap framework and jquery.
 *          Ressources for bootstrap and jquery are minified version in WebsiteFiles/
 *
 * @remarks 1/08/2025 - Ported to NETIF
 *          NETIF component is a successor of the tcpip_adapter, former network interface abstraction,
 *          which has become deprecated since IDF v4.1.
 *          esp_event_loop has been also deprecated. esp_event is the new component.
 *
 */

/*-----------------------------------------
 *-            INCLUDES
 *-----------------------------------------*/
#include <esp_wifi.h>
// #include <esp_event_loop.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <math.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"
#include "esp_mac.h"
#include <Esp_mad.h>
#include <Esp_mad_Globals_Variables.h>

#define SENSOR_JSON_BUF_SIZE 512

/*-----------------------------------------
 *-            LOCALS VARIABLES
 *-----------------------------------------*/
float travel2 = 0.0;
float angle2 = 0.0;

float voltage2 = 0.0;

extern const uint8_t esp_html_start[] asm("_binary_esp_html_start");
extern const uint8_t esp_html_end[] asm("_binary_esp_html_end");

extern const uint8_t bootstrap_min_css_start[] asm("_binary_bootstrap_min_css_start");
extern const uint8_t bootstrap_min_css_end[] asm("_binary_bootstrap_min_css_end");

extern const uint8_t bootstrap_min_js_start[] asm("_binary_bootstrap_min_js_start");
extern const uint8_t bootstrap_min_js_end[] asm("_binary_bootstrap_min_js_end");

extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[] asm("_binary_jquery_3_3_1_min_js_end");

static const char *TAG = "Esp_Server->";
static httpd_handle_t s_http_server = NULL;
static esp_netif_t *s_ap_netif = NULL;

/**
 *	@fn 	    esp_err_t main_page_get_handler (httpd_req_t *req)
 *	@brief 		An HTTP GET handler for the main esp.html page.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return		ESP_OK
 */
esp_err_t main_page_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entering ----> main_page_get-handler()\n");

    httpd_resp_set_type(req, "text/html");

    httpd_resp_send(req, (const char *)esp_html_start, (esp_html_end - 1) - esp_html_start);

    ESP_LOGI(TAG, "Exit    ----> main_page_get-handler()\n");

    return ESP_OK;

} /* end main_page_get_handler() */

httpd_uri_t main_page = {

    .uri = "/",

    .method = HTTP_GET,

    .handler = main_page_get_handler,

    .user_ctx = NULL

};

/**
 *	@fn 	    esp_err_t bootstrap_min_css_handler (httpd_req_t *req)
 *	@brief 		An HTTP GET handler for bootstrap.min.css uri.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return		ESP_OK
 */
esp_err_t bootstrap_min_css_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entering ----> bootstrap_min_css_handler()\n");

    httpd_resp_set_type(req, "text/css");

    httpd_resp_send(req, (const char *)bootstrap_min_css_start, (bootstrap_min_css_end - 1) - bootstrap_min_css_start);

    ESP_LOGI(TAG, "Exit    ----> bootstrap_min_css_handler()\n");

    return ESP_OK;

} /* end bootstrap_min_css_handler() */

httpd_uri_t bootstrap_min_css_uri = {

    .uri = "/bootstrap.min.css",

    .method = HTTP_GET,

    .handler = bootstrap_min_css_handler,

    .user_ctx = NULL

};

/**
 *	@fn 	    esp_err_t bootstrap_min_js_handler (httpd_req_t *req)
 *	@brief 		An HTTP GET handler for bootstrap.min.js uri.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return		ESP_OK.
 */
esp_err_t bootstrap_min_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entering ----> Bootstrap_min_js Requested()\n");

    httpd_resp_set_type(req, "application/javascript");

    httpd_resp_send(req, (const char *)bootstrap_min_js_start, (bootstrap_min_js_end - 1) - bootstrap_min_js_start);

    ESP_LOGI(TAG, "Exit    ----> Bootstrap_min_js Requested()\n");

    return ESP_OK;

} /* end bootstrap_min_js_handler() */

httpd_uri_t bootstrap_min_js_uri = {

    .uri = "/bootstrap.min.js",

    .method = HTTP_GET,

    .handler = bootstrap_min_js_handler,

    .user_ctx = NULL

};

/**
 *	@fn 	    esp_err_t jquery_3_3_1_min_js_handler (httpd_req_t *req)
 *	@brief 		An HTTP GET handler for jquery.3.3.1.min.js uri.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return		ESP_OK.
 */
esp_err_t jquery_3_3_1_min_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entering ----> jquery_3_3_1_min_js Requested()\n");

    httpd_resp_set_type(req, "application/javascript");

    httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, (jquery_3_3_1_min_js_end - 1) - jquery_3_3_1_min_js_start);

    ESP_LOGI(TAG, "Exit     ----> jquery_3_3_1_min_js Requested()\n");

    return ESP_OK;

} /* end jquery_3_3_1_min_js_handler() */

httpd_uri_t jquery_3_3_1_min_js_uri = {

    .uri = "/jquery-3.3.1.min.js",

    .method = HTTP_GET,

    .handler = jquery_3_3_1_min_js_handler,

    .user_ctx = NULL};

/**
 *	@fn 	    esp_err_t sensors_get_handler (httpd_req_t *req)
 *	@brief 		An HTTP GET handler to serve, travel, angle and delta of both sensors
 *	@param[in]	*req : an http_req_t pointer.
 *	@return		ESP_OK
 */
esp_err_t sensors_get_handler(httpd_req_t *req)
{

    char buf[SENSOR_JSON_BUF_SIZE];

    char *host_header = NULL;
    size_t buf_len;

    float voltage1 = 0.0;

    float relativeTravel1;
    float relativeTravel2;
    float relativeAngle1;
    float relativeAngle2;
    float targetDiff = 0.0f;
    bool targetEnabled = g_targetAngleActive;

    /* Get header value string length and allocate memory for length + 1,

     * extra byte for null termination */

    ESP_LOGI(TAG, "Entering ----> sensor_get_handler()\n");

    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;

    if (buf_len > 1)
    {

        host_header = malloc(buf_len);

        /* Copy null terminated value string into buffer */

        if (host_header && httpd_req_get_hdr_value_str(req, "Host", host_header, buf_len) == ESP_OK)
        {

            ESP_LOGI(TAG, "Found header => Host: %s", host_header);
        }

        free(host_header);
    }

    memset(buf, 0, sizeof(buf));

    /*--- Compute Min, Max and Deltas for both sensors ---*/
    relativeTravel1 = g_travel - g_travelZeroOffset;
    relativeTravel2 = travel2 - g_travel2ZeroOffset;
    relativeAngle1 = g_angle - g_angleZeroOffset;
    relativeAngle2 = angle2 - g_angle2ZeroOffset;

    if (targetEnabled)
    {
        targetDiff = fabsf(relativeAngle1 - g_targetAngle);
    }

    /*--- compute voltage in volt ---*/
    voltage1 = g_voltage / 1000.0;

    ESP_LOGI(TAG, "voltage1 %f - voltage2 %f", voltage1, voltage2);

    /*--- Preparing the buffer request in json format ---*/
    int len = snprintf(buf, SENSOR_JSON_BUF_SIZE, "{\"travel1\":%0.1f,\"travel2\":%0.1f,\"angle1\":%0.1f,\"angle2\":%0.1f,\"voltage1\":%0.2f, \"voltage2\":%0.2f,\"targetAngle\":%0.2f,\"targetDiff\":%0.2f,\"targetEnabled\":%d}",
                       relativeTravel1,
                       relativeTravel2,
                       relativeAngle1,
                       relativeAngle2,
                       voltage1,
                       voltage2,
                       g_targetAngle,
                       targetDiff,
                       targetEnabled ? 1 : 0);

    if (len < 0 || len >= SENSOR_JSON_BUF_SIZE)
    {
        ESP_LOGE(TAG, "Sensor JSON truncated (len=%d)", len);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON truncated");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[len = %d]  \n", len);

    ESP_LOGI(TAG, "json = %s\n", buf);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    /*--- Send the request ---*/
    httpd_resp_send(req, buf, len);

    ESP_LOGI(TAG, "Exit    ----> sensor_get_handler()\n");

    return ESP_OK;

} /* end sensors_get_handler() */

httpd_uri_t sensors = {

    .uri = "/sensors",

    .method = HTTP_GET,

    .handler = sensors_get_handler,

    .user_ctx = NULL

};

/**
 *	@fn 	    esp_err_t chord_post_handler (httpd_req_t *req)
 *	@brief 		An HTTP POST handler.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return
 *      - ESP_OK
 *      - ESP_FAIL
 */
esp_err_t sensor2_post_handler(httpd_req_t *req)
{

    char buf[40];

    int ret, remaining = req->content_len;
    cJSON *sensor2_json = NULL;
    const cJSON *json_angle = NULL;
    const cJSON *json_voltage = NULL;

    ESP_LOGI(TAG, "Entering ----> sensor2_post_handler()\n");
    ESP_LOGI(TAG, "method: %d\n", req->method);
    ESP_LOGI(TAG, "uri: %s\n", req->uri);

    memset(buf, 0, sizeof(buf) - 1);

    while (remaining > 0)
    {

        /* Read the data for the request */

        if ((ret = httpd_req_recv(req, buf,

                                  MIN(remaining, sizeof(buf)))) <= 0)
        {

            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {

                /* Retry receiving if timeout occurred */

                continue;
            }

            return ESP_FAIL;
        }

        /* Log data received */

        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");

        ESP_LOGI(TAG, "%.*s", ret, buf);

        ESP_LOGI(TAG, "====================================");

        /*--- Parse Json buffer received form client ---*/
        sensor2_json = cJSON_Parse(buf);
        json_angle = cJSON_GetObjectItemCaseSensitive(sensor2_json, "angle");
        angle2 = json_angle->valuedouble;
        json_voltage = cJSON_GetObjectItemCaseSensitive(sensor2_json, "voltage");
        voltage2 = json_voltage->valuedouble;
        cJSON_Delete(sensor2_json);

        /*--- Compute travel2 ---*/
        travel2 = g_chordControlSurface * sin((angle2 * (2.0 * PI) / 360.0) / 2.0) * 2.0;

        ESP_LOGI(TAG, "angle2 : %.1f - travel2 : %.1f - voltage2 : %.2f\n", angle2, travel2, voltage2);

        remaining -= ret;
    }

    ESP_LOGI(TAG, "Exit ----> sensor2_post_handler()\n");

    char response[96];
    int resp_len = snprintf(response, sizeof(response), "{\"targetAngle\":%0.2f,\"targetActive\":%d}", g_targetAngle, g_targetAngleActive ? 1 : 0);
    if (resp_len < 0 || resp_len >= (int)sizeof(response))
    {
        resp_len = snprintf(response, sizeof(response), "{\"targetAngle\":0.0,\"targetActive\":0}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, resp_len);

    return ESP_OK;

} /* end sensor2_post_handler() */

httpd_uri_t sensor2 = {

    .uri = "/sensor2",

    .method = HTTP_POST,

    .handler = sensor2_post_handler,

    .user_ctx = NULL

};

esp_err_t target_angle_post_handler(httpd_req_t *req)
{
    char buf[64];
    int ret;
    int remaining = req->content_len;
    int offset = 0;

    ESP_LOGI(TAG, "Entering ----> target_angle_post_handler()\n");

    memset(buf, 0, sizeof(buf));

    while (remaining > 0)
    {
        if (offset >= (int)(sizeof(buf) - 1))
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
            return ESP_FAIL;
        }

        ret = httpd_req_recv(req, buf + offset, MIN(remaining, (int)(sizeof(buf) - 1 - offset)));
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive body");
            return ESP_FAIL;
        }
        remaining -= ret;
        offset += ret;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const cJSON *target_angle_value = cJSON_GetObjectItemCaseSensitive(root, "targetAngle");
    if (!cJSON_IsNumber(target_angle_value))
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "targetAngle missing");
        return ESP_FAIL;
    }

    g_targetAngle = target_angle_value->valuedouble;
    g_targetAngleActive = true;

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    ESP_LOGI(TAG, "Exit ----> target_angle_post_handler()\n");

    return ESP_OK;
}

httpd_uri_t target_angle_uri = {

    .uri = "/target_angle",

    .method = HTTP_POST,

    .handler = target_angle_post_handler,

    .user_ctx = NULL

};

esp_err_t reset_post_handler(httpd_req_t *req)
{

    ESP_LOGI(TAG, "Entering ----> reset_post_handler()\n");

    g_travelZeroOffset = g_travel;
    g_travel2ZeroOffset = travel2;
    g_angleZeroOffset = g_angle;
    g_angle2ZeroOffset = angle2;

    const char *resp = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    ESP_LOGI(TAG, "Exit ----> reset_post_handler()\n");

    return ESP_OK;
}

httpd_uri_t reset_uri = {

    .uri = "/reset",

    .method = HTTP_POST,

    .handler = reset_post_handler,

    .user_ctx = NULL

};

static const char *task_state_to_string(eTaskState state)
{
    switch (state)
    {
    case eRunning:
        return "running";
    case eReady:
        return "ready";
    case eBlocked:
        return "blocked";
    case eSuspended:
        return "suspended";
    case eDeleted:
        return "deleted";
#if (INCLUDE_vTaskSuspend == 1)
    case eInvalid:
        return "invalid";
#endif
    default:
        return "unknown";
    }
}

esp_err_t runtime_stats_get_handler(httpd_req_t *req)
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));

    if (task_status_array == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Allocation error");
        return ESP_FAIL;
    }

    uint32_t total_run_time = 0;
    UBaseType_t valid_tasks = uxTaskGetSystemState(task_status_array, task_count, &total_run_time);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        vPortFree(task_status_array);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON allocation error");
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "total_runtime_ticks", total_run_time);
    cJSON_AddNumberToObject(root, "tasks_reported", valid_tasks);

    cJSON *tasks = cJSON_AddArrayToObject(root, "tasks");
    if (tasks == NULL)
    {
        cJSON_Delete(root);
        vPortFree(task_status_array);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON allocation error");
        return ESP_FAIL;
    }

    for (UBaseType_t i = 0; i < valid_tasks; ++i)
    {
        const TaskStatus_t *status = &task_status_array[i];
        cJSON *task_obj = cJSON_CreateObject();
        if (task_obj == NULL)
        {
            continue;
        }

        double cpu_percent = (total_run_time > 0)
                                 ? ((double)status->ulRunTimeCounter * 100.0 / (double)total_run_time)
                                 : 0.0;

        cJSON_AddStringToObject(task_obj, "name", status->pcTaskName);
        cJSON_AddNumberToObject(task_obj, "runtime_ticks", status->ulRunTimeCounter);
        cJSON_AddNumberToObject(task_obj, "cpu_percent", cpu_percent);
        cJSON_AddStringToObject(task_obj, "state", task_state_to_string(status->eCurrentState));
        cJSON_AddNumberToObject(task_obj, "priority", status->uxCurrentPriority);
        cJSON_AddNumberToObject(task_obj, "stack_high_water_mark", status->usStackHighWaterMark);
#if (configNUMBER_OF_CORES > 1)
        cJSON_AddNumberToObject(task_obj, "core_id", status->xCoreID);
#else
        cJSON_AddNumberToObject(task_obj, "core_id", 0);
#endif

        cJSON_AddItemToArray(tasks, task_obj);
    }

    vPortFree(task_status_array);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload == NULL)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, payload, strlen(payload));

    cJSON_free(payload);
    cJSON_Delete(root);

    return ESP_OK;
}

httpd_uri_t runtime_stats = {
    .uri = "/runtime_stats",
    .method = HTTP_GET,
    .handler = runtime_stats_get_handler,
    .user_ctx = NULL};

/**
 *	@fn 	    esp_err_t chord_post_handler (httpd_req_t *req)
 *	@brief 		An HTTP POST handler.
 *	@param[in]	*req : un http_req_t pointer.
 *	@return
 *      - ESP_OK
 *      - ESP_FAIL
 */
esp_err_t chord_post_handler(httpd_req_t *req)
{

    char buf[50];

    char param[3];

    int oldchordValue = g_chordControlSurface;

    int ret, remaining = req->content_len;

    int iTemp;

    ESP_LOGI(TAG, "Entering ----> chord_post_handler()\n");
    ESP_LOGI(TAG, "method: %d\n", req->method);
    ESP_LOGI(TAG, "uri: %s\n", req->uri);

    while (remaining > 0)
    {

        /* Read the data for the request */

        if ((ret = httpd_req_recv(req, buf,

                                  MIN(remaining, sizeof(buf)))) <= 0)
        {

            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {

                /* Retry receiving if timeout occurred */

                continue;
            }

            return ESP_FAIL;
        }

        /* Log data received */

        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");

        ESP_LOGI(TAG, "%.*s", ret, buf);

        ESP_LOGI(TAG, "====================================");

        param[0] = buf[11];
        param[1] = buf[12];
        param[2] = '\0';

        iTemp = atoi(param);

        if (iTemp > 0)
        {

            g_chordControlSurface = iTemp;

            sprintf(buf, "Changing chord from %d mm to %d mm\n", oldchordValue, g_chordControlSurface);
        }
        else
            sprintf(buf, "ERROR : chord must be a positive value\n");

        /* Send response to the client */
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, buf, strlen(buf));

        remaining -= ret;
    }

    ESP_LOGI(TAG, "Exit    ----> chord_post_handler()\n");

    return ESP_OK;

} /* end chord_post_handler() */

httpd_uri_t chord = {

    .uri = "/chord",

    .method = HTTP_POST,

    .handler = chord_post_handler,

    .user_ctx = NULL

};

/**
 *	@fn 	    httpd_handle_t start_webserver (void)
 *	@brief 		Start the http server and stet the uris handles
 *	@param[in]	void
 *	@return		NULL or a pointer on http_handle_t server.
 */
httpd_handle_t start_webserver(void)
{

    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;

    // Start the httpd server

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {

        // Set URI handlers

        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &main_page);

        httpd_register_uri_handler(server, &bootstrap_min_css_uri);

        httpd_register_uri_handler(server, &bootstrap_min_js_uri);

        httpd_register_uri_handler(server, &jquery_3_3_1_min_js_uri);

        httpd_register_uri_handler(server, &chord);

        httpd_register_uri_handler(server, &sensors);

        httpd_register_uri_handler(server, &sensor2);

        httpd_register_uri_handler(server, &target_angle_uri);

        httpd_register_uri_handler(server, &reset_uri);

        httpd_register_uri_handler(server, &runtime_stats);

        return server;
    }

    ESP_LOGI(TAG, "Error starting httpd server!");

    return NULL;
}

/**
 *	@fn 	    vod stop_webserver (httpd_handle_t server)
 *	@brief 		stop the httpd web server.
 *	@param[in]	http_handle_t server.
 *	@return		void.
 */
void stop_webserver(httpd_handle_t server)
{

    // Stop the httpd server

    httpd_stop(server);

} /* end stop_webserver() */

/**
 *	@fn 	    esp_err_t wifi_event_handler(void *ctx, system_event_t *event).
 *	@brief 		task launch the function to initialize handler event .
 *	@param[in]	*ctx : httpd_handler_t pointer.
 *	@param[in]	*event : system_event_t event pointer.
 *	@return		ESP_OK
 */
static void wifi_event_handler(void *ctx, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{

    httpd_handle_t *server = (httpd_handle_t *)ctx;

    if (event_base != WIFI_EVENT || server == NULL)
    {
        return;
    }

    switch (event_id)
    {

    case WIFI_EVENT_AP_START:

        ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START:ESP32 is started in AP mode\n");

        if (*server == NULL)
        {

            *server = start_webserver();
        }

        break;

    case WIFI_EVENT_AP_STACONNECTED:

        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED\n");

        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;

        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d/n", MAC2STR(event->mac), (int)event->aid);

        break;

    case WIFI_EVENT_AP_STADISCONNECTED:

        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED\n");

        wifi_event_ap_stadisconnected_t *event1 = (wifi_event_ap_stadisconnected_t *)event_data;

        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d/n", MAC2STR(event1->mac), (int)event1->aid);

        break;

    case WIFI_EVENT_AP_STOP:

        ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP:ESP32 is stop in AP mode\n");

        if (*server)
        {

            stop_webserver(*server);

            *server = NULL;
        }
        break;

    default:

        ESP_LOGI(TAG, "UNKNOW_EVENT:event_id = %d\n", (int)event_id);

        break;
    }

} /* end event_handler() */

/**
 *	@fn 	    static void start_dhcp_server(void *arg);
 *	@brief 		DHCP Server initialisation.
 *	@param[in]	void*
 *	@return		void.
 */
/*
static void start_dhcp_server()
{

    // initialize the tcp stack

    tcpip_adapter_init();

    // stop DHCP server

    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

    // assign a static IP to the network interface

    tcpip_adapter_ip_info_t info;

    memset(&info, 0, sizeof(info));

    IP4_ADDR(&info.ip, 192, 168, 1, 1);

    // ESP acts as router, so gw addr will be its own addr
    IP4_ADDR(&info.gw, 192, 168, 1, 1);

    IP4_ADDR(&info.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));

    // start the DHCP server

    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    ESP_LOGI(TAG, "DHCP server started \n");
}
*/

/**
 *	@fn 	    static void initialise_wifi_in_ap(void *arg);
 *	@brief 		wifi initialisation.
 *	@param[in]	void*
 *	@return		void.
 */
static void initialise_wifi_in_ap(void)
{

    /*--- disable wifi driver logging ---*/
    esp_log_level_set("wifi", ESP_LOG_NONE);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (s_ap_netif == NULL)
    {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(s_ap_netif ? ESP_OK : ESP_FAIL);
    }

    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(ip_info));
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    esp_err_t dhcps_ret = esp_netif_dhcps_stop(s_ap_netif);
    if (dhcps_ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_ERROR_CHECK(dhcps_ret);
    }
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(s_ap_netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        &s_http_server,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_WIFI_SSID,
            .ssid_len = strlen(AP_WIFI_SSID),
            .channel = 0,
            .max_connection = 3,
            .authmode = WIFI_AUTH_OPEN,
            .beacon_interval = 100},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.ap.ssid);

} /* end initialise_wifi_in_ap */

/**
 *	@fn 	    task_http_server.
 *	@brief 		task launch the function to initialize the http server .
 *	@param[in]	void*
 *	@return		void.
 */
void task_http_server(void *ignore)

{

    /*--- Initialize nvs partition ---*/
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    /*--- start dhcp_server to serve stations ---*/
 //   start_dhcp_server();

    /*--- start wifi driver in AP mode ---*/
    initialise_wifi_in_ap();

    /*--- infinite loop to serve wifi event and http request ---*/
    while (1)
    {
        vTaskDelay(300 / portTICK_PERIOD_MS);
    } /* end while() */

    /*--- this part will not be executed but we stick the free rtos reco. ---*/
    vTaskDelete(NULL);
}