#include "modules/intercom/intercom_actor.h"

#include <stdbool.h>
#include <string.h>

#include "app/app_settings.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "modules/mic/mic_actor.h"
#include "modules/wifi/wifi.h"
#include "modules/ws/ws_actor.h"
#include "modules/ws/ws_client.h"

#define INTERCOM_ACTOR_POLL_TICKS pdMS_TO_TICKS(100)
#define INTERCOM_ACTOR_AUDIO_FRAME_WAIT_TICKS pdMS_TO_TICKS(INTERCOM_AUDIO_FRAME_MS)
#define INTERCOM_ACTOR_AUDIO_SEND_YIELD_TICKS pdMS_TO_TICKS(INTERCOM_AUDIO_SEND_YIELD_MS)
#define INTERCOM_ACTOR_MIC_STOP_WAIT_TICKS pdMS_TO_TICKS(INTERCOM_AUDIO_FRAME_MS * 3)
#define INTERCOM_TALK_ACK_TIMEOUT_TICKS pdMS_TO_TICKS(3000)

typedef struct {
	QueueHandle_t queue;
	TaskHandle_t task;
	msg_sub_handle_t sub;
	bool initialized;
	bool waiting_ack;
	bool cancel_after_ack;
	bool talking;
	uint16_t audio_seq;
	uint32_t audio_send_ok_count;
	uint32_t audio_send_fail_count;
	uint32_t audio_pop_timeout_count;
	uint32_t audio_pop_error_count;
	TickType_t ack_deadline;
} intercom_actor_ctx_t;

static intercom_actor_ctx_t s_actor = {
	.sub = MSG_SUB_HANDLE_INVALID,
};

static bool intercom_actor_can_request_talk(void)
{
	return app_settings.ws_enable &&
		   wifi_module_is_connected() &&
		   ws_client_is_connected();
}

static const char *intercom_actor_ack_name(int code)
{
	switch(code) {
		case MSG_INTERCOM_TALK_ACK_OK:
			return "ok";
		case MSG_INTERCOM_TALK_ACK_BUSY:
			return "busy";
		case MSG_INTERCOM_TALK_ACK_OFFLINE:
			return "offline";
		case MSG_INTERCOM_TALK_ACK_TIMEOUT:
			return "timeout";
		case MSG_INTERCOM_TALK_ACK_LOCAL_ERROR:
			return "local_error";
		default:
			return "unknown";
	}
}

static void intercom_actor_send_ack_code(int code)
{
	(void)msg_send_sys_value(
		MSG_SRC_INTERCOM,
		MSG_EVT_SYS_INTERCOM_TALK_START_ACK,
		code,
		0
	);
}

static void intercom_actor_send_ws_start(void)
{
	(void)msg_send_cmd_value(
		MSG_SRC_INTERCOM,
		MSG_EVT_CMD_WS_INTERCOM_TALK_START,
		1,
		0
	);
}

static void intercom_actor_send_ws_stop(void)
{
	(void)msg_send_cmd_value(
		MSG_SRC_INTERCOM,
		MSG_EVT_CMD_WS_INTERCOM_TALK_STOP,
		1,
		0
	);
}

static void intercom_actor_stop_audio(void)
{
	(void)mic_actor_stop(pdMS_TO_TICKS(100));
	vTaskDelay(INTERCOM_ACTOR_MIC_STOP_WAIT_TICKS);
	(void)mic_actor_deinit();
	LOG("intercom audio stopped: sent=%u send_fail=%u pop_timeout=%u pop_error=%u",
		(unsigned)s_actor.audio_send_ok_count,
		(unsigned)s_actor.audio_send_fail_count,
		(unsigned)s_actor.audio_pop_timeout_count,
		(unsigned)s_actor.audio_pop_error_count);
}

static void intercom_actor_reset_audio_counters(void)
{
	s_actor.audio_seq = 0;
	s_actor.audio_send_ok_count = 0;
	s_actor.audio_send_fail_count = 0;
	s_actor.audio_pop_timeout_count = 0;
	s_actor.audio_pop_error_count = 0;
}

static esp_err_t intercom_actor_start_audio(void)
{
	esp_err_t err = mic_actor_init();
	if(err != ESP_OK) {
		LOG("intercom audio start failed: step=mic_init err=%s", esp_err_to_name(err));
		return err;
	}

	err = mic_actor_set_sample_rate(INTERCOM_AUDIO_SAMPLE_RATE, pdMS_TO_TICKS(100));
	if(err != ESP_OK) {
		LOG("intercom audio start failed: step=set_sample_rate err=%s", esp_err_to_name(err));
		intercom_actor_stop_audio();
		return err;
	}

	err = mic_actor_start(pdMS_TO_TICKS(100));
	if(err != ESP_OK) {
		LOG("intercom audio start failed: step=mic_start err=%s", esp_err_to_name(err));
		intercom_actor_stop_audio();
		return err;
	}

	intercom_actor_reset_audio_counters();
	LOG("intercom audio started: sample_rate=%u frame_samples=%u",
		(unsigned)INTERCOM_AUDIO_SAMPLE_RATE,
		(unsigned)INTERCOM_AUDIO_FRAME_SAMPLES);
	return ESP_OK;
}

