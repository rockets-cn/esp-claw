/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "claw_cap.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLAW_EVENT_ROUTER_DEFAULT_RULES_PATH "/fatfs/data/automation/automations.json"

typedef enum {
    CLAW_EVENT_SESSION_POLICY_CHAT = 0,
    CLAW_EVENT_SESSION_POLICY_TRIGGER = 1,
    CLAW_EVENT_SESSION_POLICY_GLOBAL = 2,
    CLAW_EVENT_SESSION_POLICY_EPHEMERAL = 3,
    CLAW_EVENT_SESSION_POLICY_NOSAVE = 4,
} claw_event_session_policy_t;

typedef struct {
    char event_id[48];
    char source_cap[32];
    char event_type[32];
    char source_channel[16];
    char target_channel[16];
    char source_endpoint[64];
    char target_endpoint[96];
    char chat_id[96];
    char sender_id[96];
    char message_id[96];
    char correlation_id[96];
    char content_type[24];
    int64_t timestamp_ms;
    claw_event_session_policy_t session_policy;
    char *text;
    char *payload_json;
} claw_event_t;

typedef size_t (*claw_event_router_session_builder_fn)(const claw_event_t *event,
                                                       char *buf,
                                                       size_t buf_size,
                                                       void *user_ctx);

typedef esp_err_t (*claw_event_router_outbound_resolver_fn)(const claw_event_t *event,
                                                            const char *target_channel,
                                                            const char *target_endpoint,
                                                            char *cap_name,
                                                            size_t cap_name_size,
                                                            void *user_ctx);

typedef struct {
    const char *rules_path;
    size_t max_rules;
    size_t max_actions_per_rule;
    size_t cap_output_size;
    uint32_t event_queue_len;
    uint32_t task_stack_size;
    UBaseType_t task_priority;
    BaseType_t task_core;
    uint32_t core_submit_timeout_ms;
    uint32_t core_receive_timeout_ms;
    bool default_route_messages_to_agent;
    claw_event_router_session_builder_fn session_builder;
    void *session_builder_user_ctx;
    claw_event_router_outbound_resolver_fn outbound_resolver;
    void *outbound_resolver_user_ctx;
} claw_event_router_config_t;

typedef struct {
    bool matched;
    int matched_rules;
    int action_count;
    int failed_actions;
    int64_t handled_at_ms;
    char first_rule_id[64];
    char ack[256];
    claw_cap_event_route_t route;
    esp_err_t last_error;
} claw_event_router_result_t;

typedef enum {
    CLAW_EVENT_ROUTER_ACTION_CALL_CAP = 0,
    CLAW_EVENT_ROUTER_ACTION_RUN_AGENT = 1,
    CLAW_EVENT_ROUTER_ACTION_RUN_SCRIPT = 2,
    CLAW_EVENT_ROUTER_ACTION_SEND_MESSAGE = 3,
    CLAW_EVENT_ROUTER_ACTION_EMIT_EVENT = 4,
    CLAW_EVENT_ROUTER_ACTION_DROP = 5,
} claw_event_router_action_kind_t;

typedef struct {
    char event_type[96];
    char event_key[96];
    char source_cap[96];
    char channel[96];
    char chat_id[96];
    char content_type[96];
    char text[96];
} claw_event_router_match_t;

typedef struct {
    claw_event_router_action_kind_t kind;
    char cap[64];
    char *input_json;
    claw_cap_caller_t caller;
    bool capture_output;
    bool fail_open;
} claw_event_router_action_t;

typedef struct {
    bool enabled;
    bool consume_on_match;
    char id[64];
    char description[160];
    char ack[256];
    char *vars_json;
    claw_event_router_match_t match;
    claw_event_router_action_t *actions;
    size_t action_count;
} claw_event_router_rule_t;

esp_err_t claw_event_router_init(const claw_event_router_config_t *config);
esp_err_t claw_event_router_start(void);
esp_err_t claw_event_router_stop(void);
esp_err_t claw_event_router_reload(void);
esp_err_t claw_event_router_publish(const claw_event_t *event);
esp_err_t claw_event_router_publish_message(const char *source_cap,
                                            const char *channel,
                                            const char *chat_id,
                                            const char *text,
                                            const char *sender_id,
                                            const char *message_id);
esp_err_t claw_event_router_publish_trigger(const char *source_cap,
                                            const char *event_type,
                                            const char *event_key,
                                            const char *payload_json);
esp_err_t claw_event_router_register_outbound_binding(const char *channel,
                                                      const char *cap_name);
esp_err_t claw_event_router_handle_event(const claw_event_t *event,
                                         claw_event_router_result_t *out_result);
esp_err_t claw_event_router_list_rules(claw_event_router_rule_t **out_rules,
                                       size_t *out_rule_count);
esp_err_t claw_event_router_get_rule(const char *id, claw_event_router_rule_t *out_rule);
esp_err_t claw_event_router_add_rule(const claw_event_router_rule_t *rule);
esp_err_t claw_event_router_update_rule(const claw_event_router_rule_t *rule);
esp_err_t claw_event_router_delete_rule(const char *id);
esp_err_t claw_event_router_add_rule_json(const char *rule_json);
esp_err_t claw_event_router_update_rule_json(const char *rule_json);
esp_err_t claw_event_router_list_rules_json(char *output, size_t output_size);
esp_err_t claw_event_router_get_rule_json(const char *id, char *output, size_t output_size);
esp_err_t claw_event_router_get_last_result(claw_event_router_result_t *out_result);
void claw_event_router_free_rule(claw_event_router_rule_t *rule);
void claw_event_router_free_rule_list(claw_event_router_rule_t *rules, size_t rule_count);
void claw_event_router_free_event(claw_event_t *event);
size_t claw_event_router_build_session_id(const claw_event_t *event,
                                          char *buf,
                                          size_t buf_size);
const char *claw_event_router_session_policy_to_string(claw_event_session_policy_t policy);

#ifdef __cplusplus
}
#endif