static void intercom_actor_start_request(void)
{
	if(s_actor.waiting_ack || s_actor.talking) {
		LOG("intercom talk start ignored: waiting_ack=%d talking=%d",
			(int)s_actor.waiting_ack,
			(int)s_actor.talking);
		return;
	}

	if(!intercom_actor_can_request_talk()) {
		LOG("intercom talk start rejected locally: ws_enable=%d wifi=%d ws=%d",
			(int)app_settings.ws_enable,
			(int)wifi_module_is_connected(),
			(int)ws_client_is_connected());
		intercom_actor_send_ack_code(MSG_INTERCOM_TALK_ACK_OFFLINE);
		return;
	}

	s_actor.waiting_ack = true;
	s_actor.cancel_after_ack = false;
	s_actor.ack_deadline = xTaskGetTickCount() + INTERCOM_TALK_ACK_TIMEOUT_TICKS;
	LOG("intercom talk start request sent: room=%s callsign=%s",
		app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default",
		app_settings.user_callsign[0] != '\0' ? app_settings.user_callsign : app_settings.ws_callsign);
	intercom_actor_send_ws_start();
}

static void intercom_actor_stop_request(void)
{
	if(s_actor.waiting_ack) {
		s_actor.cancel_after_ack = true;
		LOG("intercom talk stop deferred: waiting_ack=1");
		return;
	}

	if(s_actor.talking) {
		s_actor.talking = false;
		LOG("intercom talk stopping: sent=%u fail=%u pop_timeout=%u",
			(unsigned)s_actor.audio_send_ok_count,
			(unsigned)s_actor.audio_send_fail_count,
			(unsigned)s_actor.audio_pop_timeout_count);
		esp_err_t flush_err = ws_actor_flush_intercom_audio();
		if(flush_err != ESP_OK) {
			LOG("intercom audio flush failed: %s", esp_err_to_name(flush_err));
		}
		intercom_actor_stop_audio();
		intercom_actor_send_ws_stop();
		(void)msg_send_sys_text(MSG_SRC_INTERCOM, MSG_EVT_SYS_INTERCOM_SPEAKER_CHANGED, "", 0);
	}
}

static void intercom_actor_handle_talk_ack(const msg_t *msg)
{
	if(msg == NULL || !s_actor.waiting_ack) {
		return;
	}

	int ack_code = msg->data.value;
	bool ok = ack_code > 0;
	s_actor.waiting_ack = false;
	LOG("intercom talk ack received: code=%d reason=%s cancel_after_ack=%d",
		ack_code,
		intercom_actor_ack_name(ack_code),
		(int)s_actor.cancel_after_ack);

	if(!ok) {
		s_actor.cancel_after_ack = false;
		s_actor.talking = false;
		return;
	}

	if(s_actor.cancel_after_ack) {
		s_actor.cancel_after_ack = false;
		s_actor.talking = false;
		LOG("intercom talk ack accepted after release, sending stop");
		intercom_actor_send_ws_stop();
		return;
	}

	if(intercom_actor_start_audio() != ESP_OK) {
		s_actor.talking = false;
		intercom_actor_send_ws_stop();
		intercom_actor_send_ack_code(MSG_INTERCOM_TALK_ACK_LOCAL_ERROR);
		return;
	}

	s_actor.talking = true;
}

static void intercom_actor_apply_msg(const msg_t *msg)
{
	if(msg == NULL) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_CMD_INTERCOM_TALK_START_REQ:
			intercom_actor_start_request();
			break;

		case MSG_EVT_CMD_INTERCOM_TALK_STOP:
			intercom_actor_stop_request();
			break;

		case MSG_EVT_SYS_INTERCOM_TALK_START_ACK:
			intercom_actor_handle_talk_ack(msg);
			break;

		case MSG_EVT_SYS_WS_DISCONNECTED:
		case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
		case MSG_EVT_SYS_WIFI_DISCONNECTED:
			LOG("intercom reset by connection event: event=%d waiting_ack=%d talking=%d",
				(int)msg->event,
				(int)s_actor.waiting_ack,
				(int)s_actor.talking);
			s_actor.waiting_ack = false;
			s_actor.cancel_after_ack = false;
			if(s_actor.talking) {
				intercom_actor_stop_audio();
			}
			s_actor.talking = false;
			break;

		default:
			break;
	}
}

static void intercom_actor_check_timeout(void)
{
	if(!s_actor.waiting_ack) {
		return;
	}

	TickType_t now = xTaskGetTickCount();
	if((int32_t)(now - s_actor.ack_deadline) < 0) {
		return;
	}

	s_actor.waiting_ack = false;
	s_actor.cancel_after_ack = false;
	s_actor.talking = false;
	LOG("intercom talk ack timeout");
	intercom_actor_send_ack_code(MSG_INTERCOM_TALK_ACK_TIMEOUT);
}

static void intercom_actor_poll_audio(void)
{
	if(!s_actor.talking) {
		return;
	}

	mic_frame_t frame = {0};
	esp_err_t err = mic_actor_pop_frame(&frame, INTERCOM_ACTOR_AUDIO_FRAME_WAIT_TICKS);
	if(err == ESP_ERR_TIMEOUT) {
		s_actor.audio_pop_timeout_count++;
		if(s_actor.audio_pop_timeout_count <= 3 || (s_actor.audio_pop_timeout_count % 1000U) == 0U) {
			LOG("intercom mic frame timeout: count=%u sent=%u",
				(unsigned)s_actor.audio_pop_timeout_count,
				(unsigned)s_actor.audio_send_ok_count);
		}
		return;
	}
	if(err != ESP_OK) {
		s_actor.audio_pop_error_count++;
		if(s_actor.audio_pop_error_count <= 5 || (s_actor.audio_pop_error_count % 50U) == 0U) {
			LOG("intercom mic frame pop failed: err=%s count=%u sent=%u",
				esp_err_to_name(err),
				(unsigned)s_actor.audio_pop_error_count,
				(unsigned)s_actor.audio_send_ok_count);
		}
		return;
	}

	uint16_t seq = s_actor.audio_seq++;
	err = ws_actor_send_intercom_audio_frame(
		frame.samples,
		frame.sample_count,
		seq,
		frame.sample_rate
	);
	if(err == ESP_OK) {
		s_actor.audio_send_ok_count++;
		if(s_actor.audio_send_ok_count <= 5 || (s_actor.audio_send_ok_count % 50U) == 0U) {
			LOG("intercom audio sent: seq=%u samples=%u sample_rate=%u sent=%u",
				(unsigned)seq,
				(unsigned)frame.sample_count,
				(unsigned)frame.sample_rate,
				(unsigned)s_actor.audio_send_ok_count);
		}
	} else {
		s_actor.audio_send_fail_count++;
		if(s_actor.audio_send_fail_count <= 5 || (s_actor.audio_send_fail_count % 50U) == 0U) {
			LOG("intercom audio send failed: seq=%u err=%s fail=%u sent=%u",
				(unsigned)seq,
				esp_err_to_name(err),
				(unsigned)s_actor.audio_send_fail_count,
				(unsigned)s_actor.audio_send_ok_count);
		}
	}

	if(INTERCOM_ACTOR_AUDIO_SEND_YIELD_TICKS > 0) {
		vTaskDelay(INTERCOM_ACTOR_AUDIO_SEND_YIELD_TICKS);
	}
}

static void intercom_actor_task(void *arg)
{
	(void)arg;

	while(1) {
		msg_t msg = {0};
		while(xQueueReceive(s_actor.queue, &msg, 0) == pdTRUE) {
			intercom_actor_apply_msg(&msg);
		}

		intercom_actor_check_timeout();

		if(s_actor.talking) {
			intercom_actor_poll_audio();
		} else {
			if(xQueueReceive(s_actor.queue, &msg, INTERCOM_ACTOR_POLL_TICKS) == pdTRUE) {
				intercom_actor_apply_msg(&msg);
			}
		}
	}
}

static esp_err_t intercom_actor_subscribe(void)
{
	const msg_topic_t topics[] = {
		MSG_TOPIC_INTERCOM_CMD,
		MSG_TOPIC_INTERCOM_EVENT,
		MSG_TOPIC_WIFI_EVENT,
		MSG_TOPIC_WEBSOCKET_EVENT,
	};
	return msg_sub(s_actor.queue, topics, sizeof(topics) / sizeof(topics[0]), &s_actor.sub);
}

esp_err_t intercom_actor_init(void)
{
	if(s_actor.initialized) {
		return ESP_OK;
	}

	esp_err_t err = msg_actor_queue_create_with_len(INTERCOM_ACTOR_QUEUE_LEN, &s_actor.queue);
	if(err != ESP_OK) {
		return err;
	}

	err = intercom_actor_subscribe();
	if(err != ESP_OK) {
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
		return err;
	}

	BaseType_t ok = xTaskCreatePinnedToCore(
		intercom_actor_task,
		"intercom_actor",
		INTERCOM_ACTOR_TASK_STACK,
		NULL,
		TASK_PRIO_CON,
		&s_actor.task,
		TASK_CORE_CON
	);
	if(ok != pdPASS) {
		(void)msg_unsub(s_actor.sub, NULL, 0);
		s_actor.sub = MSG_SUB_HANDLE_INVALID;
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
		return ESP_FAIL;
	}

	s_actor.initialized = true;
	return ESP_OK;
}

esp_err_t intercom_actor_deinit(void)
{
	if(!s_actor.initialized) {
		return ESP_OK;
	}

	if(s_actor.sub != MSG_SUB_HANDLE_INVALID) {
		(void)msg_unsub(s_actor.sub, NULL, 0);
		s_actor.sub = MSG_SUB_HANDLE_INVALID;
	}

	if(s_actor.task != NULL) {
		vTaskDelete(s_actor.task);
		s_actor.task = NULL;
	}

	if(s_actor.queue != NULL) {
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
	}

	memset(&s_actor, 0, sizeof(s_actor));
	s_actor.sub = MSG_SUB_HANDLE_INVALID;
	return ESP_OK;
}
